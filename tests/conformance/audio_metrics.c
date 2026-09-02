/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mol/mol.h"

#define MOL_METRICS_SAMPLE_RATE 48000u
#define MOL_METRICS_FRAME_COUNT (MOL_METRICS_SAMPLE_RATE * 4u)
#define MOL_METRICS_GATE_FRAME MOL_METRICS_SAMPLE_RATE
#define MOL_METRICS_BLOCK_FRAMES 128u
#define MOL_METRICS_SPECTRUM_FRAMES 2048u
#define MOL_METRICS_SPECTRUM_START (MOL_METRICS_FRAME_COUNT / 5u)
#define MOL_METRICS_PI 3.14159265358979323846

typedef union metrics_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[1048576];
} metrics_storage_t;

typedef struct metrics_result {
  double peak;
  double rms;
  double dc;
  double stereo;
  double max_step;
  double centroid;
  double bands[3];
  uint32_t attack_frame;
  uint32_t active_end_frame;
} metrics_result_t;

static metrics_storage_t storage;
static float magnitudes[MOL_METRICS_FRAME_COUNT];
static float spectrum[MOL_METRICS_SPECTRUM_FRAMES];

static mol_command_t command_at(mol_command_type_t type, mol_frame_index_t frame) {
  mol_command_t command;
  memset(&command, 0, sizeof(command));
  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = type;
  command.target_frame = frame;
  return command;
}

static void analyze_spectrum(metrics_result_t* result) {
  double band_energy[3] = {0.0, 0.0, 0.0};
  double total = 0.0;
  double weighted = 0.0;
  for (uint32_t bin = 1u; bin <= MOL_METRICS_SPECTRUM_FRAMES / 2u; ++bin) {
    double real = 0.0;
    double imaginary = 0.0;
    double frequency = (double)bin * MOL_METRICS_SAMPLE_RATE / MOL_METRICS_SPECTRUM_FRAMES;
    for (uint32_t index = 0u; index < MOL_METRICS_SPECTRUM_FRAMES; ++index) {
      double window =
          0.5 - 0.5 * cos(2.0 * MOL_METRICS_PI * index / (MOL_METRICS_SPECTRUM_FRAMES - 1u));
      double phase = 2.0 * MOL_METRICS_PI * bin * index / MOL_METRICS_SPECTRUM_FRAMES;
      double sample = spectrum[index] * window;
      real += sample * cos(phase);
      imaginary -= sample * sin(phase);
    }
    {
      double energy = real * real + imaginary * imaginary;
      uint32_t band = frequency < 500.0 ? 0u : (frequency < 4000.0 ? 1u : 2u);
      band_energy[band] += energy;
      total += energy;
      weighted += energy * frequency;
    }
  }
  if (total > 0.0) {
    result->centroid = weighted / total;
    for (uint32_t band = 0u; band < 3u; ++band) {
      result->bands[band] = band_energy[band] / total;
    }
  }
}

static int render_preset(mol_preset_id_t preset_id, metrics_result_t* result) {
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  mol_command_t preset = command_at(MOL_COMMAND_SET_PRESET, 0u);
  mol_command_t note_on = command_at(MOL_COMMAND_NOTE_ON, 0u);
  mol_command_t note_off = command_at(MOL_COMMAND_NOTE_OFF, MOL_METRICS_GATE_FRAME);
  float output[MOL_METRICS_BLOCK_FRAMES * 2u];
  double square_sum = 0.0;
  double sum = 0.0;
  double difference_sum = 0.0;
  float previous[2] = {0.0f, 0.0f};

  memset(result, 0, sizeof(*result));
  memset(magnitudes, 0, sizeof(magnitudes));
  memset(spectrum, 0, sizeof(spectrum));
  config.sample_rate = MOL_METRICS_SAMPLE_RATE;
  config.channel_count = 2u;
  config.max_voices = 8u;
  if (mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) != MOL_OK) {
    return 0;
  }
  preset.payload.preset.preset = preset_id;
  preset.payload.preset.hard_switch = 1u;
  note_on.gesture_id = 1u;
  note_on.payload.note.note = 60u;
  note_on.payload.note.velocity = 0.8f;
  note_off.gesture_id = 1u;
  note_off.payload.note.note = 60u;
  note_off.payload.note.velocity = 0.8f;
  if (mol_engine_submit(engine, &preset) != MOL_OK ||
      mol_engine_submit(engine, &note_on) != MOL_OK ||
      mol_engine_submit(engine, &note_off) != MOL_OK) {
    mol_engine_shutdown(engine);
    return 0;
  }

  for (uint32_t rendered = 0u; rendered < MOL_METRICS_FRAME_COUNT;
       rendered += MOL_METRICS_BLOCK_FRAMES) {
    if (mol_engine_render_interleaved_f32(engine, output, MOL_METRICS_BLOCK_FRAMES, 2u) != MOL_OK) {
      mol_engine_shutdown(engine);
      return 0;
    }
    for (uint32_t offset = 0u; offset < MOL_METRICS_BLOCK_FRAMES; ++offset) {
      uint32_t frame = rendered + offset;
      float left = output[offset * 2u];
      float right = output[offset * 2u + 1u];
      float magnitude = fmaxf(fabsf(left), fabsf(right));
      float delta_left = fabsf(left - previous[0]);
      float delta_right = fabsf(right - previous[1]);
      if (!isfinite(left) || !isfinite(right)) {
        mol_engine_shutdown(engine);
        return 0;
      }
      result->peak = fmax(result->peak, magnitude);
      result->max_step = fmax(result->max_step, fmaxf(delta_left, delta_right));
      square_sum += (double)left * left + (double)right * right;
      sum += (double)left + right;
      difference_sum += ((double)left - right) * ((double)left - right);
      magnitudes[frame] = magnitude;
      if (frame >= MOL_METRICS_SPECTRUM_START &&
          frame < MOL_METRICS_SPECTRUM_START + MOL_METRICS_SPECTRUM_FRAMES) {
        spectrum[frame - MOL_METRICS_SPECTRUM_START] = left;
      }
      previous[0] = left;
      previous[1] = right;
    }
  }
  mol_engine_shutdown(engine);

  result->rms = sqrt(square_sum / ((double)MOL_METRICS_FRAME_COUNT * 2.0));
  result->dc = sum / ((double)MOL_METRICS_FRAME_COUNT * 2.0);
  result->stereo = sqrt(difference_sum / MOL_METRICS_FRAME_COUNT);
  for (uint32_t frame = 0u; frame < MOL_METRICS_FRAME_COUNT; ++frame) {
    if (result->attack_frame == 0u && magnitudes[frame] >= result->peak * 0.9) {
      result->attack_frame = frame;
    }
    if (magnitudes[frame] >= result->peak * 0.001) {
      result->active_end_frame = frame;
    }
  }
  analyze_spectrum(result);
  return result->peak > 0.0 && result->peak < 1.0 && result->rms > 0.0 && fabs(result->dc) < 0.01 &&
         result->stereo > 0.0 && result->max_step < 0.25 && result->centroid > 0.0;
}

static long quantize(double value, double scale) {
  double scaled = value * scale;
  return (long)(scaled >= 0.0 ? floor(scaled + 0.5) : ceil(scaled - 0.5));
}

int main(void) {
  for (uint32_t preset = 0u; preset < MOL_PRESET_COUNT; ++preset) {
    metrics_result_t result;
    if (!render_preset(preset, &result)) {
      (void)fprintf(stderr, "audio metrics failed for preset %u\n", preset);
      return 1;
    }
    (void)printf("%u %s %ld %ld %ld %ld %ld %lu %lu %ld %ld %ld %ld\n", preset,
                 mol_preset_stable_id(preset), quantize(result.peak, 1000000.0),
                 quantize(result.rms, 1000000.0), quantize(result.dc, 1000000.0),
                 quantize(result.stereo, 1000000.0), quantize(result.max_step, 1000000.0),
                 (unsigned long)quantize(
                     (double)result.attack_frame * 1000.0 / MOL_METRICS_SAMPLE_RATE, 1.0),
                 (unsigned long)quantize(
                     (double)result.active_end_frame * 1000.0 / MOL_METRICS_SAMPLE_RATE, 1.0),
                 quantize(result.centroid, 1.0), quantize(result.bands[0], 1000.0),
                 quantize(result.bands[1], 1000.0), quantize(result.bands[2], 1000.0));
  }
  return 0;
}
