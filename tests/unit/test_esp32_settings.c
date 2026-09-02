/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "device_settings.h"

static int failures;

#define EXPECT_TRUE(condition)                                                           \
  do {                                                                                   \
    if (!(condition)) {                                                                  \
      fprintf(stderr, "%s:%d expectation failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                        \
    }                                                                                    \
  } while (0)

static void refresh_crc(uint8_t encoded[MOL_DEVICE_SETTINGS_RECORD_SIZE]) {
  uint32_t crc = UINT32_C(0xffffffff);
  size_t byte;
  for (byte = 0u; byte < 124u; ++byte) {
    uint32_t bit;
    crc ^= encoded[byte];
    for (bit = 0u; bit < 8u; ++bit) {
      const uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
      crc = (crc >> 1u) ^ (UINT32_C(0xedb88320) & mask);
    }
  }
  crc = ~crc;
  encoded[124] = (uint8_t)crc;
  encoded[125] = (uint8_t)(crc >> 8u);
  encoded[126] = (uint8_t)(crc >> 16u);
  encoded[127] = (uint8_t)(crc >> 24u);
}

static void test_default_round_trip(void) {
  uint8_t encoded[MOL_DEVICE_SETTINGS_RECORD_SIZE];
  uint8_t second[MOL_DEVICE_SETTINGS_RECORD_SIZE];
  mol_device_settings_t settings = mol_device_settings_default();
  mol_device_settings_t decoded;
  settings.generation = 42u;
  EXPECT_TRUE(mol_device_settings_validate(&settings) == MOL_OK);
  EXPECT_TRUE(mol_device_settings_encode(&settings, encoded) == MOL_OK);
  EXPECT_TRUE(mol_device_settings_decode(encoded, sizeof(encoded), &decoded) == MOL_OK);
  EXPECT_TRUE(decoded.generation == 42u);
  EXPECT_TRUE(decoded.master_gain == settings.master_gain);
  EXPECT_TRUE(decoded.preset == MOL_PRESET_GRAND_PIANO);
  EXPECT_TRUE(decoded.tempo == MOL_TEMPO_DEFAULT);
  EXPECT_TRUE(decoded.time_signature_numerator == 4u);
  EXPECT_TRUE(decoded.time_signature_denominator == 4u);
  EXPECT_TRUE(decoded.output_mode == MOL_DEVICE_OUTPUT_I2S);
  EXPECT_TRUE(mol_device_settings_encode(&decoded, second) == MOL_OK);
  EXPECT_TRUE(memcmp(encoded, second, sizeof(encoded)) == 0);
}

static void test_non_default_round_trip(void) {
  uint8_t encoded[MOL_DEVICE_SETTINGS_RECORD_SIZE];
  mol_device_settings_t settings = mol_device_settings_default();
  mol_device_settings_t decoded;
  const uint8_t address[6] = {0x10u, 0x20u, 0x30u, 0x40u, 0x50u, 0x60u};
  settings.master_gain = 1.25f;
  settings.preset = MOL_PRESET_MUSIC_BOX;
  settings.octave_shift = -3;
  settings.transpose = 24;
  settings.scale_type = MOL_SCALE_MIXOLYDIAN;
  settings.scale_tonic = 11u;
  settings.scale_mapping = (uint8_t)MOL_SCALE_MAP_UP;
  settings.chord_mode = MOL_CHORD_MINOR_7;
  settings.arpeggiator_mode = MOL_ARPEGGIATOR_RANDOM_DETERMINISTIC;
  settings.arpeggiator_rate = MOL_ARPEGGIATOR_RATE_THIRTY_SECOND;
  settings.arpeggiator_gate = 0.75f;
  settings.arpeggiator_octaves = 4u;
  settings.arpeggiator_random_seed = 123u;
  settings.tempo = 299.5f;
  settings.time_signature_numerator = 6u;
  settings.time_signature_denominator = 8u;
  settings.metronome_enabled = 1u;
  settings.metronome_level = 0.9f;
  settings.portamento_mode = MOL_PORTAMENTO_ALWAYS;
  settings.portamento_time_ms = 2000.0f;
  settings.output_mode = MOL_DEVICE_OUTPUT_A2DP;
  settings.web_ui_enabled = 0u;
  settings.paired_peer_valid = 1u;
  memcpy(settings.paired_peer_address, address, sizeof(address));
  EXPECT_TRUE(mol_device_settings_encode(&settings, encoded) == MOL_OK);
  EXPECT_TRUE(mol_device_settings_decode(encoded, sizeof(encoded), &decoded) == MOL_OK);
  EXPECT_TRUE(memcmp(&decoded, &settings, sizeof(settings)) == 0);
}

static void test_corruption_is_rejected(void) {
  uint8_t encoded[MOL_DEVICE_SETTINGS_RECORD_SIZE];
  mol_device_settings_t settings = mol_device_settings_default();
  mol_device_settings_t decoded;
  size_t index;
  EXPECT_TRUE(mol_device_settings_encode(&settings, encoded) == MOL_OK);
  for (index = 0u; index < sizeof(encoded); ++index) {
    encoded[index] ^= 0x80u;
    EXPECT_TRUE(mol_device_settings_decode(encoded, sizeof(encoded), &decoded) ==
                MOL_ERROR_CORRUPT_DATA);
    encoded[index] ^= 0x80u;
  }
  EXPECT_TRUE(mol_device_settings_decode(encoded, sizeof(encoded) - 1u, &decoded) ==
              MOL_ERROR_CORRUPT_DATA);
  encoded[4] = 2u;
  refresh_crc(encoded);
  EXPECT_TRUE(mol_device_settings_decode(encoded, sizeof(encoded), &decoded) ==
              MOL_ERROR_UNSUPPORTED_VERSION);
}

static void test_noncanonical_fields_are_rejected(void) {
  uint8_t encoded[MOL_DEVICE_SETTINGS_RECORD_SIZE];
  mol_device_settings_t settings = mol_device_settings_default();
  mol_device_settings_t decoded;
  EXPECT_TRUE(mol_device_settings_encode(&settings, encoded) == MOL_OK);
  encoded[37] = 1u;
  refresh_crc(encoded);
  EXPECT_TRUE(mol_device_settings_decode(encoded, sizeof(encoded), &decoded) ==
              MOL_ERROR_CORRUPT_DATA);
  EXPECT_TRUE(mol_device_settings_encode(&settings, encoded) == MOL_OK);
  encoded[114] = 1u;
  refresh_crc(encoded);
  EXPECT_TRUE(mol_device_settings_decode(encoded, sizeof(encoded), &decoded) ==
              MOL_ERROR_CORRUPT_DATA);
}

static void test_invalid_values_are_rejected(void) {
  mol_device_settings_t settings = mol_device_settings_default();
  settings.master_gain = NAN;
  EXPECT_TRUE(mol_device_settings_validate(&settings) == MOL_ERROR_INVALID_ARGUMENT);
  settings = mol_device_settings_default();
  settings.octave_shift = 4;
  EXPECT_TRUE(mol_device_settings_validate(&settings) == MOL_ERROR_INVALID_ARGUMENT);
  settings = mol_device_settings_default();
  settings.arpeggiator_gate = 0.049f;
  EXPECT_TRUE(mol_device_settings_validate(&settings) == MOL_ERROR_INVALID_ARGUMENT);
  settings = mol_device_settings_default();
  settings.tempo = 301.0f;
  EXPECT_TRUE(mol_device_settings_validate(&settings) == MOL_ERROR_INVALID_ARGUMENT);
  settings = mol_device_settings_default();
  settings.time_signature_numerator = 7u;
  EXPECT_TRUE(mol_device_settings_validate(&settings) == MOL_ERROR_INVALID_ARGUMENT);
  settings = mol_device_settings_default();
  settings.portamento_time_ms = 2001.0f;
  EXPECT_TRUE(mol_device_settings_validate(&settings) == MOL_ERROR_INVALID_ARGUMENT);
  settings = mol_device_settings_default();
  settings.paired_peer_address[0] = 1u;
  EXPECT_TRUE(mol_device_settings_validate(&settings) == MOL_ERROR_INVALID_ARGUMENT);
  settings.paired_peer_valid = 1u;
  EXPECT_TRUE(mol_device_settings_validate(&settings) == MOL_OK);
}

int main(void) {
  test_default_round_trip();
  test_non_default_round_trip();
  test_corruption_is_rejected();
  test_noncanonical_fields_are_rejected();
  test_invalid_values_are_rejected();
  if (failures != 0) {
    fprintf(stderr, "%d ESP32 settings test(s) failed\n", failures);
    return 1;
  }
  puts("ESP32 settings tests passed");
  return 0;
}
