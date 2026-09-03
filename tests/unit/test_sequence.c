/* SPDX-License-Identifier: Apache-2.0 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mol/mol.h"

#define TEST_BUFFER_SIZE 8192u
#define TEST_EVENT_CAPACITY 32u

typedef struct memory_stream {
  uint8_t data[TEST_BUFFER_SIZE];
  size_t size;
  size_t position;
  size_t capacity;
  size_t max_read;
} memory_stream_t;

typedef struct capture {
  mol_sequence_event_t events[TEST_EVENT_CAPACITY];
  uint32_t event_count;
  uint32_t metadata_type;
  uint8_t metadata[MOL_SEQUENCE_MAX_METADATA_SIZE];
  size_t metadata_size;
} capture_t;

static int failures = 0;

#define EXPECT_TRUE(condition)                                                                  \
  do {                                                                                          \
    if (!(condition)) {                                                                         \
      (void)fprintf(stderr, "%s:%d: expectation failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                               \
    }                                                                                           \
  } while (0)

static mol_result_t write_memory(void* user_data, const uint8_t* data, size_t size) {
  memory_stream_t* stream = (memory_stream_t*)user_data;
  if (size > stream->capacity - stream->size) return MOL_ERROR_IO;
  memcpy(stream->data + stream->size, data, size);
  stream->size += size;
  return MOL_OK;
}

static size_t read_memory(void* user_data, uint8_t* data, size_t capacity) {
  memory_stream_t* stream = (memory_stream_t*)user_data;
  size_t remaining = stream->size - stream->position;
  size_t count = remaining < capacity ? remaining : capacity;
  if (stream->max_read != 0u && count > stream->max_read) count = stream->max_read;
  memcpy(data, stream->data + stream->position, count);
  stream->position += count;
  return count;
}

static mol_result_t capture_event(void* user_data, const mol_sequence_event_t* event) {
  capture_t* capture = (capture_t*)user_data;
  if (capture->event_count >= TEST_EVENT_CAPACITY) return MOL_ERROR_BUFFER_TOO_SMALL;
  capture->events[capture->event_count++] = *event;
  return MOL_OK;
}

static mol_result_t capture_metadata(void* user_data, uint32_t chunk_type, const uint8_t* data,
                                     size_t size) {
  capture_t* capture = (capture_t*)user_data;
  if (size > sizeof(capture->metadata)) return MOL_ERROR_BUFFER_TOO_SMALL;
  capture->metadata_type = chunk_type;
  memcpy(capture->metadata, data, size);
  capture->metadata_size = size;
  return MOL_OK;
}

static mol_sequence_event_t make_event(mol_command_type_t type, mol_frame_index_t frame) {
  mol_sequence_event_t event;
  memset(&event, 0, sizeof(event));
  event.struct_size = (uint32_t)sizeof(event);
  event.api_version = MOL_API_VERSION;
  event.frame = frame;
  event.command_type = type;
  event.source_id = 7u;
  event.gesture_id = 100u + frame;
  return event;
}

static size_t write_fixture(memory_stream_t* stream, mol_sequence_event_t* expected,
                            uint32_t* expected_count) {
  static const uint8_t title[] = "Scale study";
  mol_sequence_config_t config = mol_sequence_config_default(48000u);
  mol_sequence_writer_t writer = {0};
  mol_sequence_event_t event;
  writer.struct_size = (uint32_t)sizeof(writer);
  writer.api_version = MOL_API_VERSION;
  config.initial_state.preset = MOL_PRESET_ELECTRIC_PIANO;
  config.initial_state.tempo = 120.0f;
  config.initial_state.scale_type = MOL_SCALE_DORIAN;
  config.initial_state.scale_tonic = 2u;
  config.initial_state.arpeggiator_random_seed = UINT32_C(0x10203040);
  EXPECT_TRUE(mol_sequence_writer_init(&writer, &config, write_memory, stream) == MOL_OK);
  EXPECT_TRUE(mol_sequence_writer_add_metadata(&writer, UINT32_C(0x454D414E), title,
                                               sizeof(title) - 1u) == MOL_OK);

  event = make_event(MOL_COMMAND_SET_PRESET, 0u);
  event.payload.preset.preset = MOL_PRESET_ELECTRIC_PIANO;
  expected[(*expected_count)++] = event;
  EXPECT_TRUE(mol_sequence_writer_append(&writer, &event) == MOL_OK);

  event = make_event(MOL_COMMAND_NOTE_ON, 0u);
  event.payload.note.note = 62u;
  event.payload.note.velocity = 0.75f;
  expected[(*expected_count)++] = event;
  EXPECT_TRUE(mol_sequence_writer_append(&writer, &event) == MOL_OK);

  event = make_event(MOL_COMMAND_SUSTAIN, 12000u);
  event.payload.scalar.value = 1.0f;
  expected[(*expected_count)++] = event;
  EXPECT_TRUE(mol_sequence_writer_append(&writer, &event) == MOL_OK);

  event = make_event(MOL_COMMAND_SET_ARPEGGIATOR, 12000u);
  event.payload.arpeggiator.mode = MOL_ARPEGGIATOR_RANDOM_DETERMINISTIC;
  event.payload.arpeggiator.rate = MOL_ARPEGGIATOR_RATE_EIGHTH_TRIPLET;
  event.payload.arpeggiator.gate = 0.625f;
  event.payload.arpeggiator.random_seed = UINT32_C(0x10203040);
  event.payload.arpeggiator.octaves = 2u;
  expected[(*expected_count)++] = event;
  EXPECT_TRUE(mol_sequence_writer_append(&writer, &event) == MOL_OK);

  event = make_event(MOL_COMMAND_NOTE_OFF, 36000u);
  event.payload.note.note = 62u;
  event.payload.note.velocity = 0.5f;
  expected[(*expected_count)++] = event;
  EXPECT_TRUE(mol_sequence_writer_append(&writer, &event) == MOL_OK);

  event = make_event(MOL_COMMAND_SUSTAIN, 48000u);
  event.payload.scalar.value = 0.0f;
  expected[(*expected_count)++] = event;
  EXPECT_TRUE(mol_sequence_writer_append(&writer, &event) == MOL_OK);

  event = make_event(MOL_COMMAND_SET_PARAMETER, 60000u);
  event.payload.parameter.parameter = MOL_PARAMETER_REVERB_MIX;
  event.payload.parameter.value = 0.3f;
  expected[(*expected_count)++] = event;
  EXPECT_TRUE(mol_sequence_writer_append(&writer, &event) == MOL_OK);

  event = make_event(MOL_COMMAND_TRANSPORT_STOP, 72000u);
  expected[(*expected_count)++] = event;
  EXPECT_TRUE(mol_sequence_writer_append(&writer, &event) == MOL_OK);
  EXPECT_TRUE(mol_sequence_writer_finalize(&writer) == MOL_OK);
  EXPECT_TRUE(mol_sequence_writer_finalize(&writer) == MOL_ERROR_INVALID_STATE);
  return stream->size;
}

static mol_result_t read_fixture(memory_stream_t* stream, capture_t* capture,
                                 mol_sequence_config_t* config) {
  mol_sequence_callbacks_t callbacks = {0};
  callbacks.struct_size = (uint32_t)sizeof(callbacks);
  callbacks.api_version = MOL_API_VERSION;
  callbacks.on_event = capture_event;
  callbacks.on_metadata = capture_metadata;
  callbacks.user_data = capture;
  config->struct_size = (uint32_t)sizeof(*config);
  config->api_version = MOL_API_VERSION;
  stream->position = 0u;
  return mol_sequence_read_stream(read_memory, stream, config, &callbacks);
}

static void test_round_trip_and_determinism(void) {
  memory_stream_t first = {0};
  memory_stream_t second = {0};
  mol_sequence_event_t expected[TEST_EVENT_CAPACITY];
  uint32_t expected_count = 0u;
  capture_t capture = {0};
  mol_sequence_config_t config = {0};
  first.capacity = sizeof(first.data);
  first.max_read = 1u;
  second.capacity = sizeof(second.data);
  (void)write_fixture(&first, expected, &expected_count);
  {
    mol_sequence_event_t duplicate_expected[TEST_EVENT_CAPACITY];
    uint32_t duplicate_count = 0u;
    (void)write_fixture(&second, duplicate_expected, &duplicate_count);
    EXPECT_TRUE(first.size == second.size);
    EXPECT_TRUE(memcmp(first.data, second.data, first.size) == 0);
  }
  EXPECT_TRUE(first.size < 320u);
  EXPECT_TRUE(read_fixture(&first, &capture, &config) == MOL_OK);
  EXPECT_TRUE(config.sample_rate == 48000u && config.time_base == 48000u);
  EXPECT_TRUE(config.initial_state.preset == MOL_PRESET_ELECTRIC_PIANO);
  EXPECT_TRUE(config.initial_state.scale_type == MOL_SCALE_DORIAN);
  EXPECT_TRUE(capture.metadata_type == UINT32_C(0x454D414E));
  EXPECT_TRUE(capture.metadata_size == 11u && memcmp(capture.metadata, "Scale study", 11u) == 0);
  EXPECT_TRUE(capture.event_count == expected_count);
  for (uint32_t index = 0u; index < expected_count; ++index) {
    EXPECT_TRUE(memcmp(&capture.events[index], &expected[index], sizeof(expected[index])) == 0);
  }
}

static void test_truncation_corruption_and_writer_bounds(void) {
  static memory_stream_t valid;
  static memory_stream_t scratch;
  mol_sequence_event_t expected[TEST_EVENT_CAPACITY];
  uint32_t expected_count = 0u;
  size_t valid_size;
  memset(&valid, 0, sizeof(valid));
  valid.capacity = sizeof(valid.data);
  valid_size = write_fixture(&valid, expected, &expected_count);
  for (size_t truncated_size = 0u; truncated_size < valid_size; ++truncated_size) {
    mol_sequence_config_t config = {0};
    scratch = valid;
    scratch.size = truncated_size;
    scratch.position = 0u;
    scratch.max_read = 3u;
    config.struct_size = (uint32_t)sizeof(config);
    config.api_version = MOL_API_VERSION;
    EXPECT_TRUE(mol_sequence_read_stream(read_memory, &scratch, &config, NULL) != MOL_OK);
  }
  {
    mol_sequence_config_t config = {0};
    scratch = valid;
    scratch.data[100] ^= 0x20u;
    config.struct_size = (uint32_t)sizeof(config);
    config.api_version = MOL_API_VERSION;
    EXPECT_TRUE(mol_sequence_read_stream(read_memory, &scratch, &config, NULL) != MOL_OK);
  }
  {
    mol_sequence_config_t config = {0};
    scratch = valid;
    scratch.data[4] = (uint8_t)(MOL_SEQUENCE_FORMAT_VERSION + 1u);
    config.struct_size = (uint32_t)sizeof(config);
    config.api_version = MOL_API_VERSION;
    EXPECT_TRUE(mol_sequence_read_stream(read_memory, &scratch, &config, NULL) ==
                MOL_ERROR_UNSUPPORTED_VERSION);
  }
  {
    mol_sequence_config_t config = {0};
    scratch = valid;
    scratch.data[scratch.size++] = 0u;
    config.struct_size = (uint32_t)sizeof(config);
    config.api_version = MOL_API_VERSION;
    EXPECT_TRUE(mol_sequence_read_stream(read_memory, &scratch, &config, NULL) ==
                MOL_ERROR_CORRUPT_DATA);
  }
  {
    mol_sequence_config_t config = mol_sequence_config_default(48000u);
    mol_sequence_config_t decoded = {0};
    mol_sequence_writer_t writer = {0};
    memset(&scratch, 0, sizeof(scratch));
    scratch.capacity = sizeof(scratch.data);
    writer.struct_size = (uint32_t)sizeof(writer);
    writer.api_version = MOL_API_VERSION;
    EXPECT_TRUE(mol_sequence_writer_init(&writer, &config, write_memory, &scratch) == MOL_OK);
    decoded.struct_size = (uint32_t)sizeof(decoded);
    decoded.api_version = MOL_API_VERSION;
    EXPECT_TRUE(mol_sequence_read_stream(read_memory, &scratch, &decoded, NULL) ==
                MOL_ERROR_CORRUPT_DATA);
  }
  {
    mol_sequence_config_t config = mol_sequence_config_default(48000u);
    mol_sequence_writer_t writer = {0};
    memset(&scratch, 0, sizeof(scratch));
    scratch.capacity = MOL_SEQUENCE_HEADER_SIZE - 1u;
    writer.struct_size = (uint32_t)sizeof(writer);
    writer.api_version = MOL_API_VERSION;
    EXPECT_TRUE(mol_sequence_writer_init(&writer, &config, write_memory, &scratch) == MOL_ERROR_IO);
  }
  {
    mol_sequence_config_t config = mol_sequence_config_default(48000u);
    mol_sequence_writer_t writer = {0};
    mol_sequence_event_t later = make_event(MOL_COMMAND_NOTE_ON, 10u);
    mol_sequence_event_t earlier = make_event(MOL_COMMAND_NOTE_OFF, 9u);
    uint8_t oversized[MOL_SEQUENCE_MAX_METADATA_SIZE + 1u] = {0};
    memset(&scratch, 0, sizeof(scratch));
    scratch.capacity = sizeof(scratch.data);
    writer.struct_size = (uint32_t)sizeof(writer);
    writer.api_version = MOL_API_VERSION;
    later.payload.note.note = 60u;
    later.payload.note.velocity = 1.0f;
    earlier.payload.note.note = 60u;
    EXPECT_TRUE(mol_sequence_writer_init(&writer, &config, write_memory, &scratch) == MOL_OK);
    EXPECT_TRUE(mol_sequence_writer_append(&writer, &later) == MOL_OK);
    EXPECT_TRUE(mol_sequence_writer_append(&writer, &earlier) == MOL_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(mol_sequence_writer_add_metadata(&writer, 1u, oversized, sizeof(oversized)) ==
                MOL_ERROR_INVALID_ARGUMENT);
  }
}

static void populate_payload(mol_sequence_event_t* event) {
  switch (event->command_type) {
    case MOL_COMMAND_NOTE_ON:
    case MOL_COMMAND_NOTE_OFF:
    case MOL_COMMAND_POLY_PRESSURE:
      event->payload.note.note = 64u;
      event->payload.note.velocity = 0.5f;
      break;
    case MOL_COMMAND_PITCH_BEND:
      event->payload.scalar.value = -0.25f;
      break;
    case MOL_COMMAND_SUSTAIN:
      event->payload.scalar.value = 0.75f;
      break;
    case MOL_COMMAND_SET_MASTER_GAIN:
      event->payload.scalar.value = 0.2f;
      break;
    case MOL_COMMAND_SET_TEMPO:
      event->payload.scalar.value = 123.0f;
      break;
    case MOL_COMMAND_SET_PRESET:
      event->payload.preset.preset = MOL_PRESET_FLUTE;
      event->payload.preset.hard_switch = 1u;
      break;
    case MOL_COMMAND_SET_PARAMETER:
      event->payload.parameter.parameter = MOL_PARAMETER_CHORUS_MIX;
      event->payload.parameter.value = 0.4f;
      break;
    case MOL_COMMAND_SET_OCTAVE_SHIFT:
      event->payload.integer.value = -2;
      break;
    case MOL_COMMAND_SET_TRANSPOSE:
      event->payload.integer.value = 7;
      break;
    case MOL_COMMAND_SET_CHORD_MODE:
      event->payload.integer.value = MOL_CHORD_MINOR_7;
      break;
    case MOL_COMMAND_SET_SCALE:
      event->payload.scale.type = MOL_SCALE_MIXOLYDIAN;
      event->payload.scale.tonic = 5u;
      event->payload.scale.mapping = MOL_SCALE_MAP_NEAREST;
      break;
    case MOL_COMMAND_SET_ARPEGGIATOR:
      event->payload.arpeggiator.mode = MOL_ARPEGGIATOR_DOWN;
      event->payload.arpeggiator.rate = MOL_ARPEGGIATOR_RATE_SIXTEENTH;
      event->payload.arpeggiator.gate = 0.6f;
      event->payload.arpeggiator.random_seed = UINT32_C(0x12345678);
      event->payload.arpeggiator.octaves = 3u;
      break;
    case MOL_COMMAND_SET_TIME_SIGNATURE:
      event->payload.time_signature.numerator = 6u;
      event->payload.time_signature.denominator = 8u;
      break;
    case MOL_COMMAND_TRANSPORT_SEEK:
      event->payload.transport.frame = UINT64_C(0x1020304050607080);
      break;
    case MOL_COMMAND_SET_METRONOME:
      event->payload.metronome.level = 0.3f;
      event->payload.metronome.enabled = 1u;
      break;
    case MOL_COMMAND_SET_PORTAMENTO:
      event->payload.portamento.mode = MOL_PORTAMENTO_ALWAYS;
      event->payload.portamento.time_ms = 75.0f;
      break;
    default:
      break;
  }
}

static void test_every_sequence_command_payload(void) {
  static const mol_command_type_t types[] = {
      MOL_COMMAND_NOTE_ON,
      MOL_COMMAND_NOTE_OFF,
      MOL_COMMAND_POLY_PRESSURE,
      MOL_COMMAND_PITCH_BEND,
      MOL_COMMAND_SUSTAIN,
      MOL_COMMAND_ALL_NOTES_OFF,
      MOL_COMMAND_ALL_SOUND_OFF,
      MOL_COMMAND_SET_MASTER_GAIN,
      MOL_COMMAND_SET_PRESET,
      MOL_COMMAND_SET_PARAMETER,
      MOL_COMMAND_SET_OCTAVE_SHIFT,
      MOL_COMMAND_SET_TRANSPOSE,
      MOL_COMMAND_SET_SCALE,
      MOL_COMMAND_SET_CHORD_MODE,
      MOL_COMMAND_SET_ARPEGGIATOR,
      MOL_COMMAND_SET_TEMPO,
      MOL_COMMAND_SET_TIME_SIGNATURE,
      MOL_COMMAND_TRANSPORT_START,
      MOL_COMMAND_TRANSPORT_STOP,
      MOL_COMMAND_TRANSPORT_SEEK,
      MOL_COMMAND_SET_METRONOME,
      MOL_COMMAND_SET_PORTAMENTO,
  };
  memory_stream_t stream = {0};
  capture_t capture = {0};
  mol_sequence_config_t config = mol_sequence_config_default(48000u);
  mol_sequence_config_t decoded = {0};
  mol_sequence_callbacks_t callbacks = {0};
  mol_sequence_writer_t writer = {0};
  stream.capacity = sizeof(stream.data);
  writer.struct_size = (uint32_t)sizeof(writer);
  writer.api_version = MOL_API_VERSION;
  EXPECT_TRUE(mol_sequence_writer_init(&writer, &config, write_memory, &stream) == MOL_OK);
  for (uint32_t index = 0u; index < sizeof(types) / sizeof(types[0]); ++index) {
    mol_sequence_event_t event = make_event(types[index], index + 1u);
    populate_payload(&event);
    EXPECT_TRUE(mol_sequence_validate_event(&event) == MOL_OK);
    EXPECT_TRUE(mol_sequence_writer_append(&writer, &event) == MOL_OK);
  }
  EXPECT_TRUE(mol_sequence_writer_finalize(&writer) == MOL_OK);

  callbacks.struct_size = (uint32_t)sizeof(callbacks);
  callbacks.api_version = MOL_API_VERSION;
  callbacks.on_event = capture_event;
  callbacks.user_data = &capture;
  decoded.struct_size = (uint32_t)sizeof(decoded);
  decoded.api_version = MOL_API_VERSION;
  EXPECT_TRUE(mol_sequence_read_stream(read_memory, &stream, &decoded, &callbacks) == MOL_OK);
  EXPECT_TRUE(capture.event_count == sizeof(types) / sizeof(types[0]));

  {
    mol_sequence_event_t invalid = make_event((mol_command_type_t)UINT32_MAX, 1u);
    EXPECT_TRUE(mol_sequence_validate_event(NULL) == MOL_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(mol_sequence_validate_event(&invalid) == MOL_ERROR_UNSUPPORTED);
  }
}

int main(void) {
  test_round_trip_and_determinism();
  test_truncation_corruption_and_writer_bounds();
  test_every_sequence_command_payload();
  EXPECT_TRUE(strcmp(mol_result_string(MOL_ERROR_CORRUPT_DATA), "corrupt data") == 0);
  EXPECT_TRUE(strcmp(mol_result_string(MOL_ERROR_IO), "I/O error") == 0);
  if (failures != 0) {
    (void)fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
