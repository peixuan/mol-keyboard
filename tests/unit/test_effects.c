/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mol/mol.h"
#include "mol_effects.h"

static int failures = 0;

typedef union effect_engine_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[1048576];
} effect_engine_storage_t;

static effect_engine_storage_t engine_storage;

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

static mol_command_t engine_command(mol_command_type_t type) {
  mol_command_t command;
  memset(&command, 0, sizeof(command));
  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = type;
  command.target_frame = MOL_FRAME_IMMEDIATE;
  return command;
}

static void test_engine_effect_chain(void) {
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  mol_capability_flags_t effect_capabilities = MOL_CAPABILITY_LIMITER;
  float stereo[512];
  float peak = 0.0f;
  float difference = 0.0f;
  config.max_voices = 8u;
  config.command_capacity = 64u;
  config.event_capacity = 64u;
#if MOL_ENABLE_CHORUS
  effect_capabilities |= MOL_CAPABILITY_CHORUS | MOL_CAPABILITY_STEREO_EFFECTS;
#endif
#if MOL_ENABLE_DELAY
  effect_capabilities |= MOL_CAPABILITY_DELAY;
#endif
#if MOL_ENABLE_REVERB
  effect_capabilities |= MOL_CAPABILITY_REVERB | MOL_CAPABILITY_STEREO_EFFECTS;
#endif
  EXPECT_TRUE(mol_engine_init(engine_storage.bytes, sizeof(engine_storage.bytes), &config,
                              &engine) == MOL_OK);
  EXPECT_TRUE((mol_engine_get_capabilities(engine) & effect_capabilities) == effect_capabilities);
  {
    mol_command_t parameter = engine_command(MOL_COMMAND_SET_PARAMETER);
    parameter.payload.parameter.parameter = MOL_PARAMETER_LIMITER_CEILING_DB;
    parameter.payload.parameter.value = -6.0f;
    EXPECT_TRUE(mol_engine_submit(engine, &parameter) == MOL_OK);
    parameter.payload.parameter.parameter = MOL_PARAMETER_DELAY_SYNC_BEATS;
    parameter.payload.parameter.value = 0.25f;
#if MOL_ENABLE_DELAY
    EXPECT_TRUE(mol_engine_submit(engine, &parameter) == MOL_OK);
#else
    EXPECT_TRUE(mol_engine_submit(engine, &parameter) == MOL_ERROR_UNSUPPORTED);
#endif
    parameter.payload.parameter.parameter = MOL_PARAMETER_DELAY_FEEDBACK;
    parameter.payload.parameter.value = 1.0f;
#if MOL_ENABLE_DELAY
    EXPECT_TRUE(mol_engine_submit(engine, &parameter) == MOL_ERROR_INVALID_ARGUMENT);
#else
    EXPECT_TRUE(mol_engine_submit(engine, &parameter) == MOL_ERROR_UNSUPPORTED);
#endif
    parameter.payload.parameter.parameter = 999u;
    parameter.payload.parameter.value = 0.5f;
    EXPECT_TRUE(mol_engine_submit(engine, &parameter) == MOL_ERROR_UNSUPPORTED);
  }
  {
    mol_command_t gain = engine_command(MOL_COMMAND_SET_MASTER_GAIN);
    gain.payload.scalar.value = 2.0f;
    EXPECT_TRUE(mol_engine_submit(engine, &gain) == MOL_OK);
  }
  for (uint32_t note_index = 0u; note_index < 8u; ++note_index) {
    mol_command_t note = engine_command(MOL_COMMAND_NOTE_ON);
    note.gesture_id = 100u + note_index;
    note.payload.note.note = (uint8_t)(48u + note_index * 4u);
    note.payload.note.velocity = 1.0f;
    EXPECT_TRUE(mol_engine_submit(engine, &note) == MOL_OK);
  }
  for (uint32_t block = 0u; block < 200u; ++block) {
    EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, stereo, 256u, 2u) == MOL_OK);
    for (uint32_t frame = 0u; frame < 256u; ++frame) {
      float left = stereo[frame * 2u];
      float right = stereo[frame * 2u + 1u];
      EXPECT_TRUE(isfinite(left) && isfinite(right));
      peak = fmaxf(peak, fmaxf(fabsf(left), fabsf(right)));
      difference += fabsf(left - right);
    }
  }
  EXPECT_TRUE(peak > 0.05f && peak <= mol_dsp_db_to_linear(-6.0f));
#if MOL_ENABLE_CHORUS || MOL_ENABLE_REVERB
  EXPECT_TRUE(difference > 0.001f);
#else
  EXPECT_TRUE(difference == 0.0f);
#endif
  {
    mol_command_t stop = engine_command(MOL_COMMAND_ALL_SOUND_OFF);
    EXPECT_TRUE(mol_engine_submit(engine, &stop) == MOL_OK);
    for (uint32_t block = 0u; block < 16u; ++block) {
      EXPECT_TRUE(mol_engine_render_interleaved_f32(engine, stereo, 256u, 2u) == MOL_OK);
    }
    EXPECT_TRUE(fabsf(stereo[510]) < 0.000001f && fabsf(stereo[511]) < 0.000001f);
  }
  mol_engine_shutdown(engine);
}

int main(void) {
  test_chorus();
  test_delay();
  test_reverb_impulse();
  test_engine_effect_chain();
  if (failures != 0) {
    (void)fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
