/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mol/mol.h"
#include "sha256.h"

#define MOL_RENDER_BLOCK_FRAMES 256u
#define MOL_RENDER_MAX_DURATION_SECONDS 3600.0

typedef enum mol_wav_format {
  MOL_WAV_PCM16 = 0,
  MOL_WAV_PCM24 = 1,
  MOL_WAV_FLOAT32 = 2
} mol_wav_format_t;

typedef enum mol_render_quality { MOL_RENDER_NORMAL = 0, MOL_RENDER_HIGH = 1 } mol_render_quality_t;

typedef union mol_render_engine_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[1048576];
} mol_render_engine_storage_t;

typedef struct mol_render_options {
  const char* input_path;
  const char* output_path;
  const char* report_path;
  double duration_seconds;
  double tail_seconds;
  double gate_seconds;
  int duration_explicit;
  int has_gate;
  uint32_t sample_rate;
  uint32_t channel_count;
  mol_preset_id_t preset;
  mol_wav_format_t format;
  mol_render_quality_t quality;
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

typedef struct mol_sequence_buffer {
  mol_sequence_config_t config;
  mol_sequence_event_t* events;
  uint32_t count;
  uint32_t capacity;
} mol_sequence_buffer_t;

static int mol_write_bytes(FILE* file, mol_sha256_t* hash, const void* data, size_t size) {
  if (fwrite(data, 1u, size, file) != size) return 0;
  mol_sha256_update(hash, data, size);
  return 1;
}

static int mol_write_u16_le(FILE* file, mol_sha256_t* hash, uint16_t value) {
  const uint8_t bytes[2] = {(uint8_t)value, (uint8_t)(value >> 8u)};
  return mol_write_bytes(file, hash, bytes, sizeof(bytes));
}

static int mol_write_u32_le(FILE* file, mol_sha256_t* hash, uint32_t value) {
  const uint8_t bytes[4] = {(uint8_t)value, (uint8_t)(value >> 8u), (uint8_t)(value >> 16u),
                            (uint8_t)(value >> 24u)};
  return mol_write_bytes(file, hash, bytes, sizeof(bytes));
}

static int mol_write_wav_header(FILE* file, mol_sha256_t* hash, uint32_t sample_rate,
                                uint16_t channel_count, mol_wav_format_t format,
                                uint32_t data_bytes) {
  uint16_t bits = format == MOL_WAV_PCM16 ? 16u : (format == MOL_WAV_PCM24 ? 24u : 32u);
  uint16_t encoding = format == MOL_WAV_FLOAT32 ? 3u : 1u;
  uint16_t bytes_per_sample = bits / 8u;
  uint32_t byte_rate = sample_rate * (uint32_t)channel_count * bytes_per_sample;
  uint16_t block_align = (uint16_t)(channel_count * bytes_per_sample);
  return mol_write_bytes(file, hash, "RIFF", 4u) &&
         mol_write_u32_le(file, hash, 36u + data_bytes) &&
         mol_write_bytes(file, hash, "WAVEfmt ", 8u) && mol_write_u32_le(file, hash, 16u) &&
         mol_write_u16_le(file, hash, encoding) && mol_write_u16_le(file, hash, channel_count) &&
         mol_write_u32_le(file, hash, sample_rate) && mol_write_u32_le(file, hash, byte_rate) &&
         mol_write_u16_le(file, hash, block_align) && mol_write_u16_le(file, hash, bits) &&
         mol_write_bytes(file, hash, "data", 4u) && mol_write_u32_le(file, hash, data_bytes);
}

static int mol_parse_u32(const char* text, uint32_t* value) {
  char* end = NULL;
  unsigned long parsed;
  errno = 0;
  parsed = strtoul(text, &end, 10);
  if (errno != 0 || end == text || *end != '\0' || text[0] == '-' || parsed > UINT32_MAX) return 0;
  *value = (uint32_t)parsed;
  return 1;
}

static int mol_parse_double(const char* text, double* value) {
  char* end = NULL;
  double parsed;
  errno = 0;
  parsed = strtod(text, &end);
  if (errno != 0 || end == text || *end != '\0' || !isfinite(parsed)) return 0;
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
      "Usage: %s [INPUT.molseq] [--output PATH] [--report PATH]\n"
      "          [--duration SECONDS] [--tail SECONDS] [--sample-rate RATE]\n"
      "          [--channels 1|2] [--format pcm16|pcm24|float32]\n"
      "          [--quality normal|high]\n"
      "Manual note mode: [--gate SECONDS] [--preset ID|NAME]\n"
      "                  [--note 0..127] [--velocity 0..1]\n",
      executable);
}

static int mol_parse_options(int argc, char** argv, mol_render_options_t* options) {
  int index;
  memset(options, 0, sizeof(*options));
  options->output_path = "mol-output.wav";
  options->duration_seconds = 2.0;
  options->tail_seconds = 2.0;
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
    if (name[0] != '-') {
      if (options->input_path != NULL) return -1;
      options->input_path = name;
      continue;
    }
    if (index + 1 >= argc) {
      (void)fprintf(stderr, "Missing value for %s\n", name);
      return -1;
    }
    ++index;
    if (strcmp(name, "--input") == 0) {
      options->input_path = argv[index];
    } else if (strcmp(name, "--output") == 0) {
      options->output_path = argv[index];
    } else if (strcmp(name, "--report") == 0) {
      options->report_path = argv[index];
    } else if (strcmp(name, "--duration") == 0) {
      if (!mol_parse_double(argv[index], &options->duration_seconds)) return -1;
      options->duration_explicit = 1;
    } else if (strcmp(name, "--tail") == 0) {
      if (!mol_parse_double(argv[index], &options->tail_seconds)) return -1;
    } else if (strcmp(name, "--sample-rate") == 0) {
      if (!mol_parse_u32(argv[index], &options->sample_rate)) return -1;
    } else if (strcmp(name, "--gate") == 0) {
      if (!mol_parse_double(argv[index], &options->gate_seconds)) return -1;
      options->has_gate = 1;
    } else if (strcmp(name, "--channels") == 0) {
      if (!mol_parse_u32(argv[index], &options->channel_count)) return -1;
    } else if (strcmp(name, "--preset") == 0) {
      if (!mol_parse_preset(argv[index], &options->preset)) return -1;
    } else if (strcmp(name, "--note") == 0) {
      uint32_t note;
      if (!mol_parse_u32(argv[index], &note) || note > 127u) return -1;
      options->note = (uint8_t)note;
    } else if (strcmp(name, "--velocity") == 0) {
      double velocity;
      if (!mol_parse_double(argv[index], &velocity) || velocity <= 0.0 || velocity > 1.0) return -1;
      options->velocity = (float)velocity;
    } else if (strcmp(name, "--format") == 0) {
      if (strcmp(argv[index], "pcm16") == 0)
        options->format = MOL_WAV_PCM16;
      else if (strcmp(argv[index], "pcm24") == 0)
        options->format = MOL_WAV_PCM24;
      else if (strcmp(argv[index], "float32") == 0)
        options->format = MOL_WAV_FLOAT32;
      else
        return -1;
    } else if (strcmp(name, "--quality") == 0) {
      if (strcmp(argv[index], "normal") == 0)
        options->quality = MOL_RENDER_NORMAL;
      else if (strcmp(argv[index], "high") == 0)
        options->quality = MOL_RENDER_HIGH;
      else
        return -1;
    } else {
      (void)fprintf(stderr, "Unknown option: %s\n", name);
      return -1;
    }
  }
  if (options->duration_seconds < 0.1 ||
      options->duration_seconds > MOL_RENDER_MAX_DURATION_SECONDS || options->tail_seconds < 0.0 ||
      options->tail_seconds > 30.0 || options->sample_rate < 8000u ||
      options->sample_rate > 192000u ||
      (options->quality == MOL_RENDER_HIGH && options->sample_rate > 96000u) ||
      (options->channel_count != 1u && options->channel_count != 2u) ||
      (options->has_gate &&
       (options->gate_seconds <= 0.0 ||
        (options->duration_explicit && options->gate_seconds >= options->duration_seconds))))
    return -1;
  return 1;
}

static size_t mol_sequence_read_file(void* user_data, uint8_t* data, size_t capacity) {
  return fread(data, 1u, capacity, (FILE*)user_data);
}

static mol_result_t mol_capture_sequence_event(void* user_data, const mol_sequence_event_t* event) {
  mol_sequence_buffer_t* sequence = (mol_sequence_buffer_t*)user_data;
  if (sequence->count >= sequence->capacity) return MOL_ERROR_BUFFER_TOO_SMALL;
  sequence->events[sequence->count++] = *event;
  return MOL_OK;
}

static int mol_load_sequence(const char* path, uint32_t capacity, mol_sequence_buffer_t* sequence) {
  mol_sequence_callbacks_t callbacks;
  mol_result_t result;
  FILE* file = fopen(path, "rb");
  memset(sequence, 0, sizeof(*sequence));
  if (file == NULL) return 0;
  sequence->events = (mol_sequence_event_t*)malloc(sizeof(*sequence->events) * capacity);
  if (sequence->events == NULL) {
    (void)fclose(file);
    return 0;
  }
  sequence->capacity = capacity;
  sequence->config.struct_size = (uint32_t)sizeof(sequence->config);
  sequence->config.api_version = MOL_API_VERSION;
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.struct_size = (uint32_t)sizeof(callbacks);
  callbacks.api_version = MOL_API_VERSION;
  callbacks.on_event = mol_capture_sequence_event;
  callbacks.user_data = sequence;
  result = mol_sequence_read_stream(mol_sequence_read_file, file, &sequence->config, &callbacks);
  if (fclose(file) != 0 && result == MOL_OK) result = MOL_ERROR_IO;
  if (result != MOL_OK) {
    (void)fprintf(stderr, "Could not read sequence: %s\n", mol_result_string(result));
    free(sequence->events);
    sequence->events = NULL;
    return 0;
  }
  return 1;
}

static mol_command_t mol_make_command(mol_command_type_t type, mol_frame_index_t frame) {
  mol_command_t command;
  memset(&command, 0, sizeof(command));
  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = type;
  command.target_frame = frame;
  return command;
}

static int mol_convert_and_write(FILE* file, mol_sha256_t* hash, const float* samples,
                                 uint32_t count, mol_wav_format_t format,
                                 mol_render_stats_t* stats) {
  uint8_t encoded[MOL_RENDER_BLOCK_FRAMES * 2u * 4u];
  uint32_t bytes_per_sample = format == MOL_WAV_PCM16 ? 2u : (format == MOL_WAV_PCM24 ? 3u : 4u);
  for (uint32_t index = 0u; index < count; ++index) {
    float sample = samples[index];
    if (!isfinite(sample)) {
      sample = 0.0f;
      ++stats->non_finite_count;
    }
    if (fabsf(sample) > stats->peak) stats->peak = fabsf(sample);
    if (sample > 1.0f) {
      sample = 1.0f;
      ++stats->clipped_count;
    } else if (sample < -1.0f) {
      sample = -1.0f;
      ++stats->clipped_count;
    }
    stats->square_sum += (double)sample * sample;
    ++stats->sample_count;
    if (format == MOL_WAV_PCM16) {
      int32_t value = (int32_t)lrintf(sample * (sample < 0.0f ? 32768.0f : 32767.0f));
      uint16_t bits = (uint16_t)(int16_t)value;
      size_t offset = (size_t)index * 2u;
      encoded[offset] = (uint8_t)bits;
      encoded[offset + 1u] = (uint8_t)(bits >> 8u);
    } else if (format == MOL_WAV_PCM24) {
      int32_t value = (int32_t)lrintf(sample * (sample < 0.0f ? 8388608.0f : 8388607.0f));
      uint32_t bits = (uint32_t)value;
      size_t offset = (size_t)index * 3u;
      encoded[offset] = (uint8_t)bits;
      encoded[offset + 1u] = (uint8_t)(bits >> 8u);
      encoded[offset + 2u] = (uint8_t)(bits >> 16u);
    } else {
      uint32_t bits;
      size_t offset = (size_t)index * 4u;
      memcpy(&bits, &sample, sizeof(bits));
      encoded[offset] = (uint8_t)bits;
      encoded[offset + 1u] = (uint8_t)(bits >> 8u);
      encoded[offset + 2u] = (uint8_t)(bits >> 16u);
      encoded[offset + 3u] = (uint8_t)(bits >> 24u);
    }
  }
  return mol_write_bytes(file, hash, encoded, (size_t)count * bytes_per_sample);
}

static void mol_digest_hex(const uint8_t digest[32], char output[65]) {
  static const char digits[] = "0123456789abcdef";
  for (uint32_t index = 0u; index < 32u; ++index) {
    size_t offset = (size_t)index * 2u;
    output[offset] = digits[digest[index] >> 4u];
    output[offset + 1u] = digits[digest[index] & 0x0Fu];
  }
  output[64] = '\0';
}

static const char* mol_format_name(mol_wav_format_t format) {
  return format == MOL_WAV_PCM16 ? "pcm16" : (format == MOL_WAV_PCM24 ? "pcm24" : "float32");
}

static void mol_write_json_string(FILE* file, const char* text) {
  (void)fputc('"', file);
  while (*text != '\0') {
    unsigned char value = (unsigned char)*text++;
    if (value == '"' || value == '\\') (void)fputc('\\', file);
    if (value >= 0x20u)
      (void)fputc(value, file);
    else
      (void)fprintf(file, "\\u%04x", value);
  }
  (void)fputc('"', file);
}

static int mol_write_report(const char* path, const mol_render_options_t* options,
                            uint64_t total_frames, const mol_render_stats_t* stats,
                            const char* digest) {
  FILE* file = fopen(path, "wb");
  double rms = sqrt(stats->square_sum / (double)stats->sample_count);
  if (file == NULL) return 0;
  (void)fprintf(file, "{\n  \"channels\": %u,\n  \"clipped_sample_count\": %llu,\n",
                options->channel_count, (unsigned long long)stats->clipped_count);
  (void)fprintf(file, "  \"duration_seconds\": %.9f,\n  \"format\": \"%s\",\n",
                (double)total_frames / options->sample_rate, mol_format_name(options->format));
  (void)fprintf(file, "  \"nan_inf_count\": %llu,\n  \"output\": ",
                (unsigned long long)stats->non_finite_count);
  mol_write_json_string(file, options->output_path);
  (void)fprintf(file, ",\n  \"peak\": %.9g,\n  \"quality\": \"%s\",\n", (double)stats->peak,
                options->quality == MOL_RENDER_HIGH ? "high" : "normal");
  (void)fprintf(file,
                "  \"rms\": %.9g,\n  \"sample_rate\": %u,\n  \"sha256\": \"%s\",\n"
                "  \"underrun_count\": 0\n}\n",
                rms, options->sample_rate, digest);
  return fclose(file) == 0;
}

static int mol_render_file(mol_render_options_t* options) {
  static mol_render_engine_storage_t storage;
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  mol_sequence_buffer_t sequence;
  mol_render_stats_t stats = {0};
  mol_sha256_t hash;
  mol_command_t command;
  uint32_t internal_rate = options->sample_rate;
  uint32_t oversample = options->quality == MOL_RENDER_HIGH ? 2u : 1u;
  uint64_t total_frames;
  uint64_t gate_frames;
  uint64_t rendered_frames = 0u;
  uint32_t bytes_per_sample =
      options->format == MOL_WAV_PCM16 ? 2u : (options->format == MOL_WAV_PCM24 ? 3u : 4u);
  uint64_t data_bytes_64;
  float rendered[MOL_RENDER_BLOCK_FRAMES * 2u * 2u];
  float output[MOL_RENDER_BLOCK_FRAMES * 2u];
  uint8_t digest[32];
  char digest_text[65];
  char* default_report = NULL;
  FILE* file = NULL;
  mol_result_t result;
  memset(&sequence, 0, sizeof(sequence));
  if (oversample == 2u) internal_rate *= 2u;
  config.sample_rate = internal_rate;
  config.channel_count = options->channel_count;
  config.command_capacity = 64u;
  config.event_capacity = 64u;
  if (options->input_path != NULL) {
    if (!mol_load_sequence(options->input_path, config.sequence_capacity, &sequence)) return 1;
    if (!options->duration_explicit) {
      uint64_t final_frame = sequence.count == 0u ? 0u : sequence.events[sequence.count - 1u].frame;
      options->duration_seconds =
          (double)final_frame / sequence.config.time_base + options->tail_seconds;
      if (options->duration_seconds < 0.1) options->duration_seconds = 0.1;
    }
  }
  if (options->duration_seconds > MOL_RENDER_MAX_DURATION_SECONDS) {
    (void)fprintf(stderr, "Render duration exceeds %.0f seconds\n",
                  MOL_RENDER_MAX_DURATION_SECONDS);
    free(sequence.events);
    return 1;
  }
  total_frames = (uint64_t)(options->duration_seconds * options->sample_rate + 0.5);
  gate_frames = options->has_gate ? (uint64_t)(options->gate_seconds * internal_rate + 0.5)
                                  : (total_frames * oversample * 7u) / 10u;
  data_bytes_64 = total_frames * options->channel_count * bytes_per_sample;
  if (data_bytes_64 > UINT32_MAX - 36u ||
      mol_engine_query_memory(&config) > sizeof(storage.bytes)) {
    (void)fprintf(stderr, "Requested render exceeds WAV or engine memory limits\n");
    free(sequence.events);
    return 1;
  }
  result = mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine);
  if (result != MOL_OK) {
    (void)fprintf(stderr, "Engine initialization failed: %s\n", mol_result_string(result));
    free(sequence.events);
    return 1;
  }
  if (options->input_path != NULL) {
    result = mol_engine_load_sequence(engine, &sequence.config, sequence.events, sequence.count);
    if (result == MOL_OK) {
      command = mol_make_command(MOL_COMMAND_PLAYBACK_START, 0u);
      result = mol_engine_submit(engine, &command);
    }
  } else {
    command = mol_make_command(MOL_COMMAND_SET_PRESET, 0u);
    command.payload.preset.preset = options->preset;
    command.payload.preset.hard_switch = 1u;
    result = mol_engine_submit(engine, &command);
    command = mol_make_command(MOL_COMMAND_NOTE_ON, 0u);
    command.gesture_id = 1u;
    command.payload.note.note = options->note;
    command.payload.note.velocity = options->velocity;
    if (result == MOL_OK) result = mol_engine_submit(engine, &command);
    command = mol_make_command(MOL_COMMAND_NOTE_OFF, gate_frames);
    command.gesture_id = 1u;
    command.payload.note.note = options->note;
    if (result == MOL_OK) result = mol_engine_submit(engine, &command);
  }
  free(sequence.events);
  if (result != MOL_OK) {
    (void)fprintf(stderr, "Could not schedule render: %s\n", mol_result_string(result));
    mol_engine_shutdown(engine);
    return 1;
  }
  file = fopen(options->output_path, "wb");
  mol_sha256_init(&hash);
  if (file == NULL ||
      !mol_write_wav_header(file, &hash, options->sample_rate, (uint16_t)options->channel_count,
                            options->format, (uint32_t)data_bytes_64)) {
    (void)fprintf(stderr, "Could not create WAV output\n");
    if (file != NULL) (void)fclose(file);
    (void)remove(options->output_path);
    mol_engine_shutdown(engine);
    return 1;
  }
  while (rendered_frames < total_frames) {
    uint64_t remaining = total_frames - rendered_frames;
    uint32_t block =
        remaining > MOL_RENDER_BLOCK_FRAMES ? MOL_RENDER_BLOCK_FRAMES : (uint32_t)remaining;
    result = mol_engine_render_interleaved_f32(engine, rendered, block * oversample,
                                               options->channel_count);
    if (result != MOL_OK) break;
    if (oversample == 1u) {
      memcpy(output, rendered, sizeof(float) * block * options->channel_count);
    } else {
      for (uint32_t frame = 0u; frame < block; ++frame) {
        for (uint32_t channel = 0u; channel < options->channel_count; ++channel) {
          size_t first = ((size_t)frame * 2u) * options->channel_count + channel;
          size_t second = first + options->channel_count;
          output[(size_t)frame * options->channel_count + channel] =
              (rendered[first] + rendered[second]) * 0.5f;
        }
      }
    }
    if (!mol_convert_and_write(file, &hash, output, block * options->channel_count, options->format,
                               &stats)) {
      result = MOL_ERROR_IO;
      break;
    }
    rendered_frames += block;
  }
  if (fclose(file) != 0 && result == MOL_OK) result = MOL_ERROR_IO;
  mol_engine_shutdown(engine);
  if (result != MOL_OK) {
    (void)remove(options->output_path);
    (void)fprintf(stderr, "Audio rendering or WAV writing failed: %s\n", mol_result_string(result));
    return 1;
  }
  mol_sha256_finish(&hash, digest);
  mol_digest_hex(digest, digest_text);
  if (options->report_path == NULL) {
    size_t length = strlen(options->output_path);
    default_report = (char*)malloc(length + 6u);
    if (default_report == NULL) return 1;
    memcpy(default_report, options->output_path, length);
    memcpy(default_report + length, ".json", 6u);
    options->report_path = default_report;
  }
  if (!mol_write_report(options->report_path, options, total_frames, &stats, digest_text)) {
    (void)fprintf(stderr, "Could not write JSON report: %s\n", options->report_path);
    free(default_report);
    return 1;
  }
  (void)printf("output=%s\n", options->output_path);
  (void)printf("report=%s\n", options->report_path);
  (void)printf("duration_seconds=%.9f\n", (double)total_frames / options->sample_rate);
  (void)printf("peak=%.9g\n", (double)stats.peak);
  (void)printf("rms=%.9g\n", sqrt(stats.square_sum / (double)stats.sample_count));
  (void)printf("clipped_samples=%llu\n", (unsigned long long)stats.clipped_count);
  (void)printf("non_finite_samples=%llu\n", (unsigned long long)stats.non_finite_count);
  (void)printf("underruns=0\nsha256=%s\n", digest_text);
  free(default_report);
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
