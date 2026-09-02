/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mol_dsp.h"

static int failures = 0;

#define EXPECT_TRUE(condition)                                                                  \
  do {                                                                                          \
    if (!(condition)) {                                                                         \
      (void)fprintf(stderr, "%s:%d: expectation failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                               \
    }                                                                                           \
  } while (0)

static void test_conversion_phase_and_oscillators(void) {
  float phase = 0.0f;
  float previous = 0.0f;
  uint32_t crossings = 0u;
  EXPECT_TRUE(fabsf(mol_dsp_db_to_linear(-6.0f) - 0.5011872f) < 0.0001f);
  EXPECT_TRUE(fabsf(mol_dsp_linear_to_db(0.5f) + 6.0206f) < 0.001f);
  EXPECT_TRUE(fabsf(mol_dsp_sine(0.25f) - 1.0f) < 0.0001f);
  EXPECT_TRUE(fabsf(mol_dsp_sine(0.75f) + 1.0f) < 0.0001f);
  for (uint32_t index = 0u; index < 48000u; ++index) {
    float current_phase = mol_dsp_phase_advance(&phase, 440.0f / 48000.0f);
    float sine = mol_dsp_sine(current_phase);
    float saw = mol_dsp_polyblep_saw(current_phase, 440.0f / 48000.0f);
    float square = mol_dsp_polyblep_square(current_phase, 440.0f / 48000.0f);
    float pulse = mol_dsp_polyblep_pulse(current_phase, 440.0f / 48000.0f, 0.25f);
    float triangle = mol_dsp_triangle(current_phase);
    EXPECT_TRUE(isfinite(sine) && isfinite(saw) && isfinite(square));
    EXPECT_TRUE(isfinite(pulse) && isfinite(triangle));
    EXPECT_TRUE(fabsf(saw) <= 1.1f && fabsf(square) <= 1.1f);
    if (previous <= 0.0f && sine > 0.0f) {
      ++crossings;
    }
    previous = sine;
  }
  EXPECT_TRUE(crossings == 440u);
}

static void test_noise_envelopes_and_smoothing(void) {
  uint32_t first_state = 1234u;
  uint32_t second_state = 1234u;
  float pink_memory = 0.0f;
  mol_dsp_adsr_t adsr;
  mol_dsp_segment_envelope_t segments;
  mol_dsp_smoother_t smoother;
  const float targets[3] = {1.0f, 0.25f, 0.0f};
  const uint32_t frames[3] = {10u, 10u, 10u};
  float first_noise = mol_dsp_white_noise(&first_state);
  EXPECT_TRUE(first_noise == mol_dsp_white_noise(&second_state));
  for (uint32_t index = 0u; index < 10000u; ++index) {
    float white = mol_dsp_white_noise(&first_state);
    float pink = mol_dsp_pink_noise(&second_state, &pink_memory);
    EXPECT_TRUE(isfinite(white) && fabsf(white) <= 1.0f);
    EXPECT_TRUE(isfinite(pink) && fabsf(pink) <= 1.0f);
  }

  mol_dsp_adsr_configure(&adsr, 1000u, 0.01f, 0.02f, 0.5f, 0.03f);
  mol_dsp_adsr_note_on(&adsr);
  for (uint32_t index = 0u; index < 10u; ++index) {
    (void)mol_dsp_adsr_process(&adsr);
  }
  EXPECT_TRUE(fabsf(adsr.value - 1.0f) < 0.0001f);
  for (uint32_t index = 0u; index < 20u; ++index) {
    (void)mol_dsp_adsr_process(&adsr);
  }
  EXPECT_TRUE(adsr.stage == MOL_DSP_ENVELOPE_SUSTAIN && fabsf(adsr.value - 0.5f) < 0.0001f);
  mol_dsp_adsr_note_off(&adsr);
  for (uint32_t index = 0u; index < 30u; ++index) {
    (void)mol_dsp_adsr_process(&adsr);
  }
  EXPECT_TRUE(adsr.stage == MOL_DSP_ENVELOPE_IDLE && adsr.value == 0.0f);

  mol_dsp_segment_envelope_configure(&segments, targets, frames, 3u, 0.0f);
  for (uint32_t index = 0u; index < 30u; ++index) {
    EXPECT_TRUE(isfinite(mol_dsp_segment_envelope_process(&segments)));
  }
  EXPECT_TRUE(segments.segment == 3u && segments.value == 0.0f);

  mol_dsp_smoother_configure(&smoother, 1000u, 0.01f, 0.0f);
  mol_dsp_smoother_set_target(&smoother, 1.0f);
  for (uint32_t index = 0u; index < 100u; ++index) {
    (void)mol_dsp_smoother_process(&smoother);
  }
  EXPECT_TRUE(smoother.value > 0.99f && smoother.value < 1.0f);
}

static void test_filters_and_dynamics(void) {
  mol_dsp_state_variable_filter_t state_variable = {0};
  mol_dsp_biquad_t low_pass = {0};
  mol_dsp_biquad_t high_pass = {0};
  mol_dsp_dc_blocker_t blocker;
  mol_dsp_limiter_t limiter;
  float low_energy = 0.0f;
  float high_energy = 0.0f;
  float blocked = 0.0f;
  mol_dsp_state_variable_configure(&state_variable, 48000u, 1200.0f, 0.4f);
  mol_dsp_biquad_low_pass(&low_pass, 48000u, 1200.0f, 0.707f);
  mol_dsp_biquad_high_pass(&high_pass, 48000u, 1200.0f, 0.707f);
  for (uint32_t index = 0u; index < 200000u; ++index) {
    float impulse = index == 0u ? 1.0f : 0.0f;
    float low = mol_dsp_biquad_process(&low_pass, impulse);
    float high = mol_dsp_biquad_process(&high_pass, impulse);
    float state = mol_dsp_state_variable_process(&state_variable, impulse, MOL_DSP_FILTER_LOW_PASS);
    EXPECT_TRUE(isfinite(low) && isfinite(high) && isfinite(state));
    low_energy += fabsf(low);
    high_energy += fabsf(high);
  }
  EXPECT_TRUE(low_energy > 0.1f && high_energy > 0.1f);

  mol_dsp_dc_blocker_configure(&blocker, 0.995f);
  for (uint32_t index = 0u; index < 10000u; ++index) {
    blocked = mol_dsp_dc_blocker_process(&blocker, 0.5f);
  }
  EXPECT_TRUE(fabsf(blocked) < 0.0001f);

  mol_dsp_limiter_configure(&limiter, 48000u, -1.0f, 0.0001f, 0.05f);
  for (uint32_t index = 0u; index < 10000u; ++index) {
    float limited = mol_dsp_limiter_process(&limiter, index == 9999u ? NAN : 4.0f);
    EXPECT_TRUE(isfinite(limited));
    EXPECT_TRUE(fabsf(limited) <= mol_dsp_db_to_linear(-1.0f) + 0.0001f);
  }
  EXPECT_TRUE(fabsf(mol_dsp_soft_saturate(100.0f, 2.0f)) < 1.0f);
}

static void test_generators(void) {
  mol_dsp_lfo_t lfo;
  mol_dsp_fm2_t fm;
  mol_dsp_additive_t additive;
  mol_dsp_karplus_strong_t pluck;
  mol_dsp_modal_bank_t modal;
  float pluck_buffer[512];
  const float ratios[4] = {1.0f, 2.0f, 3.01f, 4.2f};
  const float gains[4] = {1.0f, 0.5f, 0.25f, 0.125f};
  const float decays[4] = {0.4f, 0.25f, 0.18f, 0.12f};
  double fm_energy = 0.0;
  double additive_energy = 0.0;
  double pluck_early = 0.0;
  double pluck_late = 0.0;
  double modal_energy = 0.0;
  mol_dsp_lfo_configure(&lfo, 100u, 1.0f, MOL_DSP_LFO_TRIANGLE, 0.0f);
  EXPECT_TRUE(fabsf(mol_dsp_lfo_process(&lfo) + 1.0f) < 0.0001f);
  mol_dsp_fm2_configure(&fm, 48000u, 440.0f, 2.0f, 3.0f);
  mol_dsp_additive_configure(&additive, 48000u, 220.0f, ratios, gains, 4u);
  mol_dsp_karplus_configure(&pluck, pluck_buffer, 512u, 48000u, 220.0f, 0.995f,
                            UINT32_C(0x12345678));
  mol_dsp_modal_configure(&modal, 48000u, 440.0f, ratios, gains, decays, 4u);
  EXPECT_TRUE(pluck.length >= 217u && pluck.length <= 219u);
  for (uint32_t index = 0u; index < 48000u; ++index) {
    float fm_sample = mol_dsp_fm2_process(&fm);
    float additive_sample = mol_dsp_additive_process(&additive);
    float pluck_sample = mol_dsp_karplus_process(&pluck);
    float modal_sample = mol_dsp_modal_process(&modal, index == 0u ? 1.0f : 0.0f);
    EXPECT_TRUE(isfinite(fm_sample) && isfinite(additive_sample));
    EXPECT_TRUE(isfinite(pluck_sample) && isfinite(modal_sample));
    fm_energy += fabs((double)fm_sample);
    additive_energy += fabs((double)additive_sample);
    modal_energy += fabs((double)modal_sample);
    if (index < 1000u) {
      pluck_early += fabs((double)pluck_sample);
    }
    if (index >= 47000u) {
      pluck_late += fabs((double)pluck_sample);
    }
  }
  EXPECT_TRUE(fm_energy > 1000.0 && additive_energy > 1000.0 && modal_energy > 1.0);
  EXPECT_TRUE(pluck_early > pluck_late);
}

int main(void) {
  test_conversion_phase_and_oscillators();
  test_noise_envelopes_and_smoothing();
  test_filters_and_dynamics();
  test_generators();
  if (failures != 0) {
    (void)fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
