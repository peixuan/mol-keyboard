/* SPDX-License-Identifier: Apache-2.0 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mol/mol.h"

#define TEST_EVENT_CAPACITY 64u
#define TEST_AUDIO_FRAMES 64u

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

static mol_command_t make_command(mol_command_type_t type, mol_frame_index_t frame) {
  mol_command_t command;
  memset(&command, 0, sizeof(command));
  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = type;
  command.target_frame = frame;
  command.source_id = 9u;
  return command;
}

static mol_engine_t* initialize_engine(test_storage_t* storage) {
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  config.max_voices = 8u;
  config.command_capacity = 64u;
  config.event_capacity = 128u;
  config.sequence_capacity = TEST_EVENT_CAPACITY;
  EXPECT_TRUE(mol_engine_query_memory(&config) <= sizeof(storage->bytes));
  EXPECT_TRUE(mol_engine_init(storage->bytes, sizeof(storage->bytes), &config, &engine) == MOL_OK);
  return engine;
}

static void submit_recording_script(mol_engine_t* engine) {
  mol_command_t command = make_command(MOL_COMMAND_RECORD_START, 0u);
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);

  command = make_command(MOL_COMMAND_SET_CHORD_MODE, 0u);
  command.payload.integer.value = MOL_CHORD_MAJOR;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);

  command = make_command(MOL_COMMAND_NOTE_ON, 10u);
  command.gesture_id = 100u;
  command.payload.note.note = 60u;
  command.payload.note.velocity = 0.75f;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);

  command = make_command(MOL_COMMAND_SUSTAIN, 20u);
  command.payload.scalar.value = 1.0f;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);

  command = make_command(MOL_COMMAND_NOTE_OFF, 30u);
  command.gesture_id = 100u;
  command.payload.note.note = 60u;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);

  command = make_command(MOL_COMMAND_PITCH_BEND, 35u);
  command.payload.scalar.value = 0.5f;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);

  command = make_command(MOL_COMMAND_SUSTAIN, 40u);
  command.payload.scalar.value = 0.0f;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);

  command = make_command(MOL_COMMAND_SET_PRESET, 45u);
  command.payload.preset.preset = MOL_PRESET_FLUTE;
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);

  command = make_command(MOL_COMMAND_RECORD_STOP, 50u);
  EXPECT_TRUE(mol_engine_submit(engine, &command) == MOL_OK);
}

static uint32_t capture_recording(mol_engine_t* engine, mol_sequence_config_t* config,
                                  mol_sequence_event_t* events) {
  float output[60u * 2u];
  uint32_t count = 0u;
  config->struct_size = (uint32_t)sizeof(*config);
  config->api_version = MOL_API_VERSION;
  EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, output, 60u, 2u) == MOL_OK);
  EXPECT_TRUE(mol_engine_copy_recording(engine, config, NULL, 0u, &count) ==
              MOL_ERROR_BUFFER_TOO_SMALL);
  EXPECT_TRUE(count >= 11u && count <= TEST_EVENT_CAPACITY);
  EXPECT_TRUE(mol_engine_copy_recording(engine, config, events, TEST_EVENT_CAPACITY, &count) ==
              MOL_OK);
  return count;
}

static void test_records_post_music_events(void) {
  static test_storage_t storage;
  mol_engine_t* engine = initialize_engine(&storage);
  mol_sequence_config_t config = {0};
  mol_sequence_event_t events[TEST_EVENT_CAPACITY];
  uint32_t count;
  uint32_t note_on_count = 0u;
  uint32_t note_off_count = 0u;
  uint8_t note_mask = 0u;

  submit_recording_script(engine);
  count = capture_recording(engine, &config, events);
  EXPECT_TRUE(config.initial_state.chord_mode == MOL_CHORD_OFF);
  {
    uint32_t chord_event_count = 0u;
    for (uint32_t index = 0u; index < count; ++index) {
      if (events[index].command_type == MOL_COMMAND_SET_CHORD_MODE && events[index].frame == 0u)
        ++chord_event_count;
    }
    EXPECT_TRUE(chord_event_count == 1u);
  }
  for (uint32_t index = 0u; index < count; ++index) {
    if (events[index].command_type == MOL_COMMAND_NOTE_ON) {
      EXPECT_TRUE(events[index].frame == 10u);
      EXPECT_TRUE(events[index].gesture_id >= 1u && events[index].gesture_id <= 3u);
      if (events[index].payload.note.note == 60u) note_mask |= 1u;
      if (events[index].payload.note.note == 64u) note_mask |= 2u;
      if (events[index].payload.note.note == 67u) note_mask |= 4u;
      ++note_on_count;
    }
    if (events[index].command_type == MOL_COMMAND_NOTE_OFF) {
      EXPECT_TRUE(events[index].frame == 40u);
      ++note_off_count;
    }
  }
  EXPECT_TRUE(note_on_count == 3u && note_off_count == 3u && note_mask == 7u);
  EXPECT_TRUE(events[count - 1u].command_type == MOL_COMMAND_SET_PRESET);
  mol_engine_shutdown(engine);
}

static uint32_t render_playback(test_storage_t* storage, const mol_sequence_config_t* config,
                                const mol_sequence_event_t* events, uint32_t event_count,
                                float* audio, mol_event_t* out_events) {
  mol_engine_t* engine = initialize_engine(storage);
  mol_command_t play = make_command(MOL_COMMAND_PLAYBACK_START, 0u);
  mol_engine_state_t state = {0};
  uint32_t count;
  EXPECT_TRUE(mol_engine_load_sequence(engine, config, events, event_count) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &play) == MOL_OK);
  EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, audio, TEST_AUDIO_FRAMES, 2u) == MOL_OK);
  state.struct_size = (uint32_t)sizeof(state);
  EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
  EXPECT_TRUE(state.playback == 0u && state.loaded_sequence_event_count == event_count);
  count = mol_engine_poll_events(engine, out_events, 128u);
  mol_engine_shutdown(engine);
  return count;
}

static void test_deterministic_playback(void) {
  static test_storage_t recording_storage;
  static test_storage_t first_storage;
  static test_storage_t second_storage;
  mol_engine_t* recording_engine = initialize_engine(&recording_storage);
  mol_sequence_config_t config = {0};
  mol_sequence_event_t sequence[TEST_EVENT_CAPACITY];
  mol_event_t first_events[128];
  mol_event_t second_events[128];
  float first_audio[TEST_AUDIO_FRAMES * 2u];
  float second_audio[TEST_AUDIO_FRAMES * 2u];
  uint32_t sequence_count;
  uint32_t first_count;
  uint32_t second_count;

  submit_recording_script(recording_engine);
  sequence_count = capture_recording(recording_engine, &config, sequence);
  mol_engine_shutdown(recording_engine);
  first_count =
      render_playback(&first_storage, &config, sequence, sequence_count, first_audio, first_events);
  second_count = render_playback(&second_storage, &config, sequence, sequence_count, second_audio,
                                 second_events);
  EXPECT_TRUE(first_count == second_count);
  EXPECT_TRUE(memcmp(first_events, second_events, sizeof(*first_events) * first_count) == 0);
  EXPECT_TRUE(memcmp(first_audio, second_audio, sizeof(first_audio)) == 0);
}

static void test_repeated_playback_resets_signal_state(void) {
  static test_storage_t recording_storage;
  static test_storage_t playback_storage;
  mol_engine_t* recording_engine = initialize_engine(&recording_storage);
  mol_engine_t* playback_engine;
  mol_sequence_config_t config = {0};
  mol_sequence_event_t sequence[TEST_EVENT_CAPACITY];
  mol_event_t discarded[128];
  mol_command_t play;
  float first_audio[TEST_AUDIO_FRAMES * 2u];
  float second_audio[TEST_AUDIO_FRAMES * 2u];
  uint32_t sequence_count;

  submit_recording_script(recording_engine);
  sequence_count = capture_recording(recording_engine, &config, sequence);
  mol_engine_shutdown(recording_engine);
  playback_engine = initialize_engine(&playback_storage);
  EXPECT_TRUE(mol_engine_load_sequence(playback_engine, &config, sequence, sequence_count) ==
              MOL_OK);
  play = make_command(MOL_COMMAND_PLAYBACK_START, MOL_FRAME_IMMEDIATE);
  EXPECT_TRUE(mol_engine_submit(playback_engine, &play) == MOL_OK);
  EXPECT_TRUE(mol_engine_render_interleaved_f32(playback_engine, first_audio, TEST_AUDIO_FRAMES,
                                                2u) == MOL_OK);
  (void)mol_engine_poll_events(playback_engine, discarded, 128u);

  EXPECT_TRUE(mol_engine_load_sequence(playback_engine, &config, sequence, sequence_count) ==
              MOL_OK);
  EXPECT_TRUE(mol_engine_submit(playback_engine, &play) == MOL_OK);
  EXPECT_TRUE(mol_engine_render_interleaved_f32(playback_engine, second_audio, TEST_AUDIO_FRAMES,
                                                2u) == MOL_OK);
  EXPECT_TRUE(memcmp(first_audio, second_audio, sizeof(first_audio)) == 0);
  mol_engine_shutdown(playback_engine);
}

static void test_time_base_scaling_and_validation(void) {
  static test_storage_t storage;
  mol_engine_t* engine = initialize_engine(&storage);
  mol_sequence_config_t config = mol_sequence_config_default(44100u);
  mol_sequence_event_t events[2] = {0};
  mol_command_t play = make_command(MOL_COMMAND_PLAYBACK_START, 0u);
  mol_event_t emitted[16];
  float audio[50u * 2u];
  uint32_t emitted_count;

  config.time_base = 960u;
  events[0].struct_size = (uint32_t)sizeof(events[0]);
  events[0].api_version = MOL_API_VERSION;
  events[0].frame = 1u;
  events[0].command_type = MOL_COMMAND_NOTE_ON;
  events[0].gesture_id = 77u;
  events[0].payload.note.note = 69u;
  events[0].payload.note.velocity = 0.8f;
  events[1] = events[0];
  events[1].frame = 2u;
  events[1].command_type = MOL_COMMAND_NOTE_OFF;

  EXPECT_TRUE(mol_engine_load_sequence(engine, &config, events, 2u) == MOL_OK);
  EXPECT_TRUE(mol_engine_submit(engine, &play) == MOL_OK);
  EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, audio, 50u, 2u) == MOL_OK);
  (void)mol_engine_poll_events(engine, emitted, 16u);
  EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, audio, 1u, 2u) == MOL_OK);
  emitted_count = mol_engine_poll_events(engine, emitted, 16u);
  EXPECT_TRUE(emitted_count == 1u);
  EXPECT_TRUE(emitted[0].event_type == MOL_EVENT_NOTE_STARTED && emitted[0].frame == 50u);

  events[1].frame = 0u;
  EXPECT_TRUE(mol_engine_load_sequence(engine, &config, events, 2u) == MOL_ERROR_INVALID_STATE);
  play = make_command(MOL_COMMAND_PLAYBACK_STOP, MOL_FRAME_IMMEDIATE);
  EXPECT_TRUE(mol_engine_submit(engine, &play) == MOL_OK);
  EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, audio, 1u, 2u) == MOL_OK);
  EXPECT_TRUE(mol_engine_load_sequence(engine, &config, events, 2u) == MOL_ERROR_CORRUPT_DATA);
  mol_engine_shutdown(engine);
}

int main(void) {
  test_records_post_music_events();
  test_deterministic_playback();
  test_repeated_playback_resets_signal_state();
  test_time_base_scaling_and_validation();
  if (failures != 0) {
    (void)fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
