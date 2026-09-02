/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mol/mol.h"

#define MOL_RENDER_BLOCK_FRAMES 256u
#define MOL_RENDER_MAX_DURATION_SECONDS 60.0

typedef union mol_render_engine_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[1048576];
} mol_render_engine_storage_t;

typedef struct mol_render_options {
  const char* output_path;
  double duration_seconds;
  double gate_seconds;
  int has_gate;
  uint32_t sample_rate;
  uint32_t channel_count;
  mol_preset_id_t preset;
  uint8_t note;
  float velocity;
} mol_render_options_t;

typedef struct mol_render_stats {
  double square_sum;
  uint64_t sample_count;
  uint64_t clipped_count;
  uint64_t non_finite_count;
  float peak;
} mol_render_stats_t;

static int mol_write_bytes(FILE* file, const void* data, size_t size) {
  return fwrite(data, 1u, size, file) == size;
}

static int mol_write_u16_le(FILE* file, uint16_t value) {
  const uint8_t bytes[2] = {(uint8_t)(value & UINT16_C(0x00FF)),
                            (uint8_t)((value >> 8u) & UINT16_C(0x00FF))};
  return mol_write_bytes(file, bytes, sizeof(bytes));
}

static int mol_write_u32_le(FILE* file, uint32_t value) {
  const uint8_t bytes[4] = {(uint8_t)(value & UINT32_C(0x000000FF)),
                            (uint8_t)((value >> 8u) & UINT32_C(0x000000FF)),
                            (uint8_t)((value >> 16u) & UINT32_C(0x000000FF)),
                            (uint8_t)((value >> 24u) & UINT32_C(0x000000FF))};
  return mol_write_bytes(file, bytes, sizeof(bytes));
}

static int mol_write_wav_header(FILE* file, uint32_t sample_rate, uint16_t channel_count,
                                uint32_t data_bytes) {
  uint32_t byte_rate = sample_rate * (uint32_t)channel_count * 2u;
  uint16_t block_align = (uint16_t)(channel_count * 2u);
  return mol_write_bytes(file, "RIFF", 4u) && mol_write_u32_le(file, 36u + data_bytes) &&
         mol_write_bytes(file, "WAVEfmt ", 8u) && mol_write_u32_le(file, 16u) &&
         mol_write_u16_le(file, 1u) && mol_write_u16_le(file, channel_count) &&
         mol_write_u32_le(file, sample_rate) && mol_write_u32_le(file, byte_rate) &&
         mol_write_u16_le(file, block_align) && mol_write_u16_le(file, 16u) &&
         mol_write_bytes(file, "data", 4u) && mol_write_u32_le(file, data_bytes);
}

static int mol_parse_u32(const char* text, uint32_t* value) {
  char* end = NULL;
  unsigned long parsed;
  errno = 0;
  parsed = strtoul(text, &end, 10);
  if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX) {
    return 0;
  }
  *value = (uint32_t)parsed;
  return 1;
}

static int mol_parse_double(const char* text, double* value) {
  char* end = NULL;
  double parsed;
  errno = 0;
  parsed = strtod(text, &end);
  if (errno != 0 || end == text || *end != '\0' || !isfinite(parsed)) {
    return 0;
  }
  *value = parsed;
  return 1;
}

static int mol_parse_preset(const char* text, mol_preset_id_t* preset) {
  uint32_t numeric;
  if (mol_parse_u32(text, &numeric)) {
    if (numeric < MOL_PRESET_COUNT) {
      *preset = numeric;
      return 1;
    }
    return 0;
  }
  for (uint32_t index = 0u; index < MOL_PRESET_COUNT; ++index) {
    if (strcmp(text, mol_preset_stable_id(index)) == 0) {
      *preset = index;
      return 1;
    }
  }
  return 0;
}

static void mol_print_usage(const char* executable) {
  (void)printf(
      "Usage: %s [--output PATH] [--duration SECONDS] [--sample-rate RATE]\n"
      "          [--gate SECONDS] [--channels 1|2] [--preset ID|NAME]\n"
      "          [--note 0..127] [--velocity 0..1]\n",
      executable);
}

static int mol_parse_options(int argc, char** argv, mol_render_options_t* options) {
  int index;
  options->output_path = "mol-output.wav";
  options->duration_seconds = 2.0;
  options->gate_seconds = 0.0;
  options->has_gate = 0;
  options->sample_rate = 48000u;
  options->channel_count = 2u;
  options->preset = MOL_PRESET_GRAND_PIANO;
  options->note = 60u;
  options->velocity = 0.8f;
  for (index = 1; index < argc; ++index) {
    const char* name = argv[index];
    if (strcmp(name, "--help") == 0) {
      mol_print_usage(argv[0]);
      return 0;
    }
    if (index + 1 >= argc) {
      (void)fprintf(stderr, "Missing value for %s\n", name);
      return -1;
    }
    ++index;
    if (strcmp(name, "--output") == 0) {
      options->output_path = argv[index];
    } else if (strcmp(name, "--duration") == 0) {
      if (!mol_parse_double(argv[index], &options->duration_seconds)) {
        return -1;
      }
    } else if (strcmp(name, "--sample-rate") == 0) {
      if (!mol_parse_u32(argv[index], &options->sample_rate)) {
        return -1;
      }
    } else if (strcmp(name, "--gate") == 0) {
      if (!mol_parse_double(argv[index], &options->gate_seconds)) {
        return -1;
      }
      options->has_gate = 1;
    } else if (strcmp(name, "--channels") == 0) {
      if (!mol_parse_u32(argv[index], &options->channel_count)) {
        return -1;
      }
    } else if (strcmp(name, "--preset") == 0) {
      if (!mol_parse_preset(argv[index], &options->preset)) {
        return -1;
      }
    } else if (strcmp(name, "--note") == 0) {
      uint32_t note;
      if (!mol_parse_u32(argv[index], &note) || note > 127u) {
        return -1;
      }
      options->note = (uint8_t)note;
    } else if (strcmp(name, "--velocity") == 0) {
      double velocity;
      if (!mol_parse_double(argv[index], &velocity) || velocity <= 0.0 || velocity > 1.0) {
        return -1;
      }
      options->velocity = (float)velocity;
    } else {
      (void)fprintf(stderr, "Unknown option: %s\n", name);
      return -1;
    }
  }
  if (options->duration_seconds < 0.1 ||
      options->duration_seconds > MOL_RENDER_MAX_DURATION_SECONDS ||
      (options->sample_rate != 32000u && options->sample_rate != 44100u &&
       options->sample_rate != 48000u) ||
      (options->channel_count != 1u && options->channel_count != 2u) ||
      (options->has_gate &&
       (options->gate_seconds <= 0.0 || options->gate_seconds >= options->duration_seconds))) {
    return -1;
  }
  return 1;
}

static mol_command_t mol_make_note_command(mol_command_type_t type, mol_frame_index_t frame,
                                           uint8_t note, float velocity) {
  mol_command_t command;
  memset(&command, 0, sizeof(command));
  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = type;
  command.target_frame = frame;
  command.gesture_id = 1u;
  command.payload.note.note = note;
  command.payload.note.velocity = velocity;
  return command;
}

static int mol_convert_and_write(FILE* file, const float* samples, uint32_t count,
                                 mol_render_stats_t* stats) {
  uint8_t pcm[MOL_RENDER_BLOCK_FRAMES * 2u * 2u];
  uint32_t index;
  for (index = 0u; index < count; ++index) {
    float sample = samples[index];
    float magnitude;
    if (!isfinite(sample)) {
      sample = 0.0f;
      ++stats->non_finite_count;
    }
    magnitude = fabsf(sample);
    if (magnitude > stats->peak) {
      stats->peak = magnitude;
    }
    if (sample > 1.0f) {
      sample = 1.0f;
      ++stats->clipped_count;
    } else if (sample < -1.0f) {
      sample = -1.0f;
      ++stats->clipped_count;
    }
    stats->square_sum += (double)sample * (double)sample;
    ++stats->sample_count;
    {
      int16_t signed_sample = (int16_t)lrintf(sample * 32767.0f);
      uint16_t encoded = (uint16_t)signed_sample;
      pcm[index * 2u] = (uint8_t)(encoded & UINT16_C(0x00FF));
      pcm[index * 2u + 1u] = (uint8_t)((encoded >> 8u) & UINT16_C(0x00FF));
    }
  }
  return mol_write_bytes(file, pcm, (size_t)count * 2u);
}

static int mol_render_file(const mol_render_options_t* options) {
  static mol_render_engine_storage_t storage;
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  mol_render_stats_t stats = {0};
  mol_command_t note_on;
  mol_command_t note_off;
  mol_command_t preset;
  uint64_t total_frames = (uint64_t)(options->duration_seconds * options->sample_rate + 0.5);
  uint64_t gate_frames = options->has_gate
                             ? (uint64_t)(options->gate_seconds * options->sample_rate + 0.5)
                             : (total_frames * 7u) / 10u;
  uint64_t data_bytes_64 = total_frames * options->channel_count * 2u;
  uint64_t rendered_frames = 0u;
  float samples[MOL_RENDER_BLOCK_FRAMES * 2u];
  FILE* file;
  mol_result_t result;

  if (data_bytes_64 > UINT32_MAX - 36u) {
    (void)fprintf(stderr, "Requested WAV file is too large\n");
    return 1;
  }
  config.sample_rate = options->sample_rate;
  config.channel_count = options->channel_count;
  config.command_capacity = 64u;
  config.event_capacity = 64u;
  if (mol_engine_query_memory(&config) > sizeof(storage.bytes)) {
    (void)fprintf(stderr, "Engine memory requirement exceeds renderer capacity\n");
    return 1;
  }
  result = mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine);
  if (result != MOL_OK) {
    (void)fprintf(stderr, "Engine initialization failed: %s\n", mol_result_string(result));
    return 1;
  }

  memset(&preset, 0, sizeof(preset));
  preset.struct_size = (uint32_t)sizeof(preset);
  preset.api_version = MOL_API_VERSION;
  preset.command_type = MOL_COMMAND_SET_PRESET;
  preset.target_frame = 0u;
  preset.payload.preset.preset = options->preset;
  preset.payload.preset.hard_switch = 1u;
  note_on = mol_make_note_command(MOL_COMMAND_NOTE_ON, 0u, options->note, options->velocity);
  note_off = mol_make_note_command(MOL_COMMAND_NOTE_OFF, (mol_frame_index_t)gate_frames,
                                   options->note, options->velocity);
  if (mol_engine_submit(engine, &preset) != MOL_OK ||
      mol_engine_submit(engine, &note_on) != MOL_OK ||
      mol_engine_submit(engine, &note_off) != MOL_OK) {
    (void)fprintf(stderr, "Could not schedule the render sequence\n");
    mol_engine_shutdown(engine);
    return 1;
  }

  file = fopen(options->output_path, "wb");
  if (file == NULL) {
    (void)fprintf(stderr, "Could not open output file: %s\n", options->output_path);
    mol_engine_shutdown(engine);
    return 1;
  }
  if (!mol_write_wav_header(file, options->sample_rate, (uint16_t)options->channel_count,
                            (uint32_t)data_bytes_64)) {
    (void)fprintf(stderr, "Could not write WAV header\n");
    (void)fclose(file);
    mol_engine_shutdown(engine);
    return 1;
  }

  while (rendered_frames < total_frames) {
    uint64_t remaining = total_frames - rendered_frames;
    uint32_t block_frames =
        remaining > MOL_RENDER_BLOCK_FRAMES ? MOL_RENDER_BLOCK_FRAMES : (uint32_t)remaining;
    uint32_t sample_count = block_frames * options->channel_count;
    result =
        mol_engine_render_interleaved_f32(engine, samples, block_frames, options->channel_count);
    if (result != MOL_OK || !mol_convert_and_write(file, samples, sample_count, &stats)) {
      (void)fprintf(stderr, "Audio rendering or WAV writing failed\n");
      (void)fclose(file);
      mol_engine_shutdown(engine);
      return 1;
    }
    rendered_frames += block_frames;
  }
  if (fclose(file) != 0) {
    (void)fprintf(stderr, "Could not finalize output file\n");
    mol_engine_shutdown(engine);
    return 1;
  }
  mol_engine_shutdown(engine);
  (void)printf("output=%s\n", options->output_path);
  (void)printf("preset=%s\n", mol_preset_stable_id(options->preset));
  (void)printf("duration_seconds=%.6f\n", (double)total_frames / options->sample_rate);
  (void)printf("gate_seconds=%.6f\n", (double)gate_frames / options->sample_rate);
  (void)printf("peak=%.8f\n", stats.peak);
  (void)printf("rms=%.8f\n", sqrt(stats.square_sum / (double)stats.sample_count));
  (void)printf("clipped_samples=%llu\n", (unsigned long long)stats.clipped_count);
  (void)printf("non_finite_samples=%llu\n", (unsigned long long)stats.non_finite_count);
  (void)printf("underruns=0\n");
  return stats.non_finite_count == 0u && stats.peak > 0.0f ? 0 : 1;
}

int main(int argc, char** argv) {
  mol_render_options_t options;
  int parse_result = mol_parse_options(argc, argv, &options);
  if (parse_result <= 0) {
    if (parse_result < 0) {
      mol_print_usage(argv[0]);
      return 2;
    }
    return 0;
  }
  return mol_render_file(&options);
}
