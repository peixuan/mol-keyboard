/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mol/mol.h"

typedef union test_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[1048576];
} test_storage_t;

static int failures = 0;

#define EXPECT_TRUE(condition)                                                                  \
  do {                                                                                          \
    if (!(condition)) {                                                                         \
      (void)fprintf(stderr, "%s:%d: expectation failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                               \
    }                                                                                           \
  } while (0)

static void test_lifecycle(void) {
  static test_storage_t storage;
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  mol_engine_state_t state = {0};
  float output[64] = {1.0f};
  size_t required = mol_engine_query_memory(&config);

  EXPECT_TRUE(required > 0u);
  EXPECT_TRUE(required <= sizeof(storage.bytes));
  EXPECT_TRUE(mol_engine_memory_alignment() <= _Alignof(test_storage_t));
  EXPECT_TRUE(mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) == MOL_OK);
  EXPECT_TRUE(engine != NULL);
  EXPECT_TRUE((mol_engine_get_capabilities(engine) & MOL_CAPABILITY_CALLER_MEMORY) != 0u);

  EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, output, 32u, 2u) == MOL_OK);
  for (size_t index = 0u; index < 64u; ++index) {
    EXPECT_TRUE(output[index] == 0.0f);
  }

  state.struct_size = (uint32_t)sizeof(state);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.current_frame == 32u);
  EXPECT_TRUE(state.sample_rate == 48000u);
  EXPECT_TRUE(state.active_voices == 0u);

  mol_engine_reset(engine);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.current_frame == 0u);
  mol_engine_shutdown(engine);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_ERROR_INVALID_ARGUMENT);
}

static void test_validation(void) {
  static test_storage_t storage;
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = (mol_engine_t*)(uintptr_t)1u;

  EXPECT_TRUE(mol_get_api_version() == MOL_API_VERSION);
  EXPECT_TRUE(mol_get_version_string() != NULL);
  EXPECT_TRUE(mol_engine_init(storage.bytes, 1u, &config, &engine) ==
              MOL_ERROR_INSUFFICIENT_MEMORY);
  EXPECT_TRUE(engine == NULL);

  config.api_version = 0u;
  EXPECT_TRUE(mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) ==
              MOL_ERROR_UNSUPPORTED_VERSION);
  EXPECT_TRUE(engine == NULL);
  config = mol_engine_config_default();
  config.sample_rate = 12345u;
  EXPECT_TRUE(mol_engine_query_memory(&config) > 0u);
  config.sample_rate = 7999u;
  EXPECT_TRUE(mol_engine_query_memory(&config) == 0u);
  config.sample_rate = 192001u;
  EXPECT_TRUE(mol_engine_query_memory(&config) == 0u);
}

static void test_tiny_embedded_budget(void) {
#if MOL_BUILD_PROFILE == 1
  static test_storage_t storage;
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  config.max_voices = 12u;
  config.command_capacity = 32u;
  config.event_capacity = 32u;
  EXPECT_TRUE(mol_engine_query_memory(&config) <= 131072u);
  EXPECT_TRUE(mol_engine_init(storage.bytes, 131072u, &config, &engine) == MOL_OK);
  mol_engine_shutdown(engine);
#endif
}

static void test_planar_and_commands(void) {
  static test_storage_t storage;
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  mol_command_t command = {0};
  float left[16] = {1.0f};
  float right[16] = {1.0f};
  float* channels[2] = {left, right};

  EXPECT_TRUE(mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) == MOL_OK);
  EXPECT_TRUE(mol_engine_render_planar_f32(engine, channels, 16u, 2u) == MOL_OK);
  for (size_t index = 0u; index < 16u; ++index) {
    EXPECT_TRUE(left[index] == 0.0f);
    EXPECT_TRUE(right[index] == 0.0f);
  }

  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = MOL_COMMAND_RESET_ENGINE;
  command.target_frame = MOL_FRAME_IMMEDIATE;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  command.command_type = MOL_COMMAND_SET_TEMPO;
  command.payload.scalar.value = 100.0f;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  mol_engine_shutdown(engine);
}

static mol_command_t boundary_command(mol_command_type_t type) {
  mol_command_t command;
  memset(&command, 0, sizeof(command));
  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = type;
  command.target_frame = MOL_FRAME_IMMEDIATE;
  return command;
}

static void render_one(mol_engine_t* engine, uint32_t channels) {
  float output[2] = {0.0f, 0.0f};
  EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, output, 1u, channels) == MOL_OK);
}

static void test_result_and_initialization_boundaries(void) {
  static test_storage_t storage;
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  for (mol_result_t result = MOL_OK; result <= MOL_ERROR_IO; ++result) {
    EXPECT_TRUE(mol_result_string(result) != NULL);
  }
  EXPECT_TRUE(strcmp(mol_result_string(999), "unknown result") == 0);

  EXPECT_TRUE(mol_engine_query_memory(NULL) == 0u);
  config.struct_size = 0u;
  EXPECT_TRUE(mol_engine_query_memory(&config) == 0u);
  config = mol_engine_config_default();
  config.channel_count = 0u;
  EXPECT_TRUE(mol_engine_query_memory(&config) == 0u);
  config = mol_engine_config_default();
  config.max_voices = 7u;
  EXPECT_TRUE(mol_engine_query_memory(&config) == 0u);
  config = mol_engine_config_default();
  config.command_capacity = 0u;
  EXPECT_TRUE(mol_engine_query_memory(&config) == 0u);
  config = mol_engine_config_default();
  config.event_capacity = 0u;
  EXPECT_TRUE(mol_engine_query_memory(&config) == 0u);
  config = mol_engine_config_default();
  config.sequence_capacity = 0u;
  EXPECT_TRUE(mol_engine_query_memory(&config) == 0u);

  config = mol_engine_config_default();
  EXPECT_TRUE(mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, NULL) ==
              MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_engine_init(storage.bytes, sizeof(storage.bytes), NULL, &engine) ==
              MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_engine_init(NULL, sizeof(storage.bytes), &config, &engine) ==
              MOL_ERROR_INVALID_ARGUMENT);
  config.struct_size = 0u;
  EXPECT_TRUE(mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) ==
              MOL_ERROR_INVALID_ARGUMENT);
  config = mol_engine_config_default();
  config.channel_count = 3u;
  EXPECT_TRUE(mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) ==
              MOL_ERROR_INVALID_ARGUMENT);
  config = mol_engine_config_default();
  EXPECT_TRUE(mol_engine_init(storage.bytes + 1u, sizeof(storage.bytes) - 1u, &config, &engine) ==
              MOL_ERROR_MISALIGNED_MEMORY);
}

static void test_command_and_parameter_boundaries(void) {
  static test_storage_t storage;
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  mol_command_t command = boundary_command(MOL_COMMAND_NOTE_ON);
  static const float valid_parameters[12] = {0.5f,  1.0f,  0.5f, 10.0f, 0.5f, 0.5f,
                                             0.25f, 10.0f, 0.5f, 0.5f,  0.5f, -1.0f};
  EXPECT_TRUE(mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) == MOL_OK);

  EXPECT_TRUE(mol_engine_submit(NULL, &command) == MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_engine_submit(engine, NULL) == MOL_ERROR_INVALID_ARGUMENT);
  command.struct_size = 0u;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command = boundary_command(MOL_COMMAND_NOTE_ON);
  command.api_version = 0u;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_UNSUPPORTED_VERSION);

  command = boundary_command(MOL_COMMAND_NOTE_ON);
  command.payload.note.note = 128u;
  command.payload.note.velocity = 1.0f;
  command.gesture_id = 1u;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command = boundary_command(MOL_COMMAND_NOTE_OFF);
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command = boundary_command(MOL_COMMAND_POLY_PRESSURE);
  command.payload.note.note = 128u;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command = boundary_command(MOL_COMMAND_PITCH_BEND);
  command.payload.scalar.value = 2.0f;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command = boundary_command(MOL_COMMAND_SET_MASTER_GAIN);
  command.payload.scalar.value = 3.0f;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command = boundary_command(MOL_COMMAND_SET_PRESET);
  command.payload.preset.preset = MOL_PRESET_COUNT;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command = boundary_command(MOL_COMMAND_SUSTAIN);
  command.payload.scalar.value = 2.0f;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command = boundary_command(MOL_COMMAND_SET_OCTAVE_SHIFT);
  command.payload.integer.value = 4;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command = boundary_command(MOL_COMMAND_SET_TRANSPOSE);
  command.payload.integer.value = 25;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command = boundary_command(MOL_COMMAND_SET_SCALE);
  command.payload.scale.type = MOL_SCALE_TYPE_COUNT;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command = boundary_command(MOL_COMMAND_SET_CHORD_MODE);
  command.payload.integer.value = MOL_CHORD_MODE_COUNT;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command = boundary_command(MOL_COMMAND_SET_ARPEGGIATOR);
  command.payload.arpeggiator.mode = MOL_ARPEGGIATOR_MODE_COUNT;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command = boundary_command(MOL_COMMAND_SET_PORTAMENTO);
  command.payload.portamento.mode = MOL_PORTAMENTO_MODE_COUNT;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command = boundary_command(MOL_COMMAND_SET_TEMPO);
  command.payload.scalar.value = 10.0f;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command = boundary_command(MOL_COMMAND_SET_TIME_SIGNATURE);
  command.payload.time_signature.numerator = 7u;
  command.payload.time_signature.denominator = 8u;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command = boundary_command(MOL_COMMAND_SET_METRONOME);
  command.payload.metronome.enabled = 2u;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command = boundary_command((mol_command_type_t)UINT32_MAX);
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_UNSUPPORTED);

  for (uint32_t parameter = 1u; parameter <= 12u; ++parameter) {
    command = boundary_command(MOL_COMMAND_SET_PARAMETER);
    command.payload.parameter.parameter = parameter;
    command.payload.parameter.value = valid_parameters[parameter - 1u];
#if !MOL_ENABLE_CHORUS
    if (parameter <= MOL_PARAMETER_CHORUS_MIX) {
      EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_UNSUPPORTED);
      continue;
    }
#endif
#if !MOL_ENABLE_DELAY
    if (parameter >= MOL_PARAMETER_DELAY_TIME_MS && parameter <= MOL_PARAMETER_DELAY_SYNC_BEATS) {
      EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_UNSUPPORTED);
      continue;
    }
#endif
#if !MOL_ENABLE_REVERB
    if (parameter >= MOL_PARAMETER_REVERB_PREDELAY_MS && parameter <= MOL_PARAMETER_REVERB_MIX) {
      EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_UNSUPPORTED);
      continue;
    }
#endif
    EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  }
  render_one(engine, 2u);
  command = boundary_command(MOL_COMMAND_SET_PARAMETER);
  command.payload.parameter.parameter = MOL_PARAMETER_LIMITER_CEILING_DB;
  command.payload.parameter.value = NAN;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command.payload.parameter.parameter = 999u;
  command.payload.parameter.value = 0.0f;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_UNSUPPORTED);
  mol_engine_shutdown(engine);
}

static void test_queue_render_and_gesture_boundaries(void) {
  static test_storage_t storage;
  static test_storage_t mono_storage;
  static test_storage_t queue_storage;
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  mol_command_t command;
  float mono[2] = {0.0f, 0.0f};
  float* mono_channels[1] = {mono};
  float* invalid_channels[2] = {mono, NULL};

  EXPECT_TRUE(mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) == MOL_OK);
  EXPECT_TRUE(mol_engine_render_interleaved_f32(NULL, mono, 1u, 2u) == MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, mono, 1u, 1u) ==
              MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, NULL, 1u, 2u) ==
              MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_engine_render_planar_f32(engine, NULL, 1u, 2u) == MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_engine_render_planar_f32(engine, invalid_channels, 1u, 2u) ==
              MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_engine_poll_events(NULL, NULL, 0u) == 0u);
  EXPECT_TRUE(mol_engine_poll_events(engine, NULL, 1u) == 0u);
  EXPECT_TRUE(mol_engine_get_capabilities(NULL) == 0u);

  command = boundary_command(MOL_COMMAND_NOTE_ON);
  command.gesture_id = 101u;
  command.payload.note.note = 60u;
  command.payload.note.velocity = 0.5f;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  render_one(engine, 2u);
  command = boundary_command(MOL_COMMAND_POLY_PRESSURE);
  command.gesture_id = 101u;
  command.payload.note.note = 60u;
  command.payload.note.velocity = 0.9f;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  render_one(engine, 2u);
  command = boundary_command(MOL_COMMAND_ALL_NOTES_OFF);
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  render_one(engine, 2u);
  mol_engine_shutdown(engine);

  config = mol_engine_config_default();
  config.channel_count = 1u;
  EXPECT_TRUE(mol_engine_init(mono_storage.bytes, sizeof(mono_storage.bytes), &config, &engine) ==
              MOL_OK);
  command = boundary_command(MOL_COMMAND_SET_PORTAMENTO);
  command.payload.portamento.mode = MOL_PORTAMENTO_ALWAYS;
  command.payload.portamento.time_ms = 25.0f;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  render_one(engine, 1u);
  command = boundary_command(MOL_COMMAND_NOTE_ON);
  command.gesture_id = 102u;
  command.payload.note.note = 64u;
  command.payload.note.velocity = 0.5f;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  command = boundary_command(MOL_COMMAND_SUSTAIN);
  command.payload.scalar.value = 1.0f;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  render_one(engine, 1u);
  command = boundary_command(MOL_COMMAND_ALL_NOTES_OFF);
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  render_one(engine, 1u);
  command = boundary_command(MOL_COMMAND_SUSTAIN);
  command.payload.scalar.value = 0.0f;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  render_one(engine, 1u);
  EXPECT_TRUE(mol_engine_render_planar_f32(engine, mono_channels, 2u, 1u) == MOL_OK);
  mol_engine_shutdown(engine);

  config = mol_engine_config_default();
  config.command_capacity = 1u;
  EXPECT_TRUE(mol_engine_init(queue_storage.bytes, sizeof(queue_storage.bytes), &config, &engine) ==
              MOL_OK);
  command = boundary_command(MOL_COMMAND_TRANSPORT_START);
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  command.command_type = MOL_COMMAND_TRANSPORT_STOP;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_QUEUE_FULL);
  mol_engine_shutdown(engine);
}

static void test_recording_and_sequence_boundaries(void) {
  static test_storage_t storage;
  static test_storage_t playback_storage;
  mol_engine_config_t config = mol_engine_config_default();
  mol_sequence_config_t sequence_config = mol_sequence_config_default(48000u);
  mol_sequence_event_t event;
  mol_engine_state_t state = {0};
  mol_engine_t* engine = NULL;
  mol_command_t command;

  config.sequence_capacity = 1u;
  EXPECT_TRUE(mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) == MOL_OK);
  command = boundary_command(MOL_COMMAND_RECORD_START);
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  render_one(engine, 2u);
  state.struct_size = (uint32_t)sizeof(state);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.recording == 0u && state.recording_event_count == 1u);
  mol_engine_shutdown(engine);

  config = mol_engine_config_default();
  EXPECT_TRUE(mol_engine_init(playback_storage.bytes, sizeof(playback_storage.bytes), &config,
                              &engine) == MOL_OK);
  memset(&event, 0, sizeof(event));
  event.struct_size = (uint32_t)sizeof(event);
  event.api_version = MOL_API_VERSION;
  event.frame = 100u;
  event.command_type = MOL_COMMAND_TRANSPORT_STOP;
  EXPECT_TRUE(mol_engine_load_sequence(NULL, &sequence_config, &event, 1u) ==
              MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_engine_load_sequence(engine, NULL, &event, 1u) == MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(
      mol_engine_load_sequence(engine, &sequence_config, &event, config.sequence_capacity + 1u) ==
      MOL_ERROR_BUFFER_TOO_SMALL);
  EXPECT_TRUE(mol_engine_load_sequence(engine, &sequence_config, NULL, 1u) ==
              MOL_ERROR_INVALID_ARGUMENT);

  {
    mol_sequence_config_t overflow_config = sequence_config;
    mol_sequence_event_t overflow_event = event;
    overflow_config.time_base = 1u;
    overflow_event.frame = UINT64_MAX - 1u;
    EXPECT_TRUE(mol_engine_load_sequence(engine, &overflow_config, &overflow_event, 1u) ==
                MOL_ERROR_OVERFLOW);
  }

  EXPECT_TRUE(mol_engine_load_sequence(engine, &sequence_config, &event, 1u) == MOL_OK);
  command = boundary_command(MOL_COMMAND_PLAYBACK_START);
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  render_one(engine, 2u);
  command = boundary_command(MOL_COMMAND_RECORD_START);
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  render_one(engine, 2u);
  command = boundary_command(MOL_COMMAND_LOAD_SEQUENCE);
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  render_one(engine, 2u);
  command = boundary_command(MOL_COMMAND_RESET_ENGINE);
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  render_one(engine, 2u);
  mol_engine_shutdown(engine);
}

int main(void) {
  test_lifecycle();
  test_validation();
  test_tiny_embedded_budget();
  test_planar_and_commands();
  test_result_and_initialization_boundaries();
  test_command_and_parameter_boundaries();
  test_queue_render_and_gesture_boundaries();
  test_recording_and_sequence_boundaries();
  if (failures != 0) {
    (void)fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
