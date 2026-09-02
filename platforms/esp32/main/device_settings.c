/* SPDX-License-Identifier: Apache-2.0 */
#include "device_settings.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

enum {
  MOL_SETTINGS_MAGIC = 0x534c4f4du,
  MOL_SETTINGS_PAYLOAD_SIZE = 112u,
  MOL_SETTINGS_CRC_OFFSET = 124u
};

static void write_u32(uint8_t* output, uint32_t value) {
  output[0] = (uint8_t)value;
  output[1] = (uint8_t)(value >> 8u);
  output[2] = (uint8_t)(value >> 16u);
  output[3] = (uint8_t)(value >> 24u);
}

static uint32_t read_u32(const uint8_t* input) {
  return (uint32_t)input[0] | ((uint32_t)input[1] << 8u) | ((uint32_t)input[2] << 16u) |
         ((uint32_t)input[3] << 24u);
}

static void write_i32(uint8_t* output, int32_t value) {
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));
  write_u32(output, bits);
}

static int32_t read_i32(const uint8_t* input) {
  const uint32_t bits = read_u32(input);
  int32_t value;
  memcpy(&value, &bits, sizeof(value));
  return value;
}

static void write_float(uint8_t* output, float value) {
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));
  write_u32(output, bits);
}

static float read_float(const uint8_t* input) {
  const uint32_t bits = read_u32(input);
  float value;
  memcpy(&value, &bits, sizeof(value));
  return value;
}

static uint32_t settings_crc32(const uint8_t* input, size_t size) {
  uint32_t crc = UINT32_C(0xffffffff);
  size_t index;
  for (index = 0u; index < size; ++index) {
    uint32_t bit;
    crc ^= input[index];
    for (bit = 0u; bit < 8u; ++bit) {
      const uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
      crc = (crc >> 1u) ^ (UINT32_C(0xedb88320) & mask);
    }
  }
  return ~crc;
}

static bool encoded_small_fields_are_canonical(const uint8_t* input, uint32_t version) {
  static const size_t offsets[] = {36u, 40u, 60u, 72u, 76u, 80u, 100u, 104u};
  size_t index;
  for (index = 0u; index < sizeof(offsets) / sizeof(offsets[0]); ++index) {
    if (read_u32(input + offsets[index]) > UINT8_MAX) {
      return false;
    }
  }
  if (version == 1u) {
    for (index = 114u; index < MOL_SETTINGS_CRC_OFFSET; ++index) {
      if (input[index] != 0u) {
        return false;
      }
    }
  } else if (read_u32(input + 120u) > UINT8_MAX) {
    return false;
  }
  return true;
}

static bool address_is_zero(const uint8_t* address, size_t size) {
  size_t index;
  for (index = 0u; index < size; ++index) {
    if (address[index] != 0u) {
      return false;
    }
  }
  return true;
}

mol_device_settings_t mol_device_settings_default(void) {
  mol_device_settings_t settings;
  memset(&settings, 0, sizeof(settings));
  settings.master_gain = 0.25f;
  settings.preset = MOL_PRESET_GRAND_PIANO;
  settings.scale_type = MOL_SCALE_CHROMATIC;
  settings.scale_mapping = (uint8_t)MOL_SCALE_MAP_NEAREST;
  settings.chord_mode = MOL_CHORD_OFF;
  settings.arpeggiator_mode = MOL_ARPEGGIATOR_OFF;
  settings.arpeggiator_rate = MOL_ARPEGGIATOR_RATE_SIXTEENTH;
  settings.arpeggiator_gate = 0.5f;
  settings.arpeggiator_octaves = 1u;
  settings.arpeggiator_random_seed = UINT32_C(0x4d4f4c31);
  settings.tempo = MOL_TEMPO_DEFAULT;
  settings.time_signature_numerator = 4u;
  settings.time_signature_denominator = 4u;
  settings.metronome_level = 0.5f;
  settings.output_mode = MOL_DEVICE_OUTPUT_I2S;
  settings.web_ui_enabled = 1u;
  return settings;
}

mol_result_t mol_device_settings_validate(const mol_device_settings_t* settings) {
  uint32_t ignored_milli_bpm;
  if (settings == NULL || !isfinite(settings->master_gain) || settings->master_gain < 0.0f ||
      settings->master_gain > 2.0f || settings->preset >= MOL_PRESET_COUNT ||
      settings->octave_shift < -3 || settings->octave_shift > 3 || settings->transpose < -24 ||
      settings->transpose > 24 || settings->scale_type >= MOL_SCALE_TYPE_COUNT ||
      settings->scale_tonic > 11u || settings->scale_mapping >= MOL_SCALE_MAPPING_COUNT ||
      settings->chord_mode >= MOL_CHORD_MODE_COUNT ||
      settings->arpeggiator_mode >= MOL_ARPEGGIATOR_MODE_COUNT ||
      settings->arpeggiator_rate >= MOL_ARPEGGIATOR_RATE_COUNT ||
      !isfinite(settings->arpeggiator_gate) || settings->arpeggiator_gate < 0.05f ||
      settings->arpeggiator_gate > 1.0f || settings->arpeggiator_octaves < 1u ||
      settings->arpeggiator_octaves > 4u ||
      mol_tempo_to_milli_bpm(settings->tempo, &ignored_milli_bpm) != MOL_OK ||
      !mol_time_signature_is_valid(settings->time_signature_numerator,
                                   settings->time_signature_denominator) ||
      settings->metronome_enabled > 1u || !isfinite(settings->metronome_level) ||
      settings->metronome_level < 0.0f || settings->metronome_level > 1.0f ||
      settings->portamento_mode >= MOL_PORTAMENTO_MODE_COUNT ||
      !isfinite(settings->portamento_time_ms) || settings->portamento_time_ms < 0.0f ||
      settings->portamento_time_ms > 2000.0f ||
      settings->output_mode >= MOL_DEVICE_OUTPUT_MODE_COUNT || settings->web_ui_enabled > 1u ||
      settings->paired_peer_valid > 1u || settings->a2dp_sink_valid > 1u) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  if ((settings->paired_peer_valid == 0u) !=
          address_is_zero(settings->paired_peer_address, sizeof(settings->paired_peer_address)) ||
      (settings->a2dp_sink_valid == 0u) !=
          address_is_zero(settings->a2dp_sink_address, sizeof(settings->a2dp_sink_address))) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  return MOL_OK;
}

mol_result_t mol_device_settings_encode(const mol_device_settings_t* settings,
                                        uint8_t output[MOL_DEVICE_SETTINGS_RECORD_SIZE]) {
  mol_result_t result;
  if (output == NULL) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  result = mol_device_settings_validate(settings);
  if (result != MOL_OK) {
    return result;
  }
  memset(output, 0, MOL_DEVICE_SETTINGS_RECORD_SIZE);
  write_u32(output + 0u, MOL_SETTINGS_MAGIC);
  write_u32(output + 4u, MOL_DEVICE_SETTINGS_VERSION);
  write_u32(output + 8u, MOL_SETTINGS_PAYLOAD_SIZE);
  write_u32(output + 12u, settings->generation);
  write_float(output + 16u, settings->master_gain);
  write_u32(output + 20u, settings->preset);
  write_i32(output + 24u, settings->octave_shift);
  write_i32(output + 28u, settings->transpose);
  write_u32(output + 32u, settings->scale_type);
  write_u32(output + 36u, settings->scale_tonic);
  write_u32(output + 40u, settings->scale_mapping);
  write_u32(output + 44u, settings->chord_mode);
  write_u32(output + 48u, settings->arpeggiator_mode);
  write_u32(output + 52u, settings->arpeggiator_rate);
  write_float(output + 56u, settings->arpeggiator_gate);
  write_u32(output + 60u, settings->arpeggiator_octaves);
  write_u32(output + 64u, settings->arpeggiator_random_seed);
  write_float(output + 68u, settings->tempo);
  write_u32(output + 72u, settings->time_signature_numerator);
  write_u32(output + 76u, settings->time_signature_denominator);
  write_u32(output + 80u, settings->metronome_enabled);
  write_float(output + 84u, settings->metronome_level);
  write_u32(output + 88u, settings->portamento_mode);
  write_float(output + 92u, settings->portamento_time_ms);
  write_u32(output + 96u, settings->output_mode);
  write_u32(output + 100u, settings->web_ui_enabled);
  write_u32(output + 104u, settings->paired_peer_valid);
  memcpy(output + 108u, settings->paired_peer_address, sizeof(settings->paired_peer_address));
  memcpy(output + 114u, settings->a2dp_sink_address, sizeof(settings->a2dp_sink_address));
  write_u32(output + 120u, settings->a2dp_sink_valid);
  write_u32(output + MOL_SETTINGS_CRC_OFFSET, settings_crc32(output, MOL_SETTINGS_CRC_OFFSET));
  return MOL_OK;
}

mol_result_t mol_device_settings_decode(const uint8_t* input, size_t input_size,
                                        mol_device_settings_t* settings) {
  mol_device_settings_t decoded;
  uint32_t version;
  if (input == NULL || settings == NULL) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  if (input_size != MOL_DEVICE_SETTINGS_RECORD_SIZE ||
      read_u32(input + MOL_SETTINGS_CRC_OFFSET) != settings_crc32(input, MOL_SETTINGS_CRC_OFFSET)) {
    return MOL_ERROR_CORRUPT_DATA;
  }
  if (read_u32(input + 0u) != MOL_SETTINGS_MAGIC ||
      read_u32(input + 8u) != MOL_SETTINGS_PAYLOAD_SIZE) {
    return MOL_ERROR_CORRUPT_DATA;
  }
  version = read_u32(input + 4u);
  if (version != 1u && version != MOL_DEVICE_SETTINGS_VERSION) {
    return MOL_ERROR_UNSUPPORTED_VERSION;
  }
  if (!encoded_small_fields_are_canonical(input, version)) {
    return MOL_ERROR_CORRUPT_DATA;
  }
  decoded = mol_device_settings_default();
  decoded.generation = read_u32(input + 12u);
  decoded.master_gain = read_float(input + 16u);
  decoded.preset = read_u32(input + 20u);
  decoded.octave_shift = read_i32(input + 24u);
  decoded.transpose = read_i32(input + 28u);
  decoded.scale_type = read_u32(input + 32u);
  decoded.scale_tonic = (uint8_t)read_u32(input + 36u);
  decoded.scale_mapping = (uint8_t)read_u32(input + 40u);
  decoded.chord_mode = read_u32(input + 44u);
  decoded.arpeggiator_mode = read_u32(input + 48u);
  decoded.arpeggiator_rate = read_u32(input + 52u);
  decoded.arpeggiator_gate = read_float(input + 56u);
  decoded.arpeggiator_octaves = (uint8_t)read_u32(input + 60u);
  decoded.arpeggiator_random_seed = read_u32(input + 64u);
  decoded.tempo = read_float(input + 68u);
  decoded.time_signature_numerator = (uint8_t)read_u32(input + 72u);
  decoded.time_signature_denominator = (uint8_t)read_u32(input + 76u);
  decoded.metronome_enabled = (uint8_t)read_u32(input + 80u);
  decoded.metronome_level = read_float(input + 84u);
  decoded.portamento_mode = read_u32(input + 88u);
  decoded.portamento_time_ms = read_float(input + 92u);
  decoded.output_mode = read_u32(input + 96u);
  decoded.web_ui_enabled = (uint8_t)read_u32(input + 100u);
  decoded.paired_peer_valid = (uint8_t)read_u32(input + 104u);
  memcpy(decoded.paired_peer_address, input + 108u, sizeof(decoded.paired_peer_address));
  if (version >= 2u) {
    memcpy(decoded.a2dp_sink_address, input + 114u, sizeof(decoded.a2dp_sink_address));
    decoded.a2dp_sink_valid = (uint8_t)read_u32(input + 120u);
  }
  if (mol_device_settings_validate(&decoded) != MOL_OK) {
    return MOL_ERROR_CORRUPT_DATA;
  }
  *settings = decoded;
  return MOL_OK;
}

static mol_command_t settings_command(mol_command_type_t command_type) {
  mol_command_t command;
  memset(&command, 0, sizeof(command));
  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = command_type;
  command.source_id = UINT32_C(0x45535032);
  command.target_frame = MOL_FRAME_IMMEDIATE;
  return command;
}

mol_result_t mol_device_settings_compile_commands(const mol_device_settings_t* settings,
                                                  mol_command_t* commands, size_t capacity,
                                                  size_t* command_count) {
  size_t index = 0u;
  mol_result_t result;
  if (command_count == NULL) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  *command_count = MOL_DEVICE_SETTINGS_COMMAND_COUNT;
  result = mol_device_settings_validate(settings);
  if (result != MOL_OK) {
    return result;
  }
  if (capacity < MOL_DEVICE_SETTINGS_COMMAND_COUNT) {
    return MOL_ERROR_BUFFER_TOO_SMALL;
  }
  if (commands == NULL) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }

  commands[index] = settings_command(MOL_COMMAND_SET_MASTER_GAIN);
  commands[index++].payload.scalar.value = settings->master_gain;
  commands[index] = settings_command(MOL_COMMAND_SET_PRESET);
  commands[index].payload.preset.preset = settings->preset;
  commands[index++].payload.preset.hard_switch = 1u;
  commands[index] = settings_command(MOL_COMMAND_SET_OCTAVE_SHIFT);
  commands[index++].payload.integer.value = settings->octave_shift;
  commands[index] = settings_command(MOL_COMMAND_SET_TRANSPOSE);
  commands[index++].payload.integer.value = settings->transpose;
  commands[index] = settings_command(MOL_COMMAND_SET_SCALE);
  commands[index].payload.scale.type = settings->scale_type;
  commands[index].payload.scale.tonic = settings->scale_tonic;
  commands[index++].payload.scale.mapping = settings->scale_mapping;
  commands[index] = settings_command(MOL_COMMAND_SET_CHORD_MODE);
  commands[index++].payload.integer.value = (int32_t)settings->chord_mode;
  commands[index] = settings_command(MOL_COMMAND_SET_ARPEGGIATOR);
  commands[index].payload.arpeggiator.mode = settings->arpeggiator_mode;
  commands[index].payload.arpeggiator.rate = settings->arpeggiator_rate;
  commands[index].payload.arpeggiator.gate = settings->arpeggiator_gate;
  commands[index].payload.arpeggiator.random_seed = settings->arpeggiator_random_seed;
  commands[index++].payload.arpeggiator.octaves = settings->arpeggiator_octaves;
  commands[index] = settings_command(MOL_COMMAND_SET_TEMPO);
  commands[index++].payload.scalar.value = settings->tempo;
  commands[index] = settings_command(MOL_COMMAND_SET_TIME_SIGNATURE);
  commands[index].payload.time_signature.numerator = settings->time_signature_numerator;
  commands[index++].payload.time_signature.denominator = settings->time_signature_denominator;
  commands[index] = settings_command(MOL_COMMAND_SET_METRONOME);
  commands[index].payload.metronome.enabled = settings->metronome_enabled;
  commands[index++].payload.metronome.level = settings->metronome_level;
  commands[index] = settings_command(MOL_COMMAND_SET_PORTAMENTO);
  commands[index].payload.portamento.mode = settings->portamento_mode;
  commands[index++].payload.portamento.time_ms = settings->portamento_time_ms;
  *command_count = index;
  return MOL_OK;
}
