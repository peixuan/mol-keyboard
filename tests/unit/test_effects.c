/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "mol_effects.h"

static int failures = 0;

#define EXPECT_TRUE(condition)                                                                  \
  do {                                                                                          \
    if (!(condition)) {                                                                         \
      (void)fprintf(stderr, "%s:%d: expectation failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                               \
    }                                                                                           \
  } while (0)

static void test_chorus(void) {
  float memory[1024];
  mol_chorus_t chorus;
  float energy = 0.0f;
  float stereo_difference = 0.0f;
  mol_chorus_configure(&chorus, memory, 1024u, 8000u);
  mol_chorus_set(&chorus, -1.0f, 100.0f, 2.0f);
  EXPECT_TRUE(chorus.rate_hz.target == 0.05f && chorus.depth_ms.target == 10.0f &&
              chorus.mix.target == 1.0f);
  for (uint32_t frame = 0u; frame < 1000u; ++frame) {
    float left;
    float right;
    mol_chorus_process(&chorus, frame == 0u ? 1.0f : 0.0f, &left, &right);
    EXPECT_TRUE(isfinite(left) && isfinite(right));
    energy += fabsf(left) + fabsf(right);
    stereo_difference += fabsf(left - right);
  }
  EXPECT_TRUE(energy > 0.1f && stereo_difference > 0.01f);
  mol_chorus_clear(&chorus);
  {
    float left;
    float right;
    mol_chorus_process(&chorus, NAN, &left, &right);
    EXPECT_TRUE(left == 0.0f && right == 0.0f);
  }
}

static void test_delay(void) {
  float memory[2048];
  mol_delay_t delay;
  float energy = 0.0f;
  mol_delay_configure(&delay, memory, 2048u, 8000u);
  mol_delay_set(&delay, 40.0f, 4.0f, -1.0f);
  EXPECT_TRUE(delay.feedback.target == 0.95f && delay.mix.target == 0.0f);
  mol_delay_set(&delay, 40.0f, 0.6f, 0.5f);
  for (uint32_t frame = 0u; frame < 2000u; ++frame) {
    float output = mol_delay_process(&delay, frame == 0u ? 1.0f : 0.0f);
    EXPECT_TRUE(isfinite(output) && fabsf(output) <= 2.0f);
    energy += fabsf(output);
  }
  EXPECT_TRUE(energy > 0.1f);
  mol_delay_clear(&delay);
  EXPECT_TRUE(mol_delay_process(&delay, 0.0f) == 0.0f);
}

static void test_reverb_impulse(void) {
  float memory[4096];
  mol_reverb_t reverb;
  float energy = 0.0f;
  float stereo_difference = 0.0f;
  mol_reverb_configure(&reverb, memory, 4096u, 8000u, 1.0f);
  mol_reverb_set(&reverb, -10.0f, 2.0f, -1.0f, 2.0f);
  EXPECT_TRUE(reverb.predelay_frames.target == 0.0f && reverb.size.target == 1.0f &&
              reverb.damping.target == 0.0f && reverb.mix.target == 1.0f);
  for (uint32_t frame = 0u; frame < 16000u; ++frame) {
    float left;
    float right;
    mol_reverb_process(&reverb, frame == 0u ? 1.0f : 0.0f, &left, &right);
    EXPECT_TRUE(isfinite(left) && isfinite(right) && fabsf(left) < 8.0f && fabsf(right) < 8.0f);
    energy += fabsf(left) + fabsf(right);
    stereo_difference += fabsf(left - right);
  }
  EXPECT_TRUE(energy > 0.1f && stereo_difference > 0.01f);
  mol_reverb_clear(&reverb);
  {
    float left;
    float right;
    mol_reverb_process(&reverb, 0.0f, &left, &right);
    EXPECT_TRUE(left == 0.0f && right == 0.0f);
  }
}

int main(void) {
  test_chorus();
  test_delay();
  test_reverb_impulse();
  if (failures != 0) {
    (void)fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
