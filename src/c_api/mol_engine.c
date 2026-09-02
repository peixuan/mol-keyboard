/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "mol/effects.h"
#include "mol/engine.h"
#include "mol/music.h"
#include "mol/patch.h"
#include "mol/transport.h"
#include "mol_dsp.h"
#include "mol_effects.h"

#define MOL_ENGINE_MAGIC UINT32_C(0x4D4F4C45)
#define MOL_MASTER_GAIN_DEFAULT 0.25f
#define MOL_SUSTAIN_ON_THRESHOLD (64.0f / 127.0f)
#define MOL_PI 3.14159265358979323846f
#define MOL_METRONOME_DURATION_SECONDS 0.025f
#define MOL_STEAL_RAMP_FRAMES 64u
#define MOL_OUTPUT_RAMP_FRAMES 128u

typedef struct mol_stereo_frame {
  float left;
  float right;
} mol_stereo_frame_t;

typedef enum mol_voice_stage {
  MOL_VOICE_IDLE = 0,
  MOL_VOICE_ATTACK = 1,
  MOL_VOICE_DECAY = 2,
  MOL_VOICE_SUSTAIN = 3,
  MOL_VOICE_RELEASE = 4,
  MOL_VOICE_HELD_BY_PEDAL = 5,
  MOL_VOICE_STOLEN_RAMP = 6
} mol_voice_stage_t;

typedef struct mol_voice {
  mol_gesture_id_t gesture_id;
  mol_frame_index_t started_at;
  mol_patch_t patch;
  mol_dsp_adsr_t amplitude;
  mol_dsp_state_variable_filter_t filter;
  mol_dsp_fm2_t fm;
  mol_dsp_additive_t additive;
  mol_dsp_karplus_strong_t pluck;
  mol_dsp_modal_bank_t modal;
  mol_dsp_lfo_t vibrato;
  float* pluck_storage;
  float phase;
  float detuned_phase;
  float phase_increment;
  float target_phase_increment;
  float glide_step;
  float envelope;
  float velocity;
  float velocity_gain;
  float instrument_gain;
  float detune_ratio;
  float pink_memory;
  float last_output;
  float stolen_tail;
  mol_voice_stage_t stage;
  uint32_t source_id;
  uint32_t glide_remaining;
  uint32_t noise_state;
  uint32_t excitation_remaining;
  uint32_t stolen_ramp_remaining;
  mol_preset_id_t preset;
  uint8_t note;
  uint8_t input_note;
  uint8_t key_down;
  uint8_t arpeggiated;
  uint8_t monophonic;
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
  uint8_t monophonic;
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
  float* pluck_memory;
  float* chorus_memory;
  float* delay_memory;
  float* reverb_memory;
  mol_gesture_t* gestures;
  mol_scheduled_command_t* commands;
  mol_event_t* events;
  uint32_t command_count;
  uint32_t event_head;
  uint32_t event_count;
  uint32_t gesture_capacity;
  mol_chorus_t chorus;
  mol_delay_t delay;
  mol_reverb_t reverb;
  mol_dsp_smoother_t master_gain;
  mol_dsp_dc_blocker_t dc_blocker[2];
  mol_dsp_limiter_t limiter[2];
  float delay_time_ms;
  float delay_sync_beats;
  float last_output[2];
  float transition_tail[2];
  uint32_t output_ramp_remaining;
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
  mol_portamento_mode_t portamento_mode;
  uint32_t portamento_frames;
  float monophonic_last_phase_increment;
  uint8_t monophonic_last_pitch_valid;
  mol_patch_t current_patch;
  mol_preset_id_t current_preset;
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

static void mol_push_preset_event(mol_engine_t* engine, const mol_command_t* command) {
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
  event->event_type = MOL_EVENT_PRESET_CHANGED;
  event->source_id = command->source_id;
  event->frame = engine->current_frame;
  event->payload[0] = (uint8_t)command->payload.preset.preset;
  event->payload[1] = command->payload.preset.hard_switch;
  ++engine->event_count;
}

static float mol_note_frequency(uint8_t note) {
  return 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
}

static void mol_voice_release(mol_engine_t* engine, mol_voice_t* voice) {
  if (voice->stage != MOL_VOICE_IDLE && voice->stage != MOL_VOICE_RELEASE) {
    mol_dsp_adsr_note_off(&voice->amplitude);
    voice->stage = MOL_VOICE_RELEASE;
    mol_push_note_event(engine, MOL_EVENT_NOTE_RELEASED, voice);
  }
}

static uint32_t mol_voice_steal_rank(const mol_voice_t* voice) {
  if (voice->stage == MOL_VOICE_RELEASE) {
    return 0u;
  }
  if (voice->stage == MOL_VOICE_HELD_BY_PEDAL) {
    return 1u;
  }
  return 2u;
}

static int mol_voice_is_better_steal(const mol_voice_t* candidate, const mol_voice_t* selected) {
  uint32_t candidate_rank = mol_voice_steal_rank(candidate);
  uint32_t selected_rank = mol_voice_steal_rank(selected);
  if (candidate_rank != selected_rank) {
    return candidate_rank < selected_rank;
  }
  if (candidate_rank < 2u && candidate->envelope != selected->envelope) {
    return candidate->envelope < selected->envelope;
  }
  if (candidate->started_at != selected->started_at) {
    return candidate->started_at < selected->started_at;
  }
  return candidate->envelope < selected->envelope;
}

static mol_voice_t* mol_allocate_voice(mol_engine_t* engine) {
  mol_voice_t* selected = &engine->voices[0];
  uint32_t index;
  for (index = 0u; index < engine->config.max_voices; ++index) {
    mol_voice_t* candidate = &engine->voices[index];
    if (candidate->stage == MOL_VOICE_IDLE) {
      return candidate;
    }
    if (mol_voice_is_better_steal(candidate, selected)) {
      selected = candidate;
    }
  }
  mol_push_note_event(engine, MOL_EVENT_VOICE_STOLEN, selected);
  return selected;
}

static float mol_patch_velocity_gain(const mol_patch_t* patch, float velocity) {
  return powf(velocity, (float)patch->velocity_curve_milli / 1000.0f);
}

static void mol_voice_configure_synthesis(mol_engine_t* engine, mol_voice_t* voice, uint8_t note) {
  static const float additive_ratios[MOL_DSP_MAX_PARTIALS] = {1.0f, 2.0f, 3.0f, 4.0f,
                                                              5.0f, 6.0f, 8.0f, 10.0f};
  static const float additive_gains[MOL_DSP_MAX_PARTIALS] = {1.0f,  0.52f, 0.31f, 0.20f,
                                                             0.14f, 0.10f, 0.07f, 0.05f};
  static const float modal_ratios[MOL_DSP_MAX_MODES] = {1.0f,  2.01f, 3.93f, 5.43f,
                                                        6.79f, 8.21f, 10.4f, 12.7f};
  static const float modal_gains[MOL_DSP_MAX_MODES] = {1.0f,  0.62f, 0.38f, 0.24f,
                                                       0.16f, 0.11f, 0.08f, 0.06f};
  float modal_decays[MOL_DSP_MAX_MODES];
  float frequency = mol_note_frequency(note);
  float model_parameter_1 = (float)voice->patch.model_parameter_1_milli / 1000.0f;
  float model_parameter_2 = (float)voice->patch.model_parameter_2_milli / 1000.0f;
  uint32_t partial_count = 3u + (uint32_t)voice->patch.model_parameter_1_milli % 6u;
  for (uint32_t index = 0u; index < MOL_DSP_MAX_MODES; ++index) {
    modal_decays[index] = (0.08f + model_parameter_2) / (1.0f + 0.35f * (float)index);
  }
  mol_dsp_adsr_configure(
      &voice->amplitude, engine->config.sample_rate, (float)voice->patch.attack_ms / 1000.0f,
      (float)voice->patch.decay_ms / 1000.0f, (float)voice->patch.sustain_milli / 1000.0f,
      (float)voice->patch.release_ms / 1000.0f);
  mol_dsp_adsr_note_on(&voice->amplitude);
  mol_dsp_state_variable_configure(&voice->filter, engine->config.sample_rate,
                                   (float)voice->patch.filter_cutoff_hz,
                                   (float)voice->patch.filter_resonance_milli / 1000.0f);
  mol_dsp_lfo_configure(&voice->vibrato, engine->config.sample_rate,
                        (float)voice->patch.vibrato_rate_millihz / 1000.0f, MOL_DSP_LFO_SINE, 0.0f);
  if (voice->patch.synthesis_model == MOL_SYNTHESIS_FM2) {
    mol_dsp_fm2_configure(&voice->fm, engine->config.sample_rate, frequency, model_parameter_1,
                          model_parameter_2);
  }
  if (voice->patch.synthesis_model == MOL_SYNTHESIS_ADDITIVE ||
      voice->patch.synthesis_model == MOL_SYNTHESIS_FORMANT) {
    mol_dsp_additive_configure(&voice->additive, engine->config.sample_rate, frequency,
                               additive_ratios, additive_gains, partial_count);
  }
  if (voice->patch.synthesis_model == MOL_SYNTHESIS_PLUCK) {
    mol_dsp_karplus_configure(&voice->pluck, voice->pluck_storage, MOL_PROFILE_PLUCK_FRAMES,
                              engine->config.sample_rate, frequency, model_parameter_1,
                              engine->config.random_seed ^ (uint32_t)voice->gesture_id ^ note);
  }
  if (voice->patch.synthesis_model == MOL_SYNTHESIS_MODAL) {
    mol_dsp_modal_configure(&voice->modal, engine->config.sample_rate, frequency, modal_ratios,
                            modal_gains, modal_decays, MOL_DSP_MAX_MODES);
  }
  voice->instrument_gain = mol_dsp_db_to_linear((float)voice->patch.gain_millidb / 1000.0f);
  voice->detune_ratio = powf(2.0f, (float)voice->patch.detune_cents / 1200.0f);
  voice->velocity_gain = mol_patch_velocity_gain(&voice->patch, voice->velocity);
  voice->noise_state = engine->config.random_seed ^ (uint32_t)voice->gesture_id ^
                       ((uint32_t)note << 24u) ^ voice->patch.preset_id_hash;
  voice->excitation_remaining = 32u;
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

static mol_voice_t* mol_start_voice(mol_engine_t* engine, const mol_gesture_t* gesture,
                                    uint8_t note, uint8_t arpeggiated) {
  mol_voice_t* voice = mol_allocate_voice(engine);
  float* pluck_storage = voice->pluck_storage;
  float stolen_tail = voice->stage != MOL_VOICE_IDLE ? voice->last_output : 0.0f;
  memset(voice, 0, sizeof(*voice));
  voice->pluck_storage = pluck_storage;
  voice->stolen_tail = stolen_tail;
  voice->stolen_ramp_remaining = stolen_tail != 0.0f ? MOL_STEAL_RAMP_FRAMES : 0u;
  voice->gesture_id = gesture->gesture_id;
  voice->started_at = engine->current_frame;
  voice->patch = engine->current_patch;
  voice->preset = engine->current_preset;
  voice->phase_increment = mol_note_frequency(note) / (float)engine->config.sample_rate;
  voice->target_phase_increment = voice->phase_increment;
  voice->velocity = gesture->velocity;
  voice->stage = MOL_VOICE_ATTACK;
  voice->source_id = gesture->source_id;
  voice->note = note;
  voice->input_note = gesture->input_note;
  voice->key_down = gesture->key_down;
  voice->arpeggiated = arpeggiated;
  mol_voice_configure_synthesis(engine, voice, note);
  mol_push_note_event(engine, MOL_EVENT_NOTE_STARTED, voice);
  return voice;
}

static mol_voice_t* mol_find_monophonic_voice(mol_engine_t* engine) {
  uint32_t index;
  for (index = 0u; index < engine->config.max_voices; ++index) {
    if (engine->voices[index].stage != MOL_VOICE_IDLE && engine->voices[index].monophonic != 0u) {
      return &engine->voices[index];
    }
  }
  return NULL;
}

static void mol_configure_glide(mol_engine_t* engine, mol_voice_t* voice,
                                float target_phase_increment, int glide) {
  voice->target_phase_increment = target_phase_increment;
  if (glide && engine->portamento_frames != 0u) {
    voice->glide_remaining = engine->portamento_frames;
    voice->glide_step =
        (target_phase_increment - voice->phase_increment) / (float)engine->portamento_frames;
  } else {
    voice->phase_increment = target_phase_increment;
    voice->glide_step = 0.0f;
    voice->glide_remaining = 0u;
  }
}

static void mol_retarget_monophonic_voice(mol_engine_t* engine, mol_voice_t* voice,
                                          const mol_gesture_t* gesture, int glide) {
  float target_phase_increment =
      mol_note_frequency(gesture->notes[0]) / (float)engine->config.sample_rate;
  voice->gesture_id = gesture->gesture_id;
  voice->started_at = engine->current_frame;
  voice->velocity = gesture->velocity;
  voice->source_id = gesture->source_id;
  voice->note = gesture->notes[0];
  voice->input_note = gesture->input_note;
  voice->key_down = gesture->key_down;
  voice->arpeggiated = 0u;
  voice->monophonic = 1u;
  if (voice->stage == MOL_VOICE_RELEASE) {
    mol_dsp_adsr_note_on(&voice->amplitude);
    voice->stage = MOL_VOICE_ATTACK;
  }
  voice->velocity_gain = mol_patch_velocity_gain(&voice->patch, voice->velocity);
  mol_configure_glide(engine, voice, target_phase_increment, glide);
  engine->monophonic_last_phase_increment = target_phase_increment;
  engine->monophonic_last_pitch_valid = 1u;
  mol_push_note_event(engine, MOL_EVENT_NOTE_STARTED, voice);
}

static void mol_start_monophonic_voice(mol_engine_t* engine, mol_gesture_t* gesture) {
  mol_voice_t* voice = mol_find_monophonic_voice(engine);
  float target_phase_increment =
      mol_note_frequency(gesture->notes[0]) / (float)engine->config.sample_rate;
  int legato = 0;
  uint32_t index;
  for (index = 0u; index < engine->gesture_capacity; ++index) {
    if (engine->gestures[index].active != 0u && engine->gestures[index].monophonic != 0u &&
        engine->gestures[index].gesture_id != gesture->gesture_id) {
      legato = 1;
      break;
    }
  }
  if (voice == NULL) {
    voice = mol_start_voice(engine, gesture, gesture->notes[0], 0u);
    voice->monophonic = 1u;
    if (engine->portamento_mode == MOL_PORTAMENTO_ALWAYS &&
        engine->monophonic_last_pitch_valid != 0u) {
      voice->phase_increment = engine->monophonic_last_phase_increment;
      mol_configure_glide(engine, voice, target_phase_increment, 1);
    }
    engine->monophonic_last_phase_increment = target_phase_increment;
    engine->monophonic_last_pitch_valid = 1u;
    return;
  }
  mol_push_note_event(engine, MOL_EVENT_NOTE_ENDED, voice);
  mol_retarget_monophonic_voice(engine, voice, gesture,
                                engine->portamento_mode == MOL_PORTAMENTO_ALWAYS || legato);
}

static mol_gesture_t* mol_latest_monophonic_gesture(mol_engine_t* engine) {
  mol_gesture_t* latest = NULL;
  uint32_t index;
  for (index = 0u; index < engine->gesture_capacity; ++index) {
    mol_gesture_t* gesture = &engine->gestures[index];
    if (gesture->active != 0u && gesture->monophonic != 0u &&
        (latest == NULL || gesture->order > latest->order)) {
      latest = gesture;
    }
  }
  return latest;
}

static void mol_finish_monophonic_gesture(mol_engine_t* engine, mol_gesture_id_t gesture_id) {
  mol_voice_t* voice = mol_find_monophonic_voice(engine);
  mol_gesture_t* fallback;
  if (voice == NULL || voice->gesture_id != gesture_id) {
    return;
  }
  fallback = mol_latest_monophonic_gesture(engine);
  if (fallback == NULL) {
    mol_voice_release(engine, voice);
    return;
  }
  mol_push_note_event(engine, MOL_EVENT_NOTE_RELEASED, voice);
  mol_retarget_monophonic_voice(engine, voice, fallback, 1);
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
  gesture->monophonic = engine->portamento_mode != MOL_PORTAMENTO_OFF &&
                                engine->chord_mode == MOL_CHORD_OFF &&
                                engine->arpeggiator_mode == MOL_ARPEGGIATOR_OFF && chord_count == 1u
                            ? 1u
                            : 0u;
  for (index = 0u; index < chord_count; ++index) {
    mol_push_mapping_event(engine, command, gesture->notes[index], (uint8_t)index,
                           (uint8_t)chord_count);
    if (engine->arpeggiator_mode == MOL_ARPEGGIATOR_OFF && gesture->monophonic == 0u) {
      mol_start_voice(engine, gesture, gesture->notes[index], 0u);
    }
  }
  if (gesture->monophonic != 0u) {
    mol_start_monophonic_voice(engine, gesture);
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

static void mol_clear_voices(mol_engine_t* engine, int emit_events) {
  for (uint32_t index = 0u; index < engine->config.max_voices; ++index) {
    mol_voice_t* voice = &engine->voices[index];
    float* pluck_storage = voice->pluck_storage;
    if (emit_events && voice->stage != MOL_VOICE_IDLE) {
      mol_push_note_event(engine, MOL_EVENT_NOTE_ENDED, voice);
    }
    memset(voice, 0, sizeof(*voice));
    voice->pluck_storage = pluck_storage;
  }
}

static void mol_clear_effect_tails(mol_engine_t* engine) {
  (void)engine;
#if MOL_ENABLE_CHORUS
  mol_chorus_clear(&engine->chorus);
#endif
#if MOL_ENABLE_DELAY
  mol_delay_clear(&engine->delay);
#endif
#if MOL_ENABLE_REVERB
  mol_reverb_clear(&engine->reverb);
#endif
}

static void mol_begin_output_transition(mol_engine_t* engine) {
  engine->transition_tail[0] = engine->last_output[0];
  engine->transition_tail[1] = engine->last_output[1];
  engine->output_ramp_remaining = MOL_OUTPUT_RAMP_FRAMES;
  mol_clear_effect_tails(engine);
}

static void mol_update_synced_delay(mol_engine_t* engine) {
#if MOL_ENABLE_DELAY
  if (engine->delay_sync_beats > 0.0f) {
    float bpm = (float)engine->tempo_milli_bpm / 1000.0f;
    float milliseconds = 60000.0f * engine->delay_sync_beats / bpm;
    mol_delay_set(&engine->delay, milliseconds, engine->delay.feedback.target,
                  engine->delay.mix.target);
  }
#else
  (void)engine;
#endif
}

static void mol_process_parameter(mol_engine_t* engine, mol_parameter_id_t parameter, float value) {
  switch (parameter) {
#if MOL_ENABLE_CHORUS
    case MOL_PARAMETER_CHORUS_RATE_HZ:
      mol_dsp_smoother_set_target(&engine->chorus.rate_hz, value);
      break;
    case MOL_PARAMETER_CHORUS_DEPTH_MS:
      mol_dsp_smoother_set_target(&engine->chorus.depth_ms, value);
      break;
    case MOL_PARAMETER_CHORUS_MIX:
      mol_dsp_smoother_set_target(&engine->chorus.mix, value);
      break;
#endif
#if MOL_ENABLE_DELAY
    case MOL_PARAMETER_DELAY_TIME_MS:
      engine->delay_time_ms = value;
      engine->delay_sync_beats = 0.0f;
      mol_delay_set(&engine->delay, value, engine->delay.feedback.target, engine->delay.mix.target);
      break;
    case MOL_PARAMETER_DELAY_FEEDBACK:
      mol_dsp_smoother_set_target(&engine->delay.feedback, value);
      break;
    case MOL_PARAMETER_DELAY_MIX:
      mol_dsp_smoother_set_target(&engine->delay.mix, value);
      break;
    case MOL_PARAMETER_DELAY_SYNC_BEATS:
      engine->delay_sync_beats = value;
      if (value == 0.0f) {
        mol_delay_set(&engine->delay, engine->delay_time_ms, engine->delay.feedback.target,
                      engine->delay.mix.target);
      } else {
        mol_update_synced_delay(engine);
      }
      break;
#endif
#if MOL_ENABLE_REVERB
    case MOL_PARAMETER_REVERB_PREDELAY_MS:
      mol_dsp_smoother_set_target(&engine->reverb.predelay_frames,
                                  value * (float)engine->config.sample_rate / 1000.0f);
      break;
    case MOL_PARAMETER_REVERB_SIZE:
      mol_dsp_smoother_set_target(&engine->reverb.size, value);
      break;
    case MOL_PARAMETER_REVERB_DAMPING:
      mol_dsp_smoother_set_target(&engine->reverb.damping, value);
      break;
    case MOL_PARAMETER_REVERB_MIX:
      mol_dsp_smoother_set_target(&engine->reverb.mix, value);
      break;
#endif
    case MOL_PARAMETER_LIMITER_CEILING_DB:
      mol_dsp_limiter_configure(&engine->limiter[0], engine->config.sample_rate, value, 0.0001f,
                                0.05f);
      mol_dsp_limiter_configure(&engine->limiter[1], engine->config.sample_rate, value, 0.0001f,
                                0.05f);
      break;
    default:
      break;
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
            if (engine->sustain >= MOL_SUSTAIN_ON_THRESHOLD &&
                engine->voices[index].stage != MOL_VOICE_RELEASE) {
              engine->voices[index].stage = MOL_VOICE_HELD_BY_PEDAL;
            }
          }
        }
        if (engine->sustain < MOL_SUSTAIN_ON_THRESHOLD) {
          gesture->active = 0u;
          if (gesture->monophonic != 0u) {
            mol_finish_monophonic_gesture(engine, command->gesture_id);
          } else {
            mol_release_gesture_voices(engine, command->gesture_id);
          }
        }
      }
      break;
    }
    case MOL_COMMAND_SUSTAIN: {
      float previous = engine->sustain;
      engine->sustain = command->payload.scalar.value;
      if (previous >= MOL_SUSTAIN_ON_THRESHOLD && engine->sustain < MOL_SUSTAIN_ON_THRESHOLD) {
        mol_voice_t* monophonic_voice = mol_find_monophonic_voice(engine);
        mol_gesture_id_t sounding_gesture =
            monophonic_voice != NULL ? monophonic_voice->gesture_id : 0u;
        for (index = 0u; index < engine->gesture_capacity; ++index) {
          if (engine->gestures[index].active != 0u && engine->gestures[index].key_down == 0u) {
            mol_gesture_id_t gesture_id = engine->gestures[index].gesture_id;
            uint8_t monophonic = engine->gestures[index].monophonic;
            engine->gestures[index].active = 0u;
            if (monophonic == 0u) {
              mol_release_gesture_voices(engine, gesture_id);
            }
          }
        }
        if (sounding_gesture != 0u && mol_find_gesture(engine, sounding_gesture) == NULL) {
          mol_finish_monophonic_gesture(engine, sounding_gesture);
        }
      }
      break;
    }
    case MOL_COMMAND_ALL_NOTES_OFF: {
      mol_voice_t* monophonic_voice = mol_find_monophonic_voice(engine);
      mol_gesture_id_t sounding_gesture =
          monophonic_voice != NULL ? monophonic_voice->gesture_id : 0u;
      for (index = 0u; index < engine->gesture_capacity; ++index) {
        if (engine->gestures[index].active != 0u) {
          mol_gesture_id_t gesture_id = engine->gestures[index].gesture_id;
          uint8_t monophonic = engine->gestures[index].monophonic;
          engine->gestures[index].key_down = 0u;
          if (engine->sustain >= MOL_SUSTAIN_ON_THRESHOLD) {
            for (uint32_t voice_index = 0u; voice_index < engine->config.max_voices;
                 ++voice_index) {
              if (engine->voices[voice_index].stage != MOL_VOICE_IDLE &&
                  engine->voices[voice_index].stage != MOL_VOICE_RELEASE &&
                  engine->voices[voice_index].gesture_id == gesture_id) {
                engine->voices[voice_index].key_down = 0u;
                engine->voices[voice_index].stage = MOL_VOICE_HELD_BY_PEDAL;
              }
            }
          }
          if (engine->sustain < MOL_SUSTAIN_ON_THRESHOLD) {
            engine->gestures[index].active = 0u;
            if (monophonic == 0u) {
              mol_release_gesture_voices(engine, gesture_id);
            }
          }
        }
      }
      if (engine->sustain < MOL_SUSTAIN_ON_THRESHOLD && sounding_gesture != 0u) {
        mol_finish_monophonic_gesture(engine, sounding_gesture);
      }
      break;
    }
    case MOL_COMMAND_ALL_SOUND_OFF:
      mol_begin_output_transition(engine);
      mol_clear_voices(engine, 1);
      memset(engine->gestures, 0, sizeof(*engine->gestures) * engine->gesture_capacity);
      engine->arpeggiator_voice_active = 0u;
      break;
    case MOL_COMMAND_SET_PRESET: {
      mol_patch_t patch = {0};
      patch.struct_size = (uint32_t)sizeof(patch);
      if (mol_builtin_patch_load(command->payload.preset.preset, &patch) == MOL_OK) {
        if (command->payload.preset.hard_switch != 0u) {
          mol_begin_output_transition(engine);
          mol_clear_voices(engine, 1);
          memset(engine->gestures, 0, sizeof(*engine->gestures) * engine->gesture_capacity);
          engine->arpeggiator_voice_active = 0u;
        } else {
          for (index = 0u; index < engine->config.max_voices; ++index) {
            engine->voices[index].monophonic = 0u;
          }
          for (index = 0u; index < engine->gesture_capacity; ++index) {
            engine->gestures[index].monophonic = 0u;
          }
        }
        engine->current_patch = patch;
        engine->current_preset = command->payload.preset.preset;
        mol_push_preset_event(engine, command);
      }
      break;
    }
    case MOL_COMMAND_SET_MASTER_GAIN:
      mol_dsp_smoother_set_target(&engine->master_gain, command->payload.scalar.value);
      break;
    case MOL_COMMAND_SET_PARAMETER:
      mol_process_parameter(engine, command->payload.parameter.parameter,
                            command->payload.parameter.value);
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
    case MOL_COMMAND_SET_PORTAMENTO:
      engine->portamento_mode = command->payload.portamento.mode;
      engine->portamento_frames = (uint32_t)(command->payload.portamento.time_ms *
                                                 (float)engine->config.sample_rate / 1000.0f +
                                             0.5f);
      break;
    case MOL_COMMAND_SET_TEMPO:
      (void)mol_tempo_to_milli_bpm(command->payload.scalar.value, &engine->tempo_milli_bpm);
      mol_update_synced_delay(engine);
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

static float mol_render_waveform(mol_voice_t* voice, float* phase, float increment) {
  float current = mol_dsp_phase_advance(phase, increment);
  switch (voice->patch.waveform) {
    case MOL_WAVEFORM_SINE:
      return mol_dsp_sine(current);
    case MOL_WAVEFORM_SQUARE:
      return mol_dsp_polyblep_square(current, increment);
    case MOL_WAVEFORM_PULSE:
      return mol_dsp_polyblep_pulse(current, increment,
                                    (float)voice->patch.pulse_width_milli / 1000.0f);
    case MOL_WAVEFORM_TRIANGLE:
      return mol_dsp_triangle(current);
    case MOL_WAVEFORM_NOISE:
      return mol_dsp_white_noise(&voice->noise_state);
    case MOL_WAVEFORM_SAW:
    default:
      return mol_dsp_polyblep_saw(current, increment);
  }
}

static float mol_render_voice_source(mol_voice_t* voice, float increment) {
  float mix = (float)voice->patch.oscillator_mix_milli / 1000.0f;
  float base = mol_render_waveform(voice, &voice->phase, increment);
  float detuned =
      mol_render_waveform(voice, &voice->detuned_phase, increment * voice->detune_ratio);
  float oscillator = base * mix + detuned * (1.0f - mix);
  float source;
  switch (voice->patch.synthesis_model) {
    case MOL_SYNTHESIS_FM2:
      voice->fm.carrier_increment = increment;
      voice->fm.modulator_increment =
          increment * ((float)voice->patch.model_parameter_1_milli / 1000.0f);
      source = 0.82f * mol_dsp_fm2_process(&voice->fm) + 0.18f * oscillator;
      break;
    case MOL_SYNTHESIS_ADDITIVE:
      voice->additive.base_increment = increment;
      source = 0.72f * mol_dsp_additive_process(&voice->additive) + 0.28f * oscillator;
      break;
    case MOL_SYNTHESIS_PLUCK:
      source = 0.88f * mol_dsp_karplus_process(&voice->pluck) + 0.12f * oscillator;
      break;
    case MOL_SYNTHESIS_MODAL: {
      float excitation = 0.0f;
      float modal_mix = mol_dsp_clamp(
          ((float)voice->patch.model_parameter_1_milli / 1000.0f - 2.35f) * 0.5f, 0.0f, 0.75f);
      if (voice->excitation_remaining != 0u) {
        excitation =
            mol_dsp_white_noise(&voice->noise_state) * ((float)voice->excitation_remaining / 32.0f);
        --voice->excitation_remaining;
      }
      source = modal_mix * mol_dsp_modal_process(&voice->modal, excitation) +
               (1.0f - modal_mix) * oscillator;
      break;
    }
    case MOL_SYNTHESIS_FORMANT:
      voice->additive.base_increment = increment;
      source = 0.58f * mol_dsp_additive_process(&voice->additive) + 0.42f * oscillator;
      break;
    case MOL_SYNTHESIS_SUBTRACTIVE:
    default:
      source = oscillator;
      break;
  }
  if (voice->patch.noise_mix_milli != 0) {
    float noise_mix = (float)voice->patch.noise_mix_milli / 1000.0f;
    float noise = mol_dsp_pink_noise(&voice->noise_state, &voice->pink_memory);
    source = source * (1.0f - noise_mix) + noise * noise_mix;
  }
  source = mol_dsp_state_variable_process(&voice->filter, source, MOL_DSP_FILTER_LOW_PASS);
  if (voice->patch.saturation_milli != 0) {
    source = mol_dsp_soft_saturate(source, 1.0f + (float)voice->patch.saturation_milli / 1000.0f);
  }
  return source;
}

static void mol_sync_voice_stage(mol_voice_t* voice) {
  if (voice->stage == MOL_VOICE_HELD_BY_PEDAL) {
    return;
  }
  switch (voice->amplitude.stage) {
    case MOL_DSP_ENVELOPE_ATTACK:
      voice->stage = MOL_VOICE_ATTACK;
      break;
    case MOL_DSP_ENVELOPE_DECAY:
      voice->stage = MOL_VOICE_DECAY;
      break;
    case MOL_DSP_ENVELOPE_SUSTAIN:
      voice->stage = MOL_VOICE_SUSTAIN;
      break;
    case MOL_DSP_ENVELOPE_RELEASE:
      voice->stage = MOL_VOICE_RELEASE;
      break;
    case MOL_DSP_ENVELOPE_IDLE:
    default:
      voice->stage = MOL_VOICE_IDLE;
      break;
  }
}

static float mol_render_voice(mol_engine_t* engine, mol_voice_t* voice) {
  float sample = 0.0f;
  float tail = 0.0f;
  mol_voice_stage_t stage_before = voice->stage;
  if (voice->stage == MOL_VOICE_IDLE && voice->stolen_ramp_remaining == 0u) {
    return 0.0f;
  }
  if (voice->glide_remaining != 0u) {
    voice->phase_increment += voice->glide_step;
    --voice->glide_remaining;
    if (voice->glide_remaining == 0u) {
      voice->phase_increment = voice->target_phase_increment;
    }
  }
  if (voice->stage != MOL_VOICE_IDLE) {
    float vibrato = mol_dsp_lfo_process(&voice->vibrato) * (float)voice->patch.vibrato_depth_cents;
    float increment = voice->phase_increment * powf(2.0f, vibrato / 1200.0f);
    voice->envelope = mol_dsp_adsr_process(&voice->amplitude);
    sample = mol_render_voice_source(voice, increment) * voice->envelope * voice->velocity_gain *
             voice->instrument_gain;
    mol_sync_voice_stage(voice);
    if (stage_before != MOL_VOICE_IDLE && voice->stage == MOL_VOICE_IDLE) {
      mol_push_note_event(engine, MOL_EVENT_NOTE_ENDED, voice);
    }
  }
  if (voice->stolen_ramp_remaining != 0u) {
    tail =
        voice->stolen_tail * ((float)voice->stolen_ramp_remaining / (float)MOL_STEAL_RAMP_FRAMES);
    --voice->stolen_ramp_remaining;
  }
  sample += tail;
  voice->last_output = isfinite(sample) ? sample : 0.0f;
  return voice->last_output;
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

static mol_stereo_frame_t mol_render_frame(mol_engine_t* engine) {
  mol_stereo_frame_t output = {0.0f, 0.0f};
  float dry = 0.0f;
  float chorus_send = 0.0f;
  float delay_send = 0.0f;
  float reverb_send = 0.0f;
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
    mol_voice_t* voice = &engine->voices[index];
    float sample = mol_render_voice(engine, voice);
    dry += sample;
    chorus_send += sample * (float)voice->patch.chorus_send_milli / 1000.0f;
    delay_send += sample * (float)voice->patch.delay_send_milli / 1000.0f;
    reverb_send += sample * (float)voice->patch.reverb_send_milli / 1000.0f;
  }
  dry += mol_render_metronome(engine);
  output.left = dry;
  output.right = dry;
#if MOL_ENABLE_CHORUS
  {
    float left;
    float right;
    mol_chorus_process(&engine->chorus, chorus_send, &left, &right);
    output.left += left;
    output.right += right;
  }
#else
  (void)chorus_send;
#endif
#if MOL_ENABLE_DELAY
  {
    float wet = mol_delay_process(&engine->delay, delay_send);
    output.left += wet;
    output.right += wet;
  }
#else
  (void)delay_send;
#endif
#if MOL_ENABLE_REVERB
  {
    float left;
    float right;
    mol_reverb_process(&engine->reverb, reverb_send, &left, &right);
    output.left += left;
    output.right += right;
  }
#else
  (void)reverb_send;
#endif
  {
    float master_gain = mol_dsp_smoother_process(&engine->master_gain);
    output.left = mol_dsp_dc_blocker_process(&engine->dc_blocker[0], output.left * master_gain);
    output.right = mol_dsp_dc_blocker_process(&engine->dc_blocker[1], output.right * master_gain);
    output.left = mol_dsp_limiter_process(&engine->limiter[0], output.left);
    output.right = mol_dsp_limiter_process(&engine->limiter[1], output.right);
  }
  if (engine->output_ramp_remaining != 0u) {
    float ramp = 1.0f - (float)engine->output_ramp_remaining / (float)MOL_OUTPUT_RAMP_FRAMES;
    output.left = output.left * ramp + engine->transition_tail[0] * (1.0f - ramp);
    output.right = output.right * ramp + engine->transition_tail[1] * (1.0f - ramp);
    --engine->output_ramp_remaining;
  }
  if (!isfinite(output.left) || !isfinite(output.right)) {
    output.left = 0.0f;
    output.right = 0.0f;
    mol_clear_effect_tails(engine);
  }
  engine->last_output[0] = output.left;
  engine->last_output[1] = output.right;
  ++engine->current_frame;
  if (engine->transport_running != 0u) {
    ++engine->transport_frame;
  }
  return output;
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
    case MOL_ERROR_CORRUPT_DATA:
      return "corrupt data";
    case MOL_ERROR_IO:
      return "I/O error";
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
  alignment = mol_max_size(alignment, _Alignof(mol_event_t));
  return mol_max_size(alignment, _Alignof(float));
}

size_t mol_engine_query_memory(const mol_engine_config_t* config) {
  size_t size = 0u;
  if (!mol_engine_config_is_valid(config) ||
      !mol_size_add_array(&size, _Alignof(mol_engine_t), sizeof(mol_engine_t), 1u) ||
      !mol_size_add_array(&size, _Alignof(mol_voice_t), sizeof(mol_voice_t), config->max_voices) ||
      !mol_size_add_array(&size, _Alignof(float), sizeof(float),
                          (size_t)config->max_voices * MOL_PROFILE_PLUCK_FRAMES) ||
#if MOL_ENABLE_CHORUS
      !mol_size_add_array(&size, _Alignof(float), sizeof(float), MOL_PROFILE_CHORUS_FRAMES) ||
#endif
#if MOL_ENABLE_DELAY
      !mol_size_add_array(&size, _Alignof(float), sizeof(float), MOL_PROFILE_DELAY_FRAMES) ||
#endif
#if MOL_ENABLE_REVERB
      !mol_size_add_array(&size, _Alignof(float), sizeof(float), MOL_PROFILE_REVERB_FRAMES) ||
#endif
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
  engine->pluck_memory =
      (float*)mol_arena_take(bytes, &offset, _Alignof(float), sizeof(float),
                             (size_t)config->max_voices * MOL_PROFILE_PLUCK_FRAMES);
#if MOL_ENABLE_CHORUS
  engine->chorus_memory = (float*)mol_arena_take(bytes, &offset, _Alignof(float), sizeof(float),
                                                 MOL_PROFILE_CHORUS_FRAMES);
#endif
#if MOL_ENABLE_DELAY
  engine->delay_memory = (float*)mol_arena_take(bytes, &offset, _Alignof(float), sizeof(float),
                                                MOL_PROFILE_DELAY_FRAMES);
#endif
#if MOL_ENABLE_REVERB
  engine->reverb_memory = (float*)mol_arena_take(bytes, &offset, _Alignof(float), sizeof(float),
                                                 MOL_PROFILE_REVERB_FRAMES);
#endif
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
    float previous_left = engine->last_output[0];
    float previous_right = engine->last_output[1];
    mol_clear_voices(engine, 0);
    memset(engine->pluck_memory, 0,
           sizeof(*engine->pluck_memory) * engine->config.max_voices * MOL_PROFILE_PLUCK_FRAMES);
    for (uint32_t index = 0u; index < engine->config.max_voices; ++index) {
      engine->voices[index].pluck_storage =
          engine->pluck_memory + (size_t)index * MOL_PROFILE_PLUCK_FRAMES;
    }
    memset(engine->gestures, 0, sizeof(*engine->gestures) * engine->gesture_capacity);
    engine->current_frame = 0u;
    engine->submit_serial = 0u;
    engine->command_count = 0u;
    engine->event_head = 0u;
    engine->event_count = 0u;
    mol_dsp_smoother_configure(&engine->master_gain, engine->config.sample_rate, 0.01f,
                               MOL_MASTER_GAIN_DEFAULT);
    mol_dsp_dc_blocker_configure(&engine->dc_blocker[0], 0.995f);
    mol_dsp_dc_blocker_configure(&engine->dc_blocker[1], 0.995f);
    mol_dsp_limiter_configure(&engine->limiter[0], engine->config.sample_rate, -1.0f, 0.0001f,
                              0.05f);
    mol_dsp_limiter_configure(&engine->limiter[1], engine->config.sample_rate, -1.0f, 0.0001f,
                              0.05f);
#if MOL_ENABLE_CHORUS
    mol_chorus_configure(&engine->chorus, engine->chorus_memory, MOL_PROFILE_CHORUS_FRAMES,
                         engine->config.sample_rate);
#endif
#if MOL_ENABLE_DELAY
    mol_delay_configure(&engine->delay, engine->delay_memory, MOL_PROFILE_DELAY_FRAMES,
                        engine->config.sample_rate);
#endif
#if MOL_ENABLE_REVERB
    mol_reverb_configure(&engine->reverb, engine->reverb_memory, MOL_PROFILE_REVERB_FRAMES,
                         engine->config.sample_rate,
                         (float)MOL_PROFILE_REVERB_SCALE_MILLI / 1000.0f);
#endif
    engine->delay_time_ms = 240.0f;
    engine->delay_sync_beats = 0.0f;
    engine->last_output[0] = previous_left;
    engine->last_output[1] = previous_right;
    engine->transition_tail[0] = previous_left;
    engine->transition_tail[1] = previous_right;
    engine->output_ramp_remaining = MOL_OUTPUT_RAMP_FRAMES;
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
    engine->portamento_mode = MOL_PORTAMENTO_OFF;
    engine->portamento_frames = 0u;
    engine->monophonic_last_phase_increment = 0.0f;
    engine->monophonic_last_pitch_valid = 0u;
    engine->current_patch.struct_size = (uint32_t)sizeof(engine->current_patch);
    engine->current_preset = MOL_PRESET_GRAND_PIANO;
    (void)mol_builtin_patch_load(engine->current_preset, &engine->current_patch);
  }
}

static mol_result_t mol_validate_parameter(const mol_engine_t* engine, mol_parameter_id_t parameter,
                                           float value) {
  (void)engine;
  if (!isfinite(value)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  switch (parameter) {
#if MOL_ENABLE_CHORUS
    case MOL_PARAMETER_CHORUS_RATE_HZ:
      return value >= MOL_CHORUS_RATE_MIN_HZ && value <= MOL_CHORUS_RATE_MAX_HZ
                 ? MOL_OK
                 : MOL_ERROR_INVALID_ARGUMENT;
    case MOL_PARAMETER_CHORUS_DEPTH_MS:
      return value >= MOL_CHORUS_DEPTH_MIN_MS && value <= MOL_CHORUS_DEPTH_MAX_MS
                 ? MOL_OK
                 : MOL_ERROR_INVALID_ARGUMENT;
    case MOL_PARAMETER_CHORUS_MIX:
      return value >= 0.0f && value <= 1.0f ? MOL_OK : MOL_ERROR_INVALID_ARGUMENT;
#endif
#if MOL_ENABLE_DELAY
    case MOL_PARAMETER_DELAY_TIME_MS: {
      float maximum =
          1000.0f * ((float)MOL_PROFILE_DELAY_FRAMES - 2.0f) / (float)engine->config.sample_rate;
      if (maximum > MOL_DELAY_TIME_MAX_MS) {
        maximum = MOL_DELAY_TIME_MAX_MS;
      }
      return value >= MOL_DELAY_TIME_MIN_MS && value <= maximum ? MOL_OK
                                                                : MOL_ERROR_INVALID_ARGUMENT;
    }
    case MOL_PARAMETER_DELAY_FEEDBACK:
      return value >= 0.0f && value <= MOL_DELAY_FEEDBACK_MAX ? MOL_OK : MOL_ERROR_INVALID_ARGUMENT;
    case MOL_PARAMETER_DELAY_MIX:
      return value >= 0.0f && value <= 1.0f ? MOL_OK : MOL_ERROR_INVALID_ARGUMENT;
    case MOL_PARAMETER_DELAY_SYNC_BEATS: {
      float milliseconds =
          value == 0.0f ? 0.0f : 60000000.0f * value / (float)engine->tempo_milli_bpm;
      float maximum =
          1000.0f * ((float)MOL_PROFILE_DELAY_FRAMES - 2.0f) / (float)engine->config.sample_rate;
      return (value == 0.0f || (value >= 0.125f && value <= MOL_DELAY_SYNC_MAX_BEATS)) &&
                     milliseconds <= maximum
                 ? MOL_OK
                 : MOL_ERROR_INVALID_ARGUMENT;
    }
#endif
#if MOL_ENABLE_REVERB
    case MOL_PARAMETER_REVERB_PREDELAY_MS: {
      float maximum = 1000.0f * ((float)engine->reverb.predelay_capacity - 2.0f) /
                      (float)engine->config.sample_rate;
      return value >= 0.0f && value <= maximum && value <= MOL_REVERB_PREDELAY_MAX_MS
                 ? MOL_OK
                 : MOL_ERROR_INVALID_ARGUMENT;
    }
    case MOL_PARAMETER_REVERB_SIZE:
    case MOL_PARAMETER_REVERB_MIX:
      return value >= 0.0f && value <= 1.0f ? MOL_OK : MOL_ERROR_INVALID_ARGUMENT;
    case MOL_PARAMETER_REVERB_DAMPING:
      return value >= 0.0f && value <= MOL_REVERB_DAMPING_MAX ? MOL_OK : MOL_ERROR_INVALID_ARGUMENT;
#endif
    case MOL_PARAMETER_LIMITER_CEILING_DB:
      return value >= MOL_LIMITER_CEILING_MIN_DB && value <= MOL_LIMITER_CEILING_MAX_DB
                 ? MOL_OK
                 : MOL_ERROR_INVALID_ARGUMENT;
    default:
      return MOL_ERROR_UNSUPPORTED;
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
    case MOL_COMMAND_SET_PRESET:
      if (command->payload.preset.preset >= MOL_PRESET_COUNT ||
          command->payload.preset.hard_switch > 1u) {
        return MOL_ERROR_INVALID_ARGUMENT;
      }
      break;
    case MOL_COMMAND_SET_PARAMETER: {
      mol_result_t result = mol_validate_parameter(engine, command->payload.parameter.parameter,
                                                   command->payload.parameter.value);
      if (result != MOL_OK) {
        return result;
      }
      break;
    }
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
    case MOL_COMMAND_SET_PORTAMENTO:
      if (command->payload.portamento.mode >= MOL_PORTAMENTO_MODE_COUNT ||
          !isfinite(command->payload.portamento.time_ms) ||
          command->payload.portamento.time_ms < 0.0f ||
          command->payload.portamento.time_ms > 2000.0f) {
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
  mol_result_t result = mol_validate_render(engine, frame_count, channel_count);
  if (result != MOL_OK || output == NULL) {
    return result != MOL_OK ? result : MOL_ERROR_INVALID_ARGUMENT;
  }
  if ((size_t)channel_count > SIZE_MAX / sizeof(*output) ||
      (size_t)frame_count > SIZE_MAX / ((size_t)channel_count * sizeof(*output))) {
    return MOL_ERROR_OVERFLOW;
  }
  for (frame = 0u; frame < frame_count; ++frame) {
    mol_stereo_frame_t sample = mol_render_frame(engine);
    if (channel_count == 1u) {
      output[frame] = 0.5f * (sample.left + sample.right);
    } else {
      output[(size_t)frame * channel_count] = sample.left;
      output[(size_t)frame * channel_count + 1u] = sample.right;
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
    mol_stereo_frame_t sample = mol_render_frame(engine);
    if (channel_count == 1u) {
      output_channels[0][frame] = 0.5f * (sample.left + sample.right);
    } else {
      output_channels[0][frame] = sample.left;
      output_channels[1][frame] = sample.right;
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
  state->portamento_mode = engine->portamento_mode;
  state->portamento_time_ms =
      (float)engine->portamento_frames * 1000.0f / (float)engine->config.sample_rate;
  state->preset = engine->current_preset;
  return MOL_OK;
}

mol_capability_flags_t mol_engine_get_capabilities(const mol_engine_t* engine) {
  mol_capability_flags_t capabilities;
  if (!mol_engine_is_valid(engine)) {
    return 0u;
  }
  capabilities = MOL_CAPABILITY_CALLER_MEMORY | MOL_CAPABILITY_INTERLEAVED_F32 |
                 MOL_CAPABILITY_PLANAR_F32 | MOL_CAPABILITY_POLYPHONIC_SYNTH |
                 MOL_CAPABILITY_SAMPLE_ACCURATE_COMMANDS | MOL_CAPABILITY_SCALE_LOCK |
                 MOL_CAPABILITY_CHORD_MODE | MOL_CAPABILITY_CONTINUOUS_SUSTAIN |
                 MOL_CAPABILITY_TRANSPORT | MOL_CAPABILITY_METRONOME | MOL_CAPABILITY_ARPEGGIATOR |
                 MOL_CAPABILITY_MONOPHONIC_PORTAMENTO | MOL_CAPABILITY_BUILTIN_PATCHES |
                 MOL_CAPABILITY_LIMITER;
#if MOL_ENABLE_CHORUS
  capabilities |= MOL_CAPABILITY_CHORUS | MOL_CAPABILITY_STEREO_EFFECTS;
#endif
#if MOL_ENABLE_DELAY
  capabilities |= MOL_CAPABILITY_DELAY;
#endif
#if MOL_ENABLE_REVERB
  capabilities |= MOL_CAPABILITY_REVERB | MOL_CAPABILITY_STEREO_EFFECTS;
#endif
  return capabilities;
}
