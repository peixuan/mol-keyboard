/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "mol/mol.h"

#define SAMPLE_RATE 48000u
#define CHANNEL_COUNT 2u
#define BLOCK_FRAMES 1024u
#define ACTIVE_GESTURES 16u
#define TOTAL_SECONDS 1800u
#define TOTAL_FRAMES ((uint64_t)SAMPLE_RATE * TOTAL_SECONDS)

typedef union test_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[2097152];
} test_storage_t;

static test_storage_t storage;
static float output[BLOCK_FRAMES * CHANNEL_COUNT];
static int failures;

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
  command.source_id = UINT32_C(0x53545253);
  return command;
}

static uint32_t next_random(uint32_t* state) {
  uint32_t value = *state;
  value ^= value << 13u;
  value ^= value >> 17u;
  value ^= value << 5u;
  *state = value;
  return value;
}

static void submit(mol_engine_t* engine, const mol_command_t* command) {
  EXPECT_TRUE(mol_engine_submit(engine, command) == MOL_OK);
}

static void submit_note(mol_engine_t* engine, mol_command_type_t type, mol_gesture_id_t gesture,
                        uint8_t note, float velocity) {
  mol_command_t command = command_now(type);
  command.gesture_id = gesture;
  command.payload.note.note = note;
  command.payload.note.velocity = velocity;
  submit(engine, &command);
}

int main(void) {
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  mol_gesture_id_t gestures[ACTIVE_GESTURES] = {0u};
  uint8_t notes[ACTIVE_GESTURES] = {0u};
  mol_event_t events[512];
  uint32_t gesture_head = 0u;
  uint32_t gesture_count = 0u;
  uint32_t random = UINT32_C(0x4D4F4C31);
  uint64_t current_frame = 0u;
  uint64_t next_note_frame = 0u;
  uint64_t next_preset_frame = 0u;
  uint64_t next_record_start = 0u;
  uint64_t record_stop_frame = 0u;
  uint64_t next_queue_frame = (uint64_t)SAMPLE_RATE * 60u;
  uint64_t non_finite = 0u;
  uint64_t event_count = 0u;
  uint32_t note_count = 0u;
  uint32_t preset_count = 0u;
  uint32_t recording_count = 0u;
  uint32_t queue_cycles = 0u;
  int recording = 0;
  float peak = 0.0f;
  const clock_t started = clock();

  config.sample_rate = SAMPLE_RATE;
  config.channel_count = CHANNEL_COUNT;
  config.max_voices = 32u;
  config.command_capacity = 256u;
  config.event_capacity = 512u;
  config.sequence_capacity = 2048u;
  EXPECT_TRUE(mol_engine_query_memory(&config) <= sizeof(storage.bytes));
  EXPECT_TRUE(mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) == MOL_OK);
  if (engine == NULL) return 1;

  {
    mol_command_t start = command_now(MOL_COMMAND_TRANSPORT_START);
    submit(engine, &start);
  }
  while (current_frame < TOTAL_FRAMES && failures == 0) {
    const uint32_t frames = TOTAL_FRAMES - current_frame < BLOCK_FRAMES
                                ? (uint32_t)(TOTAL_FRAMES - current_frame)
                                : BLOCK_FRAMES;
    int validate_recording = 0;

    if (current_frame >= next_queue_frame) {
      mol_command_t gain = command_now(MOL_COMMAND_SET_MASTER_GAIN);
      gain.payload.scalar.value = 0.8f;
      for (uint32_t index = 0u; index < 255u; ++index) submit(engine, &gain);
      ++queue_cycles;
      next_queue_frame += (uint64_t)SAMPLE_RATE * 60u;
    } else {
      if (current_frame >= next_note_frame) {
        if (gesture_count == ACTIVE_GESTURES) {
          submit_note(engine, MOL_COMMAND_NOTE_OFF, gestures[gesture_head], notes[gesture_head],
                      0.0f);
          gesture_head = (gesture_head + 1u) % ACTIVE_GESTURES;
          --gesture_count;
        }
        {
          const uint32_t tail = (gesture_head + gesture_count) % ACTIVE_GESTURES;
          const mol_gesture_id_t gesture = (mol_gesture_id_t)note_count + 1u;
          const uint8_t note = (uint8_t)(36u + next_random(&random) % 60u);
          submit_note(engine, MOL_COMMAND_NOTE_ON, gesture, note,
                      0.35f + (float)(next_random(&random) % 601u) / 1000.0f);
          gestures[tail] = gesture;
          notes[tail] = note;
          ++gesture_count;
          ++note_count;
        }
        next_note_frame += SAMPLE_RATE / 32u;
      }
      if (current_frame >= next_preset_frame) {
        mol_command_t preset = command_now(MOL_COMMAND_SET_PRESET);
        preset.payload.preset.preset = preset_count % MOL_PRESET_COUNT;
        preset.payload.preset.hard_switch = (preset_count % 7u) == 0u ? 1u : 0u;
        submit(engine, &preset);
        ++preset_count;
        next_preset_frame += (uint64_t)SAMPLE_RATE * 5u;
      }
      if (recording && current_frame >= record_stop_frame) {
        mol_command_t stop = command_now(MOL_COMMAND_RECORD_STOP);
        submit(engine, &stop);
        recording = 0;
        validate_recording = 1;
      } else if (!recording && current_frame >= next_record_start) {
        mol_command_t start = command_now(MOL_COMMAND_RECORD_START);
        submit(engine, &start);
        recording = 1;
        record_stop_frame = current_frame + SAMPLE_RATE;
        next_record_start += (uint64_t)SAMPLE_RATE * 15u;
      }
    }

    EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, output, frames, CHANNEL_COUNT) == MOL_OK);
    for (uint32_t index = 0u; index < frames * CHANNEL_COUNT; ++index) {
      const float magnitude = fabsf(output[index]);
      if (!isfinite(output[index])) ++non_finite;
      if (magnitude > peak) peak = magnitude;
    }
    event_count += mol_engine_poll_events(engine, events, 512u);
    if (validate_recording) {
      mol_engine_state_t state = {0};
      state.struct_size = (uint32_t)sizeof(state);
      EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
      EXPECT_TRUE(state.recording == 0u && state.recording_event_count != 0u &&
                  state.recording_event_count <= config.sequence_capacity);
      ++recording_count;
    }
    current_frame += frames;
  }

  {
    mol_command_t sound_off = command_now(MOL_COMMAND_ALL_SOUND_OFF);
    mol_command_t transport_stop = command_now(MOL_COMMAND_TRANSPORT_STOP);
    mol_engine_state_t state = {0};
    const double elapsed = (double)(clock() - started) / CLOCKS_PER_SEC;
    const double realtime_ratio = (double)TOTAL_SECONDS / elapsed;
    submit(engine, &sound_off);
    submit(engine, &transport_stop);
    EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, output, 1u, CHANNEL_COUNT) == MOL_OK);
    state.struct_size = (uint32_t)sizeof(state);
    EXPECT_TRUE(mol_engine_get_state(engine, &state) == MOL_OK);
    EXPECT_TRUE(state.current_frame == TOTAL_FRAMES + 1u);
    EXPECT_TRUE(state.transport_frame == TOTAL_FRAMES);
    EXPECT_TRUE(state.transport_running == 0u && state.active_voices == 0u);
    EXPECT_TRUE(non_finite == 0u && peak > 0.01f && peak <= 1.0f);
    EXPECT_TRUE(note_count >= 57000u && preset_count == 360u && recording_count == 120u &&
                queue_cycles == 29u);
    EXPECT_TRUE(realtime_ratio >= 4.0);
    (void)printf(
        "simulated_seconds=%u wall_seconds=%.3f realtime_ratio=%.2f notes=%u presets=%u "
        "recordings=%u queue_cycles=%u events=%llu peak=%.6f non_finite=%llu\n",
        TOTAL_SECONDS, elapsed, realtime_ratio, note_count, preset_count, recording_count,
        queue_cycles, (unsigned long long)event_count, (double)peak,
        (unsigned long long)non_finite);
  }
  mol_engine_shutdown(engine);
  return failures == 0 ? 0 : 1;
}
