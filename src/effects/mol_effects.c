/* SPDX-License-Identifier: Apache-2.0 */
#include "mol_effects.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define MOL_EFFECT_PI 3.14159265358979323846f

static float mol_effect_read(const float* buffer, uint32_t capacity, uint32_t write_index,
                             float delay_frames) {
  float position = (float)write_index - delay_frames;
  uint32_t first;
  uint32_t second;
  float fraction;
  while (position < 0.0f) {
    position += (float)capacity;
  }
  while (position >= (float)capacity) {
    position -= (float)capacity;
  }
  first = (uint32_t)position;
  second = first + 1u == capacity ? 0u : first + 1u;
  fraction = position - (float)first;
  return buffer[first] + fraction * (buffer[second] - buffer[first]);
}

void mol_chorus_configure(mol_chorus_t* chorus, float* buffer, uint32_t capacity,
                          uint32_t sample_rate) {
  if (chorus == NULL || buffer == NULL || capacity < 4u || sample_rate == 0u) {
    return;
  }
  memset(chorus, 0, sizeof(*chorus));
  memset(buffer, 0, sizeof(*buffer) * capacity);
  chorus->buffer = buffer;
  chorus->capacity = capacity;
  chorus->sample_rate = sample_rate;
  mol_dsp_smoother_configure(&chorus->rate_hz, sample_rate, 0.02f, 0.28f);
  mol_dsp_smoother_configure(&chorus->depth_ms, sample_rate, 0.02f, 3.5f);
  mol_dsp_smoother_configure(&chorus->mix, sample_rate, 0.02f, 0.32f);
}

void mol_chorus_set(mol_chorus_t* chorus, float rate_hz, float depth_ms, float mix) {
  if (chorus == NULL) {
    return;
  }
  mol_dsp_smoother_set_target(&chorus->rate_hz, mol_dsp_clamp(rate_hz, 0.05f, 5.0f));
  mol_dsp_smoother_set_target(&chorus->depth_ms, mol_dsp_clamp(depth_ms, 0.1f, 10.0f));
  mol_dsp_smoother_set_target(&chorus->mix, mol_dsp_clamp(mix, 0.0f, 1.0f));
}

void mol_chorus_process(mol_chorus_t* chorus, float input, float* left, float* right) {
  float rate;
  float depth;
  float mix;
  float base;
  if (left == NULL || right == NULL) {
    return;
  }
  *left = 0.0f;
  *right = 0.0f;
  if (chorus == NULL || chorus->buffer == NULL) {
    return;
  }
  input = isfinite(input) ? input : 0.0f;
  chorus->buffer[chorus->index] = input;
  rate = mol_dsp_smoother_process(&chorus->rate_hz);
  depth = mol_dsp_smoother_process(&chorus->depth_ms) * (float)chorus->sample_rate / 1000.0f;
  mix = mol_dsp_smoother_process(&chorus->mix);
  base = 0.012f * (float)chorus->sample_rate;
  base = mol_dsp_clamp(base, 1.0f, (float)chorus->capacity - depth - 2.0f);
  *left =
      mol_effect_read(chorus->buffer, chorus->capacity, chorus->index,
                      base + depth * (0.5f + 0.5f * sinf(2.0f * MOL_EFFECT_PI * chorus->phase))) *
      mix;
  *right =
      mol_effect_read(
          chorus->buffer, chorus->capacity, chorus->index,
          base + depth * (0.5f + 0.5f * sinf(2.0f * MOL_EFFECT_PI * (chorus->phase + 0.25f)))) *
      mix;
  chorus->phase += rate / (float)chorus->sample_rate;
  chorus->phase -= floorf(chorus->phase);
  chorus->index = chorus->index + 1u == chorus->capacity ? 0u : chorus->index + 1u;
}

void mol_chorus_clear(mol_chorus_t* chorus) {
  if (chorus != NULL && chorus->buffer != NULL) {
    memset(chorus->buffer, 0, sizeof(*chorus->buffer) * chorus->capacity);
    chorus->index = 0u;
    chorus->phase = 0.0f;
  }
}

void mol_delay_configure(mol_delay_t* delay, float* buffer, uint32_t capacity,
                         uint32_t sample_rate) {
  if (delay == NULL || buffer == NULL || capacity < 4u || sample_rate == 0u) {
    return;
  }
  memset(delay, 0, sizeof(*delay));
  memset(buffer, 0, sizeof(*buffer) * capacity);
  delay->buffer = buffer;
  delay->capacity = capacity;
  delay->sample_rate = sample_rate;
  mol_dsp_smoother_configure(&delay->delay_frames, sample_rate, 0.03f,
                             fminf(0.24f * (float)sample_rate, (float)capacity - 2.0f));
  mol_dsp_smoother_configure(&delay->feedback, sample_rate, 0.03f, 0.32f);
  mol_dsp_smoother_configure(&delay->mix, sample_rate, 0.03f, 0.28f);
}

void mol_delay_set(mol_delay_t* delay, float time_ms, float feedback, float mix) {
  float frames;
  if (delay == NULL || delay->capacity < 4u) {
    return;
  }
  frames = time_ms * (float)delay->sample_rate / 1000.0f;
  mol_dsp_smoother_set_target(&delay->delay_frames,
                              mol_dsp_clamp(frames, 1.0f, (float)delay->capacity - 2.0f));
  mol_dsp_smoother_set_target(&delay->feedback, mol_dsp_clamp(feedback, 0.0f, 0.95f));
  mol_dsp_smoother_set_target(&delay->mix, mol_dsp_clamp(mix, 0.0f, 1.0f));
}

float mol_delay_process(mol_delay_t* delay, float input) {
  float delayed;
  float feedback;
  float mix;
  if (delay == NULL || delay->buffer == NULL) {
    return 0.0f;
  }
  input = isfinite(input) ? input : 0.0f;
  delayed = mol_effect_read(delay->buffer, delay->capacity, delay->index,
                            mol_dsp_smoother_process(&delay->delay_frames));
  feedback = mol_dsp_smoother_process(&delay->feedback);
  mix = mol_dsp_smoother_process(&delay->mix);
  delay->buffer[delay->index] = mol_dsp_clamp(input + delayed * feedback, -4.0f, 4.0f);
  delay->index = delay->index + 1u == delay->capacity ? 0u : delay->index + 1u;
  return isfinite(delayed) ? delayed * mix : 0.0f;
}

void mol_delay_clear(mol_delay_t* delay) {
  if (delay != NULL && delay->buffer != NULL) {
    memset(delay->buffer, 0, sizeof(*delay->buffer) * delay->capacity);
    delay->index = 0u;
  }
}

static float mol_reverb_comb(mol_reverb_t* reverb, uint32_t line_index, float input, float feedback,
                             float damping) {
  mol_reverb_line_t* line = &reverb->lines[line_index];
  float output = reverb->buffer[line->offset + line->index];
  line->filtered += (1.0f - damping) * (output - line->filtered);
  reverb->buffer[line->offset + line->index] = input + line->filtered * feedback;
  line->index = line->index + 1u == line->length ? 0u : line->index + 1u;
  return output;
}

static float mol_reverb_allpass(mol_reverb_t* reverb, uint32_t line_index, float input) {
  mol_reverb_line_t* line = &reverb->lines[line_index];
  float delayed = reverb->buffer[line->offset + line->index];
  float output = delayed - input;
  reverb->buffer[line->offset + line->index] = input + delayed * 0.5f;
  line->index = line->index + 1u == line->length ? 0u : line->index + 1u;
  return output;
}

void mol_reverb_configure(mol_reverb_t* reverb, float* buffer, uint32_t capacity,
                          uint32_t sample_rate, float length_scale) {
  static const float milliseconds[MOL_REVERB_LINE_COUNT] = {29.7f, 37.1f, 41.1f, 43.7f,
                                                            5.0f,  1.7f,  5.3f,  1.9f};
  uint32_t offset;
  float requested = 0.0f;
  float fit = 1.0f;
  if (reverb == NULL || buffer == NULL || capacity < 64u || sample_rate == 0u) {
    return;
  }
  memset(reverb, 0, sizeof(*reverb));
  memset(buffer, 0, sizeof(*buffer) * capacity);
  reverb->buffer = buffer;
  reverb->capacity = capacity;
  reverb->sample_rate = sample_rate;
  reverb->predelay_capacity = sample_rate * 60u / 1000u + 2u;
  if (reverb->predelay_capacity > capacity / 4u) {
    reverb->predelay_capacity = capacity / 4u;
  }
  for (uint32_t index = 0u; index < MOL_REVERB_LINE_COUNT; ++index) {
    requested += milliseconds[index] * 0.001f * (float)sample_rate * length_scale + 2.0f;
  }
  if (requested > (float)(capacity - reverb->predelay_capacity)) {
    fit = (float)(capacity - reverb->predelay_capacity) / requested;
  }
  offset = reverb->predelay_capacity;
  for (uint32_t index = 0u; index < MOL_REVERB_LINE_COUNT; ++index) {
    uint32_t length =
        (uint32_t)(milliseconds[index] * 0.001f * (float)sample_rate * length_scale * fit) + 2u;
    reverb->lines[index].offset = offset;
    reverb->lines[index].length = length;
    offset += length;
  }
  mol_dsp_smoother_configure(&reverb->predelay_frames, sample_rate, 0.03f,
                             12.0f * sample_rate / 1000.0f);
  mol_dsp_smoother_configure(&reverb->size, sample_rate, 0.03f, 0.62f);
  mol_dsp_smoother_configure(&reverb->damping, sample_rate, 0.03f, 0.38f);
  mol_dsp_smoother_configure(&reverb->mix, sample_rate, 0.03f, 0.32f);
}

void mol_reverb_set(mol_reverb_t* reverb, float predelay_ms, float size, float damping, float mix) {
  if (reverb == NULL || reverb->predelay_capacity < 2u) {
    return;
  }
  mol_dsp_smoother_set_target(&reverb->predelay_frames,
                              mol_dsp_clamp(predelay_ms * reverb->sample_rate / 1000.0f, 0.0f,
                                            (float)reverb->predelay_capacity - 2.0f));
  mol_dsp_smoother_set_target(&reverb->size, mol_dsp_clamp(size, 0.0f, 1.0f));
  mol_dsp_smoother_set_target(&reverb->damping, mol_dsp_clamp(damping, 0.0f, 0.99f));
  mol_dsp_smoother_set_target(&reverb->mix, mol_dsp_clamp(mix, 0.0f, 1.0f));
}

void mol_reverb_process(mol_reverb_t* reverb, float input, float* left, float* right) {
  float comb[MOL_REVERB_LINE_COUNT / 2u];
  float predelayed;
  float feedback;
  float damping;
  float mix;
  uint32_t delay;
  if (left == NULL || right == NULL) {
    return;
  }
  *left = 0.0f;
  *right = 0.0f;
  if (reverb == NULL || reverb->buffer == NULL) {
    return;
  }
  input = isfinite(input) ? input : 0.0f;
  delay = (uint32_t)mol_dsp_smoother_process(&reverb->predelay_frames);
  reverb->buffer[reverb->predelay_index] = input;
  predelayed = delay == 0u
                   ? input
                   : reverb->buffer[(reverb->predelay_index + reverb->predelay_capacity - delay) %
                                    reverb->predelay_capacity];
  reverb->predelay_index =
      reverb->predelay_index + 1u == reverb->predelay_capacity ? 0u : reverb->predelay_index + 1u;
  feedback = 0.70f + 0.24f * mol_dsp_smoother_process(&reverb->size);
  damping = mol_dsp_smoother_process(&reverb->damping);
  mix = mol_dsp_smoother_process(&reverb->mix);
  for (uint32_t index = 0u; index < 4u; ++index) {
    comb[index] = mol_reverb_comb(reverb, index, predelayed, feedback, damping);
  }
  *left = (comb[0] + 0.7f * comb[2] + 0.3f * comb[3]) * 0.5f;
  *right = (comb[1] + 0.7f * comb[3] + 0.3f * comb[2]) * 0.5f;
  *left = mol_reverb_allpass(reverb, 5u, mol_reverb_allpass(reverb, 4u, *left)) * mix;
  *right = mol_reverb_allpass(reverb, 7u, mol_reverb_allpass(reverb, 6u, *right)) * mix;
  if (!isfinite(*left) || !isfinite(*right)) {
    mol_reverb_clear(reverb);
    *left = 0.0f;
    *right = 0.0f;
  }
}

void mol_reverb_clear(mol_reverb_t* reverb) {
  if (reverb != NULL && reverb->buffer != NULL) {
    memset(reverb->buffer, 0, sizeof(*reverb->buffer) * reverb->capacity);
    reverb->predelay_index = 0u;
    for (uint32_t index = 0u; index < MOL_REVERB_LINE_COUNT; ++index) {
      reverb->lines[index].index = 0u;
      reverb->lines[index].filtered = 0.0f;
    }
  }
}
