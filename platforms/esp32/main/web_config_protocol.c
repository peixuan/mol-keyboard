/* SPDX-License-Identifier: Apache-2.0 */
#include "web_config_protocol.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

enum {
  MOL_WEB_FIELD_TOKEN = 0,
  MOL_WEB_FIELD_GAIN,
  MOL_WEB_FIELD_PRESET,
  MOL_WEB_FIELD_OCTAVE,
  MOL_WEB_FIELD_TRANSPOSE,
  MOL_WEB_FIELD_TEMPO,
  MOL_WEB_FIELD_METRONOME,
  MOL_WEB_FIELD_METRONOME_LEVEL,
  MOL_WEB_FIELD_OUTPUT,
  MOL_WEB_FIELD_COUNT
};

static int hex_nibble(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  return -1;
}

static bool decode_component(const char* encoded, size_t encoded_size, char* decoded,
                             size_t decoded_capacity, size_t* decoded_size) {
  size_t input_index;
  size_t output_index = 0u;
  if (encoded == NULL || decoded == NULL || decoded_size == NULL || decoded_capacity == 0u) {
    return false;
  }
  for (input_index = 0u; input_index < encoded_size; ++input_index) {
    unsigned char value = (unsigned char)encoded[input_index];
    if (value == '%') {
      int high;
      int low;
      if (input_index + 2u >= encoded_size) {
        return false;
      }
      high = hex_nibble(encoded[input_index + 1u]);
      low = hex_nibble(encoded[input_index + 2u]);
      if (high < 0 || low < 0) {
        return false;
      }
      value = (unsigned char)((high << 4) | low);
      input_index += 2u;
    } else if (value == '+') {
      value = ' ';
    }
    if (value == 0u || value < 0x20u || value > 0x7eu || output_index + 1u >= decoded_capacity) {
      return false;
    }
    decoded[output_index++] = (char)value;
  }
  decoded[output_index] = '\0';
  *decoded_size = output_index;
  return true;
}

static int field_id(const char* key) {
  static const char* const names[MOL_WEB_FIELD_COUNT] = {
      "token", "gain",      "preset",          "octave", "transpose",
      "tempo", "metronome", "metronome_level", "output"};
  int index;
  for (index = 0; index < MOL_WEB_FIELD_COUNT; ++index) {
    if (strcmp(key, names[index]) == 0) {
      return index;
    }
  }
  return -1;
}

static bool token_matches(const char* supplied, size_t supplied_size, const char* expected) {
  unsigned int difference = 0u;
  size_t index;
  if (supplied_size != MOL_WEB_FORM_TOKEN_HEX_LENGTH ||
      strlen(expected) != MOL_WEB_FORM_TOKEN_HEX_LENGTH) {
    return false;
  }
  for (index = 0u; index < MOL_WEB_FORM_TOKEN_HEX_LENGTH; ++index) {
    difference |= (unsigned int)((unsigned char)supplied[index] ^ (unsigned char)expected[index]);
  }
  return difference == 0u;
}

static bool parse_integer(const char* text, int32_t minimum, int32_t maximum, int32_t* output) {
  bool negative = false;
  uint32_t magnitude = 0u;
  size_t index = 0u;
  int64_t signed_value;
  if (text == NULL || output == NULL || text[0] == '\0') {
    return false;
  }
  if (text[index] == '-') {
    negative = true;
    ++index;
  }
  if (text[index] == '\0') {
    return false;
  }
  for (; text[index] != '\0'; ++index) {
    const unsigned char digit = (unsigned char)text[index];
    if (digit < '0' || digit > '9' || magnitude > UINT32_C(214748364)) {
      return false;
    }
    magnitude = magnitude * 10u + (uint32_t)(digit - '0');
  }
  signed_value = negative ? -(int64_t)magnitude : (int64_t)magnitude;
  if (signed_value < minimum || signed_value > maximum) {
    return false;
  }
  *output = (int32_t)signed_value;
  return true;
}

static bool parse_decimal(const char* text, float minimum, float maximum, float* output) {
  bool negative = false;
  bool decimal_seen = false;
  bool fractional_digit_seen = false;
  size_t index = 0u;
  uint32_t integer_digits = 0u;
  double value = 0.0;
  double place = 0.1;
  if (text == NULL || output == NULL || text[0] == '\0') {
    return false;
  }
  if (text[index] == '-') {
    negative = true;
    ++index;
  }
  for (; text[index] != '\0'; ++index) {
    const unsigned char character = (unsigned char)text[index];
    if (character == '.') {
      if (decimal_seen || integer_digits == 0u) {
        return false;
      }
      decimal_seen = true;
      continue;
    }
    if (character < '0' || character > '9') {
      return false;
    }
    if (!decimal_seen) {
      if (++integer_digits > 6u) {
        return false;
      }
      value = value * 10.0 + (double)(character - '0');
    } else {
      fractional_digit_seen = true;
      value += (double)(character - '0') * place;
      place *= 0.1;
    }
  }
  if (integer_digits == 0u || (decimal_seen && !fractional_digit_seen)) {
    return false;
  }
  if (negative) {
    value = -value;
  }
  if (!isfinite(value) || value < (double)minimum || value > (double)maximum) {
    return false;
  }
  *output = (float)value;
  return true;
}

static bool apply_field(int id, const char* value, bool a2dp_supported,
                        mol_device_settings_t* candidate) {
  int32_t integer;
  float decimal;
  switch (id) {
    case MOL_WEB_FIELD_GAIN:
      if (!parse_decimal(value, 0.0f, 2.0f, &decimal)) {
        return false;
      }
      candidate->master_gain = decimal;
      return true;
    case MOL_WEB_FIELD_PRESET:
      if (!parse_integer(value, 0, (int32_t)MOL_PRESET_COUNT - 1, &integer)) {
        return false;
      }
      candidate->preset = (mol_preset_id_t)integer;
      return true;
    case MOL_WEB_FIELD_OCTAVE:
      if (!parse_integer(value, -3, 3, &candidate->octave_shift)) {
        return false;
      }
      return true;
    case MOL_WEB_FIELD_TRANSPOSE:
      if (!parse_integer(value, -24, 24, &candidate->transpose)) {
        return false;
      }
      return true;
    case MOL_WEB_FIELD_TEMPO:
      if (!parse_decimal(value, MOL_TEMPO_MIN, MOL_TEMPO_MAX, &decimal)) {
        return false;
      }
      candidate->tempo = decimal;
      return true;
    case MOL_WEB_FIELD_METRONOME:
      if (strcmp(value, "0") != 0 && strcmp(value, "1") != 0) {
        return false;
      }
      candidate->metronome_enabled = (uint8_t)(value[0] - '0');
      return true;
    case MOL_WEB_FIELD_METRONOME_LEVEL:
      if (!parse_decimal(value, 0.0f, 1.0f, &decimal)) {
        return false;
      }
      candidate->metronome_level = decimal;
      return true;
    case MOL_WEB_FIELD_OUTPUT:
      if (strcmp(value, "i2s") == 0) {
        candidate->output_mode = MOL_DEVICE_OUTPUT_I2S;
        return true;
      }
      if (a2dp_supported && strcmp(value, "a2dp") == 0) {
        candidate->output_mode = MOL_DEVICE_OUTPUT_A2DP;
        return true;
      }
      return false;
    default:
      return false;
  }
}

mol_web_form_result_t mol_web_form_apply(const char* body, size_t body_size,
                                         const char* expected_token, bool a2dp_supported,
                                         const mol_device_settings_t* current,
                                         mol_device_settings_t* output) {
  mol_device_settings_t candidate;
  uint32_t seen = 0u;
  size_t field_start = 0u;
  bool authenticated = false;
  bool setting_seen = false;
  if (body == NULL || expected_token == NULL || current == NULL || output == NULL ||
      mol_device_settings_validate(current) != MOL_OK) {
    return MOL_WEB_FORM_INVALID_ARGUMENT;
  }
  if (body_size == 0u || body_size > MOL_WEB_FORM_MAX_BODY_SIZE) {
    return body_size > MOL_WEB_FORM_MAX_BODY_SIZE ? MOL_WEB_FORM_TOO_LARGE : MOL_WEB_FORM_MALFORMED;
  }
  candidate = *current;
  while (field_start < body_size) {
    char key[32];
    char value[65];
    size_t field_end = field_start;
    size_t equal = field_start;
    size_t key_size = 0u;
    size_t value_size = 0u;
    int id;
    while (field_end < body_size && body[field_end] != '&') {
      ++field_end;
    }
    if (field_end == field_start) {
      return MOL_WEB_FORM_MALFORMED;
    }
    while (equal < field_end && body[equal] != '=') {
      ++equal;
    }
    if (equal == field_start || equal == field_end ||
        !decode_component(body + field_start, equal - field_start, key, sizeof(key), &key_size) ||
        !decode_component(body + equal + 1u, field_end - equal - 1u, value, sizeof(value),
                          &value_size) ||
        key_size == 0u || value_size == 0u) {
      return MOL_WEB_FORM_MALFORMED;
    }
    id = field_id(key);
    if (id < 0) {
      return MOL_WEB_FORM_UNKNOWN_FIELD;
    }
    if ((seen & (UINT32_C(1) << (unsigned int)id)) != 0u) {
      return MOL_WEB_FORM_DUPLICATE_FIELD;
    }
    seen |= UINT32_C(1) << (unsigned int)id;
    if (id == MOL_WEB_FIELD_TOKEN) {
      authenticated = token_matches(value, value_size, expected_token);
    } else {
      setting_seen = true;
      if (!apply_field(id, value, a2dp_supported, &candidate)) {
        return MOL_WEB_FORM_INVALID_VALUE;
      }
    }
    if (field_end == body_size) {
      field_start = body_size;
    } else {
      field_start = field_end + 1u;
      if (field_start == body_size) {
        return MOL_WEB_FORM_MALFORMED;
      }
    }
  }
  if (!authenticated || (seen & (UINT32_C(1) << MOL_WEB_FIELD_TOKEN)) == 0u) {
    return MOL_WEB_FORM_UNAUTHORIZED;
  }
  if (!setting_seen) {
    return MOL_WEB_FORM_NO_SETTINGS;
  }
  if (mol_device_settings_validate(&candidate) != MOL_OK) {
    return MOL_WEB_FORM_INVALID_VALUE;
  }
  *output = candidate;
  return MOL_WEB_FORM_OK;
}

const char* mol_web_form_result_string(mol_web_form_result_t result) {
  switch (result) {
    case MOL_WEB_FORM_OK:
      return "ok";
    case MOL_WEB_FORM_INVALID_ARGUMENT:
      return "invalid argument";
    case MOL_WEB_FORM_TOO_LARGE:
      return "body too large";
    case MOL_WEB_FORM_MALFORMED:
      return "malformed form";
    case MOL_WEB_FORM_UNAUTHORIZED:
      return "unauthorized";
    case MOL_WEB_FORM_DUPLICATE_FIELD:
      return "duplicate field";
    case MOL_WEB_FORM_UNKNOWN_FIELD:
      return "unknown field";
    case MOL_WEB_FORM_INVALID_VALUE:
      return "invalid value";
    case MOL_WEB_FORM_NO_SETTINGS:
      return "no settings";
    default:
      return "unknown result";
  }
}
