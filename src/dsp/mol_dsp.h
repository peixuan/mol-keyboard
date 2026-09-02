/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_DSP_H_
#define MOL_DSP_H_

#include <stdint.h>

#define MOL_DSP_MAX_ENVELOPE_SEGMENTS 8u
#define MOL_DSP_MAX_PARTIALS 8u
#define MOL_DSP_MAX_MODES 8u

typedef enum mol_dsp_envelope_stage {
  MOL_DSP_ENVELOPE_IDLE = 0,
  MOL_DSP_ENVELOPE_ATTACK = 1,
  MOL_DSP_ENVELOPE_DECAY = 2,
  MOL_DSP_ENVELOPE_SUSTAIN = 3,
  MOL_DSP_ENVELOPE_RELEASE = 4
} mol_dsp_envelope_stage_t;

typedef enum mol_dsp_filter_output {
  MOL_DSP_FILTER_LOW_PASS = 0,
  MOL_DSP_FILTER_BAND_PASS = 1,
  MOL_DSP_FILTER_HIGH_PASS = 2
} mol_dsp_filter_output_t;

typedef enum mol_dsp_lfo_shape {
  MOL_DSP_LFO_SINE = 0,
  MOL_DSP_LFO_TRIANGLE = 1,
  MOL_DSP_LFO_SQUARE = 2
} mol_dsp_lfo_shape_t;

typedef struct mol_dsp_adsr {
  float value;
  float sustain;
  float attack_step;
  float decay_step;
  float release_step;
  uint32_t release_frames;
  mol_dsp_envelope_stage_t stage;
} mol_dsp_adsr_t;

typedef struct mol_dsp_segment_envelope {
  float targets[MOL_DSP_MAX_ENVELOPE_SEGMENTS];
  uint32_t frames[MOL_DSP_MAX_ENVELOPE_SEGMENTS];
  float value;
  float step;
  uint32_t remaining;
  uint32_t segment;
  uint32_t count;
} mol_dsp_segment_envelope_t;

typedef struct mol_dsp_smoother {
  float value;
  float target;
  float coefficient;
} mol_dsp_smoother_t;

typedef struct mol_dsp_state_variable_filter {
  float g;
  float k;
  float integrator_1;
  float integrator_2;
} mol_dsp_state_variable_filter_t;

typedef struct mol_dsp_biquad {
  float b0;
  float b1;
  float b2;
  float a1;
  float a2;
  float z1;
  float z2;
} mol_dsp_biquad_t;

typedef struct mol_dsp_lfo {
  float phase;
  float increment;
  mol_dsp_lfo_shape_t shape;
} mol_dsp_lfo_t;

typedef struct mol_dsp_fm2 {
  float carrier_phase;
  float modulator_phase;
  float carrier_increment;
  float modulator_increment;
  float index;
} mol_dsp_fm2_t;

typedef struct mol_dsp_additive {
  float phases[MOL_DSP_MAX_PARTIALS];
  float ratios[MOL_DSP_MAX_PARTIALS];
  float gains[MOL_DSP_MAX_PARTIALS];
  float base_increment;
  uint32_t count;
} mol_dsp_additive_t;

typedef struct mol_dsp_karplus_strong {
  float* buffer;
  uint32_t capacity;
  uint32_t length;
  uint32_t index;
  uint32_t random_state;
  float damping;
} mol_dsp_karplus_strong_t;

typedef struct mol_dsp_modal_bank {
  float coefficient[MOL_DSP_MAX_MODES];
  float radius_squared[MOL_DSP_MAX_MODES];
  float gain[MOL_DSP_MAX_MODES];
  float y1[MOL_DSP_MAX_MODES];
  float y2[MOL_DSP_MAX_MODES];
  uint32_t count;
} mol_dsp_modal_bank_t;

typedef struct mol_dsp_dc_blocker {
  float previous_input;
  float previous_output;
  float coefficient;
} mol_dsp_dc_blocker_t;

typedef struct mol_dsp_limiter {
  float envelope;
  float gain;
  float ceiling;
  float attack_coefficient;
  float release_coefficient;
} mol_dsp_limiter_t;

float mol_dsp_clamp(float value, float minimum, float maximum);
float mol_dsp_db_to_linear(float decibels);
float mol_dsp_linear_to_db(float linear);
float mol_dsp_phase_advance(float* phase, float increment);
float mol_dsp_sine(float phase);
float mol_dsp_polyblep_saw(float phase, float increment);
float mol_dsp_polyblep_pulse(float phase, float increment, float width);
float mol_dsp_polyblep_square(float phase, float increment);
float mol_dsp_triangle(float phase);
float mol_dsp_white_noise(uint32_t* state);
float mol_dsp_pink_noise(uint32_t* state, float* memory);

void mol_dsp_adsr_configure(mol_dsp_adsr_t* envelope, uint32_t sample_rate, float attack_seconds,
                            float decay_seconds, float sustain, float release_seconds);
void mol_dsp_adsr_note_on(mol_dsp_adsr_t* envelope);
void mol_dsp_adsr_note_off(mol_dsp_adsr_t* envelope);
float mol_dsp_adsr_process(mol_dsp_adsr_t* envelope);

void mol_dsp_segment_envelope_configure(mol_dsp_segment_envelope_t* envelope, const float* targets,
                                        const uint32_t* frames, uint32_t count,
                                        float initial_value);
float mol_dsp_segment_envelope_process(mol_dsp_segment_envelope_t* envelope);

void mol_dsp_smoother_configure(mol_dsp_smoother_t* smoother, uint32_t sample_rate, float seconds,
                                float initial_value);
void mol_dsp_smoother_set_target(mol_dsp_smoother_t* smoother, float target);
float mol_dsp_smoother_process(mol_dsp_smoother_t* smoother);

void mol_dsp_state_variable_configure(mol_dsp_state_variable_filter_t* filter, uint32_t sample_rate,
                                      float cutoff_hz, float resonance);
float mol_dsp_state_variable_process(mol_dsp_state_variable_filter_t* filter, float input,
                                     mol_dsp_filter_output_t output);

void mol_dsp_biquad_low_pass(mol_dsp_biquad_t* filter, uint32_t sample_rate, float cutoff_hz,
                             float q);
void mol_dsp_biquad_high_pass(mol_dsp_biquad_t* filter, uint32_t sample_rate, float cutoff_hz,
                              float q);
float mol_dsp_biquad_process(mol_dsp_biquad_t* filter, float input);

void mol_dsp_lfo_configure(mol_dsp_lfo_t* lfo, uint32_t sample_rate, float rate_hz,
                           mol_dsp_lfo_shape_t shape, float phase);
float mol_dsp_lfo_process(mol_dsp_lfo_t* lfo);

void mol_dsp_fm2_configure(mol_dsp_fm2_t* fm, uint32_t sample_rate, float frequency_hz, float ratio,
                           float index);
float mol_dsp_fm2_process(mol_dsp_fm2_t* fm);

void mol_dsp_additive_configure(mol_dsp_additive_t* additive, uint32_t sample_rate,
                                float frequency_hz, const float* ratios, const float* gains,
                                uint32_t count);
float mol_dsp_additive_process(mol_dsp_additive_t* additive);

void mol_dsp_karplus_configure(mol_dsp_karplus_strong_t* pluck, float* buffer, uint32_t capacity,
                               uint32_t sample_rate, float frequency_hz, float damping,
                               uint32_t seed);
float mol_dsp_karplus_process(mol_dsp_karplus_strong_t* pluck);

void mol_dsp_modal_configure(mol_dsp_modal_bank_t* modal, uint32_t sample_rate,
                             float fundamental_hz, const float* ratios, const float* gains,
                             const float* decays, uint32_t count);
float mol_dsp_modal_process(mol_dsp_modal_bank_t* modal, float excitation);

float mol_dsp_soft_saturate(float input, float drive);
void mol_dsp_dc_blocker_configure(mol_dsp_dc_blocker_t* blocker, float coefficient);
float mol_dsp_dc_blocker_process(mol_dsp_dc_blocker_t* blocker, float input);
void mol_dsp_limiter_configure(mol_dsp_limiter_t* limiter, uint32_t sample_rate, float ceiling_db,
                               float attack_seconds, float release_seconds);
float mol_dsp_limiter_process(mol_dsp_limiter_t* limiter, float input);

#endif /* MOL_DSP_H_ */
