/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOL_ANALYZE_MAX_DATA_BYTES (UINT32_C(64) * UINT32_C(1024) * UINT32_C(1024))
#define MOL_ANALYZE_PI 3.14159265358979323846

typedef struct mol_wav {
  float* samples;
  uint64_t hash;
  uint32_t sample_rate;
  uint32_t frame_count;
  uint16_t channels;
} mol_wav_t;

typedef struct mol_metrics {
  double peak;
  double rms;
  double dc;
  double frequency;
  double attack_ms;
  double active_end_ms;
  double tail_dbfs;
  double centroid_hz;
  double low_ratio;
  double mid_ratio;
  double high_ratio;
  double stereo_difference_rms;
  double max_step;
  uint64_t clipped_samples;
  uint64_t click_candidates;
} mol_metrics_t;

typedef struct mol_options {
  const char* input;
  double expected_frequency;
  double frequency_tolerance;
  double max_dc;
  double max_peak;
  double max_step;
  int require_stereo;
} mol_options_t;

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

static int mol_load_wav_stream(FILE* file, mol_wav_t* wav) {
  uint8_t header[12];
  uint8_t chunk[8];
  uint16_t format = 0u;
  uint16_t bits = 0u;
  uint16_t block_align = 0u;
  uint32_t data_size = 0u;
  long data_offset = -1;
  memset(wav, 0, sizeof(*wav));
  if (file == NULL || !mol_read_exact(file, header, sizeof(header)) ||
      memcmp(header, "RIFF", 4u) != 0 || memcmp(header + 8u, "WAVE", 4u) != 0) {
    return 0;
  }
  while (mol_read_exact(file, chunk, sizeof(chunk))) {
    uint32_t size = mol_u32_le(chunk + 4u);
    long payload = ftell(file);
    if (memcmp(chunk, "fmt ", 4u) == 0 && size >= 16u) {
      uint8_t body[16];
      if (!mol_read_exact(file, body, sizeof(body))) {
        break;
      }
      format = mol_u16_le(body);
      wav->channels = mol_u16_le(body + 2u);
      wav->sample_rate = mol_u32_le(body + 4u);
      block_align = mol_u16_le(body + 12u);
      bits = mol_u16_le(body + 14u);
    } else if (memcmp(chunk, "data", 4u) == 0 && data_offset < 0) {
      data_offset = payload;
      data_size = size;
    }
    if (size > MOL_ANALYZE_MAX_DATA_BYTES ||
        fseek(file, payload + (long)size + (long)(size & 1u), SEEK_SET) != 0) {
      break;
    }
  }
  if (format != 1u || bits != 16u || (wav->channels != 1u && wav->channels != 2u) ||
      wav->sample_rate < 8000u || wav->sample_rate > 192000u || block_align != wav->channels * 2u ||
      data_offset < 0 || data_size == 0u || data_size > MOL_ANALYZE_MAX_DATA_BYTES ||
      data_size % block_align != 0u) {
    return 0;
  }
  wav->frame_count = data_size / block_align;
  if (wav->frame_count < 128u) {
    return 0;
  }
  wav->samples = (float*)malloc(sizeof(*wav->samples) * wav->frame_count * wav->channels);
  if (wav->samples == NULL || fseek(file, data_offset, SEEK_SET) != 0) {
    free(wav->samples);
    wav->samples = NULL;
    return 0;
  }
  wav->hash = UINT64_C(14695981039346656037);
  for (uint32_t index = 0u; index < wav->frame_count * wav->channels; ++index) {
    uint8_t encoded[2];
    int16_t sample;
    if (!mol_read_exact(file, encoded, sizeof(encoded))) {
      free(wav->samples);
      wav->samples = NULL;
      return 0;
    }
    wav->hash = (wav->hash ^ encoded[0]) * UINT64_C(1099511628211);
    wav->hash = (wav->hash ^ encoded[1]) * UINT64_C(1099511628211);
    sample = (int16_t)mol_u16_le(encoded);
    wav->samples[index] = sample < 0 ? (float)sample / 32768.0f : (float)sample / 32767.0f;
  }
  return 1;
}

static int mol_load_wav(const char* path, mol_wav_t* wav) {
  FILE* file = fopen(path, "rb");
  int loaded;
  int closed;
  if (file == NULL) return 0;
  loaded = mol_load_wav_stream(file, wav);
  closed = fclose(file) == 0;
  if (loaded && !closed) {
    free(wav->samples);
    wav->samples = NULL;
  }
  return loaded && closed;
}

static void mol_analyze_spectrum(const mol_wav_t* wav, mol_metrics_t* metrics) {
  uint32_t count = wav->frame_count < 2048u ? wav->frame_count : 2048u;
  uint32_t start = wav->frame_count / 5u;
  double bands[3] = {0.0, 0.0, 0.0};
  double weighted = 0.0;
  double total = 0.0;
  if (start + count > wav->frame_count) {
    start = wav->frame_count - count;
  }
  for (uint32_t bin = 1u; bin <= count / 2u; ++bin) {
    double real = 0.0;
    double imaginary = 0.0;
    double frequency = (double)bin * wav->sample_rate / count;
    for (uint32_t index = 0u; index < count; ++index) {
      double window = 0.5 - 0.5 * cos(2.0 * MOL_ANALYZE_PI * index / (count - 1u));
      double phase = 2.0 * MOL_ANALYZE_PI * bin * index / count;
      double sample = wav->samples[(size_t)(start + index) * wav->channels] * window;
      real += sample * cos(phase);
      imaginary -= sample * sin(phase);
    }
    {
      double energy = real * real + imaginary * imaginary;
      uint32_t band = frequency < 500.0 ? 0u : (frequency < 4000.0 ? 1u : 2u);
      bands[band] += energy;
      total += energy;
      weighted += energy * frequency;
    }
  }
  if (total > 0.0) {
    metrics->low_ratio = bands[0] / total;
    metrics->mid_ratio = bands[1] / total;
    metrics->high_ratio = bands[2] / total;
    metrics->centroid_hz = weighted / total;
  }
}

static void mol_analyze(const mol_wav_t* wav, mol_metrics_t* metrics) {
  double square_sum = 0.0;
  double sum = 0.0;
  double difference_sum = 0.0;
  double previous[2] = {0.0, 0.0};
  double previous_left = 0.0;
  uint32_t analysis_start = wav->frame_count / 10u;
  uint32_t analysis_end = wav->frame_count * 6u / 10u;
  uint32_t crossings = 0u;
  uint32_t tail_frames = wav->sample_rate / 20u;
  double tail_sum = 0.0;
  memset(metrics, 0, sizeof(*metrics));
  for (uint32_t frame = 0u; frame < wav->frame_count; ++frame) {
    double left = wav->samples[(size_t)frame * wav->channels];
    for (uint32_t channel = 0u; channel < wav->channels; ++channel) {
      double sample = wav->samples[(size_t)frame * wav->channels + channel];
      double magnitude = fabs(sample);
      double step = fabs(sample - previous[channel]);
      metrics->peak = magnitude > metrics->peak ? magnitude : metrics->peak;
      metrics->max_step = step > metrics->max_step ? step : metrics->max_step;
      metrics->clipped_samples += magnitude >= 0.999969 ? 1u : 0u;
      metrics->click_candidates += step > 0.25 ? 1u : 0u;
      square_sum += sample * sample;
      sum += sample;
      previous[channel] = sample;
    }
    if (wav->channels == 2u) {
      double difference = left - wav->samples[(size_t)frame * 2u + 1u];
      difference_sum += difference * difference;
    }
    if (frame >= analysis_start && frame < analysis_end && previous_left <= 0.0 && left > 0.0) {
      ++crossings;
    }
    previous_left = left;
    if (frame + tail_frames >= wav->frame_count) {
      tail_sum += left * left;
    }
  }
  metrics->rms = sqrt(square_sum / ((double)wav->frame_count * wav->channels));
  metrics->dc = sum / ((double)wav->frame_count * wav->channels);
  metrics->stereo_difference_rms = sqrt(difference_sum / wav->frame_count);
  metrics->frequency = crossings * (double)wav->sample_rate / (analysis_end - analysis_start);
  metrics->tail_dbfs = 20.0 * log10(fmax(sqrt(tail_sum / tail_frames), 1.0e-12));
  for (uint32_t frame = 0u; frame < wav->frame_count; ++frame) {
    if (metrics->attack_ms == 0.0 &&
        fabs(wav->samples[(size_t)frame * wav->channels]) >= metrics->peak * 0.9) {
      metrics->attack_ms = 1000.0 * frame / wav->sample_rate;
    }
    if (fabs(wav->samples[(size_t)frame * wav->channels]) >= metrics->peak * 0.001) {
      metrics->active_end_ms = 1000.0 * frame / wav->sample_rate;
    }
  }
  mol_analyze_spectrum(wav, metrics);
}

static int mol_parse_double(const char* text, double* value) {
  char* end = NULL;
  errno = 0;
  *value = strtod(text, &end);
  return errno == 0 && end != text && *end == '\0' && isfinite(*value);
}

static int mol_parse_options(int argc, char** argv, mol_options_t* options) {
  memset(options, 0, sizeof(*options));
  options->frequency_tolerance = 1.0;
  options->max_dc = 1.0;
  options->max_peak = 1.0;
  options->max_step = 2.0;
  if (argc < 2) {
    return 0;
  }
  options->input = argv[1];
  for (int index = 2; index < argc; ++index) {
    const char* name = argv[index];
    if (strcmp(name, "--require-stereo") == 0) {
      options->require_stereo = 1;
      continue;
    }
    if (++index >= argc) {
      return 0;
    }
    if (strcmp(name, "--expect-frequency") == 0) {
      if (!mol_parse_double(argv[index], &options->expected_frequency)) return 0;
    } else if (strcmp(name, "--frequency-tolerance") == 0) {
      if (!mol_parse_double(argv[index], &options->frequency_tolerance)) return 0;
    } else if (strcmp(name, "--max-dc") == 0) {
      if (!mol_parse_double(argv[index], &options->max_dc)) return 0;
    } else if (strcmp(name, "--max-peak") == 0) {
      if (!mol_parse_double(argv[index], &options->max_peak)) return 0;
    } else if (strcmp(name, "--max-step") == 0) {
      if (!mol_parse_double(argv[index], &options->max_step)) return 0;
    } else {
      return 0;
    }
  }
  return options->frequency_tolerance >= 0.0 && options->max_dc >= 0.0 && options->max_peak > 0.0 &&
         options->max_step > 0.0;
}

int main(int argc, char** argv) {
  mol_options_t options;
  mol_wav_t wav;
  mol_metrics_t metrics;
  int passed;
  if (!mol_parse_options(argc, argv, &options)) {
    (void)fprintf(stderr,
                  "Usage: %s INPUT.wav [--expect-frequency HZ] [--frequency-tolerance HZ] "
                  "[--max-dc VALUE] [--max-peak VALUE] [--max-step VALUE] [--require-stereo]\n",
                  argv[0]);
    return 2;
  }
  if (!mol_load_wav(options.input, &wav)) {
    (void)fprintf(stderr, "Could not read a bounded PCM16 RIFF/WAVE input\n");
    return 1;
  }
  mol_analyze(&wav, &metrics);
  passed = metrics.peak > 0.0 && metrics.peak <= options.max_peak &&
           fabs(metrics.dc) <= options.max_dc && metrics.max_step <= options.max_step &&
           metrics.clipped_samples == 0u &&
           (options.expected_frequency == 0.0 ||
            fabs(metrics.frequency - options.expected_frequency) <= options.frequency_tolerance) &&
           (!options.require_stereo ||
            (wav.channels == 2u && metrics.stereo_difference_rms > 0.0000001));
  (void)printf("sample_rate=%u channels=%u frames=%u hash=%016llx\n", wav.sample_rate, wav.channels,
               wav.frame_count, (unsigned long long)wav.hash);
  (void)printf("peak=%.8f rms=%.8f dc=%.8f clipped_samples=%llu\n", metrics.peak, metrics.rms,
               metrics.dc, (unsigned long long)metrics.clipped_samples);
  (void)printf("frequency_hz=%.4f attack_ms=%.3f active_end_ms=%.3f tail_dbfs=%.3f\n",
               metrics.frequency, metrics.attack_ms, metrics.active_end_ms, metrics.tail_dbfs);
  (void)printf("centroid_hz=%.3f low_ratio=%.6f mid_ratio=%.6f high_ratio=%.6f\n",
               metrics.centroid_hz, metrics.low_ratio, metrics.mid_ratio, metrics.high_ratio);
  (void)printf("stereo_difference_rms=%.8f max_step=%.8f click_candidates=%llu result=%s\n",
               metrics.stereo_difference_rms, metrics.max_step,
               (unsigned long long)metrics.click_candidates, passed ? "pass" : "fail");
  free(wav.samples);
  return passed ? 0 : 1;
}
