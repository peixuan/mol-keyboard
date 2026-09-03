/* SPDX-License-Identifier: Apache-2.0 */
#include "mol/mol.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef union audio_test_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[2097152];
} audio_test_storage_t;

static int failures = 0;

#define EXPECT_TRUE(condition)                                                 \
  do {                                                                         \
    if (!(condition)) {                                                        \
      (void)fprintf(stderr, "%s:%d: expectation failed: %s\n", __FILE__,     \
                    __LINE__, #condition);                                     \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

static mol_command_t note_command(mol_command_type_t type, mol_frame_index_t frame,
                                  mol_gesture_id_t gesture) {
  mol_command_t command;
  memset(&command, 0, sizeof(command));
  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = type;
  command.target_frame = frame;
  command.gesture_id = gesture;
  command.payload.note.note = 60u;
  command.payload.note.velocity = 1.0f;
  return command;
}

static void test_c4_frequency_and_release(void) {
  static audio_test_storage_t storage;
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  mol_command_t note_on = note_command(MOL_COMMAND_NOTE_ON, 0u, 1u);
  mol_command_t note_off = note_command(MOL_COMMAND_NOTE_OFF, 48000u, 1u);
  mol_engine_state_t state = {0};
  mol_event_t events[8];
  float block[128];
  float previous = 0.0f;
  float peak = 0.0f;
  uint32_t crossings = 0u;
  uint32_t frame = 0u;

  config.channel_count = 1u;
  EXPECT_TRUE(mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &note_on) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &note_off) == MOL_OK);

  while (frame < 48000u) {
    EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, block, 128u, 1u) == MOL_OK);
    for (uint32_t index = 0u; index < 128u; ++index) {
      uint32_t absolute_frame = frame + index;
      float magnitude = fabsf(block[index]);
      EXPECT_TRUE(isfinite(block[index]));
      if (magnitude > peak) {
        peak = magnitude;
      }
      if (absolute_frame >= 4800u && absolute_frame < 43200u && previous <= 0.0f &&
          block[index] > 0.0f) {
        ++crossings;
      }
      previous = block[index];
    }
    frame += 128u;
  }

  EXPECT_TRUE(peak > 0.01f && peak <= 1.0f);
  EXPECT_TRUE(fabsf(((float)crossings / 0.8f) - 261.6256f) < 1.0f);
  for (uint32_t release_frame = 0u; release_frame < 48000u; release_frame += 128u) {
    EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, block, 128u, 1u) == MOL_OK);
  }
  state.struct_size = (uint32_t)sizeof(state);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.active_voices == 0u);
  EXPECT_TRUE(mol_engine_poll_events(engine, events, 8u) >= 3u);
  mol_engine_shutdown(engine);
}

static void test_sample_accurate_start(void) {
  static audio_test_storage_t storage;
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  mol_command_t note_on = note_command(MOL_COMMAND_NOTE_ON, 64u, 2u);
  float block[128];
  int heard_after_target = 0;

  config.channel_count = 1u;
  EXPECT_TRUE(mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &note_on) == MOL_OK);
  EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, block, 128u, 1u) == MOL_OK);
  for (uint32_t index = 0u; index < 64u; ++index) {
    EXPECT_TRUE(block[index] == 0.0f);
  }
  for (uint32_t index = 64u; index < 128u; ++index) {
    if (block[index] != 0.0f) {
      heard_after_target = 1;
    }
  }
  EXPECT_TRUE(heard_after_target);
  mol_engine_shutdown(engine);
}

static void test_eight_voice_polyphony(void) {
  static audio_test_storage_t storage;
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  mol_engine_state_t state = {0};
  mol_event_t events[32];
  float sample = 0.0f;
  int found_steal = 0;

  config.channel_count = 1u;
  config.max_voices = 8u;
  EXPECT_TRUE(mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) == MOL_OK);
  for (uint32_t index = 0u; index < 8u; ++index) {
    mol_command_t note_on = note_command(MOL_COMMAND_NOTE_ON, 0u, index + 1u);
    note_on.payload.note.note = (uint8_t)(60u + index);
    EXPECT_TRUE(mol_engine_submit(engine, &note_on) == MOL_OK);
  }
  EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, &sample, 1u, 1u) == MOL_OK);
  state.struct_size = (uint32_t)sizeof(state);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.active_voices == 8u);

  {
    mol_command_t ninth = note_command(MOL_COMMAND_NOTE_ON, MOL_FRAME_IMMEDIATE, 9u);
    ninth.payload.note.note = 72u;
    EXPECT_TRUE(mol_engine_submit(engine, &ninth) == MOL_OK);
  }
  EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, &sample, 1u, 1u) == MOL_OK);
  {
    uint32_t count = mol_engine_poll_events(engine, events, 32u);
    for (uint32_t index = 0u; index < count; ++index) {
      if (events[index].event_type == MOL_EVENT_VOICE_STOLEN) {
        found_steal = 1;
      }
    }
  }
  EXPECT_TRUE(found_steal);
  mol_engine_shutdown(engine);
}

int main(void) {
  test_c4_frequency_and_release();
  test_sample_accurate_start();
  test_eight_voice_polyphony();
  if (failures != 0) {
    (void)fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
