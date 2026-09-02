/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mol/mol.h"

typedef union instrument_test_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[1048576];
} instrument_test_storage_t;

static instrument_test_storage_t storage;
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

static mol_command_t preset_command(mol_preset_id_t preset, uint8_t hard_switch) {
  mol_command_t command = command_now(MOL_COMMAND_SET_PRESET);
  command.payload.preset.preset = preset;
  command.payload.preset.hard_switch = hard_switch;
  return command;
}

static mol_command_t note_command(mol_command_type_t type, mol_gesture_id_t gesture, uint8_t note) {
  mol_command_t command = command_now(type);
  command.gesture_id = gesture;
  command.payload.note.note = note;
  command.payload.note.velocity = 0.8f;
  return command;
}

static mol_engine_t* initialize(uint32_t max_voices) {
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  config.channel_count = 1u;
  config.max_voices = max_voices;
  EXPECT_TRUE(mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) == MOL_OK);
  return engine;
}

static uint64_t hash_sample(uint64_t hash, float sample) {
  int32_t quantized = (int32_t)lrintf(sample * 32767.0f);
  for (uint32_t shift = 0u; shift < 32u; shift += 8u) {
    hash ^= (uint8_t)((uint32_t)quantized >> shift);
    hash *= UINT64_C(1099511628211);
  }
  return hash;
}

static void test_all_presets_across_range(void) {
  static const uint8_t notes[5] = {48u, 60u, 72u, 84u, 96u};
  mol_engine_t* engine = initialize(8u);
  uint64_t fingerprints[MOL_PRESET_COUNT];
  float block[128];
  mol_gesture_id_t gesture = 1u;

  for (uint32_t preset = 0u; preset < MOL_PRESET_COUNT; ++preset) {
    fingerprints[preset] = UINT64_C(14695981039346656037);
    for (uint32_t note_index = 0u; note_index < 5u; ++note_index) {
      mol_command_t select = preset_command(preset, 1u);
      mol_command_t note = note_command(MOL_COMMAND_NOTE_ON, gesture++, notes[note_index]);
      double energy = 0.0;
      float peak = 0.0f;
      EXPECT_TRUE(mol_engine_submit(engine, &select) == MOL_OK);
      EXPECT_TRUE(mol_engine_submit(engine, &note) == MOL_OK);
      for (uint32_t rendered = 0u; rendered < 4096u; rendered += 128u) {
        EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, block, 128u, 1u) == MOL_OK);
        for (uint32_t index = 0u; index < 128u; ++index) {
          float magnitude = fabsf(block[index]);
          EXPECT_TRUE(isfinite(block[index]));
          energy += (double)block[index] * block[index];
          if (magnitude > peak) {
            peak = magnitude;
          }
          if (note_index == 1u) {
            fingerprints[preset] = hash_sample(fingerprints[preset], block[index]);
          }
        }
      }
      EXPECT_TRUE(energy > 0.000001 && peak > 0.000001f && peak <= 1.0f);
    }
    for (uint32_t prior = 0u; prior < preset; ++prior) {
      EXPECT_TRUE(fingerprints[preset] != fingerprints[prior]);
    }
  }
  mol_engine_shutdown(engine);
}

static void test_natural_and_hard_switch(void) {
  mol_engine_t* engine = initialize(8u);
  mol_command_t grand = preset_command(MOL_PRESET_GRAND_PIANO, 1u);
  mol_command_t electric = preset_command(MOL_PRESET_ELECTRIC_PIANO, 0u);
  mol_command_t violin = preset_command(MOL_PRESET_VIOLIN, 1u);
  mol_command_t first = note_command(MOL_COMMAND_NOTE_ON, 500u, 60u);
  mol_command_t second = note_command(MOL_COMMAND_NOTE_ON, 501u, 67u);
  mol_engine_state_t state = {0};
  mol_event_t events[64];
  float before[256];
  float after[256];
  float boundary_delta;
  int preset_event_seen = 0;

  EXPECT_TRUE(mol_engine_submit(engine, &grand) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &first) == MOL_OK);
  EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, before, 256u, 1u) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &electric) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &second) == MOL_OK);
  EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, after, 256u, 1u) == MOL_OK);
  boundary_delta = fabsf(after[0] - before[255]);
  EXPECT_TRUE(boundary_delta < 0.25f);
  state.struct_size = (uint32_t)sizeof(state);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.preset == MOL_PRESET_ELECTRIC_PIANO && state.active_voices == 2u);
  {
    uint32_t event_count = mol_engine_poll_events(engine, events, 64u);
    for (uint32_t index = 0u; index < event_count; ++index) {
      if (events[index].event_type == MOL_EVENT_PRESET_CHANGED &&
          events[index].payload[0] == MOL_PRESET_ELECTRIC_PIANO) {
        preset_event_seen = 1;
      }
    }
  }
  EXPECT_TRUE(preset_event_seen);
  EXPECT_TRUE(mol_engine_submit(engine, &violin) == MOL_OK);
  EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, after, 1u, 1u) == MOL_OK);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.preset == MOL_PRESET_VIOLIN && state.active_voices == 0u &&
              state.active_gestures == 0u);

  violin.payload.preset.preset = MOL_PRESET_COUNT;
  EXPECT_TRUE(mol_engine_submit(engine, &violin) == MOL_ERROR_INVALID_ARGUMENT);
  violin.payload.preset.preset = MOL_PRESET_VIOLIN;
  violin.payload.preset.hard_switch = 2u;
  EXPECT_TRUE(mol_engine_submit(engine, &violin) == MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE((mol_engine_get_capabilities(engine) & MOL_CAPABILITY_BUILTIN_PATCHES) != 0u);
  mol_engine_shutdown(engine);
}

static void test_release_voice_is_stolen_first(void) {
  mol_engine_t* engine = initialize(8u);
  float output[256];
  mol_event_t events[64];
  int released_note_was_stolen = 0;
  for (uint32_t index = 0u; index < 8u; ++index) {
    mol_command_t note = note_command(MOL_COMMAND_NOTE_ON, 700u + index, (uint8_t)(60u + index));
    EXPECT_TRUE(mol_engine_submit(engine, &note) == MOL_OK);
  }
  EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, output, 256u, 1u) == MOL_OK);
  {
    mol_command_t release = note_command(MOL_COMMAND_NOTE_OFF, 700u, 0u);
    mol_command_t ninth = note_command(MOL_COMMAND_NOTE_ON, 708u, 72u);
    EXPECT_TRUE(mol_engine_submit(engine, &release) == MOL_OK);
    EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, output, 1u, 1u) == MOL_OK);
    EXPECT_TRUE(mol_engine_submit(engine, &ninth) == MOL_OK);
    EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, output, 64u, 1u) == MOL_OK);
  }
  {
    uint32_t event_count = mol_engine_poll_events(engine, events, 64u);
    for (uint32_t index = 0u; index < event_count; ++index) {
      if (events[index].event_type == MOL_EVENT_VOICE_STOLEN &&
          events[index].payload[MOL_EVENT_PAYLOAD_NOTE] == 60u) {
        released_note_was_stolen = 1;
      }
    }
  }
  EXPECT_TRUE(released_note_was_stolen);
  for (uint32_t index = 1u; index < 64u; ++index) {
    EXPECT_TRUE(fabsf(output[index] - output[index - 1u]) < 0.75f);
  }
  mol_engine_shutdown(engine);
}

static void test_rapidly_switch_all_presets(void) {
  mol_engine_t* engine = initialize(8u);
  float output[64];
  float previous = 0.0f;
  mol_gesture_id_t gesture = 900u;

  for (uint32_t preset = 0u; preset < MOL_PRESET_COUNT; ++preset) {
    mol_command_t select = preset_command(preset, 1u);
    mol_command_t note = note_command(MOL_COMMAND_NOTE_ON, gesture++, (uint8_t)(48u + preset));
    EXPECT_TRUE(mol_engine_submit(engine, &select) == MOL_OK);
    EXPECT_TRUE(mol_engine_submit(engine, &note) == MOL_OK);
    EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, output, 64u, 1u) == MOL_OK);
    for (uint32_t index = 0u; index < 64u; ++index) {
      EXPECT_TRUE(isfinite(output[index]));
      EXPECT_TRUE(fabsf(output[index] - previous) < 0.25f);
      previous = output[index];
    }
  }
  {
    mol_command_t final_switch = preset_command(MOL_PRESET_GRAND_PIANO, 1u);
    EXPECT_TRUE(mol_engine_submit(engine, &final_switch) == MOL_OK);
    EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, output, 64u, 1u) == MOL_OK);
    for (uint32_t index = 0u; index < 64u; ++index) {
      EXPECT_TRUE(isfinite(output[index]));
      EXPECT_TRUE(fabsf(output[index] - previous) < 0.25f);
      previous = output[index];
    }
  }
  mol_engine_shutdown(engine);
}

int main(void) {
  test_all_presets_across_range();
  test_natural_and_hard_switch();
  test_release_voice_is_stolen_first();
  test_rapidly_switch_all_presets();
  if (failures != 0) {
    (void)fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
