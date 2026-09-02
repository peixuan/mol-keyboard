// SPDX-License-Identifier: Apache-2.0
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "mol_platform/audio_runtime.h"

enum {
  TEST_SAMPLE_RATE = 48000,
  TEST_CHANNELS = 2,
  TEST_TOTAL_FRAMES = 48000,
  TEST_MAX_CALLBACK_FRAMES = 257
};

static int submit_note(mol_platform_audio_runtime_t* runtime, uint32_t command_type, uint8_t note,
                       float velocity) {
  mol_command_t command;
  memset(&command, 0, sizeof(command));
  command.struct_size = sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = command_type;
  command.target_frame = MOL_FRAME_IMMEDIATE;
  command.gesture_id = 91U;
  command.payload.note.note = note;
  command.payload.note.velocity = velocity;
  return mol_platform_audio_submit(runtime, &command) == MOL_OK ? 0 : 1;
}

int main(void) {
  static const uint32_t callback_sizes[] = {1U, 17U, 64U, 127U, 257U, 31U};
  static mol_platform_audio_runtime_t runtime;
  static float output[TEST_MAX_CALLBACK_FRAMES * TEST_CHANNELS];
  uint32_t frame_offset = 0U;
  uint32_t callback_index = 0U;
  uint32_t crossings = 0U;
  uint32_t non_finite = 0U;
  float previous = 0.0F;
  float peak = 0.0F;

  if (mol_platform_audio_init(NULL, TEST_SAMPLE_RATE, TEST_CHANNELS) !=
          MOL_ERROR_INVALID_ARGUMENT ||
      mol_platform_audio_init(&runtime, TEST_SAMPLE_RATE, TEST_CHANNELS) != MOL_OK ||
      submit_note(&runtime, MOL_COMMAND_NOTE_ON, 60U, 0.8F) != 0) {
    fputs("platform runtime initialization failed\n", stderr);
    return 1;
  }

  while (frame_offset < TEST_TOTAL_FRAMES) {
    uint32_t frames =
        callback_sizes[callback_index % (sizeof(callback_sizes) / sizeof(callback_sizes[0]))];
    uint32_t frame;
    if (frames > TEST_TOTAL_FRAMES - frame_offset) {
      frames = TEST_TOTAL_FRAMES - frame_offset;
    }
    if (mol_platform_audio_render_f32(&runtime, output, frames) != MOL_OK) {
      fputs("platform runtime render failed\n", stderr);
      return 1;
    }
    for (frame = 0U; frame < frames; ++frame) {
      const float left = output[frame * TEST_CHANNELS];
      uint32_t channel;
      for (channel = 0U; channel < TEST_CHANNELS; ++channel) {
        const float sample = output[frame * TEST_CHANNELS + channel];
        if (!isfinite(sample)) {
          ++non_finite;
        }
        if (fabsf(sample) > peak) {
          peak = fabsf(sample);
        }
      }
      if (frame_offset + frame >= TEST_SAMPLE_RATE / 10U && previous <= 0.0F && left > 0.0F) {
        ++crossings;
      }
      previous = left;
    }
    frame_offset += frames;
    ++callback_index;
  }

  if (submit_note(&runtime, MOL_COMMAND_NOTE_OFF, 60U, 0.0F) != 0) {
    fputs("platform runtime note-off failed\n", stderr);
    return 1;
  }
  mol_platform_audio_shutdown(&runtime);

  {
    const double analysis_seconds = 0.9;
    const double frequency = (double)crossings / analysis_seconds;
    printf("callbacks=%u frequency_hz=%.4f peak=%.8f non_finite=%u\n", callback_index, frequency,
           (double)peak, non_finite);
    if (callback_index < 100U || fabs(frequency - 261.625565) >= 1.0 || peak <= 0.01F ||
        non_finite != 0U || runtime.engine != NULL) {
      fputs("platform runtime conformance failed\n", stderr);
      return 1;
    }
  }
  return 0;
}
