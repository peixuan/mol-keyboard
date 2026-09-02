/* SPDX-License-Identifier: Apache-2.0 */
#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "mol/patch.h"

#define MOL_PATCH_MAGIC UINT32_C(0x504C4F4D)
#define MOL_PATCH_KNOWN_FEATURES UINT32_C(0x000003FF)
#define MOL_PATCH_FIELD_COUNT 26u
#define MOL_PATCH_REQUIRED_FIELDS UINT32_C(0x03FFFFFF)

typedef struct mol_patch_text {
  const char* data;
  size_t size;
} mol_patch_text_t;

typedef struct mol_patch_json_parser {
  const char* data;
  size_t size;
  size_t offset;
} mol_patch_json_parser_t;

typedef struct mol_patch_metadata {
  const char* stable_id;
  const char* english_name;
  const char* chinese_name;
} mol_patch_metadata_t;

static const mol_patch_metadata_t mol_patch_metadata[MOL_PRESET_COUNT] = {
    {"grand-piano", "Grand Piano", "大钢琴"},
    {"electric-piano", "Electric Piano", "电钢琴"},
    {"harpsichord", "Harpsichord", "羽管键琴"},
    {"church-organ", "Church Organ", "教堂风琴"},
    {"jazz-organ", "Jazz Organ", "爵士风琴"},
    {"nylon-guitar", "Nylon Guitar", "尼龙弦吉他"},
    {"steel-guitar", "Steel Guitar", "钢弦吉他"},
    {"violin", "Violin", "小提琴"},
    {"cello", "Cello", "大提琴"},
    {"flute", "Flute", "长笛"},
    {"clarinet", "Clarinet", "单簧管"},
    {"synth-lead", "Synth Lead", "合成主音"},
    {"synth-pad", "Synth Pad", "合成铺底"},
    {"synth-bass", "Synth Bass", "合成贝斯"},
    {"choir", "Choir", "合唱"},
    {"vibraphone", "Vibraphone", "颤音琴"},
    {"harp", "Harp", "竖琴"},
    {"music-box", "Music Box", "音乐盒"}};

static const char* const mol_patch_field_names[MOL_PATCH_FIELD_COUNT] = {"format_version",
                                                                         "id",
                                                                         "name_zh",
                                                                         "name_en",
                                                                         "synthesis",
                                                                         "waveform",
                                                                         "gain_millidb",
                                                                         "attack_ms",
                                                                         "decay_ms",
                                                                         "sustain_milli",
                                                                         "release_ms",
                                                                         "filter_cutoff_hz",
                                                                         "filter_resonance_milli",
                                                                         "oscillator_mix_milli",
                                                                         "detune_cents",
                                                                         "pulse_width_milli",
                                                                         "model_parameter_1_milli",
                                                                         "model_parameter_2_milli",
                                                                         "vibrato_rate_millihz",
                                                                         "vibrato_depth_cents",
                                                                         "velocity_curve_milli",
                                                                         "noise_mix_milli",
                                                                         "saturation_milli",
                                                                         "chorus_send_milli",
                                                                         "delay_send_milli",
                                                                         "reverb_send_milli"};

static const char* const mol_synthesis_names[MOL_SYNTHESIS_MODEL_COUNT] = {
    "subtractive", "fm2", "additive", "pluck", "modal", "formant"};

static const char* const mol_waveform_names[MOL_WAVEFORM_COUNT] = {"sine",  "saw",      "square",
                                                                   "pulse", "triangle", "noise"};

static void mol_patch_skip_whitespace(mol_patch_json_parser_t* parser) {
  while (parser->offset < parser->size) {
    char value = parser->data[parser->offset];
    if (value != ' ' && value != '\t' && value != '\r' && value != '\n') {
      break;
    }
    ++parser->offset;
  }
}

static int mol_patch_take(mol_patch_json_parser_t* parser, char expected) {
  mol_patch_skip_whitespace(parser);
  if (parser->offset >= parser->size || parser->data[parser->offset] != expected) {
    return 0;
  }
  ++parser->offset;
  return 1;
}

static int mol_patch_hex_digit(char value) {
  return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') ||
         (value >= 'A' && value <= 'F');
}

static int mol_patch_parse_string(mol_patch_json_parser_t* parser, mol_patch_text_t* text) {
  size_t begin;
  if (!mol_patch_take(parser, '"')) {
    return 0;
  }
  begin = parser->offset;
  while (parser->offset < parser->size) {
    unsigned char value = (unsigned char)parser->data[parser->offset++];
    if (value == '"') {
      text->data = parser->data + begin;
      text->size = parser->offset - begin - 1u;
      return 1;
    }
    if (value < 0x20u) {
      return 0;
    }
    if (value == '\\') {
      char escape;
      if (parser->offset >= parser->size) {
        return 0;
      }
      escape = parser->data[parser->offset++];
      if (escape == 'u') {
        for (uint32_t index = 0u; index < 4u; ++index) {
          if (parser->offset >= parser->size ||
              !mol_patch_hex_digit(parser->data[parser->offset++])) {
            return 0;
          }
        }
      } else if (strchr("\"\\/bfnrt", escape) == NULL) {
        return 0;
      }
    }
  }
  return 0;
}

static int mol_patch_text_equal(mol_patch_text_t text, const char* expected) {
  size_t length = strlen(expected);
  return text.size == length && memcmp(text.data, expected, length) == 0;
}

static int mol_patch_field_index(mol_patch_text_t key, uint32_t* out_index) {
  for (uint32_t index = 0u; index < MOL_PATCH_FIELD_COUNT; ++index) {
    if (mol_patch_text_equal(key, mol_patch_field_names[index])) {
      *out_index = index;
      return 1;
    }
  }
  return 0;
}

static int mol_patch_parse_integer(mol_patch_json_parser_t* parser, int32_t* out_value) {
  int negative = 0;
  uint64_t magnitude = 0u;
  size_t digits = 0u;
  mol_patch_skip_whitespace(parser);
  if (parser->offset < parser->size && parser->data[parser->offset] == '-') {
    negative = 1;
    ++parser->offset;
  }
  while (parser->offset < parser->size && parser->data[parser->offset] >= '0' &&
         parser->data[parser->offset] <= '9') {
    uint32_t digit = (uint32_t)(parser->data[parser->offset++] - '0');
    if (magnitude > (UINT64_MAX - digit) / 10u) {
      return 0;
    }
    magnitude = magnitude * 10u + digit;
    ++digits;
  }
  if (digits == 0u || (parser->offset < parser->size && (parser->data[parser->offset] == '.' ||
                                                         parser->data[parser->offset] == 'e' ||
                                                         parser->data[parser->offset] == 'E'))) {
    return 0;
  }
  if ((!negative && magnitude > INT32_MAX) || (negative && magnitude > (uint64_t)INT32_MAX + 1u)) {
    return 0;
  }
  *out_value = negative ? (magnitude == (uint64_t)INT32_MAX + 1u ? INT32_MIN : -(int32_t)magnitude)
                        : (int32_t)magnitude;
  return 1;
}

static int mol_patch_parse_named_value(mol_patch_json_parser_t* parser, const char* const* names,
                                       uint32_t count, uint32_t* out_value) {
  mol_patch_text_t text;
  if (!mol_patch_parse_string(parser, &text)) {
    return 0;
  }
  for (uint32_t index = 0u; index < count; ++index) {
    if (mol_patch_text_equal(text, names[index])) {
      *out_value = index;
      return 1;
    }
  }
  return 0;
}

static int32_t* mol_patch_integer_field(mol_patch_t* patch, uint32_t field) {
  switch (field) {
    case 6u:
      return &patch->gain_millidb;
    case 7u:
      return &patch->attack_ms;
    case 8u:
      return &patch->decay_ms;
    case 9u:
      return &patch->sustain_milli;
    case 10u:
      return &patch->release_ms;
    case 11u:
      return &patch->filter_cutoff_hz;
    case 12u:
      return &patch->filter_resonance_milli;
    case 13u:
      return &patch->oscillator_mix_milli;
    case 14u:
      return &patch->detune_cents;
    case 15u:
      return &patch->pulse_width_milli;
    case 16u:
      return &patch->model_parameter_1_milli;
    case 17u:
      return &patch->model_parameter_2_milli;
    case 18u:
      return &patch->vibrato_rate_millihz;
    case 19u:
      return &patch->vibrato_depth_cents;
    case 20u:
      return &patch->velocity_curve_milli;
    case 21u:
      return &patch->noise_mix_milli;
    case 22u:
      return &patch->saturation_milli;
    case 23u:
      return &patch->chorus_send_milli;
    case 24u:
      return &patch->delay_send_milli;
    case 25u:
      return &patch->reverb_send_milli;
    default:
      return NULL;
  }
}

static int mol_patch_parse_field_value(mol_patch_json_parser_t* parser, uint32_t field,
                                       mol_patch_t* patch, mol_patch_text_t* id) {
  int32_t value;
  if (field == 1u) {
    return mol_patch_parse_string(parser, id) && id->size != 0u;
  }
  if (field == 2u || field == 3u) {
    mol_patch_text_t name;
    return mol_patch_parse_string(parser, &name) && name.size != 0u;
  }
  if (field == 4u) {
    return mol_patch_parse_named_value(parser, mol_synthesis_names, MOL_SYNTHESIS_MODEL_COUNT,
                                       &patch->synthesis_model);
  }
  if (field == 5u) {
    return mol_patch_parse_named_value(parser, mol_waveform_names, MOL_WAVEFORM_COUNT,
                                       &patch->waveform);
  }
  if (!mol_patch_parse_integer(parser, &value)) {
    return 0;
  }
  if (field == 0u) {
    return value == (int32_t)MOL_PATCH_FORMAT_VERSION;
  }
  {
    int32_t* destination = mol_patch_integer_field(patch, field);
    if (destination == NULL) {
      return 0;
    }
    *destination = value;
  }
  return 1;
}

static int mol_patch_id_is_builtin(mol_patch_text_t id) {
  for (uint32_t index = 0u; index < MOL_PRESET_COUNT; ++index) {
    if (mol_patch_text_equal(id, mol_patch_metadata[index].stable_id)) {
      return 1;
    }
  }
  return 0;
}

static uint32_t mol_patch_hash_text(mol_patch_text_t text) {
  uint32_t hash = UINT32_C(2166136261);
  for (size_t index = 0u; index < text.size; ++index) {
    hash ^= (uint8_t)text.data[index];
    hash *= UINT32_C(16777619);
  }
  return hash;
}

uint32_t mol_patch_id_hash(const char* stable_id) {
  mol_patch_text_t text;
  if (stable_id == NULL) {
    return 0u;
  }
  text.data = stable_id;
  text.size = strlen(stable_id);
  return mol_patch_hash_text(text);
}

static int mol_patch_in_range(int32_t value, int32_t minimum, int32_t maximum) {
  return value >= minimum && value <= maximum;
}

mol_result_t mol_patch_validate(const mol_patch_t* patch) {
  if (patch == NULL || patch->struct_size < sizeof(*patch)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  if (patch->api_version != MOL_API_VERSION) {
    return MOL_ERROR_UNSUPPORTED_VERSION;
  }
  if (patch->preset_id_hash == 0u || (patch->feature_flags & ~MOL_PATCH_KNOWN_FEATURES) != 0u ||
      patch->synthesis_model >= MOL_SYNTHESIS_MODEL_COUNT ||
      patch->waveform >= MOL_WAVEFORM_COUNT ||
      !mol_patch_in_range(patch->gain_millidb, -24000, 6000) ||
      !mol_patch_in_range(patch->attack_ms, 0, 10000) ||
      !mol_patch_in_range(patch->decay_ms, 0, 10000) ||
      !mol_patch_in_range(patch->sustain_milli, 0, 1000) ||
      !mol_patch_in_range(patch->release_ms, 1, 20000) ||
      !mol_patch_in_range(patch->filter_cutoff_hz, 20, 20000) ||
      !mol_patch_in_range(patch->filter_resonance_milli, 0, 990) ||
      !mol_patch_in_range(patch->oscillator_mix_milli, 0, 1000) ||
      !mol_patch_in_range(patch->detune_cents, -1200, 1200) ||
      !mol_patch_in_range(patch->pulse_width_milli, 50, 950) ||
      !mol_patch_in_range(patch->model_parameter_1_milli, 0, 20000) ||
      !mol_patch_in_range(patch->model_parameter_2_milli, 0, 20000) ||
      !mol_patch_in_range(patch->vibrato_rate_millihz, 0, 20000) ||
      !mol_patch_in_range(patch->vibrato_depth_cents, 0, 200) ||
      !mol_patch_in_range(patch->velocity_curve_milli, 100, 3000) ||
      !mol_patch_in_range(patch->noise_mix_milli, 0, 1000) ||
      !mol_patch_in_range(patch->saturation_milli, 0, 10000) ||
      !mol_patch_in_range(patch->chorus_send_milli, 0, 1000) ||
      !mol_patch_in_range(patch->delay_send_milli, 0, 1000) ||
      !mol_patch_in_range(patch->reverb_send_milli, 0, 1000)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  return MOL_OK;
}

static mol_patch_feature_flags_t mol_patch_derive_features(const mol_patch_t* patch) {
  mol_patch_feature_flags_t flags = MOL_PATCH_FEATURE_OSCILLATOR | MOL_PATCH_FEATURE_FILTER;
  if (patch->waveform == MOL_WAVEFORM_NOISE || patch->noise_mix_milli != 0) {
    flags |= MOL_PATCH_FEATURE_NOISE;
  }
  switch (patch->synthesis_model) {
    case MOL_SYNTHESIS_FM2:
      flags |= MOL_PATCH_FEATURE_FM2;
      break;
    case MOL_SYNTHESIS_ADDITIVE:
    case MOL_SYNTHESIS_FORMANT:
      flags |= MOL_PATCH_FEATURE_ADDITIVE;
      break;
    case MOL_SYNTHESIS_PLUCK:
      flags |= MOL_PATCH_FEATURE_PLUCK;
      break;
    case MOL_SYNTHESIS_MODAL:
      flags |= MOL_PATCH_FEATURE_MODAL;
      break;
    default:
      break;
  }
  if (patch->chorus_send_milli != 0) {
    flags |= MOL_PATCH_FEATURE_CHORUS;
  }
  if (patch->delay_send_milli != 0) {
    flags |= MOL_PATCH_FEATURE_DELAY;
  }
  if (patch->reverb_send_milli != 0) {
    flags |= MOL_PATCH_FEATURE_REVERB;
  }
  return flags;
}

mol_result_t mol_patch_compile_json(const char* json, size_t json_size, mol_patch_t* out_patch) {
  mol_patch_json_parser_t parser;
  mol_patch_text_t id = {0};
  uint32_t seen = 0u;
  int first = 1;
  if (json == NULL || json_size == 0u || json_size > MOL_PATCH_MAX_JSON_SIZE || out_patch == NULL ||
      out_patch->struct_size < sizeof(*out_patch)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  memset(out_patch, 0, sizeof(*out_patch));
  out_patch->struct_size = (uint32_t)sizeof(*out_patch);
  out_patch->api_version = MOL_API_VERSION;
  parser.data = json;
  parser.size = json_size;
  parser.offset = 0u;
  if (!mol_patch_take(&parser, '{')) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  while (1) {
    mol_patch_text_t key;
    uint32_t field;
    mol_patch_skip_whitespace(&parser);
    if (parser.offset < parser.size && parser.data[parser.offset] == '}') {
      ++parser.offset;
      break;
    }
    if (!first && !mol_patch_take(&parser, ',')) {
      return MOL_ERROR_INVALID_ARGUMENT;
    }
    first = 0;
    if (!mol_patch_parse_string(&parser, &key) || !mol_patch_take(&parser, ':') ||
        !mol_patch_field_index(key, &field) || (seen & (UINT32_C(1) << field)) != 0u ||
        !mol_patch_parse_field_value(&parser, field, out_patch, &id)) {
      return MOL_ERROR_INVALID_ARGUMENT;
    }
    seen |= UINT32_C(1) << field;
  }
  mol_patch_skip_whitespace(&parser);
  if (parser.offset != parser.size || seen != MOL_PATCH_REQUIRED_FIELDS ||
      !mol_patch_id_is_builtin(id)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  out_patch->preset_id_hash = mol_patch_hash_text(id);
  out_patch->feature_flags = mol_patch_derive_features(out_patch);
  return mol_patch_validate(out_patch);
}

static void mol_patch_write_u16(uint8_t* output, uint16_t value) {
  output[0] = (uint8_t)value;
  output[1] = (uint8_t)(value >> 8u);
}

static void mol_patch_write_u32(uint8_t* output, uint32_t value) {
  output[0] = (uint8_t)value;
  output[1] = (uint8_t)(value >> 8u);
  output[2] = (uint8_t)(value >> 16u);
  output[3] = (uint8_t)(value >> 24u);
}

static uint16_t mol_patch_read_u16(const uint8_t* input) {
  return (uint16_t)((uint16_t)input[0] | (uint16_t)((uint16_t)input[1] << 8u));
}

static uint32_t mol_patch_read_u32(const uint8_t* input) {
  return (uint32_t)input[0] | ((uint32_t)input[1] << 8u) | ((uint32_t)input[2] << 16u) |
         ((uint32_t)input[3] << 24u);
}

static uint32_t mol_patch_crc32(const uint8_t* data, size_t size) {
  uint32_t crc = UINT32_MAX;
  for (size_t index = 0u; index < size; ++index) {
    crc ^= data[index];
    for (uint32_t bit = 0u; bit < 8u; ++bit) {
      uint32_t mask = 0u - (crc & 1u);
      crc = (crc >> 1u) ^ (UINT32_C(0xEDB88320) & mask);
    }
  }
  return ~crc;
}

static void mol_patch_write_payload(const mol_patch_t* patch, uint8_t* payload) {
  const int32_t values[22] = {(int32_t)patch->synthesis_model,
                              (int32_t)patch->waveform,
                              patch->gain_millidb,
                              patch->attack_ms,
                              patch->decay_ms,
                              patch->sustain_milli,
                              patch->release_ms,
                              patch->filter_cutoff_hz,
                              patch->filter_resonance_milli,
                              patch->oscillator_mix_milli,
                              patch->detune_cents,
                              patch->pulse_width_milli,
                              patch->model_parameter_1_milli,
                              patch->model_parameter_2_milli,
                              patch->vibrato_rate_millihz,
                              patch->vibrato_depth_cents,
                              patch->velocity_curve_milli,
                              patch->noise_mix_milli,
                              patch->saturation_milli,
                              patch->chorus_send_milli,
                              patch->delay_send_milli,
                              patch->reverb_send_milli};
  for (uint32_t index = 0u; index < 22u; ++index) {
    mol_patch_write_u32(payload + index * 4u, (uint32_t)values[index]);
  }
}

mol_result_t mol_patch_encode(const mol_patch_t* patch, uint8_t* output, size_t capacity,
                              size_t* out_size) {
  mol_result_t result;
  if (out_size == NULL) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  *out_size = MOL_PATCH_BINARY_SIZE;
  result = mol_patch_validate(patch);
  if (result != MOL_OK) {
    return result;
  }
  if (output == NULL || capacity < MOL_PATCH_BINARY_SIZE) {
    return MOL_ERROR_BUFFER_TOO_SMALL;
  }
  memset(output, 0, MOL_PATCH_BINARY_SIZE);
  mol_patch_write_u32(output, MOL_PATCH_MAGIC);
  mol_patch_write_u16(output + 4u, MOL_PATCH_FORMAT_VERSION);
  mol_patch_write_u16(output + 6u, MOL_PATCH_BINARY_HEADER_SIZE);
  mol_patch_write_u32(output + 8u, MOL_PATCH_BINARY_PAYLOAD_SIZE);
  mol_patch_write_u32(output + 12u, patch->feature_flags);
  mol_patch_write_u32(output + 16u, patch->preset_id_hash);
  mol_patch_write_payload(patch, output + MOL_PATCH_BINARY_HEADER_SIZE);
  mol_patch_write_u32(output + 20u, mol_patch_crc32(output + MOL_PATCH_BINARY_HEADER_SIZE,
                                                    MOL_PATCH_BINARY_PAYLOAD_SIZE));
  return MOL_OK;
}

static void mol_patch_read_payload(mol_patch_t* patch, const uint8_t* payload) {
  int32_t values[22];
  for (uint32_t index = 0u; index < 22u; ++index) {
    values[index] = (int32_t)mol_patch_read_u32(payload + index * 4u);
  }
  patch->synthesis_model = (uint32_t)values[0];
  patch->waveform = (uint32_t)values[1];
  patch->gain_millidb = values[2];
  patch->attack_ms = values[3];
  patch->decay_ms = values[4];
  patch->sustain_milli = values[5];
  patch->release_ms = values[6];
  patch->filter_cutoff_hz = values[7];
  patch->filter_resonance_milli = values[8];
  patch->oscillator_mix_milli = values[9];
  patch->detune_cents = values[10];
  patch->pulse_width_milli = values[11];
  patch->model_parameter_1_milli = values[12];
  patch->model_parameter_2_milli = values[13];
  patch->vibrato_rate_millihz = values[14];
  patch->vibrato_depth_cents = values[15];
  patch->velocity_curve_milli = values[16];
  patch->noise_mix_milli = values[17];
  patch->saturation_milli = values[18];
  patch->chorus_send_milli = values[19];
  patch->delay_send_milli = values[20];
  patch->reverb_send_milli = values[21];
}

mol_result_t mol_patch_decode(const uint8_t* data, size_t size, mol_patch_t* out_patch) {
  uint32_t flags;
  if (data == NULL || out_patch == NULL || out_patch->struct_size < sizeof(*out_patch)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  if (size != MOL_PATCH_BINARY_SIZE || mol_patch_read_u32(data) != MOL_PATCH_MAGIC ||
      mol_patch_read_u16(data + 6u) != MOL_PATCH_BINARY_HEADER_SIZE ||
      mol_patch_read_u32(data + 8u) != MOL_PATCH_BINARY_PAYLOAD_SIZE ||
      mol_patch_read_u32(data + 24u) != 0u || mol_patch_read_u32(data + 28u) != 0u) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  if (mol_patch_read_u16(data + 4u) != MOL_PATCH_FORMAT_VERSION) {
    return MOL_ERROR_UNSUPPORTED_VERSION;
  }
  flags = mol_patch_read_u32(data + 12u);
  if ((flags & ~MOL_PATCH_KNOWN_FEATURES) != 0u ||
      mol_patch_read_u32(data + 20u) !=
          mol_patch_crc32(data + MOL_PATCH_BINARY_HEADER_SIZE, MOL_PATCH_BINARY_PAYLOAD_SIZE)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  memset(out_patch, 0, sizeof(*out_patch));
  out_patch->struct_size = (uint32_t)sizeof(*out_patch);
  out_patch->api_version = MOL_API_VERSION;
  out_patch->feature_flags = flags;
  out_patch->preset_id_hash = mol_patch_read_u32(data + 16u);
  mol_patch_read_payload(out_patch, data + MOL_PATCH_BINARY_HEADER_SIZE);
  return mol_patch_validate(out_patch);
}

const char* mol_preset_stable_id(mol_preset_id_t preset) {
  return preset < MOL_PRESET_COUNT ? mol_patch_metadata[preset].stable_id : NULL;
}

const char* mol_preset_english_name(mol_preset_id_t preset) {
  return preset < MOL_PRESET_COUNT ? mol_patch_metadata[preset].english_name : NULL;
}

const char* mol_preset_chinese_name(mol_preset_id_t preset) {
  return preset < MOL_PRESET_COUNT ? mol_patch_metadata[preset].chinese_name : NULL;
}
