/* SPDX-License-Identifier: Apache-2.0 */
#include <stdio.h>
#include <string.h>

#include "web_config_protocol.h"

static int failures;

#define EXPECT_TRUE(condition)                                                           \
  do {                                                                                   \
    if (!(condition)) {                                                                  \
      fprintf(stderr, "%s:%d expectation failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                        \
    }                                                                                    \
  } while (0)

static const char kToken[] = "0123456789abcdef0123456789abcdef";

static mol_web_form_result_t apply(const char* body, bool a2dp_supported,
                                   const mol_device_settings_t* current,
                                   mol_device_settings_t* output) {
  return mol_web_form_apply(body, strlen(body), kToken, a2dp_supported, current, output);
}

static void test_complete_patch(void) {
  mol_device_settings_t current = mol_device_settings_default();
  mol_device_settings_t output;
  const uint8_t peer[6] = {1u, 2u, 3u, 4u, 5u, 6u};
  memset(&output, 0xa5, sizeof(output));
  current.generation = 9u;
  current.paired_peer_valid = 1u;
  memcpy(current.paired_peer_address, peer, sizeof(peer));
  EXPECT_TRUE(apply("token=0123456789abcdef0123456789abcdef&gain=1.25&preset=17&octave=-3&"
                    "transpose=24&tempo=299.5&metronome=1&metronome_level=0.75&output=a2dp",
                    true, &current, &output) == MOL_WEB_FORM_OK);
  EXPECT_TRUE(output.generation == 9u);
  EXPECT_TRUE(output.master_gain == 1.25f);
  EXPECT_TRUE(output.preset == 17u);
  EXPECT_TRUE(output.octave_shift == -3);
  EXPECT_TRUE(output.transpose == 24);
  EXPECT_TRUE(output.tempo == 299.5f);
  EXPECT_TRUE(output.metronome_enabled == 1u);
  EXPECT_TRUE(output.metronome_level == 0.75f);
  EXPECT_TRUE(output.output_mode == MOL_DEVICE_OUTPUT_A2DP);
  EXPECT_TRUE(output.paired_peer_valid == 1u &&
              memcmp(output.paired_peer_address, peer, sizeof(peer)) == 0);
}

static void test_partial_patch_preserves_unrelated_settings(void) {
  mol_device_settings_t current = mol_device_settings_default();
  mol_device_settings_t output;
  current.transpose = 7;
  current.chord_mode = MOL_CHORD_MAJOR_7;
  EXPECT_TRUE(apply("gain=0.5&token=0123456789abcdef0123456789abcdef", false, &current, &output) ==
              MOL_WEB_FORM_OK);
  EXPECT_TRUE(output.master_gain == 0.5f);
  EXPECT_TRUE(output.transpose == 7);
  EXPECT_TRUE(output.chord_mode == MOL_CHORD_MAJOR_7);
}

static void test_authentication_and_shape_rejections(void) {
  mol_device_settings_t current = mol_device_settings_default();
  mol_device_settings_t untouched;
  mol_device_settings_t expected;
  char oversized[MOL_WEB_FORM_MAX_BODY_SIZE + 2u];
  memset(&untouched, 0xa5, sizeof(untouched));
  expected = untouched;
  memset(oversized, 'x', sizeof(oversized));
  EXPECT_TRUE(apply("gain=1", false, &current, &untouched) == MOL_WEB_FORM_UNAUTHORIZED);
  EXPECT_TRUE(apply("token=1123456789abcdef0123456789abcdef&gain=1", false, &current, &untouched) ==
              MOL_WEB_FORM_UNAUTHORIZED);
  EXPECT_TRUE(apply("token=0123456789abcdef0123456789abcdef", false, &current, &untouched) ==
              MOL_WEB_FORM_NO_SETTINGS);
  EXPECT_TRUE(apply("token=0123456789abcdef0123456789abcdef&gain=1&gain=2", false, &current,
                    &untouched) == MOL_WEB_FORM_DUPLICATE_FIELD);
  EXPECT_TRUE(apply("token=0123456789abcdef0123456789abcdef&future=1", false, &current,
                    &untouched) == MOL_WEB_FORM_UNKNOWN_FIELD);
  EXPECT_TRUE(apply("token=0123456789abcdef0123456789abcdef&gain", false, &current, &untouched) ==
              MOL_WEB_FORM_MALFORMED);
  EXPECT_TRUE(apply("token=0123456789abcdef0123456789abcdef&gain=%0", false, &current,
                    &untouched) == MOL_WEB_FORM_MALFORMED);
  EXPECT_TRUE(apply("token=0123456789abcdef012345678678abcdef&gain=1&", false, &current,
                    &untouched) == MOL_WEB_FORM_MALFORMED);
  EXPECT_TRUE(mol_web_form_apply(oversized, sizeof(oversized), kToken, false, &current,
                                 &untouched) == MOL_WEB_FORM_TOO_LARGE);
  EXPECT_TRUE(memcmp(&untouched, &expected, sizeof(untouched)) == 0);
}

static void test_strict_numeric_ranges(void) {
  static const char* const invalid_forms[] = {
      "gain=2.01",        "gain=nan",       "gain=1e0",     "gain=.5",
      "preset=18",        "preset=-1",      "octave=4",     "transpose=-25",
      "tempo=29.999",     "tempo=300.1",    "metronome=on", "metronome_level=1.1",
      "output=bluetooth", "preset=9999999", "gain=1.",      "octave=+1"};
  mol_device_settings_t current = mol_device_settings_default();
  size_t index;
  for (index = 0u; index < sizeof(invalid_forms) / sizeof(invalid_forms[0]); ++index) {
    char body[160];
    mol_device_settings_t output;
    int length = snprintf(body, sizeof(body), "token=0123456789abcdef0123456789abcdef&%s",
                          invalid_forms[index]);
    EXPECT_TRUE(length > 0 && (size_t)length < sizeof(body));
    EXPECT_TRUE(apply(body, true, &current, &output) == MOL_WEB_FORM_INVALID_VALUE);
  }
  {
    mol_device_settings_t output;
    EXPECT_TRUE(apply("token=0123456789abcdef0123456789abcdef&output=a2dp", false, &current,
                      &output) == MOL_WEB_FORM_INVALID_VALUE);
    EXPECT_TRUE(apply("token=0123456789abcdef0123456789abcdef&tempo=30&gain=0", false, &current,
                      &output) == MOL_WEB_FORM_OK);
  }
}

static void test_percent_decoding_stays_strict(void) {
  mol_device_settings_t current = mol_device_settings_default();
  mol_device_settings_t output;
  EXPECT_TRUE(apply("tok%65n=0123456789abcdef0123456789abcdef&out%70ut=i2s", false, &current,
                    &output) == MOL_WEB_FORM_OK);
  EXPECT_TRUE(apply("token=0123456789abcdef0123456789abcdef&gain=%00", false, &current, &output) ==
              MOL_WEB_FORM_MALFORMED);
  EXPECT_TRUE(apply("token=0123456789abcdef0123456789abcdef&gain=1%262", false, &current,
                    &output) == MOL_WEB_FORM_INVALID_VALUE);
}

int main(void) {
  test_complete_patch();
  test_partial_patch_preserves_unrelated_settings();
  test_authentication_and_shape_rejections();
  test_strict_numeric_ranges();
  test_percent_decoding_stays_strict();
  if (failures != 0) {
    fprintf(stderr, "%d ESP32 Web protocol test(s) failed\n", failures);
    return 1;
  }
  puts("ESP32 Web protocol tests passed");
  return 0;
}
