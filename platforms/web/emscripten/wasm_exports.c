/* SPDX-License-Identifier: Apache-2.0 */
#include <emscripten/emscripten.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mol/mol.h"

#define MOL_WASM_MAX_FRAMES 128u
#define MOL_WASM_MAX_EVENTS 64u
#define MOL_WASM_SEQUENCE_BYTES 2097152u

typedef union mol_wasm_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[2097152];
} mol_wasm_storage_t;

static mol_wasm_storage_t mol_wasm_memory;
static mol_engine_t* mol_wasm_engine;
static float mol_wasm_output[MOL_WASM_MAX_FRAMES * 2u];
static mol_event_t mol_wasm_core_events[MOL_WASM_MAX_EVENTS];
static uint32_t mol_wasm_event_words[MOL_WASM_MAX_EVENTS * 4u];
static mol_sequence_config_t mol_wasm_sequence_config;
static mol_sequence_event_t mol_wasm_sequence_events[MOL_PROFILE_SEQUENCE_EVENTS];
static uint8_t mol_wasm_sequence_output[MOL_WASM_SEQUENCE_BYTES];
static uint8_t mol_wasm_sequence_input[MOL_WASM_SEQUENCE_BYTES];
static uint32_t mol_wasm_sequence_output_size;
static int32_t mol_wasm_recording_error;

typedef struct mol_wasm_sequence_writer {
  size_t offset;
} mol_wasm_sequence_writer_t;

typedef struct mol_wasm_sequence_reader {
  size_t offset;
  size_t size;
  uint32_t event_count;
} mol_wasm_sequence_reader_t;

static mol_result_t mol_wasm_write_sequence(void* user_data, const uint8_t* data, size_t size) {
  mol_wasm_sequence_writer_t* destination = (mol_wasm_sequence_writer_t*)user_data;
  if (destination == NULL || data == NULL || destination->offset > MOL_WASM_SEQUENCE_BYTES ||
      size > MOL_WASM_SEQUENCE_BYTES - destination->offset) {
    return MOL_ERROR_BUFFER_TOO_SMALL;
  }
  memcpy(mol_wasm_sequence_output + destination->offset, data, size);
  destination->offset += size;
  return MOL_OK;
}

static size_t mol_wasm_read_sequence(void* user_data, uint8_t* data, size_t capacity) {
  mol_wasm_sequence_reader_t* source = (mol_wasm_sequence_reader_t*)user_data;
  size_t remaining;
  size_t copy_size;
  if (source == NULL || data == NULL || source->offset > source->size) return 0u;
  remaining = source->size - source->offset;
  copy_size = capacity < remaining ? capacity : remaining;
  memcpy(data, mol_wasm_sequence_input + source->offset, copy_size);
  source->offset += copy_size;
  return copy_size;
}

static mol_result_t mol_wasm_collect_event(void* user_data, const mol_sequence_event_t* event) {
  mol_wasm_sequence_reader_t* reader = (mol_wasm_sequence_reader_t*)user_data;
  if (reader == NULL || event == NULL || reader->event_count >= MOL_PROFILE_SEQUENCE_EVENTS) {
    return MOL_ERROR_BUFFER_TOO_SMALL;
  }
  mol_wasm_sequence_events[reader->event_count++] = *event;
  return MOL_OK;
}

static mol_command_t mol_wasm_command(mol_command_type_t type) {
  mol_command_t command;
  memset(&command, 0, sizeof(command));
  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = type;
  command.target_frame = MOL_FRAME_IMMEDIATE;
  return command;
}

static mol_command_t mol_wasm_note_command(mol_command_type_t type, uint8_t note, float velocity,
                                           mol_gesture_id_t gesture_id) {
  mol_command_t command = mol_wasm_command(type);
  command.gesture_id = gesture_id;
  command.payload.note.note = note;
  command.payload.note.velocity = velocity;
  return command;
}

EMSCRIPTEN_KEEPALIVE int mol_wasm_initialize(uint32_t sample_rate, uint32_t channel_count,
                                             uint32_t max_voices) {
  mol_engine_config_t config = mol_engine_config_default();
  if (mol_wasm_engine != NULL) {
    mol_engine_shutdown(mol_wasm_engine);
    mol_wasm_engine = NULL;
  }
  config.sample_rate = sample_rate;
  config.channel_count = channel_count;
  config.max_voices = max_voices;
  config.command_capacity = 64u;
  config.event_capacity = 64u;
  if (mol_engine_query_memory(&config) > sizeof(mol_wasm_memory.bytes)) {
    return MOL_ERROR_INSUFFICIENT_MEMORY;
  }
  return mol_engine_init(mol_wasm_memory.bytes, sizeof(mol_wasm_memory.bytes), &config,
                         &mol_wasm_engine);
}

EMSCRIPTEN_KEEPALIVE int mol_wasm_note_on(uint32_t note, float velocity, uint32_t gesture_id) {
  mol_command_t command;
  if (note > 127u || mol_wasm_engine == NULL) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  command = mol_wasm_note_command(MOL_COMMAND_NOTE_ON, (uint8_t)note, velocity,
                                  (mol_gesture_id_t)gesture_id);
  return mol_engine_submit(mol_wasm_engine, &command);
}

EMSCRIPTEN_KEEPALIVE int mol_wasm_note_off(uint32_t gesture_id) {
  mol_command_t command;
  if (mol_wasm_engine == NULL) {
    return MOL_ERROR_INVALID_STATE;
  }
  command = mol_wasm_note_command(MOL_COMMAND_NOTE_OFF, 0u, 0.0f, (mol_gesture_id_t)gesture_id);
  return mol_engine_submit(mol_wasm_engine, &command);
}

EMSCRIPTEN_KEEPALIVE int mol_wasm_submit_scalar(uint32_t command_type, float value) {
  mol_command_t command;
  if (mol_wasm_engine == NULL ||
      (command_type != MOL_COMMAND_SUSTAIN && command_type != MOL_COMMAND_SET_MASTER_GAIN &&
       command_type != MOL_COMMAND_SET_TEMPO)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  command = mol_wasm_command(command_type);
  command.payload.scalar.value = value;
  return mol_engine_submit(mol_wasm_engine, &command);
}

EMSCRIPTEN_KEEPALIVE int mol_wasm_submit_integer(uint32_t command_type, int32_t value) {
  mol_command_t command;
  if (mol_wasm_engine == NULL ||
      (command_type != MOL_COMMAND_SET_OCTAVE_SHIFT && command_type != MOL_COMMAND_SET_TRANSPOSE &&
       command_type != MOL_COMMAND_SET_CHORD_MODE)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  command = mol_wasm_command(command_type);
  command.payload.integer.value = value;
  return mol_engine_submit(mol_wasm_engine, &command);
}

EMSCRIPTEN_KEEPALIVE int mol_wasm_submit_parameter(uint32_t parameter, float value) {
  mol_command_t command;
  if (mol_wasm_engine == NULL) return MOL_ERROR_INVALID_STATE;
  command = mol_wasm_command(MOL_COMMAND_SET_PARAMETER);
  command.payload.parameter.parameter = parameter;
  command.payload.parameter.value = value;
  return mol_engine_submit(mol_wasm_engine, &command);
}

EMSCRIPTEN_KEEPALIVE int mol_wasm_submit_preset(uint32_t preset, uint32_t hard_switch) {
  mol_command_t command;
  if (mol_wasm_engine == NULL) return MOL_ERROR_INVALID_STATE;
  command = mol_wasm_command(MOL_COMMAND_SET_PRESET);
  command.payload.preset.preset = preset;
  command.payload.preset.hard_switch = hard_switch != 0u ? 1u : 0u;
  return mol_engine_submit(mol_wasm_engine, &command);
}

EMSCRIPTEN_KEEPALIVE int mol_wasm_submit_scale(uint32_t type, uint32_t tonic, uint32_t mapping) {
  mol_command_t command;
  if (mol_wasm_engine == NULL || tonic > 11u || mapping > UINT8_MAX) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  command = mol_wasm_command(MOL_COMMAND_SET_SCALE);
  command.payload.scale.type = type;
  command.payload.scale.tonic = (uint8_t)tonic;
  command.payload.scale.mapping = (uint8_t)mapping;
  return mol_engine_submit(mol_wasm_engine, &command);
}

EMSCRIPTEN_KEEPALIVE int mol_wasm_submit_arpeggiator(uint32_t mode, uint32_t rate, float gate,
                                                     uint32_t octaves, uint32_t random_seed) {
  mol_command_t command;
  if (mol_wasm_engine == NULL || octaves > UINT8_MAX) return MOL_ERROR_INVALID_ARGUMENT;
  command = mol_wasm_command(MOL_COMMAND_SET_ARPEGGIATOR);
  command.payload.arpeggiator.mode = mode;
  command.payload.arpeggiator.rate = rate;
  command.payload.arpeggiator.gate = gate;
  command.payload.arpeggiator.octaves = (uint8_t)octaves;
  command.payload.arpeggiator.random_seed = random_seed;
  return mol_engine_submit(mol_wasm_engine, &command);
}

EMSCRIPTEN_KEEPALIVE int mol_wasm_submit_time_signature(uint32_t numerator, uint32_t denominator) {
  mol_command_t command;
  if (mol_wasm_engine == NULL || numerator > UINT8_MAX || denominator > UINT8_MAX) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  command = mol_wasm_command(MOL_COMMAND_SET_TIME_SIGNATURE);
  command.payload.time_signature.numerator = (uint8_t)numerator;
  command.payload.time_signature.denominator = (uint8_t)denominator;
  return mol_engine_submit(mol_wasm_engine, &command);
}

EMSCRIPTEN_KEEPALIVE int mol_wasm_submit_metronome(uint32_t enabled, float level) {
  mol_command_t command;
  if (mol_wasm_engine == NULL) return MOL_ERROR_INVALID_STATE;
  command = mol_wasm_command(MOL_COMMAND_SET_METRONOME);
  command.payload.metronome.enabled = enabled != 0u ? 1u : 0u;
  command.payload.metronome.level = level;
  return mol_engine_submit(mol_wasm_engine, &command);
}

EMSCRIPTEN_KEEPALIVE int mol_wasm_submit_portamento(uint32_t mode, float time_ms) {
  mol_command_t command;
  if (mol_wasm_engine == NULL) return MOL_ERROR_INVALID_STATE;
  command = mol_wasm_command(MOL_COMMAND_SET_PORTAMENTO);
  command.payload.portamento.mode = mode;
  command.payload.portamento.time_ms = time_ms;
  return mol_engine_submit(mol_wasm_engine, &command);
}

EMSCRIPTEN_KEEPALIVE int mol_wasm_submit_action(uint32_t command_type) {
  mol_command_t command;
  if (mol_wasm_engine == NULL ||
      (command_type != MOL_COMMAND_ALL_NOTES_OFF && command_type != MOL_COMMAND_ALL_SOUND_OFF &&
       command_type != MOL_COMMAND_TRANSPORT_START && command_type != MOL_COMMAND_TRANSPORT_STOP &&
       command_type != MOL_COMMAND_RECORD_START && command_type != MOL_COMMAND_RECORD_STOP &&
       command_type != MOL_COMMAND_PLAYBACK_START && command_type != MOL_COMMAND_PLAYBACK_STOP &&
       command_type != MOL_COMMAND_RESET_ENGINE)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  command = mol_wasm_command(command_type);
  return mol_engine_submit(mol_wasm_engine, &command);
}

EMSCRIPTEN_KEEPALIVE uint32_t mol_wasm_poll_events(void) {
  uint32_t count;
  uint32_t index;
  if (mol_wasm_engine == NULL) return 0u;
  count = mol_engine_poll_events(mol_wasm_engine, mol_wasm_core_events, MOL_WASM_MAX_EVENTS);
  for (index = 0u; index < count; ++index) {
    const mol_event_t* event = &mol_wasm_core_events[index];
    uint32_t* words = &mol_wasm_event_words[index * 4u];
    words[0] = event->event_type;
    words[1] = (uint32_t)event->gesture_id;
    words[2] = (uint32_t)event->frame;
    words[3] = event->payload[MOL_EVENT_PAYLOAD_NOTE];
  }
  return count;
}

EMSCRIPTEN_KEEPALIVE const uint32_t* mol_wasm_event_buffer(void) { return mol_wasm_event_words; }

EMSCRIPTEN_KEEPALIVE uint32_t mol_wasm_export_recording(void) {
  mol_sequence_writer_t writer;
  mol_wasm_sequence_writer_t destination;
  uint32_t event_count = 0u;
  uint32_t index;
  mol_result_t result;
  if (mol_wasm_engine == NULL) return 0u;
  mol_wasm_recording_error = MOL_OK;
  memset(&destination, 0, sizeof(destination));
  memset(&mol_wasm_sequence_config, 0, sizeof(mol_wasm_sequence_config));
  mol_wasm_sequence_config.struct_size = (uint32_t)sizeof(mol_wasm_sequence_config);
  mol_wasm_sequence_config.api_version = MOL_API_VERSION;
  mol_wasm_sequence_output_size = 0u;
  result = mol_engine_copy_recording(mol_wasm_engine, &mol_wasm_sequence_config,
                                     mol_wasm_sequence_events, MOL_PROFILE_SEQUENCE_EVENTS,
                                     &event_count);
  if (result != MOL_OK) {
    mol_wasm_recording_error = result;
    return 0u;
  }
  if (event_count == 0u) {
    mol_wasm_recording_error = MOL_ERROR_INVALID_STATE;
    return 0u;
  }
  memset(&writer, 0, sizeof(writer));
  writer.struct_size = (uint32_t)sizeof(writer);
  writer.api_version = MOL_API_VERSION;
  result = mol_sequence_writer_init(&writer, &mol_wasm_sequence_config, mol_wasm_write_sequence,
                                    &destination);
  for (index = 0u; result == MOL_OK && index < event_count; ++index) {
    result = mol_sequence_writer_append(&writer, &mol_wasm_sequence_events[index]);
  }
  if (result == MOL_OK) result = mol_sequence_writer_finalize(&writer);
  if (result != MOL_OK || destination.offset > UINT32_MAX) {
    mol_wasm_recording_error = result != MOL_OK ? result : MOL_ERROR_OVERFLOW;
    return 0u;
  }
  mol_wasm_sequence_output_size = (uint32_t)destination.offset;
  return mol_wasm_sequence_output_size;
}

EMSCRIPTEN_KEEPALIVE int32_t mol_wasm_recording_last_error(void) {
  return mol_wasm_recording_error;
}

EMSCRIPTEN_KEEPALIVE const uint8_t* mol_wasm_recording_buffer(void) {
  return mol_wasm_sequence_output;
}

EMSCRIPTEN_KEEPALIVE uint8_t* mol_wasm_sequence_input_buffer(void) {
  return mol_wasm_sequence_input;
}

EMSCRIPTEN_KEEPALIVE uint32_t mol_wasm_sequence_input_capacity(void) {
  return MOL_WASM_SEQUENCE_BYTES;
}

EMSCRIPTEN_KEEPALIVE int mol_wasm_load_sequence(uint32_t size) {
  mol_wasm_sequence_reader_t source;
  mol_sequence_callbacks_t callbacks;
  mol_result_t result;
  if (mol_wasm_engine == NULL || size == 0u || size > MOL_WASM_SEQUENCE_BYTES) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  memset(&source, 0, sizeof(source));
  source.size = size;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.struct_size = (uint32_t)sizeof(callbacks);
  callbacks.api_version = MOL_API_VERSION;
  callbacks.on_event = mol_wasm_collect_event;
  callbacks.user_data = &source;
  memset(&mol_wasm_sequence_config, 0, sizeof(mol_wasm_sequence_config));
  mol_wasm_sequence_config.struct_size = (uint32_t)sizeof(mol_wasm_sequence_config);
  mol_wasm_sequence_config.api_version = MOL_API_VERSION;
  result = mol_sequence_read_stream(mol_wasm_read_sequence, &source, &mol_wasm_sequence_config,
                                    &callbacks);
  if (result != MOL_OK || source.offset != source.size) return result;
  return mol_engine_load_sequence(mol_wasm_engine, &mol_wasm_sequence_config,
                                  mol_wasm_sequence_events, source.event_count);
}

EMSCRIPTEN_KEEPALIVE const float* mol_wasm_render(uint32_t frame_count, uint32_t channel_count) {
  if (mol_wasm_engine == NULL || frame_count > MOL_WASM_MAX_FRAMES || channel_count == 0u ||
      channel_count > 2u) {
    return NULL;
  }
  if (mol_engine_render_interleaved_f32(mol_wasm_engine, mol_wasm_output, frame_count,
                                        channel_count) != MOL_OK) {
    return NULL;
  }
  return mol_wasm_output;
}
