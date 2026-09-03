/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sha256.h"

#define MOL_LATENCY_MAX_DATA_BYTES (UINT32_C(512) * UINT32_C(1024) * UINT32_C(1024))
#define MOL_LATENCY_MAX_EVENTS 4096u
#define MOL_LATENCY_TEXT_LIMIT 256u

typedef struct mol_capture {
  int16_t* samples;
  uint32_t frame_count;
  uint32_t sample_rate;
  uint16_t channels;
} mol_capture_t;

typedef struct mol_options {
  const char* input;
  const char* report;
  const char* route;
  const char* device;
  const char* buffer_config;
  const char* artifact_commit;
  double trigger_threshold;
  double response_threshold;
  double min_latency_ms;
  double max_latency_ms;
  double refractory_ms;
  double p95_limit_ms;
  uint32_t trigger_channel;
  uint32_t response_channel;
  uint32_t minimum_events;
  int has_p95_limit;
} mol_options_t;

typedef struct mol_measurements {
  double values[MOL_LATENCY_MAX_EVENTS];
  uint32_t count;
  uint32_t trigger_count;
  uint32_t unmatched;
  double p50;
  double p95;
  double maximum;
} mol_measurements_t;

static uint16_t mol_u16_le(const uint8_t* bytes) {
  return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u));
}

static uint32_t mol_u32_le(const uint8_t* bytes) {
  return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) | ((uint32_t)bytes[2] << 16u) |
         ((uint32_t)bytes[3] << 24u);
}

static int mol_read_exact(FILE* file, void* output, size_t size) {
  return fread(output, 1u, size, file) == size;
}

static int mol_write_exact(FILE* file, const void* input, size_t size) {
  return fwrite(input, 1u, size, file) == size;
}

static int mol_write_u16_le(FILE* file, uint16_t value) {
  const uint8_t bytes[2] = {(uint8_t)(value & 0xffu), (uint8_t)(value >> 8u)};
  return mol_write_exact(file, bytes, sizeof(bytes));
}

static int mol_write_u32_le(FILE* file, uint32_t value) {
  const uint8_t bytes[4] = {(uint8_t)(value & 0xffu), (uint8_t)((value >> 8u) & 0xffu),
                            (uint8_t)((value >> 16u) & 0xffu), (uint8_t)(value >> 24u)};
  return mol_write_exact(file, bytes, sizeof(bytes));
}

static int mol_write_fixture(const char* path) {
  const uint32_t sample_rate = 48000u;
  const uint32_t event_count = 20u;
  const uint32_t initial_frame = sample_rate / 5u;
  const uint32_t spacing = sample_rate * 7u / 20u;
  const uint32_t frame_count = initial_frame + spacing * event_count;
  const uint32_t data_size = frame_count * 4u;
  FILE* file = fopen(path, "wb");
  int ok = file != NULL;
  if (ok) {
    ok = mol_write_exact(file, "RIFF", 4u) && mol_write_u32_le(file, 36u + data_size) &&
         mol_write_exact(file, "WAVEfmt ", 8u) && mol_write_u32_le(file, 16u) &&
         mol_write_u16_le(file, 1u) && mol_write_u16_le(file, 2u) &&
         mol_write_u32_le(file, sample_rate) && mol_write_u32_le(file, sample_rate * 4u) &&
         mol_write_u16_le(file, 4u) && mol_write_u16_le(file, 16u) &&
         mol_write_exact(file, "data", 4u) && mol_write_u32_le(file, data_size);
  }
  for (uint32_t frame = 0u; ok && frame < frame_count; ++frame) {
    int16_t trigger = 0;
    int16_t response = 0;
    for (uint32_t event = 0u; event < event_count; ++event) {
      uint32_t trigger_frame = initial_frame + event * spacing;
      uint32_t delay_frames = (10u + event) * sample_rate / 1000u;
      if (frame == trigger_frame) trigger = 26000;
      if (frame == trigger_frame + delay_frames) response = 20000;
    }
    ok = mol_write_u16_le(file, (uint16_t)trigger) && mol_write_u16_le(file, (uint16_t)response);
  }
  if (file != NULL && fclose(file) != 0) ok = 0;
  if (!ok) {
    (void)fprintf(stderr, "Could not write deterministic latency fixture: %s\n", path);
    return 1;
  }
  (void)printf("Generated synthetic validation fixture: %s\n", path);
  return 0;
}

static int mol_load_capture_stream(FILE* file, mol_capture_t* capture) {
  uint8_t header[12];
  uint8_t chunk[8];
  uint16_t format = 0u;
  uint16_t bits = 0u;
  uint16_t block_align = 0u;
  uint32_t data_size = 0u;
  long data_offset = -1;
  memset(capture, 0, sizeof(*capture));
  if (file == NULL || !mol_read_exact(file, header, sizeof(header)) ||
      memcmp(header, "RIFF", 4u) != 0 || memcmp(header + 8u, "WAVE", 4u) != 0) {
    return 0;
  }
  while (mol_read_exact(file, chunk, sizeof(chunk))) {
    uint32_t size = mol_u32_le(chunk + 4u);
    long payload = ftell(file);
    if (payload < 0 || size > MOL_LATENCY_MAX_DATA_BYTES) break;
    if (memcmp(chunk, "fmt ", 4u) == 0 && size >= 16u) {
      uint8_t body[16];
      if (!mol_read_exact(file, body, sizeof(body))) break;
      format = mol_u16_le(body);
      capture->channels = mol_u16_le(body + 2u);
      capture->sample_rate = mol_u32_le(body + 4u);
      block_align = mol_u16_le(body + 12u);
      bits = mol_u16_le(body + 14u);
    } else if (memcmp(chunk, "data", 4u) == 0 && data_offset < 0) {
      data_offset = payload;
      data_size = size;
    }
    if (fseek(file, payload + (long)size + (long)(size & 1u), SEEK_SET) != 0) break;
  }
  if (format != 1u || bits != 16u || capture->channels < 2u || capture->channels > 32u ||
      capture->sample_rate < 8000u || capture->sample_rate > 384000u ||
      block_align != capture->channels * 2u || data_offset < 0 || data_size == 0u ||
      data_size > MOL_LATENCY_MAX_DATA_BYTES || data_size % block_align != 0u) {
    return 0;
  }
  capture->frame_count = data_size / block_align;
  capture->samples = (int16_t*)malloc((size_t)data_size);
  if (capture->samples == NULL || fseek(file, data_offset, SEEK_SET) != 0) {
    free(capture->samples);
    capture->samples = NULL;
    return 0;
  }
  for (uint32_t index = 0u; index < data_size / 2u; ++index) {
    uint8_t encoded[2];
    if (!mol_read_exact(file, encoded, sizeof(encoded))) {
      free(capture->samples);
      capture->samples = NULL;
      return 0;
    }
    capture->samples[index] = (int16_t)mol_u16_le(encoded);
  }
  return 1;
}

static int mol_load_capture(const char* path, mol_capture_t* capture) {
  FILE* file = fopen(path, "rb");
  int loaded;
  int closed;
  if (file == NULL) return 0;
  loaded = mol_load_capture_stream(file, capture);
  closed = fclose(file) == 0;
  if (loaded && !closed) {
    free(capture->samples);
    capture->samples = NULL;
  }
  return loaded && closed;
}

static double mol_magnitude(int16_t sample) {
  int32_t value = sample;
  if (value < 0) value = -value;
  return (double)value / 32768.0;
}

static int mol_compare_double(const void* left, const void* right) {
  double a = *(const double*)left;
  double b = *(const double*)right;
  return (a > b) - (a < b);
}

static double mol_percentile(const double* sorted, uint32_t count, double fraction) {
  double position = fraction * (double)(count - 1u);
  uint32_t lower = (uint32_t)position;
  uint32_t upper = lower + 1u < count ? lower + 1u : lower;
  double weight = position - lower;
  return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
}

static void mol_measure(const mol_capture_t* capture, const mol_options_t* options,
                        mol_measurements_t* measurements) {
  uint32_t trigger_channel = options->trigger_channel - 1u;
  uint32_t response_channel = options->response_channel - 1u;
  uint32_t min_frames = (uint32_t)ceil(options->min_latency_ms * capture->sample_rate / 1000.0);
  uint32_t max_frames = (uint32_t)floor(options->max_latency_ms * capture->sample_rate / 1000.0);
  uint32_t refractory_frames =
      (uint32_t)ceil(options->refractory_ms * capture->sample_rate / 1000.0);
  uint32_t last_trigger = 0u;
  int has_trigger = 0;
  double previous = 0.0;
  memset(measurements, 0, sizeof(*measurements));
  for (uint32_t frame = 0u; frame < capture->frame_count; ++frame) {
    double current =
        mol_magnitude(capture->samples[(size_t)frame * capture->channels + trigger_channel]);
    int separated = !has_trigger || frame - last_trigger >= refractory_frames;
    if (current >= options->trigger_threshold && previous < options->trigger_threshold &&
        separated) {
      uint32_t first = frame + min_frames;
      uint32_t last = frame + max_frames;
      int found = 0;
      ++measurements->trigger_count;
      last_trigger = frame;
      has_trigger = 1;
      if (last >= capture->frame_count) last = capture->frame_count - 1u;
      for (uint32_t response_frame = first; response_frame <= last; ++response_frame) {
        double response = mol_magnitude(
            capture->samples[(size_t)response_frame * capture->channels + response_channel]);
        if (response >= options->response_threshold) {
          if (measurements->count < MOL_LATENCY_MAX_EVENTS) {
            measurements->values[measurements->count++] =
                1000.0 * (response_frame - frame) / capture->sample_rate;
          }
          found = 1;
          break;
        }
      }
      if (!found) ++measurements->unmatched;
    }
    previous = current;
  }
  if (measurements->count > 0u) {
    qsort(measurements->values, measurements->count, sizeof(measurements->values[0]),
          mol_compare_double);
    measurements->p50 = mol_percentile(measurements->values, measurements->count, 0.50);
    measurements->p95 = mol_percentile(measurements->values, measurements->count, 0.95);
    measurements->maximum = measurements->values[measurements->count - 1u];
  }
}

static int mol_parse_double(const char* text, double* value) {
  char* end = NULL;
  errno = 0;
  *value = strtod(text, &end);
  return errno == 0 && end != text && *end == '\0' && isfinite(*value);
}

static int mol_parse_u32(const char* text, uint32_t* value) {
  char* end = NULL;
  unsigned long parsed;
  errno = 0;
  parsed = strtoul(text, &end, 10);
  if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX) return 0;
  *value = (uint32_t)parsed;
  return 1;
}

static int mol_text_is_valid(const char* text) {
  size_t length = text == NULL ? 0u : strlen(text);
  return length > 0u && length <= MOL_LATENCY_TEXT_LIMIT;
}

static int mol_parse_options(int argc, char** argv, mol_options_t* options) {
  memset(options, 0, sizeof(*options));
  options->trigger_channel = 1u;
  options->response_channel = 2u;
  options->trigger_threshold = 0.5;
  options->response_threshold = 0.1;
  options->min_latency_ms = 1.0;
  options->max_latency_ms = 200.0;
  options->refractory_ms = 300.0;
  options->minimum_events = 20u;
  if (argc < 2) return 0;
  options->input = argv[1];
  for (int index = 2; index < argc; ++index) {
    const char* name = argv[index];
    const char* value;
    if (++index >= argc) return 0;
    value = argv[index];
    if (strcmp(name, "--report") == 0) {
      options->report = value;
    } else if (strcmp(name, "--route") == 0) {
      options->route = value;
    } else if (strcmp(name, "--device") == 0) {
      options->device = value;
    } else if (strcmp(name, "--buffer-config") == 0) {
      options->buffer_config = value;
    } else if (strcmp(name, "--artifact-commit") == 0) {
      options->artifact_commit = value;
    } else if (strcmp(name, "--trigger-channel") == 0) {
      if (!mol_parse_u32(value, &options->trigger_channel)) return 0;
    } else if (strcmp(name, "--response-channel") == 0) {
      if (!mol_parse_u32(value, &options->response_channel)) return 0;
    } else if (strcmp(name, "--minimum-events") == 0) {
      if (!mol_parse_u32(value, &options->minimum_events)) return 0;
    } else if (strcmp(name, "--trigger-threshold") == 0) {
      if (!mol_parse_double(value, &options->trigger_threshold)) return 0;
    } else if (strcmp(name, "--response-threshold") == 0) {
      if (!mol_parse_double(value, &options->response_threshold)) return 0;
    } else if (strcmp(name, "--min-latency-ms") == 0) {
      if (!mol_parse_double(value, &options->min_latency_ms)) return 0;
    } else if (strcmp(name, "--max-latency-ms") == 0) {
      if (!mol_parse_double(value, &options->max_latency_ms)) return 0;
    } else if (strcmp(name, "--refractory-ms") == 0) {
      if (!mol_parse_double(value, &options->refractory_ms)) return 0;
    } else if (strcmp(name, "--p95-limit-ms") == 0) {
      if (!mol_parse_double(value, &options->p95_limit_ms)) return 0;
      options->has_p95_limit = 1;
    } else {
      return 0;
    }
  }
  return mol_text_is_valid(options->report) && mol_text_is_valid(options->route) &&
         mol_text_is_valid(options->device) && mol_text_is_valid(options->buffer_config) &&
         mol_text_is_valid(options->artifact_commit) && options->trigger_channel > 0u &&
         options->response_channel > 0u && options->trigger_channel != options->response_channel &&
         options->minimum_events > 0u && options->minimum_events <= MOL_LATENCY_MAX_EVENTS &&
         options->trigger_threshold > 0.0 && options->trigger_threshold <= 1.0 &&
         options->response_threshold > 0.0 && options->response_threshold <= 1.0 &&
         options->min_latency_ms >= 0.0 && options->max_latency_ms > options->min_latency_ms &&
         options->max_latency_ms <= 3600000.0 && options->refractory_ms <= 3600000.0 &&
         options->refractory_ms >= options->max_latency_ms &&
         (!options->has_p95_limit || options->p95_limit_ms > 0.0);
}

static int mol_hash_file(const char* path, char text[65]) {
  uint8_t buffer[16384];
  uint8_t digest[32];
  mol_sha256_t hash;
  FILE* file = fopen(path, "rb");
  if (file == NULL) return 0;
  mol_sha256_init(&hash);
  for (;;) {
    size_t count = fread(buffer, 1u, sizeof(buffer), file);
    if (count > 0u) mol_sha256_update(&hash, buffer, count);
    if (count < sizeof(buffer)) {
      if (ferror(file)) {
        (void)fclose(file);
        return 0;
      }
      break;
    }
  }
  if (fclose(file) != 0) return 0;
  mol_sha256_finish(&hash, digest);
  for (uint32_t index = 0u; index < 32u; ++index) {
    (void)snprintf(text + index * 2u, 3u, "%02x", digest[index]);
  }
  text[64] = '\0';
  return 1;
}

static void mol_write_json_string(FILE* file, const char* value) {
  (void)fputc('"', file);
  for (const unsigned char* cursor = (const unsigned char*)value; *cursor != 0u; ++cursor) {
    if (*cursor == '"' || *cursor == '\\') {
      (void)fputc('\\', file);
      (void)fputc(*cursor, file);
    } else if (*cursor < 0x20u) {
      (void)fprintf(file, "\\u%04x", *cursor);
    } else {
      (void)fputc(*cursor, file);
    }
  }
  (void)fputc('"', file);
}

static int mol_write_report(const mol_options_t* options, const mol_capture_t* capture,
                            const mol_measurements_t* measurements, const char* hash,
                            const char* result) {
  FILE* file = fopen(options->report, "wb");
  if (file == NULL) return 0;
  (void)fprintf(file, "{\n  \"schema_version\": 1,\n  \"tool\": \"mol-latency-probe\",\n");
  (void)fprintf(file, "  \"method\": \"dual-channel threshold crossing\",\n");
  (void)fprintf(file, "  \"capture\": ");
  mol_write_json_string(file, options->input);
  (void)fprintf(file, ",\n  \"capture_sha256\": \"%s\",\n  \"route\": ", hash);
  mol_write_json_string(file, options->route);
  (void)fprintf(file, ",\n  \"device\": ");
  mol_write_json_string(file, options->device);
  (void)fprintf(file, ",\n  \"buffer_config\": ");
  mol_write_json_string(file, options->buffer_config);
  (void)fprintf(file, ",\n  \"artifact_commit\": ");
  mol_write_json_string(file, options->artifact_commit);
  (void)fprintf(file,
                ",\n  \"sample_rate\": %u,\n  \"channels\": %u,\n"
                "  \"frame_count\": %u,\n  \"trigger_channel\": %u,\n"
                "  \"response_channel\": %u,\n  \"trigger_threshold\": %.6f,\n"
                "  \"response_threshold\": %.6f,\n  \"minimum_latency_ms\": %.6f,\n"
                "  \"maximum_latency_ms\": %.6f,\n  \"refractory_ms\": %.6f,\n"
                "  \"trigger_count\": %u,\n  \"event_count\": %u,\n"
                "  \"unmatched_triggers\": %u,\n  \"p50_ms\": %.6f,\n"
                "  \"p95_ms\": %.6f,\n  \"maximum_ms\": %.6f,\n",
                capture->sample_rate, capture->channels, capture->frame_count,
                options->trigger_channel, options->response_channel, options->trigger_threshold,
                options->response_threshold, options->min_latency_ms, options->max_latency_ms,
                options->refractory_ms, measurements->trigger_count, measurements->count,
                measurements->unmatched, measurements->p50, measurements->p95,
                measurements->maximum);
  if (options->has_p95_limit) {
    (void)fprintf(file, "  \"p95_limit_ms\": %.6f,\n", options->p95_limit_ms);
  } else {
    (void)fprintf(file, "  \"p95_limit_ms\": null,\n");
  }
  (void)fprintf(file, "  \"measurements_ms\": [");
  for (uint32_t index = 0u; index < measurements->count; ++index) {
    (void)fprintf(file, "%s%.6f", index == 0u ? "" : ", ", measurements->values[index]);
  }
  (void)fprintf(file, "],\n  \"result\": \"%s\"\n}\n", result);
  return fclose(file) == 0;
}

static void mol_print_usage(const char* executable) {
  (void)fprintf(
      stderr,
      "Usage:\n"
      "  %s --generate-fixture OUTPUT.wav\n"
      "  %s CAPTURE.wav --report REPORT.json --route NAME --device DESCRIPTION\n"
      "     --buffer-config DESCRIPTION --artifact-commit COMMIT [options]\n"
      "Options:\n"
      "  --trigger-channel N       1-based trigger channel (default 1)\n"
      "  --response-channel N      1-based captured-audio channel (default 2)\n"
      "  --trigger-threshold VALUE absolute PCM threshold (default 0.5)\n"
      "  --response-threshold VALUE absolute PCM threshold (default 0.1)\n"
      "  --min-latency-ms VALUE    response search start (default 1)\n"
      "  --max-latency-ms VALUE    response search end (default 200)\n"
      "  --refractory-ms VALUE     minimum trigger spacing (default 300)\n"
      "  --minimum-events N        required matched events (default 20)\n"
      "  --p95-limit-ms VALUE      fail when P95 exceeds this limit; omit for record-only\n",
      executable, executable);
}

int main(int argc, char** argv) {
  mol_options_t options;
  mol_capture_t capture;
  mol_measurements_t measurements;
  char hash[65];
  int passed;
  const char* result;
  if (argc == 3 && strcmp(argv[1], "--generate-fixture") == 0) {
    return mol_write_fixture(argv[2]);
  }
  if (!mol_parse_options(argc, argv, &options)) {
    mol_print_usage(argv[0]);
    return 2;
  }
  if (!mol_load_capture(options.input, &capture)) {
    (void)fprintf(stderr, "Could not read a bounded multichannel PCM16 RIFF/WAVE capture\n");
    return 1;
  }
  if (options.trigger_channel > capture.channels || options.response_channel > capture.channels ||
      capture.frame_count == 0u || !mol_hash_file(options.input, hash)) {
    (void)fprintf(stderr,
                  "Capture does not contain the selected channels or could not be hashed\n");
    free(capture.samples);
    return 1;
  }
  mol_measure(&capture, &options, &measurements);
  passed = measurements.count >= options.minimum_events && measurements.unmatched == 0u &&
           (!options.has_p95_limit || measurements.p95 <= options.p95_limit_ms);
  result = passed ? (options.has_p95_limit ? "pass" : "recorded") : "fail";
  if (!mol_write_report(&options, &capture, &measurements, hash, result)) {
    (void)fprintf(stderr, "Could not write latency report: %s\n", options.report);
    free(capture.samples);
    return 1;
  }
  (void)printf("events=%u unmatched=%u p50_ms=%.3f p95_ms=%.3f maximum_ms=%.3f result=%s\n",
               measurements.count, measurements.unmatched, measurements.p50, measurements.p95,
               measurements.maximum, result);
  free(capture.samples);
  return passed ? 0 : 1;
}
