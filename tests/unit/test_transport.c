/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mol/mol.h"

typedef union transport_test_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[1048576];
} transport_test_storage_t;

static int failures = 0;

#define EXPECT_TRUE(condition)                                                                  \
  do {                                                                                          \
    if (!(condition)) {                                                                         \
      (void)fprintf(stderr, "%s:%d: expectation failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                               \
    }                                                                                           \
  } while (0)

static mol_command_t command_at(mol_command_type_t type, mol_frame_index_t frame) {
  mol_command_t command;
  memset(&command, 0, sizeof(command));
  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = type;
  command.target_frame = frame;
  return command;
}

static mol_engine_t* initialize(transport_test_storage_t* storage) {
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  config.channel_count = 1u;
  EXPECT_TRUE(mol_engine_init(storage->bytes, sizeof(storage->bytes), &config, &engine) == MOL_OK);
  return engine;
}

static double render_energy(mol_engine_t* engine, uint32_t frame_count) {
  float output[128];
  double energy = 0.0;
  while (frame_count != 0u) {
    uint32_t block = frame_count < 128u ? frame_count : 128u;
    EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, output, block, 1u) == MOL_OK);
    for (uint32_t index = 0u; index < block; ++index) {
      EXPECT_TRUE(isfinite(output[index]));
      energy += fabs((double)output[index]);
    }
    frame_count -= block;
  }
  return energy;
}

static void test_exact_transport_math(void) {
  mol_frame_index_t frame = 0u;
  uint64_t step = 0u;
  uint32_t milli_bpm = 0u;

  EXPECT_TRUE(mol_tempo_to_milli_bpm(120.0f, &milli_bpm) == MOL_OK);
  EXPECT_TRUE(milli_bpm == 120000u);
  EXPECT_TRUE(mol_transport_step_frame(48000u, milli_bpm, 1u, 1u, &frame) == MOL_OK);
  EXPECT_TRUE(frame == 24000u);
  EXPECT_TRUE(mol_transport_step_frame(48000u, milli_bpm, 3u, 1u, &frame) == MOL_OK);
  EXPECT_TRUE(frame == 8000u);

  for (step = 0u; step <= 14400u; ++step) {
    EXPECT_TRUE(mol_transport_step_frame(48000u, milli_bpm, 1u, step, &frame) == MOL_OK);
    EXPECT_TRUE(frame == step * UINT64_C(24000));
  }
  EXPECT_TRUE(frame == UINT64_C(345600000));
  EXPECT_TRUE(mol_transport_step_at_or_after(48000u, milli_bpm, 1u, UINT64_C(345600000), &step) ==
              MOL_OK);
  EXPECT_TRUE(step == 14400u);

  EXPECT_TRUE(mol_time_signature_is_valid(2u, 4u));
  EXPECT_TRUE(mol_time_signature_is_valid(3u, 4u));
  EXPECT_TRUE(mol_time_signature_is_valid(4u, 4u));
  EXPECT_TRUE(mol_time_signature_is_valid(5u, 4u));
  EXPECT_TRUE(mol_time_signature_is_valid(6u, 8u));
  EXPECT_TRUE(!mol_time_signature_is_valid(7u, 8u));
  EXPECT_TRUE(mol_tempo_to_milli_bpm(29.99f, &milli_bpm) == MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_tempo_to_milli_bpm(300.01f, &milli_bpm) == MOL_ERROR_INVALID_ARGUMENT);
}

static void test_metronome_and_transport_state(void) {
  static transport_test_storage_t storage;
  mol_engine_t* engine = initialize(&storage);
  mol_command_t tempo = command_at(MOL_COMMAND_SET_TEMPO, 0u);
  mol_command_t metronome = command_at(MOL_COMMAND_SET_METRONOME, 0u);
  mol_command_t start = command_at(MOL_COMMAND_TRANSPORT_START, 0u);
  mol_command_t stop = command_at(MOL_COMMAND_TRANSPORT_STOP, MOL_FRAME_IMMEDIATE);
  mol_command_t seek = command_at(MOL_COMMAND_TRANSPORT_SEEK, MOL_FRAME_IMMEDIATE);
  mol_event_t events[32];
  mol_engine_state_t state = {0};
  uint32_t event_count;
  uint32_t tick_count = 0u;
  uint32_t accent_count = 0u;
  static const mol_frame_index_t expected_frames[6] = {0u, 24000u, 48000u, 72000u, 96000u, 120000u};

  tempo.payload.scalar.value = 120.0f;
  metronome.payload.metronome.enabled = 1u;
  metronome.payload.metronome.level = 0.5f;
  EXPECT_TRUE(mol_engine_submit(engine, &tempo) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &metronome) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &start) == MOL_OK);
  EXPECT_TRUE(render_energy(engine, 120001u) > 1.0);

  state.struct_size = (uint32_t)sizeof(state);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.transport_running == 1u);
  EXPECT_TRUE(state.transport_frame == 120001u);
  EXPECT_TRUE(state.tempo == 120.0f);
  EXPECT_TRUE(state.time_signature_numerator == 4u);
  EXPECT_TRUE(state.time_signature_denominator == 4u);
  EXPECT_TRUE(state.metronome_enabled == 1u);

  event_count = mol_engine_poll_events(engine, events, 32u);
  for (uint32_t index = 0u; index < event_count; ++index) {
    if (events[index].event_type == MOL_EVENT_METRONOME_TICK) {
      EXPECT_TRUE(tick_count < 6u);
      if (tick_count < 6u) {
        EXPECT_TRUE(events[index].frame == expected_frames[tick_count]);
      }
      EXPECT_TRUE(events[index].payload[MOL_EVENT_PAYLOAD_METRONOME_BEAT] ==
                  (uint8_t)(tick_count % 4u));
      accent_count += events[index].payload[MOL_EVENT_PAYLOAD_METRONOME_ACCENT];
      ++tick_count;
    }
  }
  EXPECT_TRUE(tick_count == 6u && accent_count == 2u);

  EXPECT_TRUE(mol_engine_submit(engine, &stop) == MOL_OK);
  (void)render_energy(engine, 1u);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.transport_running == 0u && state.transport_frame == 120001u);
  (void)render_energy(engine, 128u);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.transport_frame == 120001u);

  seek.payload.transport.frame = 48000u;
  EXPECT_TRUE(mol_engine_submit(engine, &seek) == MOL_OK);
  (void)render_energy(engine, 1u);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.transport_frame == 48000u);
  EXPECT_TRUE((mol_engine_get_capabilities(engine) & MOL_CAPABILITY_METRONOME) != 0u);
  mol_engine_shutdown(engine);
}

static void test_transport_validation(void) {
  static transport_test_storage_t storage;
  mol_engine_t* engine = initialize(&storage);
  mol_command_t command = command_at(MOL_COMMAND_SET_TIME_SIGNATURE, 0u);
  static const uint8_t numerators[5] = {2u, 3u, 4u, 5u, 6u};
  static const uint8_t denominators[5] = {4u, 4u, 4u, 4u, 8u};

  for (uint32_t index = 0u; index < 5u; ++index) {
    command.payload.time_signature.numerator = numerators[index];
    command.payload.time_signature.denominator = denominators[index];
    EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
  }
  command.payload.time_signature.numerator = 7u;
  command.payload.time_signature.denominator = 8u;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command.command_type = MOL_COMMAND_SET_METRONOME;
  command.payload.metronome.enabled = 2u;
  command.payload.metronome.level = 0.5f;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  mol_engine_shutdown(engine);
}

int main(void) {
  test_exact_transport_math();
  test_metronome_and_transport_state();
  test_transport_validation();
  if (failures != 0) {
    (void)fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
