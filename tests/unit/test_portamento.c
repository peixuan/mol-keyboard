/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mol/mol.h"

typedef union portamento_test_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[2097152];
} portamento_test_storage_t;

static float audio[60000];
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
  command.payload.note.velocity = 1.0f;
  return command;
}

static mol_engine_t* initialize(portamento_test_storage_t* storage) {
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  config.channel_count = 1u;
  EXPECT_TRUE(mol_engine_init(storage->bytes, sizeof(storage->bytes), &config, &engine) == MOL_OK);
  return engine;
}

static void render(mol_engine_t* engine, float* output, uint32_t frame_count) {
  EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, output, frame_count, 1u) == MOL_OK);
  for (uint32_t index = 0u; index < frame_count; ++index) {
    EXPECT_TRUE(isfinite(output[index]));
  }
}

static float crossing_frequency(const float* samples, uint32_t first, uint32_t end) {
  uint32_t crossings = 0u;
  float previous = samples[first];
  for (uint32_t index = first + 1u; index < end; ++index) {
    if (previous <= 0.0f && samples[index] > 0.0f) {
      ++crossings;
    }
    previous = samples[index];
  }
  return (float)crossings * 48000.0f / (float)(end - first);
}

static void test_linear_legato_glide(void) {
  static portamento_test_storage_t storage;
  mol_engine_t* engine = initialize(&storage);
  mol_command_t portamento = command_at(MOL_COMMAND_SET_PORTAMENTO, 0u);
  mol_command_t c4 = note_at(MOL_COMMAND_NOTE_ON, 0u, 1u, 60u);
  mol_command_t c5 = note_at(MOL_COMMAND_NOTE_ON, 4800u, 2u, 72u);
  mol_command_t c5_off = note_at(MOL_COMMAND_NOTE_OFF, 12000u, 2u, 0u);
  mol_command_t c4_off = note_at(MOL_COMMAND_NOTE_OFF, 16800u, 1u, 0u);
  float before;
  float early;
  float late;
  float target;

  portamento.payload.portamento.mode = MOL_PORTAMENTO_LEGATO_ONLY;
  portamento.payload.portamento.time_ms = 100.0f;
  EXPECT_TRUE(mol_engine_submit(engine, &portamento) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &c4) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &c5) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &c5_off) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &c4_off) == MOL_OK);
  render(engine, audio, 28000u);

  before = crossing_frequency(audio, 2400u, 4400u);
  early = crossing_frequency(audio, 5000u, 6000u);
  late = crossing_frequency(audio, 8400u, 9400u);
  target = crossing_frequency(audio, 9800u, 11800u);
  EXPECT_TRUE(fabsf(before - 261.6256f) < 15.0f);
  EXPECT_TRUE(late > early + 100.0f);
  EXPECT_TRUE(fabsf(target - 523.2511f) < 20.0f);
  mol_engine_shutdown(engine);
}

static void test_last_note_ownership_and_sustain(void) {
  static portamento_test_storage_t storage;
  mol_engine_t* engine = initialize(&storage);
  mol_command_t portamento = command_at(MOL_COMMAND_SET_PORTAMENTO, 0u);
  mol_command_t first = note_at(MOL_COMMAND_NOTE_ON, 0u, 10u, 60u);
  mol_command_t second = note_at(MOL_COMMAND_NOTE_ON, 10u, 11u, 64u);
  mol_command_t pedal_on = command_at(MOL_COMMAND_SUSTAIN, 11u);
  mol_command_t second_off = note_at(MOL_COMMAND_NOTE_OFF, 12u, 11u, 0u);
  mol_command_t pedal_off = command_at(MOL_COMMAND_SUSTAIN, 20u);
  mol_command_t first_off = note_at(MOL_COMMAND_NOTE_OFF, 30u, 10u, 0u);
  mol_engine_state_t state = {0};
  mol_event_t events[32];
  uint32_t event_count;
  uint32_t first_starts = 0u;
  uint32_t second_starts = 0u;

  portamento.payload.portamento.mode = MOL_PORTAMENTO_LEGATO_ONLY;
  portamento.payload.portamento.time_ms = 50.0f;
  pedal_on.payload.scalar.value = 1.0f;
  pedal_off.payload.scalar.value = 0.0f;
  EXPECT_TRUE(mol_engine_submit(engine, &portamento) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &first) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &second) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &pedal_on) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &second_off) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &pedal_off) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &first_off) == MOL_OK);
  render(engine, audio, 31u);
  state.struct_size = (uint32_t)sizeof(state);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.active_voices == 1u);
  EXPECT_TRUE(state.active_gestures == 0u);
  EXPECT_TRUE(state.portamento_mode == MOL_PORTAMENTO_LEGATO_ONLY);
  EXPECT_TRUE(state.portamento_time_ms == 50.0f);
  event_count = mol_engine_poll_events(engine, events, 32u);
  for (uint32_t index = 0u; index < event_count; ++index) {
    if (events[index].event_type == MOL_EVENT_NOTE_STARTED) {
      first_starts += events[index].gesture_id == 10u ? 1u : 0u;
      second_starts += events[index].gesture_id == 11u ? 1u : 0u;
    }
  }
  EXPECT_TRUE(first_starts == 2u && second_starts == 1u);
  render(engine, audio, 48000u);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.active_voices == 0u);
  mol_engine_shutdown(engine);
}

static void test_always_mode_and_chord_boundary(void) {
  static portamento_test_storage_t storage;
  mol_engine_t* engine = initialize(&storage);
  mol_command_t portamento = command_at(MOL_COMMAND_SET_PORTAMENTO, 0u);
  mol_command_t first = note_at(MOL_COMMAND_NOTE_ON, 0u, 20u, 60u);
  mol_command_t first_off = note_at(MOL_COMMAND_NOTE_OFF, 1000u, 20u, 0u);
  mol_command_t second = note_at(MOL_COMMAND_NOTE_ON, 12000u, 21u, 72u);
  float early;
  float target;

  portamento.payload.portamento.mode = MOL_PORTAMENTO_ALWAYS;
  portamento.payload.portamento.time_ms = 100.0f;
  EXPECT_TRUE(mol_engine_submit(engine, &portamento) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &first) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &first_off) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &second) == MOL_OK);
  render(engine, audio, 18000u);
  early = crossing_frequency(audio, 12200u, 13200u);
  target = crossing_frequency(audio, 16800u, 17800u);
  EXPECT_TRUE(target > early + 100.0f);
  EXPECT_TRUE(fabsf(target - 523.2511f) < 30.0f);

  mol_engine_reset(engine);
  {
    mol_command_t chord = command_at(MOL_COMMAND_SET_CHORD_MODE, 0u);
    mol_command_t note = note_at(MOL_COMMAND_NOTE_ON, 0u, 30u, 60u);
    mol_engine_state_t state = {0};
    portamento = command_at(MOL_COMMAND_SET_PORTAMENTO, 0u);
    portamento.payload.portamento.mode = MOL_PORTAMENTO_ALWAYS;
    portamento.payload.portamento.time_ms = 100.0f;
    chord.payload.integer.value = (int32_t)MOL_CHORD_MAJOR;
    EXPECT_TRUE(mol_engine_submit(engine, &portamento) == MOL_OK);
    EXPECT_TRUE(mol_engine_submit(engine, &chord) == MOL_OK);
    EXPECT_TRUE(mol_engine_submit(engine, &note) == MOL_OK);
    render(engine, audio, 1u);
    state.struct_size = (uint32_t)sizeof(state);
    EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
    EXPECT_TRUE(state.active_voices == 3u);
  }

  portamento.payload.portamento.mode = MOL_PORTAMENTO_MODE_COUNT;
  EXPECT_TRUE(mol_engine_submit(engine, &portamento) == MOL_ERROR_INVALID_ARGUMENT);
  portamento.payload.portamento.mode = MOL_PORTAMENTO_ALWAYS;
  portamento.payload.portamento.time_ms = 2000.1f;
  EXPECT_TRUE(mol_engine_submit(engine, &portamento) == MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE((mol_engine_get_capabilities(engine) & MOL_CAPABILITY_MONOPHONIC_PORTAMENTO) != 0u);
  mol_engine_shutdown(engine);
}

int main(void) {
  test_linear_legato_glide();
  test_last_note_ownership_and_sustain();
  test_always_mode_and_chord_boundary();
  if (failures != 0) {
    (void)fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
