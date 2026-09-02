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
  const uint8_t sink_address[6] = {0x60u, 0x50u, 0x40u, 0x30u, 0x20u, 0x10u};
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
  settings.a2dp_sink_valid = 1u;
  memcpy(settings.a2dp_sink_address, sink_address, sizeof(sink_address));
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
  encoded[4] = 3u;
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
  encoded[121] = 1u;
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
  settings = mol_device_settings_default();
  settings.a2dp_sink_address[5] = 1u;
  EXPECT_TRUE(mol_device_settings_validate(&settings) == MOL_ERROR_INVALID_ARGUMENT);
  settings.a2dp_sink_valid = 1u;
  EXPECT_TRUE(mol_device_settings_validate(&settings) == MOL_OK);
}

static void test_version_one_migrates_without_a2dp_sink(void) {
  uint8_t encoded[MOL_DEVICE_SETTINGS_RECORD_SIZE];
  mol_device_settings_t settings = mol_device_settings_default();
  mol_device_settings_t decoded;
  const uint8_t keyboard_address[6] = {1u, 2u, 3u, 4u, 5u, 6u};
  settings.generation = 17u;
  settings.paired_peer_valid = 1u;
  memcpy(settings.paired_peer_address, keyboard_address, sizeof(keyboard_address));
  EXPECT_TRUE(mol_device_settings_encode(&settings, encoded) == MOL_OK);
  encoded[4] = 1u;
  encoded[5] = 0u;
  encoded[6] = 0u;
  encoded[7] = 0u;
  memset(encoded + 114u, 0, 10u);
  refresh_crc(encoded);
  EXPECT_TRUE(mol_device_settings_decode(encoded, sizeof(encoded), &decoded) == MOL_OK);
  EXPECT_TRUE(decoded.generation == 17u);
  EXPECT_TRUE(decoded.paired_peer_valid == 1u);
  EXPECT_TRUE(memcmp(decoded.paired_peer_address, keyboard_address, sizeof(keyboard_address)) == 0);
  EXPECT_TRUE(decoded.a2dp_sink_valid == 0u);
  EXPECT_TRUE(memcmp(decoded.a2dp_sink_address, "\0\0\0\0\0\0", 6u) == 0);
}

static void test_command_compilation(void) {
  mol_device_settings_t settings = mol_device_settings_default();
  mol_command_t commands[MOL_DEVICE_SETTINGS_COMMAND_COUNT];
  mol_command_t untouched;
  size_t count = 0u;
  memset(&untouched, 0xa5, sizeof(untouched));
  settings.master_gain = 0.75f;
  settings.preset = MOL_PRESET_VIOLIN;
  settings.octave_shift = 2;
  settings.transpose = -5;
  settings.scale_type = MOL_SCALE_DORIAN;
  settings.scale_tonic = 3u;
  settings.scale_mapping = (uint8_t)MOL_SCALE_MAP_DOWN;
  settings.chord_mode = MOL_CHORD_POWER_5;
  settings.arpeggiator_mode = MOL_ARPEGGIATOR_UP_DOWN;
  settings.arpeggiator_rate = MOL_ARPEGGIATOR_RATE_EIGHTH_TRIPLET;
  settings.arpeggiator_gate = 0.6f;
  settings.arpeggiator_octaves = 3u;
  settings.arpeggiator_random_seed = 99u;
  settings.tempo = 140.0f;
  settings.time_signature_numerator = 5u;
  settings.metronome_enabled = 1u;
  settings.metronome_level = 0.7f;
  settings.portamento_mode = MOL_PORTAMENTO_LEGATO_ONLY;
  settings.portamento_time_ms = 45.0f;
  EXPECT_TRUE(mol_device_settings_compile_commands(&settings, &untouched, 1u, &count) ==
              MOL_ERROR_BUFFER_TOO_SMALL);
  EXPECT_TRUE(count == MOL_DEVICE_SETTINGS_COMMAND_COUNT);
  {
    mol_command_t expected;
    memset(&expected, 0xa5, sizeof(expected));
    EXPECT_TRUE(memcmp(&untouched, &expected, sizeof(expected)) == 0);
  }
  EXPECT_TRUE(mol_device_settings_compile_commands(
                  &settings, commands, MOL_DEVICE_SETTINGS_COMMAND_COUNT, &count) == MOL_OK);
  EXPECT_TRUE(count == MOL_DEVICE_SETTINGS_COMMAND_COUNT);
  EXPECT_TRUE(commands[0].command_type == MOL_COMMAND_SET_MASTER_GAIN &&
              commands[0].payload.scalar.value == 0.75f);
  EXPECT_TRUE(commands[1].command_type == MOL_COMMAND_SET_PRESET &&
              commands[1].payload.preset.preset == MOL_PRESET_VIOLIN &&
              commands[1].payload.preset.hard_switch == 1u);
  EXPECT_TRUE(commands[2].payload.integer.value == 2);
  EXPECT_TRUE(commands[3].payload.integer.value == -5);
  EXPECT_TRUE(commands[4].payload.scale.type == MOL_SCALE_DORIAN &&
              commands[4].payload.scale.tonic == 3u &&
              commands[4].payload.scale.mapping == MOL_SCALE_MAP_DOWN);
  EXPECT_TRUE(commands[5].payload.integer.value == (int32_t)MOL_CHORD_POWER_5);
  EXPECT_TRUE(commands[6].payload.arpeggiator.mode == MOL_ARPEGGIATOR_UP_DOWN &&
              commands[6].payload.arpeggiator.rate == MOL_ARPEGGIATOR_RATE_EIGHTH_TRIPLET &&
              commands[6].payload.arpeggiator.gate == 0.6f &&
              commands[6].payload.arpeggiator.octaves == 3u &&
              commands[6].payload.arpeggiator.random_seed == 99u);
  EXPECT_TRUE(commands[7].payload.scalar.value == 140.0f);
  EXPECT_TRUE(commands[8].payload.time_signature.numerator == 5u &&
              commands[8].payload.time_signature.denominator == 4u);
  EXPECT_TRUE(commands[9].payload.metronome.enabled == 1u &&
              commands[9].payload.metronome.level == 0.7f);
  EXPECT_TRUE(commands[10].payload.portamento.mode == MOL_PORTAMENTO_LEGATO_ONLY &&
              commands[10].payload.portamento.time_ms == 45.f);
  EXPECT_TRUE(commands[10].struct_size == sizeof(mol_command_t) &&
              commands[10].api_version == MOL_API_VERSION &&
              commands[10].target_frame == MOL_FRAME_IMMEDIATE);
}

int main(void) {
  test_default_round_trip();
  test_non_default_round_trip();
  test_corruption_is_rejected();
  test_noncanonical_fields_are_rejected();
  test_invalid_values_are_rejected();
  test_version_one_migrates_without_a2dp_sink();
  test_command_compilation();
  if (failures != 0) {
    fprintf(stderr, "%d ESP32 settings test(s) failed\n", failures);
    return 1;
  }
  puts("ESP32 settings tests passed");
  return 0;
}
