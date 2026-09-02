/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mol/mol.h"

typedef union semantics_test_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[65536];
} semantics_test_storage_t;

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
  command.source_id = 7u;
  command.gesture_id = gesture;
  command.payload.note.note = note;
  command.payload.note.velocity = 0.75f;
  return command;
}

static mol_engine_t* initialize(semantics_test_storage_t* storage) {
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  config.channel_count = 1u;
  EXPECT_TRUE(mol_engine_init(storage->bytes, sizeof(storage->bytes), &config, &engine) == MOL_OK);
  return engine;
}

static void render_frames(mol_engine_t* engine, uint32_t count) {
  float output[128];
  while (count != 0u) {
    uint32_t block = count < 128u ? count : 128u;
    uint32_t index;
    EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, output, block, 1u) == MOL_OK);
    for (index = 0u; index < block; ++index) {
      EXPECT_TRUE(isfinite(output[index]));
    }
    count -= block;
  }
}

static void test_transform_order_and_ownership(void) {
  static semantics_test_storage_t storage;
  mol_engine_t* engine = initialize(&storage);
  mol_command_t octave = command_at(MOL_COMMAND_SET_OCTAVE_SHIFT, 0u);
  mol_command_t transpose = command_at(MOL_COMMAND_SET_TRANSPOSE, 0u);
  mol_command_t scale = command_at(MOL_COMMAND_SET_SCALE, 0u);
  mol_command_t chord = command_at(MOL_COMMAND_SET_CHORD_MODE, 0u);
  mol_command_t note_on = note_at(MOL_COMMAND_NOTE_ON, 0u, 101u, 60u);
  mol_command_t change_octave = command_at(MOL_COMMAND_SET_OCTAVE_SHIFT, 1u);
  mol_command_t change_scale = command_at(MOL_COMMAND_SET_SCALE, 1u);
  mol_command_t change_chord = command_at(MOL_COMMAND_SET_CHORD_MODE, 1u);
  mol_command_t note_off = note_at(MOL_COMMAND_NOTE_OFF, 2u, 101u, 0u);
  mol_engine_state_t state = {0};
  mol_event_t events[32];
  uint32_t count;
  uint32_t mapped = 0u;
  uint32_t started = 0u;
  uint32_t released = 0u;
  static const uint8_t expected_notes[3] = {72u, 75u, 79u};

  octave.payload.integer.value = 1;
  transpose.payload.integer.value = 1;
  scale.payload.scale.type = MOL_SCALE_MAJOR;
  scale.payload.scale.tonic = 0u;
  scale.payload.scale.mapping = MOL_SCALE_MAP_NEAREST;
  chord.payload.integer.value = (int32_t)MOL_CHORD_MINOR;
  change_octave.payload.integer.value = -3;
  change_scale.payload.scale.type = MOL_SCALE_BLUES;
  change_scale.payload.scale.tonic = 11u;
  change_scale.payload.scale.mapping = MOL_SCALE_MAP_UP;
  change_chord.payload.integer.value = (int32_t)MOL_CHORD_OCTAVE;

  EXPECT_TRUE(mol_engine_submit(engine, &octave) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &transpose) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &scale) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &chord) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &note_on) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &change_octave) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &change_scale) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &change_chord) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &note_off) == MOL_OK);
  render_frames(engine, 3u);

  state.struct_size = (uint32_t)sizeof(state);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.active_voices == 3u);
  EXPECT_TRUE(state.octave_shift == -3);
  EXPECT_TRUE(state.scale_type == MOL_SCALE_BLUES);
  EXPECT_TRUE(state.scale_tonic == 11u);
  EXPECT_TRUE(state.chord_mode == MOL_CHORD_OCTAVE);
  count = mol_engine_poll_events(engine, events, 32u);
  EXPECT_TRUE(count == 9u);
  for (uint32_t index = 0u; index < count; ++index) {
    EXPECT_TRUE(events[index].gesture_id == 101u);
    EXPECT_TRUE(events[index].source_id == 7u);
    if (events[index].event_type == MOL_EVENT_GESTURE_MAPPED) {
      EXPECT_TRUE(events[index].payload[MOL_EVENT_PAYLOAD_NOTE] == expected_notes[mapped]);
      EXPECT_TRUE(events[index].payload[MOL_EVENT_PAYLOAD_INPUT_NOTE] == 60u);
      EXPECT_TRUE(events[index].payload[MOL_EVENT_PAYLOAD_MAPPED_INDEX] == mapped);
      EXPECT_TRUE(events[index].payload[MOL_EVENT_PAYLOAD_MAPPED_COUNT] == 3u);
      ++mapped;
    } else if (events[index].event_type == MOL_EVENT_NOTE_STARTED) {
      EXPECT_TRUE(events[index].payload[MOL_EVENT_PAYLOAD_NOTE] == expected_notes[started]);
      ++started;
    } else if (events[index].event_type == MOL_EVENT_NOTE_RELEASED) {
      EXPECT_TRUE(events[index].payload[MOL_EVENT_PAYLOAD_NOTE] == expected_notes[released]);
      ++released;
    }
  }
  EXPECT_TRUE(mapped == 3u && started == 3u && released == 3u);
  mol_engine_shutdown(engine);
}

static void test_sustain_and_all_sound_off(void) {
  static semantics_test_storage_t storage;
  mol_engine_t* engine = initialize(&storage);
  mol_command_t note_on = note_at(MOL_COMMAND_NOTE_ON, 0u, 201u, 60u);
  mol_command_t pedal_on = command_at(MOL_COMMAND_SUSTAIN, 1u);
  mol_command_t note_off = note_at(MOL_COMMAND_NOTE_OFF, 2u, 201u, 0u);
  mol_command_t pedal_off = command_at(MOL_COMMAND_SUSTAIN, MOL_FRAME_IMMEDIATE);
  mol_command_t sound_off = command_at(MOL_COMMAND_ALL_SOUND_OFF, MOL_FRAME_IMMEDIATE);
  mol_event_t events[16];
  mol_engine_state_t state = {0};
  uint32_t count;
  int release_seen = 0;

  pedal_on.payload.scalar.value = 1.0f;
  pedal_off.payload.scalar.value = 0.0f;
  EXPECT_TRUE(mol_engine_submit(engine, &note_on) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &pedal_on) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &note_off) == MOL_OK);
  render_frames(engine, 128u);
  count = mol_engine_poll_events(engine, events, 16u);
  for (uint32_t index = 0u; index < count; ++index) {
    EXPECT_TRUE(events[index].event_type != MOL_EVENT_NOTE_RELEASED);
  }
  state.struct_size = (uint32_t)sizeof(state);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.active_voices == 1u && state.sustain == 1.0f);

  EXPECT_TRUE(mol_engine_submit(engine, &pedal_off) == MOL_OK);
  render_frames(engine, 1u);
  count = mol_engine_poll_events(engine, events, 16u);
  for (uint32_t index = 0u; index < count; ++index) {
    if (events[index].event_type == MOL_EVENT_NOTE_RELEASED && events[index].gesture_id == 201u) {
      release_seen = 1;
    }
  }
  EXPECT_TRUE(release_seen);
  render_frames(engine, 10000u);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.active_voices == 0u);

  note_on = note_at(MOL_COMMAND_NOTE_ON, MOL_FRAME_IMMEDIATE, 202u, 64u);
  EXPECT_TRUE(mol_engine_submit(engine, &note_on) == MOL_OK);
  render_frames(engine, 1u);
  EXPECT_TRUE(mol_engine_submit(engine, &sound_off) == MOL_OK);
  render_frames(engine, 1u);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.active_voices == 0u);
  mol_engine_shutdown(engine);
}

static void test_independent_and_invalid_gestures(void) {
  static semantics_test_storage_t storage;
  mol_engine_t* engine = initialize(&storage);
  mol_command_t first = note_at(MOL_COMMAND_NOTE_ON, 0u, 301u, 60u);
  mol_command_t second = note_at(MOL_COMMAND_NOTE_ON, 0u, 302u, 60u);
  mol_command_t duplicate = note_at(MOL_COMMAND_NOTE_ON, 1u, 301u, 67u);
  mol_command_t first_off = note_at(MOL_COMMAND_NOTE_OFF, 2u, 301u, 0u);
  mol_command_t octave = command_at(MOL_COMMAND_SET_OCTAVE_SHIFT, 3u);
  mol_command_t out_of_range = note_at(MOL_COMMAND_NOTE_ON, 3u, 303u, 127u);
  mol_event_t events[32];
  uint32_t count;
  int duplicate_error = 0;
  int range_error = 0;
  int first_released = 0;
  int second_released = 0;

  octave.payload.integer.value = 3;
  EXPECT_TRUE(mol_engine_submit(engine, &first) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &second) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &duplicate) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &first_off) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &octave) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &out_of_range) == MOL_OK);
  render_frames(engine, 4u);
  count = mol_engine_poll_events(engine, events, 32u);
  for (uint32_t index = 0u; index < count; ++index) {
    if (events[index].event_type == MOL_EVENT_ERROR_REPORTED &&
        events[index].payload[0] == MOL_MUSIC_ERROR_DUPLICATE_GESTURE) {
      duplicate_error = 1;
    }
    if (events[index].event_type == MOL_EVENT_ERROR_REPORTED &&
        events[index].payload[0] == MOL_MUSIC_ERROR_NOTE_OUT_OF_RANGE) {
      range_error = 1;
    }
    if (events[index].event_type == MOL_EVENT_NOTE_RELEASED) {
      first_released += events[index].gesture_id == 301u ? 1 : 0;
      second_released += events[index].gesture_id == 302u ? 1 : 0;
    }
  }
  EXPECT_TRUE(duplicate_error && range_error);
  EXPECT_TRUE(first_released == 1 && second_released == 0);
  EXPECT_TRUE((mol_engine_get_capabilities(engine) & MOL_CAPABILITY_CONTINUOUS_SUSTAIN) != 0u);

  octave.payload.integer.value = 4;
  EXPECT_TRUE(mol_engine_submit(engine, &octave) == MOL_ERROR_INVALID_ARGUMENT);
  octave.command_type = MOL_COMMAND_SET_TRANSPOSE;
  octave.payload.integer.value = -25;
  EXPECT_TRUE(mol_engine_submit(engine, &octave) == MOL_ERROR_INVALID_ARGUMENT);
  mol_engine_shutdown(engine);
}

int main(void) {
  test_transform_order_and_ownership();
  test_sustain_and_all_sound_off();
  test_independent_and_invalid_gestures();
  if (failures != 0) {
    (void)fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
