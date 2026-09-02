/* SPDX-License-Identifier: Apache-2.0 */
#include "mol/engine.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define MOL_ENGINE_MAGIC UINT32_C(0x4D4F4C45)
#define MOL_MASTER_GAIN_DEFAULT 0.25f
#define MOL_SUSTAIN_LEVEL 0.70f

typedef enum mol_voice_stage {
  MOL_VOICE_IDLE = 0,
  MOL_VOICE_ATTACK = 1,
  MOL_VOICE_DECAY = 2,
  MOL_VOICE_SUSTAIN = 3,
  MOL_VOICE_RELEASE = 4
} mol_voice_stage_t;

typedef struct mol_voice {
  mol_gesture_id_t gesture_id;
  mol_frame_index_t started_at;
  float phase;
  float phase_increment;
  float envelope;
  float release_step;
  float velocity;
  mol_voice_stage_t stage;
  uint8_t note;
} mol_voice_t;

typedef struct mol_scheduled_command {
  mol_command_t command;
  uint64_t serial;
} mol_scheduled_command_t;

struct mol_engine {
  uint32_t magic;
  mol_engine_config_t config;
  mol_frame_index_t current_frame;
  uint64_t submit_serial;
  size_t memory_size;
  mol_voice_t* voices;
  mol_scheduled_command_t* commands;
  mol_event_t* events;
  uint32_t command_count;
  uint32_t event_head;
  uint32_t event_count;
  float master_gain;
};

static int mol_engine_config_is_valid(const mol_engine_config_t* config) {
  if (config == NULL || config->struct_size < sizeof(*config) ||
      config->api_version != MOL_API_VERSION) {
    return 0;
  }
  if (config->sample_rate != 32000u && config->sample_rate != 44100u &&
      config->sample_rate != 48000u) {
    return 0;
  }
  if ((config->channel_count != 1u && config->channel_count != 2u) ||
      config->max_voices < 8u || config->max_voices > MOL_PROFILE_MAX_VOICES ||
      config->command_capacity == 0u || config->event_capacity == 0u) {
    return 0;
  }
  return 1;
}

static int mol_engine_is_valid(const mol_engine_t* engine) {
  return engine != NULL && engine->magic == MOL_ENGINE_MAGIC;
}

static size_t mol_max_size(size_t left, size_t right) { return left > right ? left : right; }

static int mol_size_add_array(size_t* size, size_t alignment, size_t element_size,
                              size_t count) {
  size_t padding;
  size_t bytes;
  if (size == NULL || alignment == 0u || element_size == 0u || count == 0u) {
    return 0;
  }
  padding = (alignment - (*size % alignment)) % alignment;
  if (*size > SIZE_MAX - padding || count > SIZE_MAX / element_size) {
    return 0;
  }
  bytes = count * element_size;
  if (*size + padding > SIZE_MAX - bytes) {
    return 0;
  }
  *size += padding + bytes;
  return 1;
}

static void* mol_arena_take(unsigned char* memory, size_t* offset, size_t alignment,
                            size_t element_size, size_t count) {
  size_t padding = (alignment - (*offset % alignment)) % alignment;
  void* result = memory + *offset + padding;
  *offset += padding + (element_size * count);
  return result;
}

static int mol_command_precedes(const mol_scheduled_command_t* left,
                                const mol_scheduled_command_t* right) {
  if (left->command.target_frame != right->command.target_frame) {
    return left->command.target_frame < right->command.target_frame;
  }
  return left->serial < right->serial;
}

static void mol_command_heap_push(mol_engine_t* engine,
                                  const mol_scheduled_command_t* scheduled) {
  uint32_t child = engine->command_count++;
  engine->commands[child] = *scheduled;
  while (child != 0u) {
    uint32_t parent = (child - 1u) / 2u;
    mol_scheduled_command_t temporary;
    if (mol_command_precedes(&engine->commands[parent], &engine->commands[child])) {
      break;
    }
    temporary = engine->commands[parent];
    engine->commands[parent] = engine->commands[child];
    engine->commands[child] = temporary;
    child = parent;
  }
}

static mol_scheduled_command_t mol_command_heap_pop(mol_engine_t* engine) {
  mol_scheduled_command_t first = engine->commands[0];
  uint32_t parent = 0u;
  --engine->command_count;
  if (engine->command_count == 0u) {
    return first;
  }
  engine->commands[0] = engine->commands[engine->command_count];
  for (;;) {
    uint32_t left = parent * 2u + 1u;
    uint32_t right = left + 1u;
    uint32_t smallest;
    mol_scheduled_command_t temporary;
    if (left >= engine->command_count) {
      break;
    }
    smallest = left;
    if (right < engine->command_count &&
        mol_command_precedes(&engine->commands[right], &engine->commands[left])) {
      smallest = right;
    }
    if (mol_command_precedes(&engine->commands[parent], &engine->commands[smallest])) {
      break;
    }
    temporary = engine->commands[parent];
    engine->commands[parent] = engine->commands[smallest];
    engine->commands[smallest] = temporary;
    parent = smallest;
  }
  return first;
}

static void mol_push_note_event(mol_engine_t* engine, mol_event_type_t event_type,
                                const mol_voice_t* voice) {
  mol_event_t* event;
  uint32_t tail;
  if (engine->event_count >= engine->config.event_capacity) {
    return;
  }
  tail = (engine->event_head + engine->event_count) % engine->config.event_capacity;
  event = &engine->events[tail];
  memset(event, 0, sizeof(*event));
  event->struct_size = (uint32_t)sizeof(*event);
  event->api_version = MOL_API_VERSION;
  event->event_type = event_type;
  event->frame = engine->current_frame;
  event->gesture_id = voice->gesture_id;
  event->payload[0] = voice->note;
  ++engine->event_count;
}

static float mol_note_frequency(uint8_t note) {
  return 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
}

static void mol_voice_release(mol_engine_t* engine, mol_voice_t* voice) {
  if (voice->stage != MOL_VOICE_IDLE && voice->stage != MOL_VOICE_RELEASE) {
    voice->stage = MOL_VOICE_RELEASE;
    voice->release_step = voice->envelope / (0.20f * (float)engine->config.sample_rate);
    mol_push_note_event(engine, MOL_EVENT_NOTE_RELEASED, voice);
  }
}

static mol_voice_t* mol_allocate_voice(mol_engine_t* engine) {
  mol_voice_t* selected = &engine->voices[0];
  uint32_t index;
  for (index = 0u; index < engine->config.max_voices; ++index) {
    mol_voice_t* candidate = &engine->voices[index];
    if (candidate->stage == MOL_VOICE_IDLE) {
      return candidate;
    }
    if (candidate->started_at < selected->started_at) {
      selected = candidate;
    }
  }
  mol_push_note_event(engine, MOL_EVENT_VOICE_STOLEN, selected);
  return selected;
}

static void mol_process_note_on(mol_engine_t* engine, const mol_command_t* command) {
  mol_voice_t* voice = mol_allocate_voice(engine);
  memset(voice, 0, sizeof(*voice));
  voice->gesture_id = command->gesture_id;
  voice->started_at = engine->current_frame;
  voice->phase_increment = mol_note_frequency(command->payload.note.note) /
                           (float)engine->config.sample_rate;
  voice->velocity = command->payload.note.velocity;
  voice->stage = MOL_VOICE_ATTACK;
  voice->note = command->payload.note.note;
  mol_push_note_event(engine, MOL_EVENT_NOTE_STARTED, voice);
}

static void mol_process_command(mol_engine_t* engine, const mol_command_t* command) {
  uint32_t index;
  switch (command->command_type) {
    case MOL_COMMAND_NOTE_ON:
      mol_process_note_on(engine, command);
      break;
    case MOL_COMMAND_NOTE_OFF:
      for (index = 0u; index < engine->config.max_voices; ++index) {
        if (engine->voices[index].stage != MOL_VOICE_IDLE &&
            engine->voices[index].gesture_id == command->gesture_id) {
          mol_voice_release(engine, &engine->voices[index]);
        }
      }
      break;
    case MOL_COMMAND_ALL_NOTES_OFF:
      for (index = 0u; index < engine->config.max_voices; ++index) {
        mol_voice_release(engine, &engine->voices[index]);
      }
      break;
    case MOL_COMMAND_ALL_SOUND_OFF:
      memset(engine->voices, 0, sizeof(*engine->voices) * engine->config.max_voices);
      break;
    case MOL_COMMAND_SET_MASTER_GAIN:
      engine->master_gain = command->payload.scalar.value;
      break;
    case MOL_COMMAND_RESET_ENGINE:
      mol_engine_reset(engine);
      break;
    default:
      break;
  }
}

static float mol_poly_blep(float phase, float phase_increment) {
  if (phase < phase_increment) {
    float position = phase / phase_increment;
    return position + position - position * position - 1.0f;
  }
  if (phase > 1.0f - phase_increment) {
    float position = (phase - 1.0f) / phase_increment;
    return position * position + position + position + 1.0f;
  }
  return 0.0f;
}

static float mol_render_voice(mol_engine_t* engine, mol_voice_t* voice) {
  float sample;
  if (voice->stage == MOL_VOICE_IDLE) {
    return 0.0f;
  }
  sample = (2.0f * voice->phase) - 1.0f - mol_poly_blep(voice->phase, voice->phase_increment);
  voice->phase += voice->phase_increment;
  if (voice->phase >= 1.0f) {
    voice->phase -= 1.0f;
  }

  switch (voice->stage) {
    case MOL_VOICE_ATTACK:
      voice->envelope += 1.0f / (0.005f * (float)engine->config.sample_rate);
      if (voice->envelope >= 1.0f) {
        voice->envelope = 1.0f;
        voice->stage = MOL_VOICE_DECAY;
      }
      break;
    case MOL_VOICE_DECAY:
      voice->envelope -=
          (1.0f - MOL_SUSTAIN_LEVEL) / (0.10f * (float)engine->config.sample_rate);
      if (voice->envelope <= MOL_SUSTAIN_LEVEL) {
        voice->envelope = MOL_SUSTAIN_LEVEL;
        voice->stage = MOL_VOICE_SUSTAIN;
      }
      break;
    case MOL_VOICE_RELEASE:
      voice->envelope -= voice->release_step;
      if (voice->envelope <= 0.0f) {
        voice->envelope = 0.0f;
        mol_push_note_event(engine, MOL_EVENT_NOTE_ENDED, voice);
        voice->stage = MOL_VOICE_IDLE;
      }
      break;
    case MOL_VOICE_SUSTAIN:
    case MOL_VOICE_IDLE:
    default:
      break;
  }
  return sample * voice->envelope * voice->velocity;
}

static float mol_render_frame(mol_engine_t* engine) {
  float mixed = 0.0f;
  uint32_t index;
  while (engine->command_count != 0u &&
         engine->commands[0].command.target_frame <= engine->current_frame) {
    mol_scheduled_command_t scheduled = mol_command_heap_pop(engine);
    mol_process_command(engine, &scheduled.command);
  }
  for (index = 0u; index < engine->config.max_voices; ++index) {
    mixed += mol_render_voice(engine, &engine->voices[index]);
  }
  mixed *= engine->master_gain;
  if (!isfinite(mixed)) {
    mixed = 0.0f;
  } else if (mixed > 1.0f) {
    mixed = 1.0f;
  } else if (mixed < -1.0f) {
    mixed = -1.0f;
  }
  ++engine->current_frame;
  return mixed;
}

static mol_result_t mol_validate_render(const mol_engine_t* engine, uint32_t frame_count,
                                        uint32_t channel_count) {
  if (!mol_engine_is_valid(engine) || channel_count == 0u ||
      channel_count != engine->config.channel_count) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  if (UINT64_MAX - engine->current_frame < frame_count) {
    return MOL_ERROR_OVERFLOW;
  }
  return MOL_OK;
}

uint32_t mol_get_api_version(void) { return MOL_API_VERSION; }

const char* mol_get_version_string(void) { return "0.1.0"; }

const char* mol_result_string(mol_result_t result) {
  switch (result) {
    case MOL_OK:
      return "ok";
    case MOL_ERROR_INVALID_ARGUMENT:
      return "invalid argument";
    case MOL_ERROR_UNSUPPORTED_VERSION:
      return "unsupported version";
    case MOL_ERROR_INSUFFICIENT_MEMORY:
      return "insufficient memory";
    case MOL_ERROR_MISALIGNED_MEMORY:
      return "misaligned memory";
    case MOL_ERROR_INVALID_STATE:
      return "invalid state";
    case MOL_ERROR_QUEUE_FULL:
      return "queue full";
    case MOL_ERROR_BUFFER_TOO_SMALL:
      return "buffer too small";
    case MOL_ERROR_UNSUPPORTED:
      return "unsupported";
    case MOL_ERROR_OVERFLOW:
      return "overflow";
    case MOL_ERROR_INTERNAL:
      return "internal error";
    default:
      return "unknown result";
  }
}

mol_engine_config_t mol_engine_config_default(void) {
  mol_engine_config_t config;
  config.struct_size = (uint32_t)sizeof(config);
  config.api_version = MOL_API_VERSION;
  config.sample_rate = 48000u;
  config.channel_count = 2u;
  config.max_voices = MOL_PROFILE_MAX_VOICES;
  config.command_capacity = 256u;
  config.event_capacity = 256u;
  config.random_seed = UINT32_C(0x4D4F4C31);
  return config;
}

size_t mol_engine_memory_alignment(void) {
  size_t alignment = _Alignof(mol_engine_t);
  alignment = mol_max_size(alignment, _Alignof(mol_voice_t));
  alignment = mol_max_size(alignment, _Alignof(mol_scheduled_command_t));
  return mol_max_size(alignment, _Alignof(mol_event_t));
}

size_t mol_engine_query_memory(const mol_engine_config_t* config) {
  size_t size = 0u;
  if (!mol_engine_config_is_valid(config) ||
      !mol_size_add_array(&size, _Alignof(mol_engine_t), sizeof(mol_engine_t), 1u) ||
      !mol_size_add_array(&size, _Alignof(mol_voice_t), sizeof(mol_voice_t),
                          config->max_voices) ||
      !mol_size_add_array(&size, _Alignof(mol_scheduled_command_t),
                          sizeof(mol_scheduled_command_t), config->command_capacity) ||
      !mol_size_add_array(&size, _Alignof(mol_event_t), sizeof(mol_event_t),
                          config->event_capacity)) {
    return 0u;
  }
  return size;
}

mol_result_t mol_engine_init(void* memory, size_t memory_size,
                             const mol_engine_config_t* config,
                             mol_engine_t** out_engine) {
  unsigned char* bytes = (unsigned char*)memory;
  size_t offset = 0u;
  size_t required;
  mol_engine_t* engine;
  if (out_engine == NULL) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  *out_engine = NULL;
  if (config == NULL || memory == NULL) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  if (config->struct_size < sizeof(*config)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  if (config->api_version != MOL_API_VERSION) {
    return MOL_ERROR_UNSUPPORTED_VERSION;
  }
  required = mol_engine_query_memory(config);
  if (required == 0u) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  if (((uintptr_t)memory % mol_engine_memory_alignment()) != 0u) {
    return MOL_ERROR_MISALIGNED_MEMORY;
  }
  if (memory_size < required) {
    return MOL_ERROR_INSUFFICIENT_MEMORY;
  }
  memset(memory, 0, required);
  engine = (mol_engine_t*)mol_arena_take(bytes, &offset, _Alignof(mol_engine_t),
                                         sizeof(mol_engine_t), 1u);
  engine->voices = (mol_voice_t*)mol_arena_take(bytes, &offset, _Alignof(mol_voice_t),
                                                sizeof(mol_voice_t), config->max_voices);
  engine->commands = (mol_scheduled_command_t*)mol_arena_take(
      bytes, &offset, _Alignof(mol_scheduled_command_t), sizeof(mol_scheduled_command_t),
      config->command_capacity);
  engine->events = (mol_event_t*)mol_arena_take(bytes, &offset, _Alignof(mol_event_t),
                                                sizeof(mol_event_t), config->event_capacity);
  engine->config = *config;
  engine->memory_size = required;
  engine->master_gain = MOL_MASTER_GAIN_DEFAULT;
  engine->magic = MOL_ENGINE_MAGIC;
  *out_engine = engine;
  return MOL_OK;
}

void mol_engine_shutdown(mol_engine_t* engine) {
  if (mol_engine_is_valid(engine)) {
    size_t memory_size = engine->memory_size;
    memset(engine, 0, memory_size);
  }
}

void mol_engine_reset(mol_engine_t* engine) {
  if (mol_engine_is_valid(engine)) {
    memset(engine->voices, 0, sizeof(*engine->voices) * engine->config.max_voices);
    engine->current_frame = 0u;
    engine->submit_serial = 0u;
    engine->command_count = 0u;
    engine->event_head = 0u;
    engine->event_count = 0u;
    engine->master_gain = MOL_MASTER_GAIN_DEFAULT;
  }
}

mol_result_t mol_engine_submit(mol_engine_t* engine, const mol_command_t* command) {
  mol_scheduled_command_t scheduled;
  if (!mol_engine_is_valid(engine) || command == NULL ||
      command->struct_size < sizeof(*command)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  if (command->api_version != MOL_API_VERSION) {
    return MOL_ERROR_UNSUPPORTED_VERSION;
  }
  switch (command->command_type) {
    case MOL_COMMAND_NOTE_ON:
      if (command->payload.note.note > 127u || !isfinite(command->payload.note.velocity) ||
          command->payload.note.velocity <= 0.0f || command->payload.note.velocity > 1.0f ||
          command->gesture_id == 0u) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      break;
    case MOL_COMMAND_NOTE_OFF:
      if (command->gesture_id == 0u) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      break;
    case MOL_COMMAND_SET_MASTER_GAIN:
      if (!isfinite(command->payload.scalar.value) || command->payload.scalar.value < 0.0f ||
          command->payload.scalar.value > 2.0f) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      break;
    case MOL_COMMAND_ALL_NOTES_OFF:
    case MOL_COMMAND_ALL_SOUND_OFF:
    case MOL_COMMAND_RESET_ENGINE:
      break;
    default:
      return MOL_ERROR_UNSUPPORTED;
  }
  if (engine->command_count >= engine->config.command_capacity) {
    return MOL_ERROR_QUEUE_FULL;
  }
  scheduled.command = *command;
  if (scheduled.command.target_frame == MOL_FRAME_IMMEDIATE ||
      scheduled.command.target_frame < engine->current_frame) {
    scheduled.command.target_frame = engine->current_frame;
  }
  scheduled.serial = engine->submit_serial++;
  mol_command_heap_push(engine, &scheduled);
  return MOL_OK;
}

mol_result_t mol_engine_render_interleaved_f32(mol_engine_t* engine, float* output,
                                                uint32_t frame_count,
                                                uint32_t channel_count) {
  uint32_t frame;
  uint32_t channel;
  mol_result_t result = mol_validate_render(engine, frame_count, channel_count);
  if (result != MOL_OK || output == NULL) {
    return result != MOL_OK ? result : MOL_ERROR_INVALID_ARGUMENT;
  }
  if ((size_t)channel_count > SIZE_MAX / sizeof(*output) ||
      (size_t)frame_count > SIZE_MAX / ((size_t)channel_count * sizeof(*output))) {
    return MOL_ERROR_OVERFLOW;
  }
  for (frame = 0u; frame < frame_count; ++frame) {
    float sample = mol_render_frame(engine);
    for (channel = 0u; channel < channel_count; ++channel) {
      output[(size_t)frame * channel_count + channel] = sample;
    }
  }
  return MOL_OK;
}

mol_result_t mol_engine_render_planar_f32(mol_engine_t* engine,
                                          float* const* output_channels,
                                          uint32_t frame_count,
                                          uint32_t channel_count) {
  uint32_t frame;
  uint32_t channel;
  mol_result_t result = mol_validate_render(engine, frame_count, channel_count);
  if (result != MOL_OK || output_channels == NULL) {
    return result != MOL_OK ? result : MOL_ERROR_INVALID_ARGUMENT;
  }
  for (channel = 0u; channel < channel_count; ++channel) {
    if (output_channels[channel] == NULL) {
      return MOL_ERROR_INVALID_ARGUMENT;
    }
  }
  for (frame = 0u; frame < frame_count; ++frame) {
    float sample = mol_render_frame(engine);
    for (channel = 0u; channel < channel_count; ++channel) {
      output_channels[channel][frame] = sample;
    }
  }
  return MOL_OK;
}

uint32_t mol_engine_poll_events(mol_engine_t* engine, mol_event_t* events,
                                uint32_t capacity) {
  uint32_t copied = 0u;
  if (!mol_engine_is_valid(engine) || (events == NULL && capacity != 0u)) {
    return 0u;
  }
  while (copied < capacity && engine->event_count != 0u) {
    events[copied++] = engine->events[engine->event_head];
    engine->event_head = (engine->event_head + 1u) % engine->config.event_capacity;
    --engine->event_count;
  }
  return copied;
}

mol_result_t mol_engine_get_state(const mol_engine_t* engine, mol_engine_state_t* state) {
  uint32_t active_voices = 0u;
  uint32_t index;
  if (!mol_engine_is_valid(engine) || state == NULL || state->struct_size < sizeof(*state)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  for (index = 0u; index < engine->config.max_voices; ++index) {
    if (engine->voices[index].stage != MOL_VOICE_IDLE) {
      ++active_voices;
    }
  }
  state->struct_size = (uint32_t)sizeof(*state);
  state->api_version = MOL_API_VERSION;
  state->current_frame = engine->current_frame;
  state->sample_rate = engine->config.sample_rate;
  state->channel_count = engine->config.channel_count;
  state->max_voices = engine->config.max_voices;
  state->active_voices = active_voices;
  return MOL_OK;
}

mol_capability_flags_t mol_engine_get_capabilities(const mol_engine_t* engine) {
  if (!mol_engine_is_valid(engine)) {
    return 0u;
  }
  return MOL_CAPABILITY_CALLER_MEMORY | MOL_CAPABILITY_INTERLEAVED_F32 |
         MOL_CAPABILITY_PLANAR_F32 | MOL_CAPABILITY_POLYPHONIC_SYNTH |
         MOL_CAPABILITY_SAMPLE_ACCURATE_COMMANDS;
}
