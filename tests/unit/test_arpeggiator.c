/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mol/mol.h"

typedef union arpeggiator_test_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[1048576];
} arpeggiator_test_storage_t;

typedef struct started_note {
  mol_frame_index_t frame;
  mol_gesture_id_t gesture_id;
  uint8_t note;
} started_note_t;

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

static mol_command_t note_at(mol_command_type_t type, mol_frame_index_t frame,
                             mol_gesture_id_t gesture, uint8_t note) {
  mol_command_t command = command_at(type, frame);
  command.gesture_id = gesture;
  command.payload.note.note = note;
  command.payload.note.velocity = 0.8f;
  return command;
}

static mol_engine_t* initialize(arpeggiator_test_storage_t* storage) {
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  config.channel_count = 1u;
  EXPECT_TRUE(mol_engine_init(storage->bytes, sizeof(storage->bytes), &config, &engine) == MOL_OK);
  return engine;
}

static void render_pattern(mol_engine_t* engine, uint32_t frame_count) {
  static const uint32_t pattern[5] = {17u, 128u, 31u, 64u, 113u};
  float output[128];
  uint32_t pattern_index = 0u;
  while (frame_count != 0u) {
    uint32_t block = pattern[pattern_index++ % 5u];
    if (block > frame_count) {
      block = frame_count;
    }
    EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, output, block, 1u) == MOL_OK);
    for (uint32_t index = 0u; index < block; ++index) {
      EXPECT_TRUE(isfinite(output[index]));
    }
    frame_count -= block;
  }
}

static uint32_t collect_started(mol_engine_t* engine, started_note_t* started, uint32_t capacity) {
  mol_event_t events[128];
  uint32_t event_count = mol_engine_poll_events(engine, events, 128u);
  uint32_t count = 0u;
  for (uint32_t index = 0u; index < event_count; ++index) {
    if (events[index].event_type == MOL_EVENT_NOTE_STARTED && count < capacity) {
      started[count].frame = events[index].frame;
      started[count].gesture_id = events[index].gesture_id;
      started[count].note = events[index].payload[MOL_EVENT_PAYLOAD_NOTE];
      ++count;
    }
  }
  return count;
}

static uint32_t run_mode(arpeggiator_test_storage_t* storage, mol_arpeggiator_mode_t mode,
                         uint8_t octaves, uint32_t seed, started_note_t* started, uint32_t capacity,
                         uint32_t frame_count) {
  mol_engine_t* engine = initialize(storage);
  mol_command_t tempo = command_at(MOL_COMMAND_SET_TEMPO, 0u);
  mol_command_t arpeggiator = command_at(MOL_COMMAND_SET_ARPEGGIATOR, 0u);
  mol_command_t start = command_at(MOL_COMMAND_TRANSPORT_START, 0u);
  static const uint8_t notes[3] = {64u, 60u, 67u};
  uint32_t result;

  tempo.payload.scalar.value = 120.0f;
  arpeggiator.payload.arpeggiator.mode = mode;
  arpeggiator.payload.arpeggiator.rate = MOL_ARPEGGIATOR_RATE_SIXTEENTH;
  arpeggiator.payload.arpeggiator.gate = 0.5f;
  arpeggiator.payload.arpeggiator.random_seed = seed;
  arpeggiator.payload.arpeggiator.octaves = octaves;
  EXPECT_TRUE(mol_engine_submit(engine, &tempo) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &arpeggiator) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &start) == MOL_OK);
  for (uint32_t index = 0u; index < 3u; ++index) {
    mol_command_t note = note_at(MOL_COMMAND_NOTE_ON, 0u, index + 1u, notes[index]);
    EXPECT_TRUE(mol_engine_submit(engine, &note) == MOL_OK);
  }
  render_pattern(engine, frame_count);
  result = collect_started(engine, started, capacity);
  mol_engine_shutdown(engine);
  return result;
}

static void expect_mode(mol_arpeggiator_mode_t mode, const uint8_t* expected) {
  static arpeggiator_test_storage_t storage;
  started_note_t started[8];
  uint32_t count = run_mode(&storage, mode, 1u, 1234u, started, 8u, 24001u);
  EXPECT_TRUE(count == 5u);
  for (uint32_t index = 0u; index < count && index < 5u; ++index) {
    EXPECT_TRUE(started[index].frame == (mol_frame_index_t)index * 6000u);
    EXPECT_TRUE(started[index].note == expected[index]);
  }
}

static void test_modes_and_cross_block_scheduling(void) {
  static const uint8_t up[5] = {60u, 64u, 67u, 60u, 64u};
  static const uint8_t down[5] = {67u, 64u, 60u, 67u, 64u};
  static const uint8_t up_down[5] = {60u, 64u, 67u, 64u, 60u};
  static const uint8_t down_up[5] = {67u, 64u, 60u, 64u, 67u};
  static const uint8_t as_played[5] = {64u, 60u, 67u, 64u, 60u};
  expect_mode(MOL_ARPEGGIATOR_UP, up);
  expect_mode(MOL_ARPEGGIATOR_DOWN, down);
  expect_mode(MOL_ARPEGGIATOR_UP_DOWN, up_down);
  expect_mode(MOL_ARPEGGIATOR_DOWN_UP, down_up);
  expect_mode(MOL_ARPEGGIATOR_AS_PLAYED, as_played);
}

static void test_octaves_chords_and_random(void) {
  static arpeggiator_test_storage_t first_storage;
  static arpeggiator_test_storage_t second_storage;
  started_note_t first[8];
  started_note_t second[8];
  uint32_t first_count = run_mode(&first_storage, MOL_ARPEGGIATOR_UP, 2u, 55u, first, 8u, 42001u);
  EXPECT_TRUE(first_count == 8u);
  if (first_count >= 8u) {
    static const uint8_t expected[8] = {60u, 64u, 67u, 72u, 76u, 79u, 60u, 64u};
    for (uint32_t index = 0u; index < 8u; ++index) {
      EXPECT_TRUE(first[index].note == expected[index]);
    }
  }

  first_count = run_mode(&first_storage, MOL_ARPEGGIATOR_RANDOM_DETERMINISTIC, 2u,
                         UINT32_C(0x12345678), first, 8u, 42001u);
  {
    uint32_t second_count = run_mode(&second_storage, MOL_ARPEGGIATOR_RANDOM_DETERMINISTIC, 2u,
                                     UINT32_C(0x12345678), second, 8u, 42001u);
    EXPECT_TRUE(first_count == 8u && second_count == first_count);
    for (uint32_t index = 0u; index < first_count && index < 8u; ++index) {
      EXPECT_TRUE(first[index].frame == second[index].frame);
      EXPECT_TRUE(first[index].gesture_id == second[index].gesture_id);
      EXPECT_TRUE(first[index].note == second[index].note);
    }
  }

  {
    mol_engine_t* engine = initialize(&first_storage);
    mol_command_t chord = command_at(MOL_COMMAND_SET_CHORD_MODE, 0u);
    mol_command_t arpeggiator = command_at(MOL_COMMAND_SET_ARPEGGIATOR, 0u);
    mol_command_t start = command_at(MOL_COMMAND_TRANSPORT_START, 0u);
    mol_command_t note = note_at(MOL_COMMAND_NOTE_ON, 0u, 40u, 60u);
    chord.payload.integer.value = (int32_t)MOL_CHORD_MAJOR;
    arpeggiator.payload.arpeggiator.mode = MOL_ARPEGGIATOR_UP;
    arpeggiator.payload.arpeggiator.rate = MOL_ARPEGGIATOR_RATE_EIGHTH;
    arpeggiator.payload.arpeggiator.gate = 0.5f;
    arpeggiator.payload.arpeggiator.octaves = 1u;
    EXPECT_TRUE(mol_engine_submit(engine, &chord) == MOL_OK);
    EXPECT_TRUE(mol_engine_submit(engine, &arpeggiator) == MOL_OK);
    EXPECT_TRUE(mol_engine_submit(engine, &start) == MOL_OK);
    EXPECT_TRUE(mol_engine_submit(engine, &note) == MOL_OK);
    render_pattern(engine, 57601u);
    first_count = collect_started(engine, first, 8u);
    EXPECT_TRUE(first_count == 5u);
    if (first_count >= 3u) {
      EXPECT_TRUE(first[0].note == 60u && first[1].note == 64u && first[2].note == 67u);
    }
    mol_engine_shutdown(engine);
  }
}

static void test_sustain_and_disable_cleanup(void) {
  static arpeggiator_test_storage_t storage;
  mol_engine_t* engine = initialize(&storage);
  mol_command_t arpeggiator = command_at(MOL_COMMAND_SET_ARPEGGIATOR, 0u);
  mol_command_t start = command_at(MOL_COMMAND_TRANSPORT_START, 0u);
  mol_command_t pedal_on = command_at(MOL_COMMAND_SUSTAIN, 1u);
  mol_command_t note_on = note_at(MOL_COMMAND_NOTE_ON, 0u, 90u, 60u);
  mol_command_t note_off = note_at(MOL_COMMAND_NOTE_OFF, 2u, 90u, 0u);
  mol_command_t pedal_off = command_at(MOL_COMMAND_SUSTAIN, 15000u);
  mol_engine_state_t state = {0};
  started_note_t started[8];
  uint32_t count;

  arpeggiator.payload.arpeggiator.mode = MOL_ARPEGGIATOR_UP;
  arpeggiator.payload.arpeggiator.rate = MOL_ARPEGGIATOR_RATE_SIXTEENTH;
  arpeggiator.payload.arpeggiator.gate = 0.5f;
  arpeggiator.payload.arpeggiator.octaves = 1u;
  pedal_on.payload.scalar.value = 1.0f;
  pedal_off.payload.scalar.value = 0.0f;
  EXPECT_TRUE(mol_engine_submit(engine, &arpeggiator) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &start) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &note_on) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &pedal_on) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &note_off) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &pedal_off) == MOL_OK);
  render_pattern(engine, 30001u);
  count = collect_started(engine, started, 8u);
  EXPECT_TRUE(count == 3u);
  state.struct_size = (uint32_t)sizeof(state);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.active_gestures == 0u);

  note_on = note_at(MOL_COMMAND_NOTE_ON, MOL_FRAME_IMMEDIATE, 91u, 64u);
  EXPECT_TRUE(mol_engine_submit(engine, &note_on) == MOL_OK);
  render_pattern(engine, 6000u);
  arpeggiator.target_frame = MOL_FRAME_IMMEDIATE;
  arpeggiator.payload.arpeggiator.mode = MOL_ARPEGGIATOR_OFF;
  EXPECT_TRUE(mol_engine_submit(engine, &arpeggiator) == MOL_OK);
  render_pattern(engine, 1u);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.arpeggiator_mode == MOL_ARPEGGIATOR_OFF);
  mol_engine_shutdown(engine);
}

static void test_rates_and_validation(void) {
  static const uint32_t expected[MOL_ARPEGGIATOR_RATE_COUNT] = {1u, 2u, 3u, 4u, 6u, 8u};
  static arpeggiator_test_storage_t storage;
  mol_engine_t* engine = initialize(&storage);
  mol_command_t command = command_at(MOL_COMMAND_SET_ARPEGGIATOR, 0u);
  for (uint32_t rate = 0u; rate < MOL_ARPEGGIATOR_RATE_COUNT; ++rate) {
    EXPECT_TRUE(mol_arpeggiator_steps_per_quarter(rate) == expected[rate]);
  }
  command.payload.arpeggiator.mode = MOL_ARPEGGIATOR_UP;
  command.payload.arpeggiator.rate = MOL_ARPEGGIATOR_RATE_SIXTEENTH;
  command.payload.arpeggiator.gate = 0.01f;
  command.payload.arpeggiator.octaves = 1u;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  command.payload.arpeggiator.gate = 0.5f;
  command.payload.arpeggiator.octaves = 5u;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_ERROR_INVALID_ARGUMENT);
  mol_engine_shutdown(engine);
}

int main(void) {
  test_modes_and_cross_block_scheduling();
  test_octaves_chords_and_random();
  test_sustain_and_disable_cleanup();
  test_rates_and_validation();
  if (failures != 0) {
    (void)fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
