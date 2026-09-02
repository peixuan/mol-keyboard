/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_ESP32_DEVICE_SETTINGS_H_
#define MOL_ESP32_DEVICE_SETTINGS_H_

#include <stddef.h>
#include <stdint.h>

#include "mol/mol.h"

#define MOL_DEVICE_SETTINGS_RECORD_SIZE 128u
#define MOL_DEVICE_SETTINGS_VERSION 1u

typedef uint32_t mol_device_output_mode_t;
enum { MOL_DEVICE_OUTPUT_I2S = 0u, MOL_DEVICE_OUTPUT_A2DP = 1u, MOL_DEVICE_OUTPUT_MODE_COUNT = 2u };

typedef struct mol_device_settings {
  uint32_t generation;
  float master_gain;
  mol_preset_id_t preset;
  int32_t octave_shift;
  int32_t transpose;
  mol_scale_type_t scale_type;
  uint8_t scale_tonic;
  uint8_t scale_mapping;
  mol_chord_mode_t chord_mode;
  mol_arpeggiator_mode_t arpeggiator_mode;
  mol_arpeggiator_rate_t arpeggiator_rate;
  float arpeggiator_gate;
  uint8_t arpeggiator_octaves;
  uint32_t arpeggiator_random_seed;
  float tempo;
  uint8_t time_signature_numerator;
  uint8_t time_signature_denominator;
  uint8_t metronome_enabled;
  float metronome_level;
  mol_portamento_mode_t portamento_mode;
  float portamento_time_ms;
  mol_device_output_mode_t output_mode;
  uint8_t web_ui_enabled;
  uint8_t paired_peer_valid;
  uint8_t paired_peer_address[6];
} mol_device_settings_t;

mol_device_settings_t mol_device_settings_default(void);
mol_result_t mol_device_settings_validate(const mol_device_settings_t* settings);
mol_result_t mol_device_settings_encode(const mol_device_settings_t* settings,
                                        uint8_t output[MOL_DEVICE_SETTINGS_RECORD_SIZE]);
mol_result_t mol_device_settings_decode(const uint8_t* input, size_t input_size,
                                        mol_device_settings_t* settings);

#endif /* MOL_ESP32_DEVICE_SETTINGS_H_ */
