/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mol/mol.h"

typedef union property_test_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[2097152];
} property_test_storage_t;

static property_test_storage_t first_storage;
static property_test_storage_t second_storage;
static float first_audio[30000];
static float second_audio[30000];
static int failures = 0;

#define EXPECT_TRUE(condition)                                                                  \
  do {                                                                                          \
    if (!(condition)) {                                                                         \
      (void)fprintf(stderr, "%s:%d: expectation failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                               \
    }                                                                                           \
  } while (0)

static mol_command_t command_now(mol_command_type_t type) {
  mol_command_t command;
  memset(&command, 0, sizeof(command));
  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = type;
  command.target_frame = MOL_FRAME_IMMEDIATE;
  return command;
}

static mol_command_t note_command(mol_command_type_t type, mol_frame_index_t frame,
                                  mol_gesture_id_t gesture, uint8_t note) {
  mol_command_t command = command_now(type);
  command.target_frame = frame;
  command.gesture_id = gesture;
  command.payload.note.note = note;
  command.payload.note.velocity = 0.7f;
  return command;
}

static mol_engine_t* initialize(property_test_storage_t* storage) {
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  config.channel_count = 1u;
  EXPECT_TRUE(mol_engine_init(storage->bytes, sizeof(storage->bytes), &config, &engine) == MOL_OK);
  return engine;
}

static uint32_t next_random(uint32_t* state) {
  uint32_t value = *state;
  value ^= value << 13u;
  value ^= value >> 17u;
  value ^= value << 5u;
  *state = value;
  return value;
}

static void submit_pair(mol_engine_t* first, mol_engine_t* second, const mol_command_t* command) {
  EXPECT_TRUE(mol_engine_submit(first, command) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(second, command) == MOL_OK);
}

static void submit_block_invariance_sequence(mol_engine_t* first, mol_engine_t* second) {
  mol_command_t command = command_now(MOL_COMMAND_SET_TEMPO);
  command.target_frame = 0u;
  command.payload.scalar.value = 137.5f;
  submit_pair(first, second, &command);
  command = command_now(MOL_COMMAND_SET_TIME_SIGNATURE);
  command.target_frame = 0u;
  command.payload.time_signature.numerator = 5u;
  command.payload.time_signature.denominator = 4u;
  submit_pair(first, second, &command);
  command = command_now(MOL_COMMAND_SET_METRONOME);
  command.target_frame = 0u;
  command.payload.metronome.enabled = 1u;
  command.payload.metronome.level = 0.2f;
  submit_pair(first, second, &command);
  command = command_now(MOL_COMMAND_SET_SCALE);
  command.target_frame = 0u;
  command.payload.scale.type = MOL_SCALE_MIXOLYDIAN;
  command.payload.scale.tonic = 7u;
  command.payload.scale.mapping = MOL_SCALE_MAP_NEAREST;
  submit_pair(first, second, &command);
  command = command_now(MOL_COMMAND_SET_CHORD_MODE);
  command.target_frame = 0u;
  command.payload.integer.value = (int32_t)MOL_CHORD_POWER_5;
  submit_pair(first, second, &command);
  command = command_now(MOL_COMMAND_SET_ARPEGGIATOR);
  command.target_frame = 0u;
  command.payload.arpeggiator.mode = MOL_ARPEGGIATOR_RANDOM_DETERMINISTIC;
  command.payload.arpeggiator.rate = MOL_ARPEGGIATOR_RATE_SIXTEENTH_TRIPLET;
  command.payload.arpeggiator.gate = 0.375f;
  command.payload.arpeggiator.random_seed = UINT32_C(0x55667788);
  command.payload.arpeggiator.octaves = 3u;
  submit_pair(first, second, &command);
  command = command_now(MOL_COMMAND_TRANSPORT_START);
  command.target_frame = 0u;
  submit_pair(first, second, &command);

  for (uint32_t index = 0u; index < 8u; ++index) {
    mol_frame_index_t start = 500u + (mol_frame_index_t)index * 997u;
    mol_command_t on =
        note_command(MOL_COMMAND_NOTE_ON, start, index + 1u, (uint8_t)(48u + index * 3u));
    mol_command_t off =
        note_command(MOL_COMMAND_NOTE_OFF, start + 5000u + (index % 3u) * 1000u, index + 1u, 0u);
    submit_pair(first, second, &on);
    submit_pair(first, second, &off);
  }
  command = command_now(MOL_COMMAND_SUSTAIN);
  command.target_frame = 4000u;
  command.payload.scalar.value = 1.0f;
  submit_pair(first, second, &command);
  command.target_frame = 20000u;
  command.payload.scalar.value = 0.0f;
  submit_pair(first, second, &command);
  command = command_now(MOL_COMMAND_ALL_SOUND_OFF);
  command.target_frame = 28000u;
  submit_pair(first, second, &command);
  command = command_now(MOL_COMMAND_TRANSPORT_STOP);
  command.target_frame = 29000u;
  submit_pair(first, second, &command);
}

static void render_with_pattern(mol_engine_t* engine, float* output, const uint32_t* pattern,
                                uint32_t pattern_count) {
  uint32_t rendered = 0u;
  uint32_t pattern_index = 0u;
  while (rendered < 30000u) {
    uint32_t block = pattern[pattern_index++ % pattern_count];
    if (block > 30000u - rendered) {
      block = 30000u - rendered;
    }
    EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, output + rendered, block, 1u) == MOL_OK);
    rendered += block;
  }
}

static void test_block_size_invariance(void) {
  static const uint32_t first_pattern[5] = {1u, 7u, 128u, 13u, 64u};
  static const uint32_t second_pattern[4] = {113u, 5u, 31u, 97u};
  mol_engine_t* first = initialize(&first_storage);
  mol_engine_t* second = initialize(&second_storage);
  mol_event_t first_events[256];
  mol_event_t second_events[256];
  mol_engine_state_t first_state = {0};
  mol_engine_state_t second_state = {0};
  uint32_t first_count;
  uint32_t second_count;

  submit_block_invariance_sequence(first, second);
  render_with_pattern(first, first_audio, first_pattern, 5u);
  render_with_pattern(second, second_audio, second_pattern, 4u);
  for (uint32_t index = 0u; index < 30000u; ++index) {
    EXPECT_TRUE(isfinite(first_audio[index]));
    EXPECT_TRUE(isfinite(second_audio[index]));
    EXPECT_TRUE(first_audio[index] == second_audio[index]);
  }
  first_count = mol_engine_poll_events(first, first_events, 256u);
  second_count = mol_engine_poll_events(second, second_events, 256u);
  EXPECT_TRUE(first_count == second_count);
  for (uint32_t index = 0u; index < first_count && index < second_count; ++index) {
    EXPECT_TRUE(first_events[index].event_type == second_events[index].event_type);
    EXPECT_TRUE(first_events[index].source_id == second_events[index].source_id);
    EXPECT_TRUE(first_events[index].frame == second_events[index].frame);
    EXPECT_TRUE(first_events[index].gesture_id == second_events[index].gesture_id);
    EXPECT_TRUE(memcmp(first_events[index].payload, second_events[index].payload,
                       sizeof(first_events[index].payload)) == 0);
  }
  first_state.struct_size = (uint32_t)sizeof(first_state);
  second_state.struct_size = (uint32_t)sizeof(second_state);
  EXPECT_TRUE(mol_engine_get_state(first, &first_state) == MOL_OK);
  EXPECT_TRUE(mol_engine_get_state(second, &second_state) == MOL_OK);
  EXPECT_TRUE(first_state.current_frame == second_state.current_frame);
  EXPECT_TRUE(first_state.transport_frame == second_state.transport_frame);
  EXPECT_TRUE(first_state.active_voices == 0u && first_state.active_gestures == 0u);
  mol_engine_shutdown(first);
  mol_engine_shutdown(second);
}

static void remove_active_gesture(mol_gesture_id_t* gestures, uint32_t* count, uint32_t index) {
  gestures[index] = gestures[*count - 1u];
  --*count;
}

static void test_random_legal_sequences(void) {
  mol_engine_t* engine = initialize(&first_storage);
  mol_gesture_id_t gestures[64];
  uint32_t gesture_count = 0u;
  mol_gesture_id_t next_gesture = 1000u;
  uint32_t random = UINT32_C(0xC001D00D);
  float output[257];
  mol_event_t events[64];

  for (uint32_t operation = 0u; operation < 2000u; ++operation) {
    uint32_t value = next_random(&random);
    uint32_t choice = value % 14u;
    mol_command_t command;
    if (choice < 4u && gesture_count < 64u) {
      command = note_command(MOL_COMMAND_NOTE_ON, MOL_FRAME_IMMEDIATE, next_gesture,
                             (uint8_t)(next_random(&random) % 128u));
      command.payload.note.velocity = 0.1f + (float)(next_random(&random) % 901u) / 1000.0f;
      EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
      gestures[gesture_count++] = next_gesture++;
    } else if (choice < 6u && gesture_count != 0u) {
      uint32_t index = next_random(&random) % gesture_count;
      command = note_command(MOL_COMMAND_NOTE_OFF, MOL_FRAME_IMMEDIATE, gestures[index], 0u);
      EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
      remove_active_gesture(gestures, &gesture_count, index);
    } else if (choice == 6u) {
      command = command_now(MOL_COMMAND_SET_SCALE);
      command.payload.scale.type = next_random(&random) % MOL_SCALE_TYPE_COUNT;
      command.payload.scale.tonic = (uint8_t)(next_random(&random) % 12u);
      command.payload.scale.mapping = (uint8_t)(next_random(&random) % MOL_SCALE_MAPPING_COUNT);
      EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
    } else if (choice == 7u) {
      command = command_now(MOL_COMMAND_SET_CHORD_MODE);
      command.payload.integer.value = (int32_t)(next_random(&random) % MOL_CHORD_MODE_COUNT);
      EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
    } else if (choice == 8u) {
      command = command_now(MOL_COMMAND_SUSTAIN);
      command.payload.scalar.value = (float)(next_random(&random) % 128u) / 127.0f;
      EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
    } else if (choice == 9u) {
      command = command_now(MOL_COMMAND_SET_ARPEGGIATOR);
      command.payload.arpeggiator.mode = next_random(&random) % MOL_ARPEGGIATOR_MODE_COUNT;
      command.payload.arpeggiator.rate = next_random(&random) % MOL_ARPEGGIATOR_RATE_COUNT;
      command.payload.arpeggiator.gate = (float)(50u + next_random(&random) % 951u) / 1000.0f;
      command.payload.arpeggiator.random_seed = next_random(&random);
      command.payload.arpeggiator.octaves = (uint8_t)(1u + next_random(&random) % 4u);
      EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
    } else if (choice == 10u) {
      command = command_now(MOL_COMMAND_SET_PORTAMENTO);
      command.payload.portamento.mode = next_random(&random) % MOL_PORTAMENTO_MODE_COUNT;
      command.payload.portamento.time_ms = (float)(next_random(&random) % 2001u);
      EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
    } else if (choice == 11u) {
      command = command_now(MOL_COMMAND_SET_TEMPO);
      command.payload.scalar.value = 30.0f + (float)(next_random(&random) % 270001u) / 1000.0f;
      EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
    } else if (choice == 12u) {
      command = command_now((next_random(&random) & 1u) != 0u ? MOL_COMMAND_TRANSPORT_START
                                                              : MOL_COMMAND_TRANSPORT_STOP);
      EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
    } else {
      command = command_now(MOL_COMMAND_ALL_SOUND_OFF);
      EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
      gesture_count = 0u;
    }

    {
      uint32_t frames = 1u + next_random(&random) % 257u;
      EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, output, frames, 1u) == MOL_OK);
      for (uint32_t index = 0u; index < frames; ++index) {
        EXPECT_TRUE(isfinite(output[index]));
      }
      (void)mol_engine_poll_events(engine, events, 64u);
    }
  }

  {
    mol_command_t sustain_off = command_now(MOL_COMMAND_SUSTAIN);
    mol_command_t arpeggiator_off = command_now(MOL_COMMAND_SET_ARPEGGIATOR);
    mol_command_t sound_off = command_now(MOL_COMMAND_ALL_SOUND_OFF);
    mol_command_t stop = command_now(MOL_COMMAND_TRANSPORT_STOP);
    mol_engine_state_t state = {0};
    sustain_off.payload.scalar.value = 0.0f;
    arpeggiator_off.payload.arpeggiator.mode = MOL_ARPEGGIATOR_OFF;
    arpeggiator_off.payload.arpeggiator.rate = MOL_ARPEGGIATOR_RATE_SIXTEENTH;
    arpeggiator_off.payload.arpeggiator.gate = 0.5f;
    arpeggiator_off.payload.arpeggiator.octaves = 1u;
    EXPECT_TRUE(mol_engine_submit(engine, &sustain_off) == MOL_OK);
    EXPECT_TRUE(mol_engine_submit(engine, &arpeggiator_off) == MOL_OK);
    EXPECT_TRUE(mol_engine_submit(engine, &sound_off) == MOL_OK);
    EXPECT_TRUE(mol_engine_submit(engine, &stop) == MOL_OK);
    EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, output, 1u, 1u) == MOL_OK);
    state.struct_size = (uint32_t)sizeof(state);
    EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
    EXPECT_TRUE(state.active_voices == 0u);
    EXPECT_TRUE(state.active_gestures == 0u);
  }
  mol_engine_shutdown(engine);
}

int main(void) {
  test_block_size_invariance();
  test_random_legal_sequences();
  if (failures != 0) {
    (void)fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
