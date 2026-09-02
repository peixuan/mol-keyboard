/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_EFFECTS_H_
#define MOL_EFFECTS_H_

#include <stdint.h>

#include "mol_dsp.h"

#define MOL_REVERB_LINE_COUNT 8u

typedef struct mol_chorus {
  float* buffer;
  uint32_t capacity;
  uint32_t index;
  uint32_t sample_rate;
  float phase;
  mol_dsp_smoother_t rate_hz;
  mol_dsp_smoother_t depth_ms;
  mol_dsp_smoother_t mix;
} mol_chorus_t;

typedef struct mol_delay {
  float* buffer;
  uint32_t capacity;
  uint32_t index;
  uint32_t sample_rate;
  mol_dsp_smoother_t delay_frames;
  mol_dsp_smoother_t feedback;
  mol_dsp_smoother_t mix;
} mol_delay_t;

typedef struct mol_reverb_line {
  uint32_t offset;
  uint32_t length;
  uint32_t index;
  float filtered;
} mol_reverb_line_t;

typedef struct mol_reverb {
  float* buffer;
  uint32_t capacity;
  uint32_t sample_rate;
  uint32_t predelay_capacity;
  uint32_t predelay_index;
  mol_reverb_line_t lines[MOL_REVERB_LINE_COUNT];
  mol_dsp_smoother_t predelay_frames;
  mol_dsp_smoother_t size;
  mol_dsp_smoother_t damping;
  mol_dsp_smoother_t mix;
} mol_reverb_t;

void mol_chorus_configure(mol_chorus_t* chorus, float* buffer, uint32_t capacity,
                          uint32_t sample_rate);
void mol_chorus_set(mol_chorus_t* chorus, float rate_hz, float depth_ms, float mix);
void mol_chorus_process(mol_chorus_t* chorus, float input, float* left, float* right);
void mol_chorus_clear(mol_chorus_t* chorus);

void mol_delay_configure(mol_delay_t* delay, float* buffer, uint32_t capacity,
                         uint32_t sample_rate);
void mol_delay_set(mol_delay_t* delay, float time_ms, float feedback, float mix);
float mol_delay_process(mol_delay_t* delay, float input);
void mol_delay_clear(mol_delay_t* delay);

void mol_reverb_configure(mol_reverb_t* reverb, float* buffer, uint32_t capacity,
                          uint32_t sample_rate, float length_scale);
void mol_reverb_set(mol_reverb_t* reverb, float predelay_ms, float size, float damping, float mix);
void mol_reverb_process(mol_reverb_t* reverb, float input, float* left, float* right);
void mol_reverb_clear(mol_reverb_t* reverb);

#endif /* MOL_EFFECTS_H_ */
