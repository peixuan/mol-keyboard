/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "mol/engine.h"
#include "mol/music.h"
#include "mol/transport.h"

#define MOL_ENGINE_MAGIC UINT32_C(0x4D4F4C45)
#define MOL_MASTER_GAIN_DEFAULT 0.25f
#define MOL_SUSTAIN_LEVEL 0.70f
#define MOL_SUSTAIN_ON_THRESHOLD (64.0f / 127.0f)
#define MOL_PI 3.14159265358979323846f
#define MOL_METRONOME_DURATION_SECONDS 0.025f

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
  uint32_t source_id;
  uint8_t note;
  uint8_t input_note;
  uint8_t key_down;
  uint8_t arpeggiated;
} mol_voice_t;

typedef struct mol_gesture {
  mol_gesture_id_t gesture_id;
  uint64_t order;
  uint32_t source_id;
  float velocity;
  uint8_t notes[4];
  uint8_t note_count;
  uint8_t input_note;
  uint8_t key_down;
  uint8_t active;
} mol_gesture_t;

typedef struct mol_arpeggiator_candidate {
  mol_gesture_t* gesture;
  uint8_t note;
} mol_arpeggiator_candidate_t;

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
  mol_gesture_t* gestures;
  mol_scheduled_command_t* commands;
  mol_event_t* events;
  uint32_t command_count;
  uint32_t event_head;
  uint32_t event_count;
  uint32_t gesture_capacity;
  float master_gain;
  float sustain;
  int32_t octave_shift;
  int32_t transpose;
  mol_scale_type_t scale_type;
  mol_scale_mapping_t scale_mapping;
  mol_chord_mode_t chord_mode;
  mol_frame_index_t transport_frame;
  mol_frame_index_t metronome_next_frame;
  uint64_t metronome_beat_index;
  uint32_t tempo_milli_bpm;
  uint32_t metronome_remaining;
  uint32_t metronome_duration;
  float metronome_level;
  float metronome_phase;
  float metronome_phase_increment;
  uint8_t scale_tonic;
  uint8_t time_signature_numerator;
  uint8_t time_signature_denominator;
  uint8_t transport_running;
  uint8_t metronome_enabled;
  mol_frame_index_t arpeggiator_next_frame;
  mol_frame_index_t arpeggiator_gate_off_frame;
  uint64_t arpeggiator_step_index;
  uint64_t gesture_serial;
  uint32_t arpeggiator_random_seed;
  uint16_t arpeggiator_gate_milli;
  mol_arpeggiator_mode_t arpeggiator_mode;
  mol_arpeggiator_rate_t arpeggiator_rate;
  uint8_t arpeggiator_octaves;
  uint8_t arpeggiator_voice_active;
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
  if ((config->channel_count != 1u && config->channel_count != 2u) || config->max_voices < 8u ||
      config->max_voices > MOL_PROFILE_MAX_VOICES || config->command_capacity == 0u ||
      config->event_capacity == 0u) {
    return 0;
  }
  return 1;
}

static int mol_engine_is_valid(const mol_engine_t* engine) {
  return engine != NULL && engine->magic == MOL_ENGINE_MAGIC;
}

static size_t mol_max_size(size_t left, size_t right) { return left > right ? left : right; }

static int mol_size_add_array(size_t* size, size_t alignment, size_t element_size, size_t count) {
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

static void mol_command_heap_push(mol_engine_t* engine, const mol_scheduled_command_t* scheduled) {
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
  event->source_id = voice->source_id;
  event->frame = engine->current_frame;
  event->gesture_id = voice->gesture_id;
  event->payload[MOL_EVENT_PAYLOAD_NOTE] = voice->note;
  event->payload[MOL_EVENT_PAYLOAD_INPUT_NOTE] = voice->input_note;
  ++engine->event_count;
}

static void mol_push_mapping_event(mol_engine_t* engine, const mol_command_t* command,
                                   uint8_t mapped_note, uint8_t index, uint8_t count) {
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
  event->event_type = MOL_EVENT_GESTURE_MAPPED;
  event->source_id = command->source_id;
  event->frame = engine->current_frame;
  event->gesture_id = command->gesture_id;
  event->payload[MOL_EVENT_PAYLOAD_NOTE] = mapped_note;
  event->payload[MOL_EVENT_PAYLOAD_INPUT_NOTE] = command->payload.note.note;
  event->payload[MOL_EVENT_PAYLOAD_MAPPED_INDEX] = index;
  event->payload[MOL_EVENT_PAYLOAD_MAPPED_COUNT] = count;
  ++engine->event_count;
}

static void mol_push_music_error(mol_engine_t* engine, const mol_command_t* command,
                                 uint8_t error) {
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
  event->event_type = MOL_EVENT_ERROR_REPORTED;
  event->source_id = command->source_id;
  event->frame = engine->current_frame;
  event->gesture_id = command->gesture_id;
  event->payload[0] = error;
  event->payload[1] = command->payload.note.note;
  ++engine->event_count;
}

static void mol_push_transport_event(mol_engine_t* engine, const mol_command_t* command) {
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
  event->event_type = MOL_EVENT_TRANSPORT_CHANGED;
  event->source_id = command->source_id;
  event->frame = engine->current_frame;
  event->payload[0] = (uint8_t)command->command_type;
  ++engine->event_count;
}

static void mol_push_metronome_event(mol_engine_t* engine, uint8_t accent, uint8_t beat) {
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
  event->event_type = MOL_EVENT_METRONOME_TICK;
  event->frame = engine->current_frame;
  event->payload[MOL_EVENT_PAYLOAD_METRONOME_ACCENT] = accent;
  event->payload[MOL_EVENT_PAYLOAD_METRONOME_BEAT] = beat;
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

static mol_gesture_t* mol_find_gesture(mol_engine_t* engine, mol_gesture_id_t gesture_id) {
  uint32_t index;
  for (index = 0u; index < engine->gesture_capacity; ++index) {
    if (engine->gestures[index].active != 0u && engine->gestures[index].gesture_id == gesture_id) {
      return &engine->gestures[index];
    }
  }
  return NULL;
}

static mol_gesture_t* mol_allocate_gesture(mol_engine_t* engine) {
  uint32_t index;
  for (index = 0u; index < engine->gesture_capacity; ++index) {
    if (engine->gestures[index].active == 0u) {
      return &engine->gestures[index];
    }
  }
  return NULL;
}

static void mol_start_voice(mol_engine_t* engine, const mol_gesture_t* gesture, uint8_t note,
                            uint8_t arpeggiated) {
  mol_voice_t* voice = mol_allocate_voice(engine);
  memset(voice, 0, sizeof(*voice));
  voice->gesture_id = gesture->gesture_id;
  voice->started_at = engine->current_frame;
  voice->phase_increment = mol_note_frequency(note) / (float)engine->config.sample_rate;
  voice->velocity = gesture->velocity;
  voice->stage = MOL_VOICE_ATTACK;
  voice->source_id = gesture->source_id;
  voice->note = note;
  voice->input_note = gesture->input_note;
  voice->key_down = gesture->key_down;
  voice->arpeggiated = arpeggiated;
  mol_push_note_event(engine, MOL_EVENT_NOTE_STARTED, voice);
}

static void mol_process_note_on(mol_engine_t* engine, const mol_command_t* command) {
  mol_gesture_t* gesture;
  uint8_t mapped_note;
  uint32_t chord_count = 0u;
  uint32_t index;
  int shifted_note;
  mol_result_t result;
  if (mol_find_gesture(engine, command->gesture_id) != NULL) {
    mol_push_music_error(engine, command, MOL_MUSIC_ERROR_DUPLICATE_GESTURE);
    return;
  }
  shifted_note = (int)command->payload.note.note + (engine->octave_shift * 12) + engine->transpose;
  if (shifted_note < 0 || shifted_note > 127) {
    mol_push_music_error(engine, command, MOL_MUSIC_ERROR_NOTE_OUT_OF_RANGE);
    return;
  }
  result = mol_scale_map_note((uint8_t)shifted_note, engine->scale_tonic, engine->scale_type,
                              engine->scale_mapping, &mapped_note);
  if (result != MOL_OK) {
    mol_push_music_error(engine, command, MOL_MUSIC_ERROR_SCALE_MAPPING);
    return;
  }
  gesture = mol_allocate_gesture(engine);
  if (gesture == NULL) {
    mol_push_music_error(engine, command, MOL_MUSIC_ERROR_GESTURE_CAPACITY);
    return;
  }
  memset(gesture, 0, sizeof(*gesture));
  result = mol_chord_expand(mapped_note, engine->chord_mode, gesture->notes, 4u, &chord_count);
  if (result != MOL_OK || chord_count == 0u) {
    mol_push_music_error(engine, command, MOL_MUSIC_ERROR_NOTE_OUT_OF_RANGE);
    return;
  }
  gesture->gesture_id = command->gesture_id;
  gesture->order = engine->gesture_serial++;
  gesture->source_id = command->source_id;
  gesture->velocity = command->payload.note.velocity;
  gesture->note_count = (uint8_t)chord_count;
  gesture->input_note = command->payload.note.note;
  gesture->key_down = 1u;
  gesture->active = 1u;
  for (index = 0u; index < chord_count; ++index) {
    mol_push_mapping_event(engine, command, gesture->notes[index], (uint8_t)index,
                           (uint8_t)chord_count);
    if (engine->arpeggiator_mode == MOL_ARPEGGIATOR_OFF) {
      mol_start_voice(engine, gesture, gesture->notes[index], 0u);
    }
  }
}

static void mol_release_gesture_voices(mol_engine_t* engine, mol_gesture_id_t gesture_id) {
  uint32_t index;
  for (index = 0u; index < engine->config.max_voices; ++index) {
    if (engine->voices[index].stage != MOL_VOICE_IDLE &&
        engine->voices[index].gesture_id == gesture_id) {
      engine->voices[index].key_down = 0u;
      mol_voice_release(engine, &engine->voices[index]);
    }
  }
}

static void mol_release_arpeggiated_voices(mol_engine_t* engine) {
  uint32_t index;
  for (index = 0u; index < engine->config.max_voices; ++index) {
    if (engine->voices[index].stage != MOL_VOICE_IDLE && engine->voices[index].arpeggiated != 0u) {
      engine->voices[index].key_down = 0u;
      mol_voice_release(engine, &engine->voices[index]);
    }
  }
  engine->arpeggiator_voice_active = 0u;
}

static uint32_t mol_gesture_candidate_count(const mol_gesture_t* gesture, uint8_t octaves) {
  uint32_t count = 0u;
  uint32_t octave;
  uint32_t note_index;
  for (octave = 0u; octave < octaves; ++octave) {
    for (note_index = 0u; note_index < gesture->note_count; ++note_index) {
      if ((uint32_t)gesture->notes[note_index] + octave * 12u <= 127u) {
        ++count;
      }
    }
  }
  return count;
}

static uint32_t mol_arpeggiator_candidate_count(const mol_engine_t* engine) {
  uint32_t count = 0u;
  uint32_t index;
  for (index = 0u; index < engine->gesture_capacity; ++index) {
    if (engine->gestures[index].active != 0u) {
      count += mol_gesture_candidate_count(&engine->gestures[index], engine->arpeggiator_octaves);
    }
  }
  return count;
}

static int mol_gesture_candidate_at(mol_gesture_t* gesture, uint8_t octaves, uint32_t rank,
                                    mol_arpeggiator_candidate_t* out_candidate) {
  uint32_t octave;
  uint32_t note_index;
  for (octave = 0u; octave < octaves; ++octave) {
    for (note_index = 0u; note_index < gesture->note_count; ++note_index) {
      uint32_t note = (uint32_t)gesture->notes[note_index] + octave * 12u;
      if (note <= 127u) {
        if (rank == 0u) {
          out_candidate->gesture = gesture;
          out_candidate->note = (uint8_t)note;
          return 1;
        }
        --rank;
      }
    }
  }
  return 0;
}

static int mol_arpeggiator_candidate_as_played(mol_engine_t* engine, uint32_t rank,
                                               mol_arpeggiator_candidate_t* out_candidate) {
  uint32_t index;
  for (index = 0u; index < engine->gesture_capacity; ++index) {
    mol_gesture_t* gesture = &engine->gestures[index];
    uint32_t preceding = 0u;
    uint32_t other_index;
    uint32_t candidate_count;
    if (gesture->active == 0u) {
      continue;
    }
    for (other_index = 0u; other_index < engine->gesture_capacity; ++other_index) {
      const mol_gesture_t* other = &engine->gestures[other_index];
      if (other->active != 0u && other->order < gesture->order) {
        preceding += mol_gesture_candidate_count(other, engine->arpeggiator_octaves);
      }
    }
    candidate_count = mol_gesture_candidate_count(gesture, engine->arpeggiator_octaves);
    if (rank >= preceding && rank - preceding < candidate_count) {
      return mol_gesture_candidate_at(gesture, engine->arpeggiator_octaves, rank - preceding,
                                      out_candidate);
    }
  }
  return 0;
}

static int mol_arpeggiator_candidate_sorted(mol_engine_t* engine, uint32_t rank,
                                            mol_arpeggiator_candidate_t* out_candidate) {
  uint32_t note;
  for (note = 0u; note <= 127u; ++note) {
    uint32_t gesture_index;
    for (gesture_index = 0u; gesture_index < engine->gesture_capacity; ++gesture_index) {
      mol_gesture_t* gesture = &engine->gestures[gesture_index];
      uint32_t octave;
      uint32_t note_index;
      if (gesture->active == 0u) {
        continue;
      }
      for (octave = 0u; octave < engine->arpeggiator_octaves; ++octave) {
        for (note_index = 0u; note_index < gesture->note_count; ++note_index) {
          uint32_t candidate_note = (uint32_t)gesture->notes[note_index] + octave * 12u;
          if (candidate_note == note) {
            if (rank == 0u) {
              out_candidate->gesture = gesture;
              out_candidate->note = (uint8_t)note;
              return 1;
            }
            --rank;
          }
        }
      }
    }
  }
  return 0;
}

static uint32_t mol_arpeggiator_random_rank(const mol_engine_t* engine, uint32_t count) {
  uint32_t value = engine->arpeggiator_random_seed ^ (uint32_t)engine->arpeggiator_step_index ^
                   (uint32_t)(engine->arpeggiator_step_index >> 32u) * UINT32_C(0x9E3779B9);
  value ^= value << 13u;
  value ^= value >> 17u;
  value ^= value << 5u;
  return count == 0u ? 0u : value % count;
}

static int mol_select_arpeggiator_candidate(mol_engine_t* engine, uint32_t count,
                                            mol_arpeggiator_candidate_t* out_candidate) {
  uint64_t sequence_step = engine->arpeggiator_step_index;
  uint32_t rank;
  if (count == 0u || out_candidate == NULL) {
    return 0;
  }
  switch (engine->arpeggiator_mode) {
    case MOL_ARPEGGIATOR_UP:
      rank = (uint32_t)(sequence_step % count);
      break;
    case MOL_ARPEGGIATOR_DOWN:
      rank = count - 1u - (uint32_t)(sequence_step % count);
      break;
    case MOL_ARPEGGIATOR_UP_DOWN:
    case MOL_ARPEGGIATOR_DOWN_UP:
      if (count == 1u) {
        rank = 0u;
      } else {
        uint32_t period = count * 2u - 2u;
        uint32_t position = (uint32_t)(sequence_step % period);
        rank = position < count ? position : period - position;
        if (engine->arpeggiator_mode == MOL_ARPEGGIATOR_DOWN_UP) {
          rank = count - 1u - rank;
        }
      }
      break;
    case MOL_ARPEGGIATOR_AS_PLAYED:
      rank = (uint32_t)(sequence_step % count);
      return mol_arpeggiator_candidate_as_played(engine, rank, out_candidate);
    case MOL_ARPEGGIATOR_RANDOM_DETERMINISTIC:
      rank = mol_arpeggiator_random_rank(engine, count);
      break;
    default:
      return 0;
  }
  return mol_arpeggiator_candidate_sorted(engine, rank, out_candidate);
}

static void mol_reschedule_arpeggiator(mol_engine_t* engine) {
  uint32_t steps_per_quarter = mol_arpeggiator_steps_per_quarter(engine->arpeggiator_rate);
  if (steps_per_quarter == 0u ||
      mol_transport_step_at_or_after(engine->config.sample_rate, engine->tempo_milli_bpm,
                                     steps_per_quarter, engine->transport_frame,
                                     &engine->arpeggiator_step_index) != MOL_OK ||
      mol_transport_step_frame(engine->config.sample_rate, engine->tempo_milli_bpm,
                               steps_per_quarter, engine->arpeggiator_step_index,
                               &engine->arpeggiator_next_frame) != MOL_OK) {
    engine->arpeggiator_next_frame = UINT64_MAX;
  }
}

static void mol_process_arpeggiator_tick(mol_engine_t* engine) {
  mol_arpeggiator_candidate_t candidate;
  mol_frame_index_t step_frame = engine->arpeggiator_next_frame;
  mol_frame_index_t next_frame;
  uint32_t candidate_count = mol_arpeggiator_candidate_count(engine);
  uint32_t steps_per_quarter = mol_arpeggiator_steps_per_quarter(engine->arpeggiator_rate);
  if (engine->arpeggiator_voice_active != 0u) {
    mol_release_arpeggiated_voices(engine);
  }
  if (mol_select_arpeggiator_candidate(engine, candidate_count, &candidate)) {
    mol_start_voice(engine, candidate.gesture, candidate.note, 1u);
    engine->arpeggiator_voice_active = 1u;
  }
  ++engine->arpeggiator_step_index;
  if (mol_transport_step_frame(engine->config.sample_rate, engine->tempo_milli_bpm,
                               steps_per_quarter, engine->arpeggiator_step_index,
                               &next_frame) != MOL_OK) {
    engine->arpeggiator_next_frame = UINT64_MAX;
    engine->arpeggiator_gate_off_frame = UINT64_MAX;
    return;
  }
  engine->arpeggiator_next_frame = next_frame;
  if (engine->arpeggiator_voice_active != 0u) {
    uint64_t interval = next_frame > step_frame ? next_frame - step_frame : 1u;
    uint64_t gate_frames = interval * engine->arpeggiator_gate_milli / 1000u;
    if (gate_frames == 0u) {
      gate_frames = 1u;
    }
    engine->arpeggiator_gate_off_frame = step_frame + gate_frames;
  }
}

static void mol_reschedule_metronome(mol_engine_t* engine) {
  uint32_t steps_per_quarter = (uint32_t)engine->time_signature_denominator / 4u;
  if (mol_transport_step_at_or_after(engine->config.sample_rate, engine->tempo_milli_bpm,
                                     steps_per_quarter, engine->transport_frame,
                                     &engine->metronome_beat_index) != MOL_OK ||
      mol_transport_step_frame(engine->config.sample_rate, engine->tempo_milli_bpm,
                               steps_per_quarter, engine->metronome_beat_index,
                               &engine->metronome_next_frame) != MOL_OK) {
    engine->metronome_next_frame = UINT64_MAX;
  }
}

static void mol_process_metronome_tick(mol_engine_t* engine) {
  uint8_t beat = (uint8_t)(engine->metronome_beat_index % engine->time_signature_numerator);
  uint8_t accent = beat == 0u ? 1u : 0u;
  engine->metronome_duration =
      (uint32_t)(MOL_METRONOME_DURATION_SECONDS * (float)engine->config.sample_rate);
  engine->metronome_remaining = engine->metronome_duration;
  engine->metronome_phase = 0.0f;
  engine->metronome_phase_increment =
      (accent != 0u ? 1760.0f : 1320.0f) / (float)engine->config.sample_rate;
  mol_push_metronome_event(engine, accent, beat);
  ++engine->metronome_beat_index;
  if (mol_transport_step_frame(engine->config.sample_rate, engine->tempo_milli_bpm,
                               (uint32_t)engine->time_signature_denominator / 4u,
                               engine->metronome_beat_index,
                               &engine->metronome_next_frame) != MOL_OK) {
    engine->metronome_next_frame = UINT64_MAX;
  }
}

static void mol_process_command(mol_engine_t* engine, const mol_command_t* command) {
  uint32_t index;
  switch (command->command_type) {
    case MOL_COMMAND_NOTE_ON:
      mol_process_note_on(engine, command);
      break;
    case MOL_COMMAND_NOTE_OFF: {
      mol_gesture_t* gesture = mol_find_gesture(engine, command->gesture_id);
      if (gesture != NULL) {
        gesture->key_down = 0u;
        for (index = 0u; index < engine->config.max_voices; ++index) {
          if (engine->voices[index].stage != MOL_VOICE_IDLE &&
              engine->voices[index].gesture_id == command->gesture_id) {
            engine->voices[index].key_down = 0u;
          }
        }
        if (engine->sustain < MOL_SUSTAIN_ON_THRESHOLD) {
          gesture->active = 0u;
          mol_release_gesture_voices(engine, command->gesture_id);
        }
      }
      break;
    }
    case MOL_COMMAND_SUSTAIN: {
      float previous = engine->sustain;
      engine->sustain = command->payload.scalar.value;
      if (previous >= MOL_SUSTAIN_ON_THRESHOLD && engine->sustain < MOL_SUSTAIN_ON_THRESHOLD) {
        for (index = 0u; index < engine->gesture_capacity; ++index) {
          if (engine->gestures[index].active != 0u && engine->gestures[index].key_down == 0u) {
            mol_gesture_id_t gesture_id = engine->gestures[index].gesture_id;
            engine->gestures[index].active = 0u;
            mol_release_gesture_voices(engine, gesture_id);
          }
        }
      }
      break;
    }
    case MOL_COMMAND_ALL_NOTES_OFF:
      for (index = 0u; index < engine->gesture_capacity; ++index) {
        if (engine->gestures[index].active != 0u) {
          mol_gesture_id_t gesture_id = engine->gestures[index].gesture_id;
          engine->gestures[index].key_down = 0u;
          if (engine->sustain < MOL_SUSTAIN_ON_THRESHOLD) {
            engine->gestures[index].active = 0u;
            mol_release_gesture_voices(engine, gesture_id);
          }
        }
      }
      break;
    case MOL_COMMAND_ALL_SOUND_OFF:
      for (index = 0u; index < engine->config.max_voices; ++index) {
        if (engine->voices[index].stage != MOL_VOICE_IDLE) {
          mol_push_note_event(engine, MOL_EVENT_NOTE_ENDED, &engine->voices[index]);
        }
      }
      memset(engine->voices, 0, sizeof(*engine->voices) * engine->config.max_voices);
      memset(engine->gestures, 0, sizeof(*engine->gestures) * engine->gesture_capacity);
      engine->arpeggiator_voice_active = 0u;
      break;
    case MOL_COMMAND_SET_MASTER_GAIN:
      engine->master_gain = command->payload.scalar.value;
      break;
    case MOL_COMMAND_SET_OCTAVE_SHIFT:
      engine->octave_shift = command->payload.integer.value;
      break;
    case MOL_COMMAND_SET_TRANSPOSE:
      engine->transpose = command->payload.integer.value;
      break;
    case MOL_COMMAND_SET_SCALE:
      engine->scale_type = command->payload.scale.type;
      engine->scale_tonic = command->payload.scale.tonic;
      engine->scale_mapping = command->payload.scale.mapping;
      break;
    case MOL_COMMAND_SET_CHORD_MODE:
      engine->chord_mode = (mol_chord_mode_t)command->payload.integer.value;
      break;
    case MOL_COMMAND_SET_ARPEGGIATOR:
      for (index = 0u; index < engine->gesture_capacity; ++index) {
        if (engine->gestures[index].active != 0u) {
          mol_release_gesture_voices(engine, engine->gestures[index].gesture_id);
        }
      }
      engine->arpeggiator_mode = command->payload.arpeggiator.mode;
      engine->arpeggiator_rate = command->payload.arpeggiator.rate;
      engine->arpeggiator_gate_milli =
          (uint16_t)(command->payload.arpeggiator.gate * 1000.0f + 0.5f);
      engine->arpeggiator_random_seed = command->payload.arpeggiator.random_seed;
      engine->arpeggiator_octaves = command->payload.arpeggiator.octaves;
      engine->arpeggiator_voice_active = 0u;
      mol_reschedule_arpeggiator(engine);
      break;
    case MOL_COMMAND_SET_TEMPO:
      (void)mol_tempo_to_milli_bpm(command->payload.scalar.value, &engine->tempo_milli_bpm);
      mol_reschedule_metronome(engine);
      mol_reschedule_arpeggiator(engine);
      mol_push_transport_event(engine, command);
      break;
    case MOL_COMMAND_SET_TIME_SIGNATURE:
      engine->time_signature_numerator = command->payload.time_signature.numerator;
      engine->time_signature_denominator = command->payload.time_signature.denominator;
      mol_reschedule_metronome(engine);
      mol_push_transport_event(engine, command);
      break;
    case MOL_COMMAND_TRANSPORT_START:
      engine->transport_running = 1u;
      mol_reschedule_metronome(engine);
      mol_reschedule_arpeggiator(engine);
      mol_push_transport_event(engine, command);
      break;
    case MOL_COMMAND_TRANSPORT_STOP:
      engine->transport_running = 0u;
      engine->metronome_remaining = 0u;
      mol_release_arpeggiated_voices(engine);
      mol_push_transport_event(engine, command);
      break;
    case MOL_COMMAND_TRANSPORT_SEEK:
      engine->transport_frame = command->payload.transport.frame;
      engine->metronome_remaining = 0u;
      mol_release_arpeggiated_voices(engine);
      mol_reschedule_metronome(engine);
      mol_reschedule_arpeggiator(engine);
      mol_push_transport_event(engine, command);
      break;
    case MOL_COMMAND_SET_METRONOME:
      engine->metronome_enabled = command->payload.metronome.enabled;
      engine->metronome_level = command->payload.metronome.level;
      engine->metronome_remaining = 0u;
      mol_reschedule_metronome(engine);
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
      voice->envelope -= (1.0f - MOL_SUSTAIN_LEVEL) / (0.10f * (float)engine->config.sample_rate);
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

static float mol_render_metronome(mol_engine_t* engine) {
  float envelope;
  float sample;
  if (engine->metronome_remaining == 0u || engine->metronome_duration == 0u) {
    return 0.0f;
  }
  envelope = (float)engine->metronome_remaining / (float)engine->metronome_duration;
  sample = sinf(2.0f * MOL_PI * engine->metronome_phase) * envelope * engine->metronome_level;
  engine->metronome_phase += engine->metronome_phase_increment;
  if (engine->metronome_phase >= 1.0f) {
    engine->metronome_phase -= 1.0f;
  }
  --engine->metronome_remaining;
  return sample;
}

static float mol_render_frame(mol_engine_t* engine) {
  float mixed = 0.0f;
  uint32_t index;
  while (engine->command_count != 0u &&
         engine->commands[0].command.target_frame <= engine->current_frame) {
    mol_scheduled_command_t scheduled = mol_command_heap_pop(engine);
    mol_process_command(engine, &scheduled.command);
  }
  if (engine->transport_running != 0u && engine->arpeggiator_mode != MOL_ARPEGGIATOR_OFF) {
    if (engine->arpeggiator_voice_active != 0u &&
        engine->transport_frame >= engine->arpeggiator_gate_off_frame) {
      mol_release_arpeggiated_voices(engine);
    }
    if (engine->transport_frame >= engine->arpeggiator_next_frame) {
      mol_process_arpeggiator_tick(engine);
    }
  }
  if (engine->transport_running != 0u && engine->metronome_enabled != 0u &&
      engine->transport_frame >= engine->metronome_next_frame) {
    mol_process_metronome_tick(engine);
  }
  for (index = 0u; index < engine->config.max_voices; ++index) {
    mixed += mol_render_voice(engine, &engine->voices[index]);
  }
  mixed += mol_render_metronome(engine);
  mixed *= engine->master_gain;
  if (!isfinite(mixed)) {
    mixed = 0.0f;
  } else if (mixed > 1.0f) {
    mixed = 1.0f;
  } else if (mixed < -1.0f) {
    mixed = -1.0f;
  }
  ++engine->current_frame;
  if (engine->transport_running != 0u) {
    ++engine->transport_frame;
  }
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
  alignment = mol_max_size(alignment, _Alignof(mol_gesture_t));
  alignment = mol_max_size(alignment, _Alignof(mol_scheduled_command_t));
  return mol_max_size(alignment, _Alignof(mol_event_t));
}

size_t mol_engine_query_memory(const mol_engine_config_t* config) {
  size_t size = 0u;
  if (!mol_engine_config_is_valid(config) ||
      !mol_size_add_array(&size, _Alignof(mol_engine_t), sizeof(mol_engine_t), 1u) ||
      !mol_size_add_array(&size, _Alignof(mol_voice_t), sizeof(mol_voice_t), config->max_voices) ||
      !mol_size_add_array(&size, _Alignof(mol_gesture_t), sizeof(mol_gesture_t),
                          config->command_capacity) ||
      !mol_size_add_array(&size, _Alignof(mol_scheduled_command_t), sizeof(mol_scheduled_command_t),
                          config->command_capacity) ||
      !mol_size_add_array(&size, _Alignof(mol_event_t), sizeof(mol_event_t),
                          config->event_capacity)) {
    return 0u;
  }
  return size;
}

mol_result_t mol_engine_init(void* memory, size_t memory_size, const mol_engine_config_t* config,
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
  engine->gestures = (mol_gesture_t*)mol_arena_take(
      bytes, &offset, _Alignof(mol_gesture_t), sizeof(mol_gesture_t), config->command_capacity);
  engine->commands = (mol_scheduled_command_t*)mol_arena_take(
      bytes, &offset, _Alignof(mol_scheduled_command_t), sizeof(mol_scheduled_command_t),
      config->command_capacity);
  engine->events = (mol_event_t*)mol_arena_take(bytes, &offset, _Alignof(mol_event_t),
                                                sizeof(mol_event_t), config->event_capacity);
  engine->config = *config;
  engine->gesture_capacity = config->command_capacity;
  engine->memory_size = required;
  engine->magic = MOL_ENGINE_MAGIC;
  mol_engine_reset(engine);
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
    memset(engine->gestures, 0, sizeof(*engine->gestures) * engine->gesture_capacity);
    engine->current_frame = 0u;
    engine->submit_serial = 0u;
    engine->command_count = 0u;
    engine->event_head = 0u;
    engine->event_count = 0u;
    engine->master_gain = MOL_MASTER_GAIN_DEFAULT;
    engine->sustain = 0.0f;
    engine->octave_shift = 0;
    engine->transpose = 0;
    engine->scale_type = MOL_SCALE_CHROMATIC;
    engine->scale_tonic = 0u;
    engine->scale_mapping = MOL_SCALE_MAP_NEAREST;
    engine->chord_mode = MOL_CHORD_OFF;
    engine->transport_frame = 0u;
    engine->metronome_next_frame = 0u;
    engine->metronome_beat_index = 0u;
    engine->tempo_milli_bpm = 100000u;
    engine->metronome_remaining = 0u;
    engine->metronome_duration = 0u;
    engine->metronome_level = 0.5f;
    engine->metronome_phase = 0.0f;
    engine->metronome_phase_increment = 0.0f;
    engine->time_signature_numerator = 4u;
    engine->time_signature_denominator = 4u;
    engine->transport_running = 0u;
    engine->metronome_enabled = 0u;
    engine->arpeggiator_next_frame = 0u;
    engine->arpeggiator_gate_off_frame = 0u;
    engine->arpeggiator_step_index = 0u;
    engine->gesture_serial = 0u;
    engine->arpeggiator_random_seed = engine->config.random_seed;
    engine->arpeggiator_gate_milli = 500u;
    engine->arpeggiator_mode = MOL_ARPEGGIATOR_OFF;
    engine->arpeggiator_rate = MOL_ARPEGGIATOR_RATE_SIXTEENTH;
    engine->arpeggiator_octaves = 1u;
    engine->arpeggiator_voice_active = 0u;
  }
}

mol_result_t mol_engine_submit(mol_engine_t* engine, const mol_command_t* command) {
  mol_scheduled_command_t scheduled;
  if (!mol_engine_is_valid(engine) || command == NULL || command->struct_size < sizeof(*command)) {
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
    case MOL_COMMAND_SUSTAIN:
      if (!isfinite(command->payload.scalar.value) || command->payload.scalar.value < 0.0f ||
          command->payload.scalar.value > 1.0f) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      break;
    case MOL_COMMAND_SET_OCTAVE_SHIFT:
      if (command->payload.integer.value < -3 || command->payload.integer.value > 3) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      break;
    case MOL_COMMAND_SET_TRANSPOSE:
      if (command->payload.integer.value < -24 || command->payload.integer.value > 24) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      break;
    case MOL_COMMAND_SET_SCALE:
      if (command->payload.scale.type >= MOL_SCALE_TYPE_COUNT ||
          command->payload.scale.tonic > 11u ||
          command->payload.scale.mapping >= MOL_SCALE_MAPPING_COUNT) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      break;
    case MOL_COMMAND_SET_CHORD_MODE:
      if (command->payload.integer.value < 0 ||
          (uint32_t)command->payload.integer.value >= MOL_CHORD_MODE_COUNT) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      break;
    case MOL_COMMAND_SET_ARPEGGIATOR:
      if (command->payload.arpeggiator.mode >= MOL_ARPEGGIATOR_MODE_COUNT ||
          command->payload.arpeggiator.rate >= MOL_ARPEGGIATOR_RATE_COUNT ||
          !isfinite(command->payload.arpeggiator.gate) ||
          command->payload.arpeggiator.gate < 0.05f || command->payload.arpeggiator.gate > 1.0f ||
          command->payload.arpeggiator.octaves < 1u || command->payload.arpeggiator.octaves > 4u) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      break;
    case MOL_COMMAND_SET_TEMPO: {
      uint32_t ignored;
      if (mol_tempo_to_milli_bpm(command->payload.scalar.value, &ignored) != MOL_OK) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      break;
    }
    case MOL_COMMAND_SET_TIME_SIGNATURE:
      if (!mol_time_signature_is_valid(command->payload.time_signature.numerator,
                                       command->payload.time_signature.denominator)) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      break;
    case MOL_COMMAND_SET_METRONOME:
      if (command->payload.metronome.enabled > 1u || !isfinite(command->payload.metronome.level) ||
          command->payload.metronome.level < 0.0f || command->payload.metronome.level > 1.0f) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      break;
    case MOL_COMMAND_TRANSPORT_START:
    case MOL_COMMAND_TRANSPORT_STOP:
    case MOL_COMMAND_TRANSPORT_SEEK:
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
                                               uint32_t frame_count, uint32_t channel_count) {
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

mol_result_t mol_engine_render_planar_f32(mol_engine_t* engine, float* const* output_channels,
                                          uint32_t frame_count, uint32_t channel_count) {
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

uint32_t mol_engine_poll_events(mol_engine_t* engine, mol_event_t* events, uint32_t capacity) {
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
  uint32_t active_gestures = 0u;
  uint32_t index;
  if (!mol_engine_is_valid(engine) || state == NULL || state->struct_size < sizeof(*state)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  for (index = 0u; index < engine->config.max_voices; ++index) {
    if (engine->voices[index].stage != MOL_VOICE_IDLE) {
      ++active_voices;
    }
  }
  for (index = 0u; index < engine->gesture_capacity; ++index) {
    if (engine->gestures[index].active != 0u) {
      ++active_gestures;
    }
  }
  state->struct_size = (uint32_t)sizeof(*state);
  state->api_version = MOL_API_VERSION;
  state->current_frame = engine->current_frame;
  state->sample_rate = engine->config.sample_rate;
  state->channel_count = engine->config.channel_count;
  state->max_voices = engine->config.max_voices;
  state->active_voices = active_voices;
  state->octave_shift = engine->octave_shift;
  state->transpose = engine->transpose;
  state->scale_type = engine->scale_type;
  state->scale_tonic = engine->scale_tonic;
  state->scale_mapping = engine->scale_mapping;
  state->chord_mode = engine->chord_mode;
  state->sustain = engine->sustain;
  state->transport_frame = engine->transport_frame;
  state->tempo = (float)engine->tempo_milli_bpm / 1000.0f;
  state->time_signature_numerator = engine->time_signature_numerator;
  state->time_signature_denominator = engine->time_signature_denominator;
  state->transport_running = engine->transport_running;
  state->metronome_enabled = engine->metronome_enabled;
  state->active_gestures = active_gestures;
  state->arpeggiator_mode = engine->arpeggiator_mode;
  state->arpeggiator_rate = engine->arpeggiator_rate;
  state->arpeggiator_gate = (float)engine->arpeggiator_gate_milli / 1000.0f;
  state->arpeggiator_random_seed = engine->arpeggiator_random_seed;
  state->arpeggiator_octaves = engine->arpeggiator_octaves;
  return MOL_OK;
}

mol_capability_flags_t mol_engine_get_capabilities(const mol_engine_t* engine) {
  if (!mol_engine_is_valid(engine)) {
    return 0u;
  }
  return MOL_CAPABILITY_CALLER_MEMORY | MOL_CAPABILITY_INTERLEAVED_F32 | MOL_CAPABILITY_PLANAR_F32 |
         MOL_CAPABILITY_POLYPHONIC_SYNTH | MOL_CAPABILITY_SAMPLE_ACCURATE_COMMANDS |
         MOL_CAPABILITY_SCALE_LOCK | MOL_CAPABILITY_CHORD_MODE | MOL_CAPABILITY_CONTINUOUS_SUSTAIN |
         MOL_CAPABILITY_TRANSPORT | MOL_CAPABILITY_METRONOME | MOL_CAPABILITY_ARPEGGIATOR;
}
