// SPDX-License-Identifier: Apache-2.0
#include "sequence_document.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace molseq {
namespace {

constexpr std::size_t kMaximumFileBytes = 128u * 1024u * 1024u;

struct CommandName {
  mol_command_type_t type;
  const char* name;
};

constexpr std::array<CommandName, 22> kCommandNames = {
    {{MOL_COMMAND_NOTE_ON, "note_on"},
     {MOL_COMMAND_NOTE_OFF, "note_off"},
     {MOL_COMMAND_POLY_PRESSURE, "poly_pressure"},
     {MOL_COMMAND_PITCH_BEND, "pitch_bend"},
     {MOL_COMMAND_SUSTAIN, "sustain"},
     {MOL_COMMAND_ALL_NOTES_OFF, "all_notes_off"},
     {MOL_COMMAND_ALL_SOUND_OFF, "all_sound_off"},
     {MOL_COMMAND_SET_MASTER_GAIN, "set_master_gain"},
     {MOL_COMMAND_SET_PRESET, "set_preset"},
     {MOL_COMMAND_SET_PARAMETER, "set_parameter"},
     {MOL_COMMAND_SET_OCTAVE_SHIFT, "set_octave_shift"},
     {MOL_COMMAND_SET_TRANSPOSE, "set_transpose"},
     {MOL_COMMAND_SET_SCALE, "set_scale"},
     {MOL_COMMAND_SET_CHORD_MODE, "set_chord_mode"},
     {MOL_COMMAND_SET_ARPEGGIATOR, "set_arpeggiator"},
     {MOL_COMMAND_SET_TEMPO, "set_tempo"},
     {MOL_COMMAND_SET_TIME_SIGNATURE, "set_time_signature"},
     {MOL_COMMAND_TRANSPORT_START, "transport_start"},
     {MOL_COMMAND_TRANSPORT_STOP, "transport_stop"},
     {MOL_COMMAND_TRANSPORT_SEEK, "transport_seek"},
     {MOL_COMMAND_SET_METRONOME, "set_metronome"},
     {MOL_COMMAND_SET_PORTAMENTO, "set_portamento"}}};

std::string read_file(const std::string& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) throw std::runtime_error("cannot open input: " + path);
  stream.seekg(0, std::ios::end);
  const std::streamoff length = stream.tellg();
  if (length < 0 || static_cast<std::uint64_t>(length) > kMaximumFileBytes)
    throw std::runtime_error("input exceeds fixed size limit: " + path);
  stream.seekg(0, std::ios::beg);
  std::string data(static_cast<std::size_t>(length), '\0');
  if (!data.empty() && !stream.read(data.data(), length))
    throw std::runtime_error("cannot read input: " + path);
  return data;
}

void write_file(const std::string& path, const std::string& data) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream || !stream.write(data.data(), static_cast<std::streamsize>(data.size())))
    throw std::runtime_error("cannot write output: " + path);
  stream.close();
  if (!stream) throw std::runtime_error("cannot finalize output: " + path);
}

std::uint32_t u32(const Json& value) {
  return static_cast<std::uint32_t>(json_u64(value, std::numeric_limits<std::uint32_t>::max()));
}

std::uint8_t u8(const Json& value, std::uint8_t maximum = 255u) {
  return static_cast<std::uint8_t>(json_u64(value, maximum));
}

float real(const Json& value, double minimum, double maximum) {
  return static_cast<float>(json_double(value, minimum, maximum));
}

Json initial_to_json(const mol_sequence_initial_state_t& state) {
  Json::Object object;
  object["arpeggiator_gate"] = Json::number(state.arpeggiator_gate);
  object["arpeggiator_mode"] = Json::number(state.arpeggiator_mode);
  object["arpeggiator_octaves"] = Json::number(state.arpeggiator_octaves);
  object["arpeggiator_random_seed"] = Json::number(state.arpeggiator_random_seed);
  object["arpeggiator_rate"] = Json::number(state.arpeggiator_rate);
  object["chord_mode"] = Json::number(state.chord_mode);
  object["master_gain"] = Json::number(state.master_gain);
  object["metronome_enabled"] = Json::boolean_value(state.metronome_enabled != 0u);
  object["metronome_level"] = Json::number(state.metronome_level);
  object["octave_shift"] = Json::number(static_cast<std::int64_t>(state.octave_shift));
  object["pitch_bend"] = Json::number(state.pitch_bend);
  object["portamento_mode"] = Json::number(state.portamento_mode);
  object["portamento_time_ms"] = Json::number(state.portamento_time_ms);
  object["preset"] = Json::number(state.preset);
  object["scale_mapping"] = Json::number(state.scale_mapping);
  object["scale_tonic"] = Json::number(state.scale_tonic);
  object["scale_type"] = Json::number(state.scale_type);
  object["sustain"] = Json::number(state.sustain);
  object["tempo"] = Json::number(state.tempo);
  object["time_signature_denominator"] = Json::number(state.time_signature_denominator);
  object["time_signature_numerator"] = Json::number(state.time_signature_numerator);
  object["transpose"] = Json::number(static_cast<std::int64_t>(state.transpose));
  return Json::object_value(std::move(object));
}

mol_sequence_initial_state_t initial_from_json(const Json& value) {
  mol_sequence_initial_state_t state = mol_sequence_initial_state_default();
  state.arpeggiator_gate = real(require_member(value, "arpeggiator_gate"), 0.05, 1.0);
  state.arpeggiator_mode = u32(require_member(value, "arpeggiator_mode"));
  state.arpeggiator_octaves = u8(require_member(value, "arpeggiator_octaves"), 4u);
  state.arpeggiator_random_seed = u32(require_member(value, "arpeggiator_random_seed"));
  state.arpeggiator_rate = u32(require_member(value, "arpeggiator_rate"));
  state.chord_mode = u32(require_member(value, "chord_mode"));
  state.master_gain = real(require_member(value, "master_gain"), 0.0, 2.0);
  state.metronome_enabled = json_bool(require_member(value, "metronome_enabled")) ? 1u : 0u;
  state.metronome_level = real(require_member(value, "metronome_level"), 0.0, 1.0);
  state.octave_shift =
      static_cast<std::int8_t>(json_i64(require_member(value, "octave_shift"), -4, 4));
  state.pitch_bend = real(require_member(value, "pitch_bend"), -1.0, 1.0);
  state.portamento_mode = u32(require_member(value, "portamento_mode"));
  state.portamento_time_ms = real(require_member(value, "portamento_time_ms"), 0.0, 10000.0);
  state.preset = u32(require_member(value, "preset"));
  state.scale_mapping = u8(require_member(value, "scale_mapping"));
  state.scale_tonic = u8(require_member(value, "scale_tonic"), 11u);
  state.scale_type = u32(require_member(value, "scale_type"));
  state.sustain = real(require_member(value, "sustain"), 0.0, 1.0);
  state.tempo = real(require_member(value, "tempo"), MOL_TEMPO_MIN, MOL_TEMPO_MAX);
  state.time_signature_denominator = u8(require_member(value, "time_signature_denominator"));
  state.time_signature_numerator = u8(require_member(value, "time_signature_numerator"));
  state.transpose = static_cast<std::int8_t>(json_i64(require_member(value, "transpose"), -24, 24));
  return state;
}

Json event_to_json(const mol_sequence_event_t& event) {
  Json::Object object;
  object["frame"] = Json::number(event.frame);
  object["gesture_id"] = Json::number(event.gesture_id);
  object["source_id"] = Json::number(event.source_id);
  object["type"] = Json::string(command_name(event.command_type));
  switch (event.command_type) {
    case MOL_COMMAND_NOTE_ON:
    case MOL_COMMAND_NOTE_OFF:
    case MOL_COMMAND_POLY_PRESSURE:
      object["note"] = Json::number(event.payload.note.note);
      object["velocity"] = Json::number(event.payload.note.velocity);
      break;
    case MOL_COMMAND_PITCH_BEND:
    case MOL_COMMAND_SUSTAIN:
    case MOL_COMMAND_SET_MASTER_GAIN:
    case MOL_COMMAND_SET_TEMPO:
      object["value"] = Json::number(event.payload.scalar.value);
      break;
    case MOL_COMMAND_SET_PRESET:
      object["hard_switch"] = Json::boolean_value(event.payload.preset.hard_switch != 0u);
      object["preset"] = Json::number(event.payload.preset.preset);
      break;
    case MOL_COMMAND_SET_PARAMETER:
      object["parameter"] = Json::number(event.payload.parameter.parameter);
      object["value"] = Json::number(event.payload.parameter.value);
      break;
    case MOL_COMMAND_SET_OCTAVE_SHIFT:
    case MOL_COMMAND_SET_TRANSPOSE:
    case MOL_COMMAND_SET_CHORD_MODE:
      object["value"] = Json::number(static_cast<std::int64_t>(event.payload.integer.value));
      break;
    case MOL_COMMAND_SET_SCALE:
      object["mapping"] = Json::number(event.payload.scale.mapping);
      object["scale_type"] = Json::number(event.payload.scale.type);
      object["tonic"] = Json::number(event.payload.scale.tonic);
      break;
    case MOL_COMMAND_SET_ARPEGGIATOR:
      object["gate"] = Json::number(event.payload.arpeggiator.gate);
      object["mode"] = Json::number(event.payload.arpeggiator.mode);
      object["octaves"] = Json::number(event.payload.arpeggiator.octaves);
      object["random_seed"] = Json::number(event.payload.arpeggiator.random_seed);
      object["rate"] = Json::number(event.payload.arpeggiator.rate);
      break;
    case MOL_COMMAND_SET_TIME_SIGNATURE:
      object["denominator"] = Json::number(event.payload.time_signature.denominator);
      object["numerator"] = Json::number(event.payload.time_signature.numerator);
      break;
    case MOL_COMMAND_TRANSPORT_SEEK:
      object["transport_frame"] = Json::number(event.payload.transport.frame);
      break;
    case MOL_COMMAND_SET_METRONOME:
      object["enabled"] = Json::boolean_value(event.payload.metronome.enabled != 0u);
      object["level"] = Json::number(event.payload.metronome.level);
      break;
    case MOL_COMMAND_SET_PORTAMENTO:
      object["mode"] = Json::number(event.payload.portamento.mode);
      object["time_ms"] = Json::number(event.payload.portamento.time_ms);
      break;
    default:
      break;
  }
  return Json::object_value(std::move(object));
}

mol_sequence_event_t event_from_json(const Json& value) {
  mol_sequence_event_t event{};
  event.struct_size = static_cast<std::uint32_t>(sizeof(event));
  event.api_version = MOL_API_VERSION;
  event.frame = json_u64(require_member(value, "frame"), UINT64_MAX - 1u);
  event.command_type = command_type(json_string(require_member(value, "type")));
  event.source_id = u32(require_member(value, "source_id"));
  event.gesture_id = json_u64(require_member(value, "gesture_id"), UINT64_MAX);
  switch (event.command_type) {
    case MOL_COMMAND_NOTE_ON:
    case MOL_COMMAND_NOTE_OFF:
    case MOL_COMMAND_POLY_PRESSURE:
      event.payload.note.note = u8(require_member(value, "note"), 127u);
      event.payload.note.velocity = real(require_member(value, "velocity"), 0.0, 1.0);
      break;
    case MOL_COMMAND_PITCH_BEND:
      event.payload.scalar.value = real(require_member(value, "value"), -1.0, 1.0);
      break;
    case MOL_COMMAND_SUSTAIN:
      event.payload.scalar.value = real(require_member(value, "value"), 0.0, 1.0);
      break;
    case MOL_COMMAND_SET_MASTER_GAIN:
      event.payload.scalar.value = real(require_member(value, "value"), 0.0, 2.0);
      break;
    case MOL_COMMAND_SET_TEMPO:
      event.payload.scalar.value =
          real(require_member(value, "value"), MOL_TEMPO_MIN, MOL_TEMPO_MAX);
      break;
    case MOL_COMMAND_SET_PRESET:
      event.payload.preset.preset = u32(require_member(value, "preset"));
      event.payload.preset.hard_switch = json_bool(require_member(value, "hard_switch")) ? 1u : 0u;
      break;
    case MOL_COMMAND_SET_PARAMETER:
      event.payload.parameter.parameter = u32(require_member(value, "parameter"));
      event.payload.parameter.value = real(require_member(value, "value"), -100000.0, 100000.0);
      break;
    case MOL_COMMAND_SET_OCTAVE_SHIFT:
      event.payload.integer.value =
          static_cast<std::int32_t>(json_i64(require_member(value, "value"), -4, 4));
      break;
    case MOL_COMMAND_SET_TRANSPOSE:
      event.payload.integer.value =
          static_cast<std::int32_t>(json_i64(require_member(value, "value"), -24, 24));
      break;
    case MOL_COMMAND_SET_CHORD_MODE:
      event.payload.integer.value = static_cast<std::int32_t>(
          json_i64(require_member(value, "value"), 0, MOL_CHORD_MODE_COUNT - 1));
      break;
    case MOL_COMMAND_SET_SCALE:
      event.payload.scale.type = u32(require_member(value, "scale_type"));
      event.payload.scale.tonic = u8(require_member(value, "tonic"), 11u);
      event.payload.scale.mapping = u8(require_member(value, "mapping"));
      break;
    case MOL_COMMAND_SET_ARPEGGIATOR:
      event.payload.arpeggiator.mode = u32(require_member(value, "mode"));
      event.payload.arpeggiator.rate = u32(require_member(value, "rate"));
      event.payload.arpeggiator.gate = real(require_member(value, "gate"), 0.05, 1.0);
      event.payload.arpeggiator.random_seed = u32(require_member(value, "random_seed"));
      event.payload.arpeggiator.octaves = u8(require_member(value, "octaves"), 4u);
      break;
    case MOL_COMMAND_SET_TIME_SIGNATURE:
      event.payload.time_signature.numerator = u8(require_member(value, "numerator"));
      event.payload.time_signature.denominator = u8(require_member(value, "denominator"));
      break;
    case MOL_COMMAND_TRANSPORT_SEEK:
      event.payload.transport.frame =
          json_u64(require_member(value, "transport_frame"), UINT64_MAX);
      break;
    case MOL_COMMAND_SET_METRONOME:
      event.payload.metronome.enabled = json_bool(require_member(value, "enabled")) ? 1u : 0u;
      event.payload.metronome.level = real(require_member(value, "level"), 0.0, 1.0);
      break;
    case MOL_COMMAND_SET_PORTAMENTO:
      event.payload.portamento.mode = u32(require_member(value, "mode"));
      event.payload.portamento.time_ms = real(require_member(value, "time_ms"), 0.0, 10000.0);
      break;
    default:
      break;
  }
  if (mol_sequence_validate_event(&event) != MOL_OK)
    throw std::runtime_error("invalid event at frame " + std::to_string(event.frame));
  return event;
}

std::string fourcc_to_string(std::uint32_t type) {
  std::string result(4u, '\0');
  for (std::uint32_t index = 0u; index < 4u; ++index)
    result[index] = static_cast<char>(type >> (index * 8u));
  return result;
}

std::uint32_t fourcc_from_string(const std::string& value) {
  if (value.size() != 4u) throw std::runtime_error("metadata type must be a four-character code");
  std::uint32_t result = 0u;
  for (std::uint32_t index = 0u; index < 4u; ++index)
    result |= static_cast<std::uint32_t>(static_cast<unsigned char>(value[index])) << (index * 8u);
  return result;
}

char hex_digit(std::uint8_t value) {
  return static_cast<char>(value < 10u ? '0' + value : 'a' + value - 10u);
}

std::string encode_hex(const std::vector<std::uint8_t>& data) {
  std::string result(data.size() * 2u, '0');
  for (std::size_t index = 0u; index < data.size(); ++index) {
    result[index * 2u] = hex_digit(static_cast<std::uint8_t>(data[index] >> 4u));
    result[index * 2u + 1u] = hex_digit(static_cast<std::uint8_t>(data[index] & 0x0Fu));
  }
  return result;
}

std::uint8_t decode_hex_digit(char value) {
  if (value >= '0' && value <= '9') return static_cast<std::uint8_t>(value - '0');
  if (value >= 'a' && value <= 'f') return static_cast<std::uint8_t>(value - 'a' + 10);
  if (value >= 'A' && value <= 'F') return static_cast<std::uint8_t>(value - 'A' + 10);
  throw std::runtime_error("invalid metadata hex data");
}

std::vector<std::uint8_t> decode_hex(const std::string& value) {
  if (value.size() % 2u != 0u || value.size() / 2u > MOL_SEQUENCE_MAX_METADATA_SIZE)
    throw std::runtime_error("metadata hex data exceeds fixed limits");
  std::vector<std::uint8_t> result(value.size() / 2u);
  for (std::size_t index = 0u; index < result.size(); ++index)
    result[index] = static_cast<std::uint8_t>((decode_hex_digit(value[index * 2u]) << 4u) |
                                              decode_hex_digit(value[index * 2u + 1u]));
  return result;
}

struct MemoryReader {
  const std::uint8_t* data;
  std::size_t size;
  std::size_t position;
};

std::size_t read_memory(void* user_data, std::uint8_t* output, std::size_t capacity) {
  auto* reader = static_cast<MemoryReader*>(user_data);
  const std::size_t remaining = reader->size - reader->position;
  const std::size_t count = remaining < capacity ? remaining : capacity;
  if (count != 0u) std::memcpy(output, reader->data + reader->position, count);
  reader->position += count;
  return count;
}

mol_result_t capture_event(void* user_data, const mol_sequence_event_t* event) {
  auto* document = static_cast<SequenceDocument*>(user_data);
  if (document->events.size() >= MOL_SEQUENCE_MAX_EVENTS) return MOL_ERROR_BUFFER_TOO_SMALL;
  document->events.push_back(*event);
  return MOL_OK;
}

mol_result_t capture_metadata(void* user_data, std::uint32_t type, const std::uint8_t* data,
                              std::size_t size) {
  auto* document = static_cast<SequenceDocument*>(user_data);
  if (document->metadata.size() >= 4096u) return MOL_ERROR_BUFFER_TOO_SMALL;
  Metadata metadata;
  metadata.type = type;
  metadata.data.assign(data, data + size);
  document->metadata.push_back(std::move(metadata));
  return MOL_OK;
}

mol_result_t write_stream(void* user_data, const std::uint8_t* data, std::size_t size) {
  auto* file = static_cast<std::FILE*>(user_data);
  return std::fwrite(data, 1u, size, file) == size ? MOL_OK : MOL_ERROR_IO;
}

}  // namespace

const char* command_name(mol_command_type_t type) {
  for (const CommandName& entry : kCommandNames)
    if (entry.type == type) return entry.name;
  throw std::runtime_error("unsupported sequence command type: " + std::to_string(type));
}

mol_command_type_t command_type(const std::string& name) {
  for (const CommandName& entry : kCommandNames)
    if (name == entry.name) return entry.type;
  throw std::runtime_error("unknown sequence command name: " + name);
}

Json document_to_json(const SequenceDocument& document) {
  Json::Object root;
  Json::Array metadata;
  Json::Array events;
  root["format"] = Json::string("molseq");
  root["version"] = Json::number(MOL_SEQUENCE_FORMAT_VERSION);
  root["sample_rate"] = Json::number(document.config.sample_rate);
  root["time_base"] = Json::number(document.config.time_base);
  root["initial"] = initial_to_json(document.config.initial_state);
  for (const Metadata& chunk : document.metadata) {
    Json::Object item;
    item["data_hex"] = Json::string(encode_hex(chunk.data));
    item["type"] = Json::string(fourcc_to_string(chunk.type));
    metadata.push_back(Json::object_value(std::move(item)));
  }
  for (const mol_sequence_event_t& event : document.events) events.push_back(event_to_json(event));
  root["metadata"] = Json::array_value(std::move(metadata));
  root["events"] = Json::array_value(std::move(events));
  return Json::object_value(std::move(root));
}

SequenceDocument document_from_json(const Json& root) {
  if (json_string(require_member(root, "format")) != "molseq")
    throw std::runtime_error("JSON format must be 'molseq'");
  if (json_u64(require_member(root, "version"), UINT32_MAX) != MOL_SEQUENCE_FORMAT_VERSION)
    throw std::runtime_error("unsupported JSON sequence version");
  SequenceDocument document;
  document.config = mol_sequence_config_default(u32(require_member(root, "sample_rate")));
  document.config.time_base = u32(require_member(root, "time_base"));
  document.config.initial_state = initial_from_json(require_member(root, "initial"));
  if (mol_sequence_validate_config(&document.config) != MOL_OK)
    throw std::runtime_error("invalid sequence configuration");

  const Json& metadata = require_member(root, "metadata");
  if (metadata.type != Json::Type::Array) throw std::runtime_error("metadata must be an array");
  if (metadata.array.size() > 4096u) throw std::runtime_error("too many metadata chunks");
  for (const Json& item : metadata.array) {
    Metadata chunk;
    chunk.type = fourcc_from_string(json_string(require_member(item, "type")));
    chunk.data = decode_hex(json_string(require_member(item, "data_hex")));
    document.metadata.push_back(std::move(chunk));
  }

  const Json& events = require_member(root, "events");
  if (events.type != Json::Type::Array) throw std::runtime_error("events must be an array");
  if (events.array.size() > MOL_SEQUENCE_MAX_EVENTS) throw std::runtime_error("too many events");
  mol_frame_index_t previous_frame = 0u;
  for (std::size_t index = 0u; index < events.array.size(); ++index) {
    mol_sequence_event_t event = event_from_json(events.array[index]);
    if (index != 0u && event.frame < previous_frame)
      throw std::runtime_error("events must be ordered by nondecreasing frame");
    previous_frame = event.frame;
    document.events.push_back(event);
  }
  return document;
}

SequenceDocument load_binary(const std::string& path) {
  const std::string data = read_file(path);
  MemoryReader reader{reinterpret_cast<const std::uint8_t*>(data.data()), data.size(), 0u};
  SequenceDocument document;
  mol_sequence_callbacks_t callbacks{};
  document.config.struct_size = static_cast<std::uint32_t>(sizeof(document.config));
  document.config.api_version = MOL_API_VERSION;
  callbacks.struct_size = static_cast<std::uint32_t>(sizeof(callbacks));
  callbacks.api_version = MOL_API_VERSION;
  callbacks.on_event = capture_event;
  callbacks.on_metadata = capture_metadata;
  callbacks.user_data = &document;
  const mol_result_t result =
      mol_sequence_read_stream(read_memory, &reader, &document.config, &callbacks);
  if (result != MOL_OK)
    throw std::runtime_error("invalid Mol Sequence: " + std::string(mol_result_string(result)));
  return document;
}

void save_binary(const std::string& path, const SequenceDocument& document) {
  std::FILE* file = nullptr;
#if defined(_WIN32)
  if (fopen_s(&file, path.c_str(), "wb") != 0) file = nullptr;
#else
  file = std::fopen(path.c_str(), "wb");
#endif
  if (file == nullptr) throw std::runtime_error("cannot open output: " + path);
  mol_sequence_writer_t writer{};
  writer.struct_size = static_cast<std::uint32_t>(sizeof(writer));
  writer.api_version = MOL_API_VERSION;
  mol_result_t result = mol_sequence_writer_init(&writer, &document.config, write_stream, file);
  for (const Metadata& chunk : document.metadata) {
    if (result == MOL_OK)
      result = mol_sequence_writer_add_metadata(&writer, chunk.type, chunk.data.data(),
                                                chunk.data.size());
  }
  for (const mol_sequence_event_t& event : document.events) {
    if (result == MOL_OK) result = mol_sequence_writer_append(&writer, &event);
  }
  if (result == MOL_OK) result = mol_sequence_writer_finalize(&writer);
  if (std::fclose(file) != 0 && result == MOL_OK) result = MOL_ERROR_IO;
  if (result != MOL_OK) {
    (void)std::remove(path.c_str());
    throw std::runtime_error("cannot write Mol Sequence: " +
                             std::string(mol_result_string(result)));
  }
}

SequenceDocument load_json(const std::string& path) {
  return document_from_json(parse_json(read_file(path)));
}

void save_json(const std::string& path, const SequenceDocument& document) {
  write_file(path, write_json(document_to_json(document)));
}

}  // namespace molseq
