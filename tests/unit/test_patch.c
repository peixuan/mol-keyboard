/* SPDX-License-Identifier: Apache-2.0 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mol/mol.h"

static int failures = 0;

#define EXPECT_TRUE(condition)                                                                  \
  do {                                                                                          \
    if (!(condition)) {                                                                         \
      (void)fprintf(stderr, "%s:%d: expectation failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                               \
    }                                                                                           \
  } while (0)

static const char valid_patch[] =
    "{\n"
    "  \"format_version\": 1,\n"
    "  \"id\": \"grand-piano\",\n"
    "  \"name_zh\": \"Grand\",\n"
    "  \"name_en\": \"Grand Piano\",\n"
    "  \"synthesis\": \"modal\",\n"
    "  \"waveform\": \"triangle\",\n"
    "  \"gain_millidb\": -8000,\n"
    "  \"attack_ms\": 4,\n"
    "  \"decay_ms\": 900,\n"
    "  \"sustain_milli\": 120,\n"
    "  \"release_ms\": 700,\n"
    "  \"filter_cutoff_hz\": 7600,\n"
    "  \"filter_resonance_milli\": 180,\n"
    "  \"oscillator_mix_milli\": 700,\n"
    "  \"detune_cents\": 2,\n"
    "  \"pulse_width_milli\": 500,\n"
    "  \"model_parameter_1_milli\": 2350,\n"
    "  \"model_parameter_2_milli\": 720,\n"
    "  \"vibrato_rate_millihz\": 0,\n"
    "  \"vibrato_depth_cents\": 0,\n"
    "  \"velocity_curve_milli\": 1250,\n"
    "  \"noise_mix_milli\": 20,\n"
    "  \"saturation_milli\": 450,\n"
    "  \"chorus_send_milli\": 80,\n"
    "  \"delay_send_milli\": 30,\n"
    "  \"reverb_send_milli\": 220\n"
    "}\n";

static void test_compile_encode_decode(void) {
  mol_patch_t first = {0};
  mol_patch_t second = {0};
  uint8_t first_binary[MOL_PATCH_BINARY_SIZE];
  uint8_t second_binary[MOL_PATCH_BINARY_SIZE];
  size_t first_size = 0u;
  size_t second_size = 0u;
  first.struct_size = (uint32_t)sizeof(first);
  second.struct_size = (uint32_t)sizeof(second);
  EXPECT_TRUE(mol_patch_compile_json(valid_patch, sizeof(valid_patch) - 1u, &first) == MOL_OK);
  EXPECT_TRUE(first.synthesis_model == MOL_SYNTHESIS_MODAL);
  EXPECT_TRUE(first.feature_flags ==
              (MOL_PATCH_FEATURE_OSCILLATOR | MOL_PATCH_FEATURE_FILTER | MOL_PATCH_FEATURE_NOISE |
               MOL_PATCH_FEATURE_MODAL | MOL_PATCH_FEATURE_CHORUS | MOL_PATCH_FEATURE_DELAY |
               MOL_PATCH_FEATURE_REVERB));
  EXPECT_TRUE(first.preset_id_hash == mol_patch_id_hash("grand-piano"));
  EXPECT_TRUE(mol_patch_encode(&first, first_binary, sizeof(first_binary), &first_size) == MOL_OK);
  EXPECT_TRUE(mol_patch_encode(&first, second_binary, sizeof(second_binary), &second_size) ==
              MOL_OK);
  EXPECT_TRUE(first_size == MOL_PATCH_BINARY_SIZE && first_size == second_size);
  EXPECT_TRUE(memcmp(first_binary, second_binary, first_size) == 0);
  second.struct_size = (uint32_t)sizeof(second);
  EXPECT_TRUE(mol_patch_decode(first_binary, first_size, &second) == MOL_OK);
  EXPECT_TRUE(second.preset_id_hash == first.preset_id_hash);
  EXPECT_TRUE(second.model_parameter_1_milli == first.model_parameter_1_milli);
  EXPECT_TRUE(second.reverb_send_milli == first.reverb_send_milli);
}

static void test_rejected_inputs(void) {
  mol_patch_t patch = {0};
  uint8_t binary[MOL_PATCH_BINARY_SIZE];
  size_t size = 0u;
  char unknown[sizeof(valid_patch) + 32u];
  patch.struct_size = (uint32_t)sizeof(patch);
  EXPECT_TRUE(mol_patch_compile_json("{}", 2u, &patch) == MOL_ERROR_INVALID_ARGUMENT);
  (void)snprintf(unknown, sizeof(unknown), "{\"unknown\":1,%s", valid_patch + 1u);
  patch.struct_size = (uint32_t)sizeof(patch);
  EXPECT_TRUE(mol_patch_compile_json(unknown, strlen(unknown), &patch) ==
              MOL_ERROR_INVALID_ARGUMENT);
  patch.struct_size = (uint32_t)sizeof(patch);
  EXPECT_TRUE(mol_patch_compile_json(valid_patch, sizeof(valid_patch) - 1u, &patch) == MOL_OK);
  EXPECT_TRUE(mol_patch_encode(&patch, binary, sizeof(binary), &size) == MOL_OK);
  patch.struct_size = (uint32_t)sizeof(patch);
  EXPECT_TRUE(mol_patch_decode(binary, size - 1u, &patch) == MOL_ERROR_INVALID_ARGUMENT);
  patch.struct_size = (uint32_t)sizeof(patch);
  EXPECT_TRUE(mol_patch_decode(binary, size + 1u, &patch) == MOL_ERROR_INVALID_ARGUMENT);
  binary[4] = 2u;
  patch.struct_size = (uint32_t)sizeof(patch);
  EXPECT_TRUE(mol_patch_decode(binary, size, &patch) == MOL_ERROR_UNSUPPORTED_VERSION);
  binary[4] = 1u;
  binary[MOL_PATCH_BINARY_HEADER_SIZE + 3u] ^= 0x55u;
  patch.struct_size = (uint32_t)sizeof(patch);
  EXPECT_TRUE(mol_patch_decode(binary, size, &patch) == MOL_ERROR_INVALID_ARGUMENT);

  EXPECT_TRUE(mol_patch_id_hash(NULL) == 0u);
  EXPECT_TRUE(mol_patch_validate(NULL) == MOL_ERROR_INVALID_ARGUMENT);
  memset(&patch, 0, sizeof(patch));
  patch.struct_size = (uint32_t)sizeof(patch) - 1u;
  EXPECT_TRUE(mol_patch_validate(&patch) == MOL_ERROR_INVALID_ARGUMENT);
  patch.struct_size = (uint32_t)sizeof(patch);
  patch.api_version = MOL_API_VERSION + 1u;
  EXPECT_TRUE(mol_patch_validate(&patch) == MOL_ERROR_UNSUPPORTED_VERSION);

  EXPECT_TRUE(mol_patch_compile_json(NULL, 1u, &patch) == MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_patch_compile_json(valid_patch, 0u, &patch) == MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_patch_compile_json(valid_patch, MOL_PATCH_MAX_JSON_SIZE + 1u, &patch) ==
              MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_patch_compile_json(valid_patch, sizeof(valid_patch) - 1u, NULL) ==
              MOL_ERROR_INVALID_ARGUMENT);
  patch.struct_size = 0u;
  EXPECT_TRUE(mol_patch_compile_json(valid_patch, sizeof(valid_patch) - 1u, &patch) ==
              MOL_ERROR_INVALID_ARGUMENT);
  patch.struct_size = (uint32_t)sizeof(patch);
  EXPECT_TRUE(mol_patch_compile_json("[]", 2u, &patch) == MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_patch_compile_json("{1}", strlen("{1}"), &patch) == MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_patch_compile_json("{\"id\" \"grand-piano\"}", strlen("{\"id\" \"grand-piano\"}"),
                                     &patch) == MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_patch_compile_json("{\"\\u12xz\":1}", strlen("{\"\\u12xz\":1}"), &patch) ==
              MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_patch_compile_json("{\"bad\\q\":1}", strlen("{\"bad\\q\":1}"), &patch) ==
              MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_patch_compile_json("{\"bad\\", strlen("{\"bad\\"), &patch) ==
              MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_patch_compile_json("{\"id\":\"unterminated", strlen("{\"id\":\"unterminated"),
                                     &patch) == MOL_ERROR_INVALID_ARGUMENT);

  EXPECT_TRUE(mol_patch_encode(&patch, binary, sizeof(binary), NULL) == MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(mol_patch_compile_json(valid_patch, sizeof(valid_patch) - 1u, &patch) == MOL_OK);
  patch.api_version = MOL_API_VERSION + 1u;
  EXPECT_TRUE(mol_patch_encode(&patch, binary, sizeof(binary), &size) ==
              MOL_ERROR_UNSUPPORTED_VERSION);
  patch.api_version = MOL_API_VERSION;
  EXPECT_TRUE(mol_patch_encode(&patch, NULL, 0u, &size) == MOL_ERROR_BUFFER_TOO_SMALL);
  EXPECT_TRUE(mol_patch_decode(NULL, size, &patch) == MOL_ERROR_INVALID_ARGUMENT);
  patch.struct_size = 0u;
  EXPECT_TRUE(mol_patch_decode(binary, size, &patch) == MOL_ERROR_INVALID_ARGUMENT);
}

static void test_synthesis_feature_derivation(void) {
  static const char* const models[MOL_SYNTHESIS_MODEL_COUNT] = {"subtractive", "fm2",   "additive",
                                                                "pluck",       "modal", "formant"};
  const char* marker = strstr(valid_patch, "\"modal\"");
  char variant[sizeof(valid_patch) + 32u];
  EXPECT_TRUE(marker != NULL);
  if (marker == NULL) return;
  for (uint32_t model = 0u; model < MOL_SYNTHESIS_MODEL_COUNT; ++model) {
    mol_patch_t patch = {0};
    const int written =
        snprintf(variant, sizeof(variant), "%.*s\"%s\"%s", (int)(marker - valid_patch), valid_patch,
                 models[model], marker + strlen("\"modal\""));
    patch.struct_size = (uint32_t)sizeof(patch);
    EXPECT_TRUE(written > 0 && (size_t)written < sizeof(variant));
    EXPECT_TRUE(mol_patch_compile_json(variant, (size_t)written, &patch) == MOL_OK);
    EXPECT_TRUE(patch.synthesis_model == model);
  }
}

static void test_metadata(void) {
  uint32_t distinct_hashes[MOL_PRESET_COUNT];
  for (uint32_t preset = 0u; preset < MOL_PRESET_COUNT; ++preset) {
    const char* id = mol_preset_stable_id(preset);
    mol_patch_t patch = {0};
    size_t binary_size = 0u;
    EXPECT_TRUE(id != NULL && mol_preset_english_name(preset) != NULL &&
                mol_preset_chinese_name(preset) != NULL);
    distinct_hashes[preset] = mol_patch_id_hash(id);
    EXPECT_TRUE(distinct_hashes[preset] != 0u);
    EXPECT_TRUE(mol_builtin_patch_binary(preset, &binary_size) != NULL);
    EXPECT_TRUE(binary_size == MOL_PATCH_BINARY_SIZE);
    patch.struct_size = (uint32_t)sizeof(patch);
    EXPECT_TRUE(mol_builtin_patch_load(preset, &patch) == MOL_OK);
    EXPECT_TRUE(patch.preset_id_hash == distinct_hashes[preset]);
    for (uint32_t prior = 0u; prior < preset; ++prior) {
      EXPECT_TRUE(distinct_hashes[preset] != distinct_hashes[prior]);
    }
  }
  EXPECT_TRUE(mol_preset_stable_id(MOL_PRESET_COUNT) == NULL);
  EXPECT_TRUE(mol_builtin_patch_binary(MOL_PRESET_COUNT, NULL) == NULL);
}

int main(void) {
  test_compile_encode_decode();
  test_rejected_inputs();
  test_synthesis_feature_derivation();
  test_metadata();
  if (failures != 0) {
    (void)fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
