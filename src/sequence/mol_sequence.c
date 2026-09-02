/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <string.h>

#include "mol/sequence.h"

#define MOL_SEQUENCE_MAGIC UINT32_C(0x534C4F4D)
#define MOL_SEQUENCE_INITIAL_SIZE 80u
#define MOL_SEQUENCE_RECORD_EVENT 1u
#define MOL_SEQUENCE_RECORD_METADATA 2u
#define MOL_SEQUENCE_RECORD_END 255u
#define MOL_SEQUENCE_EVENT_BODY_MAX 64u

static void mol_seq_write_u16(uint8_t* output, uint16_t value) {
  output[0] = (uint8_t)(value & UINT16_C(0xFF));
  output[1] = (uint8_t)(value >> 8u);
}

static void mol_seq_write_u32(uint8_t* output, uint32_t value) {
  for (uint32_t index = 0u; index < 4u; ++index) {
    output[index] = (uint8_t)(value >> (index * 8u));
  }
}

static void mol_seq_write_u64(uint8_t* output, uint64_t value) {
  for (uint32_t index = 0u; index < 8u; ++index) {
    output[index] = (uint8_t)(value >> (index * 8u));
  }
}

static uint16_t mol_seq_read_u16(const uint8_t* input) {
  return (uint16_t)((uint16_t)input[0] | ((uint16_t)input[1] << 8u));
}

static uint32_t mol_seq_read_u32(const uint8_t* input) {
  return (uint32_t)input[0] | ((uint32_t)input[1] << 8u) | ((uint32_t)input[2] << 16u) |
         ((uint32_t)input[3] << 24u);
}

static uint64_t mol_seq_read_u64(const uint8_t* input) {
  uint64_t value = 0u;
  for (uint32_t index = 0u; index < 8u; ++index) {
    value |= (uint64_t)input[index] << (index * 8u);
  }
  return value;
}

static void mol_seq_write_float(uint8_t* output, float value) {
  uint32_t bits = 0u;
  memcpy(&bits, &value, sizeof(bits));
  mol_seq_write_u32(output, bits);
}

static float mol_seq_read_float(const uint8_t* input) {
  uint32_t bits = mol_seq_read_u32(input);
  float value = 0.0f;
  memcpy(&value, &bits, sizeof(value));
  return value;
}

static uint32_t mol_seq_crc32_update(uint32_t crc, const uint8_t* data, size_t size) {
  for (size_t index = 0u; index < size; ++index) {
    crc ^= data[index];
    for (uint32_t bit = 0u; bit < 8u; ++bit) {
      uint32_t mask = 0u - (crc & 1u);
      crc = (crc >> 1u) ^ (UINT32_C(0xEDB88320) & mask);
    }
  }
  return crc;
}

static size_t mol_seq_encode_varint(uint64_t value, uint8_t* output) {
  size_t size = 0u;
  do {
    uint8_t byte = (uint8_t)(value & UINT64_C(0x7F));
    value >>= 7u;
    output[size++] = (uint8_t)(byte | (value != 0u ? 0x80u : 0u));
  } while (value != 0u);
  return size;
}

static int mol_seq_decode_varint(const uint8_t* input, size_t size, size_t* offset,
                                 uint64_t* out_value) {
  uint64_t value = 0u;
  uint32_t shift = 0u;
  size_t start = *offset;
  while (*offset < size && *offset - start < 10u) {
    uint8_t byte = input[(*offset)++];
    if (shift == 63u && (byte & 0x7Eu) != 0u) return 0;
    value |= (uint64_t)(byte & 0x7Fu) << shift;
    if ((byte & 0x80u) == 0u) {
      uint8_t canonical[10];
      size_t canonical_size = mol_seq_encode_varint(value, canonical);
      if (canonical_size != *offset - start) return 0;
      *out_value = value;
      return 1;
    }
    shift += 7u;
  }
  return 0;
}

static int mol_seq_initial_state_is_valid(const mol_sequence_initial_state_t* initial) {
  if (initial == NULL || initial->struct_size < sizeof(*initial) ||
      initial->api_version != MOL_API_VERSION || initial->preset >= MOL_PRESET_COUNT ||
      !isfinite(initial->master_gain) || initial->master_gain < 0.0f ||
      initial->master_gain > 4.0f || !isfinite(initial->tempo) || initial->tempo < MOL_TEMPO_MIN ||
      initial->tempo > MOL_TEMPO_MAX ||
      !mol_time_signature_is_valid(initial->time_signature_numerator,
                                   initial->time_signature_denominator) ||
      initial->octave_shift < -4 || initial->octave_shift > 4 || initial->transpose < -24 ||
      initial->transpose > 24 || initial->scale_type >= MOL_SCALE_TYPE_COUNT ||
      initial->scale_tonic >= 12u || initial->scale_mapping >= MOL_SCALE_MAPPING_COUNT ||
      initial->chord_mode >= MOL_CHORD_MODE_COUNT ||
      initial->arpeggiator_mode >= MOL_ARPEGGIATOR_MODE_COUNT ||
      initial->arpeggiator_rate >= MOL_ARPEGGIATOR_RATE_COUNT ||
      !isfinite(initial->arpeggiator_gate) || initial->arpeggiator_gate < 0.05f ||
      initial->arpeggiator_gate > 1.0f || initial->arpeggiator_octaves < 1u ||
      initial->arpeggiator_octaves > 4u || !isfinite(initial->sustain) || initial->sustain < 0.0f ||
      initial->sustain > 1.0f || !isfinite(initial->pitch_bend) || initial->pitch_bend < -1.0f ||
      initial->pitch_bend > 1.0f || initial->portamento_mode >= MOL_PORTAMENTO_MODE_COUNT ||
      !isfinite(initial->portamento_time_ms) || initial->portamento_time_ms < 0.0f ||
      initial->portamento_time_ms > 10000.0f || !isfinite(initial->metronome_level) ||
      initial->metronome_level < 0.0f || initial->metronome_level > 1.0f ||
      initial->metronome_enabled > 1u) {
    return 0;
  }
  for (uint32_t index = 0u; index < sizeof(initial->reserved_0); ++index) {
    if (initial->reserved_0[index] != 0u) return 0;
  }
  for (uint32_t index = 0u; index < sizeof(initial->reserved_1); ++index) {
    if (initial->reserved_1[index] != 0u) return 0;
  }
  for (uint32_t index = 0u; index < sizeof(initial->reserved_2); ++index) {
    if (initial->reserved_2[index] != 0u) return 0;
  }
  return 1;
}

static int mol_seq_config_is_valid(const mol_sequence_config_t* config) {
  return config != NULL && config->struct_size >= sizeof(*config) &&
         config->api_version == MOL_API_VERSION && config->sample_rate >= 8000u &&
         config->sample_rate <= 192000u && config->time_base != 0u &&
         config->time_base <= UINT32_C(1000000) &&
         mol_seq_initial_state_is_valid(&config->initial_state);
}

mol_sequence_initial_state_t mol_sequence_initial_state_default(void) {
  mol_sequence_initial_state_t initial;
  memset(&initial, 0, sizeof(initial));
  initial.struct_size = (uint32_t)sizeof(initial);
  initial.api_version = MOL_API_VERSION;
  initial.preset = MOL_PRESET_GRAND_PIANO;
  initial.master_gain = 0.25f;
  initial.tempo = MOL_TEMPO_DEFAULT;
  initial.time_signature_numerator = 4u;
  initial.time_signature_denominator = 4u;
  initial.scale_type = MOL_SCALE_CHROMATIC;
  initial.scale_mapping = MOL_SCALE_MAP_NEAREST;
  initial.chord_mode = MOL_CHORD_OFF;
  initial.arpeggiator_mode = MOL_ARPEGGIATOR_OFF;
  initial.arpeggiator_rate = MOL_ARPEGGIATOR_RATE_SIXTEENTH;
  initial.arpeggiator_gate = 0.5f;
  initial.arpeggiator_random_seed = UINT32_C(0x4D4F4C31);
  initial.arpeggiator_octaves = 1u;
  initial.portamento_mode = MOL_PORTAMENTO_OFF;
  initial.metronome_level = 0.2f;
  return initial;
}

mol_sequence_config_t mol_sequence_config_default(uint32_t sample_rate) {
  mol_sequence_config_t config;
  memset(&config, 0, sizeof(config));
  config.struct_size = (uint32_t)sizeof(config);
  config.api_version = MOL_API_VERSION;
  config.sample_rate = sample_rate;
  config.time_base = sample_rate;
  config.initial_state = mol_sequence_initial_state_default();
  return config;
}

static void mol_seq_encode_header(const mol_sequence_config_t* config,
                                  uint8_t header[MOL_SEQUENCE_HEADER_SIZE]) {
  const mol_sequence_initial_state_t* initial = &config->initial_state;
  memset(header, 0, MOL_SEQUENCE_HEADER_SIZE);
  mol_seq_write_u32(header, MOL_SEQUENCE_MAGIC);
  mol_seq_write_u16(header + 4u, MOL_SEQUENCE_FORMAT_VERSION);
  mol_seq_write_u16(header + 6u, MOL_SEQUENCE_HEADER_SIZE);
  mol_seq_write_u32(header + 8u, config->sample_rate);
  mol_seq_write_u32(header + 12u, config->time_base);
  mol_seq_write_u32(header + 20u, MOL_SEQUENCE_INITIAL_SIZE);
  mol_seq_write_u32(header + 32u, initial->preset);
  mol_seq_write_float(header + 36u, initial->master_gain);
  mol_seq_write_float(header + 40u, initial->tempo);
  header[44] = initial->time_signature_numerator;
  header[45] = initial->time_signature_denominator;
  header[46] = (uint8_t)initial->octave_shift;
  header[47] = (uint8_t)initial->transpose;
  mol_seq_write_u32(header + 48u, initial->scale_type);
  header[52] = initial->scale_tonic;
  header[53] = initial->scale_mapping;
  mol_seq_write_u32(header + 56u, initial->chord_mode);
  mol_seq_write_u32(header + 60u, initial->arpeggiator_mode);
  mol_seq_write_u32(header + 64u, initial->arpeggiator_rate);
  mol_seq_write_float(header + 68u, initial->arpeggiator_gate);
  mol_seq_write_u32(header + 72u, initial->arpeggiator_random_seed);
  header[76] = initial->arpeggiator_octaves;
  mol_seq_write_float(header + 80u, initial->sustain);
  mol_seq_write_float(header + 84u, initial->pitch_bend);
  mol_seq_write_u32(header + 88u, initial->portamento_mode);
  mol_seq_write_float(header + 92u, initial->portamento_time_ms);
  mol_seq_write_float(header + 96u, initial->metronome_level);
  header[100] = initial->metronome_enabled;
}

static mol_result_t mol_seq_writer_emit(mol_sequence_writer_t* writer, const uint8_t* data,
                                        size_t size, int update_crc) {
  mol_result_t result = writer->write(writer->user_data, data, size);
  if (result != MOL_OK) {
    writer->active = 0u;
    return MOL_ERROR_IO;
  }
  if (update_crc) writer->crc_state = mol_seq_crc32_update(writer->crc_state, data, size);
  return MOL_OK;
}

mol_result_t mol_sequence_writer_init(mol_sequence_writer_t* writer,
                                      const mol_sequence_config_t* config,
                                      mol_sequence_write_fn write, void* user_data) {
  uint8_t header[MOL_SEQUENCE_HEADER_SIZE];
  uint32_t struct_size;
  if (writer == NULL || writer->struct_size < sizeof(*writer) ||
      writer->api_version != MOL_API_VERSION || write == NULL || !mol_seq_config_is_valid(config)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  struct_size = writer->struct_size;
  memset(writer, 0, sizeof(*writer));
  writer->struct_size = struct_size;
  writer->api_version = MOL_API_VERSION;
  writer->write = write;
  writer->user_data = user_data;
  writer->crc_state = UINT32_MAX;
  writer->active = 1u;
  mol_seq_encode_header(config, header);
  return mol_seq_writer_emit(writer, header, sizeof(header), 1);
}

static mol_result_t mol_seq_encode_payload(const mol_sequence_event_t* event, uint8_t* output,
                                           size_t* out_size) {
  const mol_command_payload_t* payload = &event->payload;
  switch (event->command_type) {
    case MOL_COMMAND_NOTE_ON:
    case MOL_COMMAND_NOTE_OFF:
    case MOL_COMMAND_POLY_PRESSURE:
      if (payload->note.note > 127u || !isfinite(payload->note.velocity) ||
          payload->note.velocity < 0.0f || payload->note.velocity > 1.0f)
        return MOL_ERROR_INVALID_ARGUMENT;
      output[0] = payload->note.note;
      mol_seq_write_float(output + 1u, payload->note.velocity);
      *out_size = 5u;
      return MOL_OK;
    case MOL_COMMAND_PITCH_BEND:
      if (!isfinite(payload->scalar.value) || payload->scalar.value < -1.0f ||
          payload->scalar.value > 1.0f)
        return MOL_ERROR_INVALID_ARGUMENT;
      mol_seq_write_float(output, payload->scalar.value);
      *out_size = 4u;
      return MOL_OK;
    case MOL_COMMAND_SUSTAIN:
      if (!isfinite(payload->scalar.value) || payload->scalar.value < 0.0f ||
          payload->scalar.value > 1.0f)
        return MOL_ERROR_INVALID_ARGUMENT;
      mol_seq_write_float(output, payload->scalar.value);
      *out_size = 4u;
      return MOL_OK;
    case MOL_COMMAND_SET_MASTER_GAIN:
      if (!isfinite(payload->scalar.value) || payload->scalar.value < 0.0f ||
          payload->scalar.value > 4.0f)
        return MOL_ERROR_INVALID_ARGUMENT;
      mol_seq_write_float(output, payload->scalar.value);
      *out_size = 4u;
      return MOL_OK;
    case MOL_COMMAND_SET_TEMPO: {
      uint32_t ignored_milli_bpm;
      if (mol_tempo_to_milli_bpm(payload->scalar.value, &ignored_milli_bpm) != MOL_OK)
        return MOL_ERROR_INVALID_ARGUMENT;
      mol_seq_write_float(output, payload->scalar.value);
      *out_size = 4u;
      return MOL_OK;
    }
    case MOL_COMMAND_ALL_NOTES_OFF:
    case MOL_COMMAND_ALL_SOUND_OFF:
    case MOL_COMMAND_TRANSPORT_START:
    case MOL_COMMAND_TRANSPORT_STOP:
      *out_size = 0u;
      return MOL_OK;
    case MOL_COMMAND_SET_PRESET:
      if (payload->preset.preset >= MOL_PRESET_COUNT || payload->preset.hard_switch > 1u)
        return MOL_ERROR_INVALID_ARGUMENT;
      mol_seq_write_u32(output, payload->preset.preset);
      output[4] = payload->preset.hard_switch;
      *out_size = 5u;
      return MOL_OK;
    case MOL_COMMAND_SET_PARAMETER:
      if (payload->parameter.parameter == 0u || payload->parameter.parameter > 12u ||
          !isfinite(payload->parameter.value))
        return MOL_ERROR_INVALID_ARGUMENT;
      mol_seq_write_u32(output, payload->parameter.parameter);
      mol_seq_write_float(output + 4u, payload->parameter.value);
      *out_size = 8u;
      return MOL_OK;
    case MOL_COMMAND_SET_OCTAVE_SHIFT:
      if (payload->integer.value < -4 || payload->integer.value > 4)
        return MOL_ERROR_INVALID_ARGUMENT;
      mol_seq_write_u32(output, (uint32_t)payload->integer.value);
      *out_size = 4u;
      return MOL_OK;
    case MOL_COMMAND_SET_TRANSPOSE:
      if (payload->integer.value < -24 || payload->integer.value > 24)
        return MOL_ERROR_INVALID_ARGUMENT;
      mol_seq_write_u32(output, (uint32_t)payload->integer.value);
      *out_size = 4u;
      return MOL_OK;
    case MOL_COMMAND_SET_CHORD_MODE:
      if (payload->integer.value < 0 || payload->integer.value >= (int32_t)MOL_CHORD_MODE_COUNT)
        return MOL_ERROR_INVALID_ARGUMENT;
      mol_seq_write_u32(output, (uint32_t)payload->integer.value);
      *out_size = 4u;
      return MOL_OK;
    case MOL_COMMAND_SET_SCALE:
      if (payload->scale.type >= MOL_SCALE_TYPE_COUNT || payload->scale.tonic >= 12u ||
          payload->scale.mapping >= MOL_SCALE_MAPPING_COUNT)
        return MOL_ERROR_INVALID_ARGUMENT;
      mol_seq_write_u32(output, payload->scale.type);
      output[4] = payload->scale.tonic;
      output[5] = payload->scale.mapping;
      *out_size = 6u;
      return MOL_OK;
    case MOL_COMMAND_SET_ARPEGGIATOR:
      if (payload->arpeggiator.mode >= MOL_ARPEGGIATOR_MODE_COUNT ||
          payload->arpeggiator.rate >= MOL_ARPEGGIATOR_RATE_COUNT ||
          !isfinite(payload->arpeggiator.gate) || payload->arpeggiator.gate < 0.05f ||
          payload->arpeggiator.gate > 1.0f || payload->arpeggiator.octaves < 1u ||
          payload->arpeggiator.octaves > 4u)
        return MOL_ERROR_INVALID_ARGUMENT;
      mol_seq_write_u32(output, payload->arpeggiator.mode);
      mol_seq_write_u32(output + 4u, payload->arpeggiator.rate);
      mol_seq_write_float(output + 8u, payload->arpeggiator.gate);
      mol_seq_write_u32(output + 12u, payload->arpeggiator.random_seed);
      output[16] = payload->arpeggiator.octaves;
      *out_size = 17u;
      return MOL_OK;
    case MOL_COMMAND_SET_TIME_SIGNATURE:
      if (!mol_time_signature_is_valid(payload->time_signature.numerator,
                                       payload->time_signature.denominator))
        return MOL_ERROR_INVALID_ARGUMENT;
      output[0] = payload->time_signature.numerator;
      output[1] = payload->time_signature.denominator;
      *out_size = 2u;
      return MOL_OK;
    case MOL_COMMAND_TRANSPORT_SEEK:
      mol_seq_write_u64(output, payload->transport.frame);
      *out_size = 8u;
      return MOL_OK;
    case MOL_COMMAND_SET_METRONOME:
      if (!isfinite(payload->metronome.level) || payload->metronome.level < 0.0f ||
          payload->metronome.level > 1.0f || payload->metronome.enabled > 1u)
        return MOL_ERROR_INVALID_ARGUMENT;
      mol_seq_write_float(output, payload->metronome.level);
      output[4] = payload->metronome.enabled;
      *out_size = 5u;
      return MOL_OK;
    case MOL_COMMAND_SET_PORTAMENTO:
      if (payload->portamento.mode >= MOL_PORTAMENTO_MODE_COUNT ||
          !isfinite(payload->portamento.time_ms) || payload->portamento.time_ms < 0.0f ||
          payload->portamento.time_ms > 10000.0f)
        return MOL_ERROR_INVALID_ARGUMENT;
      mol_seq_write_u32(output, payload->portamento.mode);
      mol_seq_write_float(output + 4u, payload->portamento.time_ms);
      *out_size = 8u;
      return MOL_OK;
    default:
      return MOL_ERROR_UNSUPPORTED;
  }
}

mol_result_t mol_sequence_writer_append(mol_sequence_writer_t* writer,
                                        const mol_sequence_event_t* event) {
  uint8_t body[MOL_SEQUENCE_EVENT_BODY_MAX];
  uint8_t record[2u + 10u + MOL_SEQUENCE_EVENT_BODY_MAX];
  uint8_t payload[32];
  size_t body_size = 0u;
  size_t payload_size = 0u;
  size_t record_size = 0u;
  mol_result_t result;
  if (writer == NULL || writer->struct_size < sizeof(*writer) ||
      writer->api_version != MOL_API_VERSION || writer->active == 0u || writer->finalized != 0u ||
      event == NULL || event->struct_size < sizeof(*event) ||
      event->api_version != MOL_API_VERSION || event->frame == MOL_FRAME_IMMEDIATE ||
      event->frame < writer->previous_frame || writer->event_count >= MOL_SEQUENCE_MAX_EVENTS)
    return MOL_ERROR_INVALID_ARGUMENT;
  result = mol_seq_encode_payload(event, payload, &payload_size);
  if (result != MOL_OK) return result;
  body_size += mol_seq_encode_varint(event->frame - writer->previous_frame, body + body_size);
  body_size += mol_seq_encode_varint(event->command_type, body + body_size);
  body_size += mol_seq_encode_varint(event->source_id, body + body_size);
  body_size += mol_seq_encode_varint(event->gesture_id, body + body_size);
  body_size += mol_seq_encode_varint(payload_size, body + body_size);
  memcpy(body + body_size, payload, payload_size);
  body_size += payload_size;
  record[record_size++] = MOL_SEQUENCE_RECORD_EVENT;
  record_size += mol_seq_encode_varint(body_size, record + record_size);
  memcpy(record + record_size, body, body_size);
  record_size += body_size;
  result = mol_seq_writer_emit(writer, record, record_size, 1);
  if (result == MOL_OK) {
    writer->previous_frame = event->frame;
    ++writer->event_count;
  }
  return result;
}

mol_result_t mol_sequence_writer_add_metadata(mol_sequence_writer_t* writer, uint32_t chunk_type,
                                              const uint8_t* data, size_t size) {
  uint8_t header[12];
  size_t header_size = 0u;
  mol_result_t result;
  if (writer == NULL || writer->struct_size < sizeof(*writer) ||
      writer->api_version != MOL_API_VERSION || writer->active == 0u || writer->finalized != 0u ||
      chunk_type == 0u || size > MOL_SEQUENCE_MAX_METADATA_SIZE || (size != 0u && data == NULL))
    return MOL_ERROR_INVALID_ARGUMENT;
  header[header_size++] = MOL_SEQUENCE_RECORD_METADATA;
  header_size += mol_seq_encode_varint(size + 4u, header + header_size);
  mol_seq_write_u32(header + header_size, chunk_type);
  header_size += 4u;
  result = mol_seq_writer_emit(writer, header, header_size, 1);
  if (result == MOL_OK && size != 0u) result = mol_seq_writer_emit(writer, data, size, 1);
  return result;
}

mol_result_t mol_sequence_writer_finalize(mol_sequence_writer_t* writer) {
  uint8_t header[12];
  uint8_t body[24];
  uint8_t checksum[4];
  size_t body_prefix_size = 0u;
  size_t header_size = 0u;
  mol_result_t result;
  if (writer == NULL || writer->struct_size < sizeof(*writer) ||
      writer->api_version != MOL_API_VERSION || writer->active == 0u || writer->finalized != 0u)
    return MOL_ERROR_INVALID_STATE;
  body_prefix_size += mol_seq_encode_varint(writer->event_count, body + body_prefix_size);
  body_prefix_size += mol_seq_encode_varint(writer->previous_frame, body + body_prefix_size);
  header[header_size++] = MOL_SEQUENCE_RECORD_END;
  header_size += mol_seq_encode_varint(body_prefix_size + 4u, header + header_size);
  result = mol_seq_writer_emit(writer, header, header_size, 1);
  if (result == MOL_OK) result = mol_seq_writer_emit(writer, body, body_prefix_size, 1);
  if (result != MOL_OK) return result;
  mol_seq_write_u32(checksum, ~writer->crc_state);
  result = mol_seq_writer_emit(writer, checksum, sizeof(checksum), 0);
  if (result == MOL_OK) {
    writer->active = 0u;
    writer->finalized = 1u;
  }
  return result;
}

static mol_result_t mol_seq_read_exact(mol_sequence_read_fn read, void* user_data, uint8_t* output,
                                       size_t size, uint32_t* crc, int update_crc) {
  size_t offset = 0u;
  while (offset < size) {
    size_t received = read(user_data, output + offset, size - offset);
    if (received == 0u) return MOL_ERROR_CORRUPT_DATA;
    if (received > size - offset) return MOL_ERROR_IO;
    if (update_crc) *crc = mol_seq_crc32_update(*crc, output + offset, received);
    offset += received;
  }
  return MOL_OK;
}

static mol_result_t mol_seq_read_varint_stream(mol_sequence_read_fn read, void* user_data,
                                               uint32_t* crc, uint64_t* out_value) {
  uint8_t bytes[10];
  size_t size = 0u;
  while (size < sizeof(bytes)) {
    mol_result_t result = mol_seq_read_exact(read, user_data, bytes + size, 1u, crc, 1);
    if (result != MOL_OK) return result;
    ++size;
    if ((bytes[size - 1u] & 0x80u) == 0u) {
      size_t offset = 0u;
      return mol_seq_decode_varint(bytes, size, &offset, out_value) && offset == size
                 ? MOL_OK
                 : MOL_ERROR_CORRUPT_DATA;
    }
  }
  return MOL_ERROR_CORRUPT_DATA;
}

static mol_result_t mol_seq_decode_header(const uint8_t header[MOL_SEQUENCE_HEADER_SIZE],
                                          mol_sequence_config_t* config) {
  uint32_t config_size = config->struct_size;
  if (mol_seq_read_u32(header) != MOL_SEQUENCE_MAGIC ||
      mol_seq_read_u16(header + 6u) != MOL_SEQUENCE_HEADER_SIZE ||
      mol_seq_read_u32(header + 16u) != 0u ||
      mol_seq_read_u32(header + 20u) != MOL_SEQUENCE_INITIAL_SIZE ||
      mol_seq_read_u32(header + 24u) != 0u || mol_seq_read_u32(header + 28u) != 0u ||
      mol_seq_read_u16(header + 54u) != 0u || header[77] != 0u || header[78] != 0u ||
      header[79] != 0u || header[101] != 0u || header[102] != 0u || header[103] != 0u ||
      mol_seq_read_u64(header + 104u) != 0u)
    return MOL_ERROR_CORRUPT_DATA;
  if (mol_seq_read_u16(header + 4u) != MOL_SEQUENCE_FORMAT_VERSION)
    return MOL_ERROR_UNSUPPORTED_VERSION;
  memset(config, 0, sizeof(*config));
  config->struct_size = config_size;
  config->api_version = MOL_API_VERSION;
  config->sample_rate = mol_seq_read_u32(header + 8u);
  config->time_base = mol_seq_read_u32(header + 12u);
  config->initial_state = mol_sequence_initial_state_default();
  config->initial_state.preset = mol_seq_read_u32(header + 32u);
  config->initial_state.master_gain = mol_seq_read_float(header + 36u);
  config->initial_state.tempo = mol_seq_read_float(header + 40u);
  config->initial_state.time_signature_numerator = header[44];
  config->initial_state.time_signature_denominator = header[45];
  config->initial_state.octave_shift = (int8_t)header[46];
  config->initial_state.transpose = (int8_t)header[47];
  config->initial_state.scale_type = mol_seq_read_u32(header + 48u);
  config->initial_state.scale_tonic = header[52];
  config->initial_state.scale_mapping = header[53];
  config->initial_state.chord_mode = mol_seq_read_u32(header + 56u);
  config->initial_state.arpeggiator_mode = mol_seq_read_u32(header + 60u);
  config->initial_state.arpeggiator_rate = mol_seq_read_u32(header + 64u);
  config->initial_state.arpeggiator_gate = mol_seq_read_float(header + 68u);
  config->initial_state.arpeggiator_random_seed = mol_seq_read_u32(header + 72u);
  config->initial_state.arpeggiator_octaves = header[76];
  config->initial_state.sustain = mol_seq_read_float(header + 80u);
  config->initial_state.pitch_bend = mol_seq_read_float(header + 84u);
  config->initial_state.portamento_mode = mol_seq_read_u32(header + 88u);
  config->initial_state.portamento_time_ms = mol_seq_read_float(header + 92u);
  config->initial_state.metronome_level = mol_seq_read_float(header + 96u);
  config->initial_state.metronome_enabled = header[100];
  return mol_seq_config_is_valid(config) ? MOL_OK : MOL_ERROR_CORRUPT_DATA;
}

static mol_result_t mol_seq_decode_payload(mol_sequence_event_t* event, const uint8_t* input,
                                           size_t size) {
  mol_command_payload_t* payload = &event->payload;
  memset(payload, 0, sizeof(*payload));
  switch (event->command_type) {
    case MOL_COMMAND_NOTE_ON:
    case MOL_COMMAND_NOTE_OFF:
    case MOL_COMMAND_POLY_PRESSURE:
      if (size != 5u) return MOL_ERROR_CORRUPT_DATA;
      payload->note.note = input[0];
      payload->note.velocity = mol_seq_read_float(input + 1u);
      break;
    case MOL_COMMAND_PITCH_BEND:
    case MOL_COMMAND_SUSTAIN:
    case MOL_COMMAND_SET_MASTER_GAIN:
    case MOL_COMMAND_SET_TEMPO:
      if (size != 4u) return MOL_ERROR_CORRUPT_DATA;
      payload->scalar.value = mol_seq_read_float(input);
      break;
    case MOL_COMMAND_ALL_NOTES_OFF:
    case MOL_COMMAND_ALL_SOUND_OFF:
    case MOL_COMMAND_TRANSPORT_START:
    case MOL_COMMAND_TRANSPORT_STOP:
      if (size != 0u) return MOL_ERROR_CORRUPT_DATA;
      break;
    case MOL_COMMAND_SET_PRESET:
      if (size != 5u) return MOL_ERROR_CORRUPT_DATA;
      payload->preset.preset = mol_seq_read_u32(input);
      payload->preset.hard_switch = input[4];
      break;
    case MOL_COMMAND_SET_PARAMETER:
      if (size != 8u) return MOL_ERROR_CORRUPT_DATA;
      payload->parameter.parameter = mol_seq_read_u32(input);
      payload->parameter.value = mol_seq_read_float(input + 4u);
      break;
    case MOL_COMMAND_SET_OCTAVE_SHIFT:
    case MOL_COMMAND_SET_TRANSPOSE:
    case MOL_COMMAND_SET_CHORD_MODE:
      if (size != 4u) return MOL_ERROR_CORRUPT_DATA;
      payload->integer.value = (int32_t)mol_seq_read_u32(input);
      break;
    case MOL_COMMAND_SET_SCALE:
      if (size != 6u) return MOL_ERROR_CORRUPT_DATA;
      payload->scale.type = mol_seq_read_u32(input);
      payload->scale.tonic = input[4];
      payload->scale.mapping = input[5];
      break;
    case MOL_COMMAND_SET_ARPEGGIATOR:
      if (size != 17u) return MOL_ERROR_CORRUPT_DATA;
      payload->arpeggiator.mode = mol_seq_read_u32(input);
      payload->arpeggiator.rate = mol_seq_read_u32(input + 4u);
      payload->arpeggiator.gate = mol_seq_read_float(input + 8u);
      payload->arpeggiator.random_seed = mol_seq_read_u32(input + 12u);
      payload->arpeggiator.octaves = input[16];
      break;
    case MOL_COMMAND_SET_TIME_SIGNATURE:
      if (size != 2u) return MOL_ERROR_CORRUPT_DATA;
      payload->time_signature.numerator = input[0];
      payload->time_signature.denominator = input[1];
      break;
    case MOL_COMMAND_TRANSPORT_SEEK:
      if (size != 8u) return MOL_ERROR_CORRUPT_DATA;
      payload->transport.frame = mol_seq_read_u64(input);
      break;
    case MOL_COMMAND_SET_METRONOME:
      if (size != 5u) return MOL_ERROR_CORRUPT_DATA;
      payload->metronome.level = mol_seq_read_float(input);
      payload->metronome.enabled = input[4];
      break;
    case MOL_COMMAND_SET_PORTAMENTO:
      if (size != 8u) return MOL_ERROR_CORRUPT_DATA;
      payload->portamento.mode = mol_seq_read_u32(input);
      payload->portamento.time_ms = mol_seq_read_float(input + 4u);
      break;
    default:
      return MOL_ERROR_UNSUPPORTED;
  }
  {
    uint8_t canonical[32];
    size_t canonical_size = 0u;
    mol_result_t result = mol_seq_encode_payload(event, canonical, &canonical_size);
    return result == MOL_OK && canonical_size == size ? MOL_OK : MOL_ERROR_CORRUPT_DATA;
  }
}

static mol_result_t mol_seq_decode_event(const uint8_t* body, size_t body_size,
                                         mol_frame_index_t previous_frame,
                                         mol_sequence_event_t* event) {
  uint64_t delta;
  uint64_t command_type;
  uint64_t source_id;
  uint64_t gesture_id;
  uint64_t payload_size;
  size_t offset = 0u;
  memset(event, 0, sizeof(*event));
  event->struct_size = (uint32_t)sizeof(*event);
  event->api_version = MOL_API_VERSION;
  if (!mol_seq_decode_varint(body, body_size, &offset, &delta) ||
      !mol_seq_decode_varint(body, body_size, &offset, &command_type) ||
      !mol_seq_decode_varint(body, body_size, &offset, &source_id) ||
      !mol_seq_decode_varint(body, body_size, &offset, &gesture_id) ||
      !mol_seq_decode_varint(body, body_size, &offset, &payload_size) ||
      delta > UINT64_MAX - previous_frame || command_type > UINT32_MAX || source_id > UINT32_MAX ||
      payload_size > body_size - offset || payload_size != body_size - offset)
    return MOL_ERROR_CORRUPT_DATA;
  event->frame = previous_frame + delta;
  event->command_type = (uint32_t)command_type;
  event->source_id = (uint32_t)source_id;
  event->gesture_id = gesture_id;
  return mol_seq_decode_payload(event, body + offset, (size_t)payload_size);
}

mol_result_t mol_sequence_read_stream(mol_sequence_read_fn read, void* read_user_data,
                                      mol_sequence_config_t* out_config,
                                      const mol_sequence_callbacks_t* callbacks) {
  uint8_t header[MOL_SEQUENCE_HEADER_SIZE];
  uint8_t body[MOL_SEQUENCE_MAX_RECORD_SIZE];
  uint32_t crc = UINT32_MAX;
  uint32_t event_count = 0u;
  mol_frame_index_t previous_frame = 0u;
  mol_result_t result;
  if (read == NULL || out_config == NULL || out_config->struct_size < sizeof(*out_config) ||
      out_config->api_version != MOL_API_VERSION ||
      (callbacks != NULL &&
       (callbacks->struct_size < sizeof(*callbacks) || callbacks->api_version != MOL_API_VERSION)))
    return MOL_ERROR_INVALID_ARGUMENT;
  result = mol_seq_read_exact(read, read_user_data, header, sizeof(header), &crc, 1);
  if (result != MOL_OK) return result;
  result = mol_seq_decode_header(header, out_config);
  if (result != MOL_OK) return result;
  for (;;) {
    uint8_t type;
    uint64_t record_size;
    result = mol_seq_read_exact(read, read_user_data, &type, 1u, &crc, 1);
    if (result != MOL_OK) return result;
    result = mol_seq_read_varint_stream(read, read_user_data, &crc, &record_size);
    if (result != MOL_OK) return result;
    if (record_size > MOL_SEQUENCE_MAX_RECORD_SIZE) return MOL_ERROR_CORRUPT_DATA;
    if (type == MOL_SEQUENCE_RECORD_END) {
      uint32_t expected_crc;
      uint64_t declared_count;
      uint64_t declared_final_frame;
      size_t offset = 0u;
      uint8_t extra;
      if (record_size < 6u) return MOL_ERROR_CORRUPT_DATA;
      result = mol_seq_read_exact(read, read_user_data, body, (size_t)record_size - 4u, &crc, 1);
      if (result != MOL_OK) return result;
      result = mol_seq_read_exact(read, read_user_data, body + record_size - 4u, 4u, &crc, 0);
      if (result != MOL_OK) return result;
      expected_crc = mol_seq_read_u32(body + record_size - 4u);
      if (!mol_seq_decode_varint(body, (size_t)record_size - 4u, &offset, &declared_count) ||
          !mol_seq_decode_varint(body, (size_t)record_size - 4u, &offset, &declared_final_frame) ||
          offset != record_size - 4u || declared_count != event_count ||
          declared_final_frame != previous_frame || expected_crc != ~crc)
        return MOL_ERROR_CORRUPT_DATA;
      if (read(read_user_data, &extra, 1u) != 0u) return MOL_ERROR_CORRUPT_DATA;
      return MOL_OK;
    }
    result = mol_seq_read_exact(read, read_user_data, body, (size_t)record_size, &crc, 1);
    if (result != MOL_OK) return result;
    if (type == MOL_SEQUENCE_RECORD_EVENT) {
      mol_sequence_event_t event;
      if (record_size == 0u || record_size > MOL_SEQUENCE_EVENT_BODY_MAX ||
          event_count >= MOL_SEQUENCE_MAX_EVENTS)
        return MOL_ERROR_CORRUPT_DATA;
      result = mol_seq_decode_event(body, (size_t)record_size, previous_frame, &event);
      if (result != MOL_OK) return result;
      previous_frame = event.frame;
      ++event_count;
      if (callbacks != NULL && callbacks->on_event != NULL) {
        result = callbacks->on_event(callbacks->user_data, &event);
        if (result != MOL_OK) return result;
      }
    } else if (type == MOL_SEQUENCE_RECORD_METADATA) {
      if (record_size < 4u || record_size > MOL_SEQUENCE_MAX_METADATA_SIZE + 4u)
        return MOL_ERROR_CORRUPT_DATA;
      if (callbacks != NULL && callbacks->on_metadata != NULL) {
        result = callbacks->on_metadata(callbacks->user_data, mol_seq_read_u32(body), body + 4u,
                                        (size_t)record_size - 4u);
        if (result != MOL_OK) return result;
      }
    } else if (type == 0u || (type & 0x80u) == 0u) {
      return MOL_ERROR_UNSUPPORTED_VERSION;
    }
  }
}
