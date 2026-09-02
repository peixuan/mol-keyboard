// SPDX-License-Identifier: Apache-2.0
#include "midi.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace molseq {
namespace {

constexpr std::size_t kMaximumMidiBytes = 128u * 1024u * 1024u;
constexpr std::uint32_t kExportPpqn = 480u;

struct RawEvent {
  std::uint64_t tick = 0u;
  std::uint32_t track = 0u;
  std::uint32_t order = 0u;
  std::uint8_t status = 0u;
  std::uint8_t data1 = 0u;
  std::uint8_t data2 = 0u;
  std::uint8_t meta_type = 0u;
  std::vector<std::uint8_t> meta;
};

struct OrderedEvent {
  mol_sequence_event_t event{};
  std::uint64_t order = 0u;
};

std::vector<std::uint8_t> read_midi(const std::string& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) throw std::runtime_error("cannot open MIDI input: " + path);
  stream.seekg(0, std::ios::end);
  const std::streamoff length = stream.tellg();
  if (length < 0 || static_cast<std::uint64_t>(length) > kMaximumMidiBytes)
    throw std::runtime_error("MIDI input exceeds fixed size limit");
  stream.seekg(0, std::ios::beg);
  std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
  if (!data.empty() && !stream.read(reinterpret_cast<char*>(data.data()), length))
    throw std::runtime_error("cannot read MIDI input");
  return data;
}

std::uint16_t read_u16(const std::uint8_t* data) {
  return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8u) | data[1]);
}

std::uint32_t read_u32(const std::uint8_t* data) {
  return (static_cast<std::uint32_t>(data[0]) << 24u) |
         (static_cast<std::uint32_t>(data[1]) << 16u) |
         (static_cast<std::uint32_t>(data[2]) << 8u) | data[3];
}

std::uint32_t read_vlq(const std::vector<std::uint8_t>& data, std::size_t& position,
                       std::size_t end) {
  std::uint32_t value = 0u;
  for (std::uint32_t index = 0u; index < 4u; ++index) {
    if (position >= end) throw std::runtime_error("truncated MIDI variable-length value");
    const std::uint8_t byte = data[position++];
    value = (value << 7u) | (byte & 0x7Fu);
    if ((byte & 0x80u) == 0u) return value;
  }
  throw std::runtime_error("MIDI variable-length value exceeds four bytes");
}

void parse_track(const std::vector<std::uint8_t>& data, std::size_t begin, std::size_t end,
                 std::uint32_t track_index, std::vector<RawEvent>& output) {
  std::size_t position = begin;
  std::uint64_t tick = 0u;
  std::uint8_t running_status = 0u;
  std::uint32_t order = 0u;
  while (position < end) {
    const std::uint32_t delta = read_vlq(data, position, end);
    if (tick > UINT64_MAX - delta) throw std::runtime_error("MIDI tick overflow");
    tick += delta;
    if (position >= end) throw std::runtime_error("truncated MIDI event");
    std::uint8_t status = data[position];
    if ((status & 0x80u) != 0u) {
      ++position;
      if (status < 0xF0u) running_status = status;
    } else {
      if (running_status == 0u) throw std::runtime_error("MIDI running status without status byte");
      status = running_status;
    }
    if (status == 0xFFu) {
      running_status = 0u;
      if (position >= end) throw std::runtime_error("truncated MIDI meta event");
      RawEvent event;
      event.tick = tick;
      event.track = track_index;
      event.order = order++;
      event.status = status;
      event.meta_type = data[position++];
      const std::uint32_t length = read_vlq(data, position, end);
      if (length > end - position) throw std::runtime_error("truncated MIDI meta payload");
      event.meta.assign(data.begin() + static_cast<std::ptrdiff_t>(position),
                        data.begin() + static_cast<std::ptrdiff_t>(position + length));
      position += length;
      output.push_back(std::move(event));
      if (output.back().meta_type == 0x2Fu) break;
      continue;
    }
    if (status == 0xF0u || status == 0xF7u) {
      running_status = 0u;
      const std::uint32_t length = read_vlq(data, position, end);
      if (length > end - position) throw std::runtime_error("truncated MIDI SysEx payload");
      position += length;
      continue;
    }
    if (status >= 0xF0u) throw std::runtime_error("unsupported MIDI system event");
    const std::uint8_t kind = status & 0xF0u;
    const std::uint32_t data_count = kind == 0xC0u || kind == 0xD0u ? 1u : 2u;
    if (data_count > end - position) throw std::runtime_error("truncated MIDI channel event");
    RawEvent event;
    event.tick = tick;
    event.track = track_index;
    event.order = order++;
    event.status = status;
    event.data1 = data[position++];
    if (data_count == 2u) event.data2 = data[position++];
    if (event.data1 > 127u || event.data2 > 127u)
      throw std::runtime_error("invalid MIDI data byte");
    output.push_back(std::move(event));
    if (output.size() > MOL_SEQUENCE_MAX_EVENTS * 4ull)
      throw std::runtime_error("MIDI event limit exceeded");
  }
}

mol_sequence_event_t make_event(mol_command_type_t type, mol_frame_index_t frame,
                                std::uint32_t source, mol_gesture_id_t gesture) {
  mol_sequence_event_t event{};
  event.struct_size = static_cast<std::uint32_t>(sizeof(event));
  event.api_version = MOL_API_VERSION;
  event.frame = frame;
  event.command_type = type;
  event.source_id = source;
  event.gesture_id = gesture;
  return event;
}

mol_preset_id_t gm_to_preset(std::uint8_t program) {
  if (program <= 5u)
    return program == 4u || program == 5u ? MOL_PRESET_ELECTRIC_PIANO : MOL_PRESET_GRAND_PIANO;
  if (program == 6u) return MOL_PRESET_HARPSICHORD;
  if (program == 10u) return MOL_PRESET_MUSIC_BOX;
  if (program == 11u) return MOL_PRESET_VIBRAPHONE;
  if (program >= 16u && program <= 23u)
    return program == 16u ? MOL_PRESET_CHURCH_ORGAN : MOL_PRESET_JAZZ_ORGAN;
  if (program >= 24u && program <= 27u) return MOL_PRESET_NYLON_GUITAR;
  if (program >= 28u && program <= 31u) return MOL_PRESET_STEEL_GUITAR;
  if (program >= 32u && program <= 39u) return MOL_PRESET_SYNTH_BASS;
  if (program == 40u || program == 41u) return MOL_PRESET_VIOLIN;
  if (program == 42u || program == 43u) return MOL_PRESET_CELLO;
  if (program == 46u) return MOL_PRESET_HARP;
  if (program >= 52u && program <= 54u) return MOL_PRESET_CHOIR;
  if (program >= 71u && program <= 72u) return MOL_PRESET_CLARINET;
  if (program >= 73u && program <= 79u) return MOL_PRESET_FLUTE;
  if (program >= 80u && program <= 87u) return MOL_PRESET_SYNTH_LEAD;
  if (program >= 88u && program <= 95u) return MOL_PRESET_SYNTH_PAD;
  return MOL_PRESET_GRAND_PIANO;
}

std::uint8_t preset_to_gm(mol_preset_id_t preset) {
  constexpr std::array<std::uint8_t, MOL_PRESET_COUNT> programs = {
      0u, 4u, 6u, 19u, 16u, 24u, 25u, 40u, 42u, 73u, 71u, 80u, 88u, 38u, 52u, 11u, 46u, 10u};
  return preset < programs.size() ? programs[preset] : 0u;
}

std::uint64_t advance_frames(std::uint64_t delta_ticks, std::uint32_t microseconds_per_quarter,
                             std::uint32_t sample_rate, std::uint16_t ppqn,
                             std::uint64_t& remainder) {
  const std::uint64_t factor = static_cast<std::uint64_t>(microseconds_per_quarter) * sample_rate;
  const std::uint64_t denominator = static_cast<std::uint64_t>(ppqn) * 1000000u;
  if (delta_ticks != 0u && factor > UINT64_MAX / delta_ticks)
    throw std::runtime_error("MIDI duration exceeds supported range");
  const std::uint64_t numerator = delta_ticks * factor;
  if (numerator > UINT64_MAX - remainder) throw std::runtime_error("MIDI time overflow");
  const std::uint64_t total = numerator + remainder;
  remainder = total % denominator;
  return total / denominator;
}

void append_vlq(std::vector<std::uint8_t>& output, std::uint64_t value) {
  if (value > 0x0FFFFFFFu) throw std::runtime_error("MIDI delta exceeds VLQ range");
  std::uint8_t bytes[4];
  std::uint32_t count = 0u;
  bytes[count++] = static_cast<std::uint8_t>(value & 0x7Fu);
  while ((value >>= 7u) != 0u) bytes[count++] = static_cast<std::uint8_t>((value & 0x7Fu) | 0x80u);
  while (count != 0u) output.push_back(bytes[--count]);
}

void write_u16(std::ofstream& stream, std::uint16_t value) {
  const std::array<char, 2> bytes = {static_cast<char>(value >> 8u), static_cast<char>(value)};
  stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void write_u32(std::ofstream& stream, std::uint32_t value) {
  const std::array<char, 4> bytes = {static_cast<char>(value >> 24u),
                                     static_cast<char>(value >> 16u),
                                     static_cast<char>(value >> 8u), static_cast<char>(value)};
  stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void append_meta(std::vector<std::uint8_t>& track, std::uint64_t delta, std::uint8_t type,
                 const std::vector<std::uint8_t>& data) {
  append_vlq(track, delta);
  track.push_back(0xFFu);
  track.push_back(type);
  append_vlq(track, data.size());
  track.insert(track.end(), data.begin(), data.end());
}

void append_channel(std::vector<std::uint8_t>& track, std::uint64_t delta, std::uint8_t status,
                    std::uint8_t data1, int data2) {
  append_vlq(track, delta);
  track.push_back(status);
  track.push_back(data1);
  if (data2 >= 0) track.push_back(static_cast<std::uint8_t>(data2));
}

std::uint32_t tempo_to_microseconds(float bpm) {
  return static_cast<std::uint32_t>(60000000.0 / static_cast<double>(bpm) + 0.5);
}

std::uint64_t frames_to_ticks(std::uint64_t delta_frames, std::uint32_t time_base,
                              std::uint32_t microseconds_per_quarter) {
  const long double numerator = static_cast<long double>(delta_frames) * kExportPpqn * 1000000.0L;
  const long double denominator = static_cast<long double>(time_base) * microseconds_per_quarter;
  const long double result = numerator / denominator;
  if (result > static_cast<long double>(UINT64_MAX))
    throw std::runtime_error("sequence duration exceeds MIDI range");
  return static_cast<std::uint64_t>(result + 0.5L);
}

}  // namespace

SequenceDocument import_midi(const std::string& path, std::uint32_t sample_rate) {
  if (sample_rate < 8000u || sample_rate > 192000u)
    throw std::runtime_error("MIDI import sample rate must be 8000..192000");
  const std::vector<std::uint8_t> data = read_midi(path);
  if (data.size() < 14u || std::string(reinterpret_cast<const char*>(data.data()), 4u) != "MThd")
    throw std::runtime_error("missing MIDI header");
  const std::uint32_t header_length = read_u32(data.data() + 4u);
  if (header_length < 6u || header_length > data.size() - 8u)
    throw std::runtime_error("invalid MIDI header length");
  const std::uint16_t format = read_u16(data.data() + 8u);
  const std::uint16_t track_count = read_u16(data.data() + 10u);
  const std::uint16_t division = read_u16(data.data() + 12u);
  if (format > 1u || track_count == 0u || track_count > 256u || (division & 0x8000u) != 0u ||
      division == 0u)
    throw std::runtime_error("unsupported MIDI format or time division");

  std::size_t position = 8u + header_length;
  std::vector<RawEvent> raw;
  for (std::uint32_t track = 0u; track < track_count; ++track) {
    if (position + 8u > data.size() ||
        std::string(reinterpret_cast<const char*>(data.data() + position), 4u) != "MTrk")
      throw std::runtime_error("missing MIDI track chunk");
    const std::uint32_t length = read_u32(data.data() + position + 4u);
    position += 8u;
    if (length > data.size() - position) throw std::runtime_error("truncated MIDI track");
    parse_track(data, position, position + length, track, raw);
    position += length;
  }
  if (position != data.size()) throw std::runtime_error("trailing bytes after MIDI tracks");
  std::stable_sort(raw.begin(), raw.end(), [](const RawEvent& left, const RawEvent& right) {
    if (left.tick != right.tick) return left.tick < right.tick;
    if (left.track != right.track) return left.track < right.track;
    return left.order < right.order;
  });

  SequenceDocument document;
  document.config = mol_sequence_config_default(sample_rate);
  std::vector<OrderedEvent> converted;
  std::map<std::uint16_t, std::vector<mol_gesture_id_t>> active_notes;
  std::uint32_t tempo_us = 500000u;
  std::uint64_t last_tick = 0u;
  std::uint64_t current_frame = 0u;
  std::uint64_t time_remainder = 0u;
  std::uint64_t output_order = 0u;
  mol_gesture_id_t next_gesture = 1u;
  bool have_name = false;
  converted.push_back({make_event(MOL_COMMAND_TRANSPORT_START, 0u, 0u, 0u), output_order++});
  for (const RawEvent& input : raw) {
    const std::uint64_t delta = input.tick - last_tick;
    const std::uint64_t frames =
        advance_frames(delta, tempo_us, sample_rate, division, time_remainder);
    if (current_frame > UINT64_MAX - frames) throw std::runtime_error("MIDI frame overflow");
    current_frame += frames;
    last_tick = input.tick;
    if (input.status == 0xFFu) {
      if (input.meta_type == 0x51u && input.meta.size() == 3u) {
        const std::uint32_t next_tempo = (static_cast<std::uint32_t>(input.meta[0]) << 16u) |
                                         (static_cast<std::uint32_t>(input.meta[1]) << 8u) |
                                         input.meta[2];
        if (next_tempo == 0u) throw std::runtime_error("invalid zero MIDI tempo");
        mol_sequence_event_t event = make_event(MOL_COMMAND_SET_TEMPO, current_frame, 0u, 0u);
        event.payload.scalar.value = 60000000.0f / static_cast<float>(next_tempo);
        if (event.payload.scalar.value >= MOL_TEMPO_MIN &&
            event.payload.scalar.value <= MOL_TEMPO_MAX)
          converted.push_back({event, output_order++});
        tempo_us = next_tempo;
      } else if (input.meta_type == 0x58u && input.meta.size() >= 2u && input.meta[1] <= 7u) {
        mol_sequence_event_t event =
            make_event(MOL_COMMAND_SET_TIME_SIGNATURE, current_frame, 0u, 0u);
        event.payload.time_signature.numerator = input.meta[0];
        event.payload.time_signature.denominator = static_cast<std::uint8_t>(1u << input.meta[1]);
        if (mol_sequence_validate_event(&event) == MOL_OK)
          converted.push_back({event, output_order++});
      } else if (!have_name && input.meta_type == 0x03u && !input.meta.empty()) {
        Metadata name;
        name.type = UINT32_C(0x454D414E);
        name.data.assign(
            input.meta.begin(),
            input.meta.begin() + static_cast<std::ptrdiff_t>(std::min<std::size_t>(
                                     input.meta.size(), MOL_SEQUENCE_MAX_METADATA_SIZE)));
        document.metadata.push_back(std::move(name));
        have_name = true;
      }
      continue;
    }
    const std::uint8_t kind = input.status & 0xF0u;
    const std::uint8_t channel = input.status & 0x0Fu;
    const std::uint32_t source = static_cast<std::uint32_t>(channel) + 1u;
    const std::uint16_t key = static_cast<std::uint16_t>((channel << 8u) | input.data1);
    if (kind == 0x90u && input.data2 != 0u) {
      const mol_gesture_id_t gesture = next_gesture++;
      active_notes[key].push_back(gesture);
      mol_sequence_event_t event = make_event(MOL_COMMAND_NOTE_ON, current_frame, source, gesture);
      event.payload.note.note = input.data1;
      event.payload.note.velocity = static_cast<float>(input.data2) / 127.0f;
      converted.push_back({event, output_order++});
    } else if (kind == 0x80u || (kind == 0x90u && input.data2 == 0u)) {
      auto found = active_notes.find(key);
      if (found != active_notes.end() && !found->second.empty()) {
        const mol_gesture_id_t gesture = found->second.back();
        found->second.pop_back();
        mol_sequence_event_t event =
            make_event(MOL_COMMAND_NOTE_OFF, current_frame, source, gesture);
        event.payload.note.note = input.data1;
        event.payload.note.velocity = static_cast<float>(input.data2) / 127.0f;
        converted.push_back({event, output_order++});
      }
    } else if (kind == 0xA0u) {
      mol_sequence_event_t event = make_event(MOL_COMMAND_POLY_PRESSURE, current_frame, source, 0u);
      event.payload.note.note = input.data1;
      event.payload.note.velocity = static_cast<float>(input.data2) / 127.0f;
      converted.push_back({event, output_order++});
    } else if (kind == 0xB0u && input.data1 == 64u) {
      mol_sequence_event_t event = make_event(MOL_COMMAND_SUSTAIN, current_frame, source, 0u);
      event.payload.scalar.value = static_cast<float>(input.data2) / 127.0f;
      converted.push_back({event, output_order++});
    } else if (kind == 0xC0u) {
      mol_sequence_event_t event = make_event(MOL_COMMAND_SET_PRESET, current_frame, source, 0u);
      event.payload.preset.preset = gm_to_preset(input.data1);
      converted.push_back({event, output_order++});
    } else if (kind == 0xE0u) {
      const std::int32_t wheel =
          static_cast<std::int32_t>(input.data1) | (static_cast<std::int32_t>(input.data2) << 7u);
      mol_sequence_event_t event = make_event(MOL_COMMAND_PITCH_BEND, current_frame, source, 0u);
      event.payload.scalar.value = wheel < 8192 ? static_cast<float>(wheel - 8192) / 8192.0f
                                                : static_cast<float>(wheel - 8192) / 8191.0f;
      converted.push_back({event, output_order++});
    }
    if (converted.size() > MOL_SEQUENCE_MAX_EVENTS - 1u)
      throw std::runtime_error("converted sequence event limit exceeded");
  }
  converted.push_back(
      {make_event(MOL_COMMAND_TRANSPORT_STOP, current_frame, 0u, 0u), output_order++});
  std::stable_sort(converted.begin(), converted.end(),
                   [](const OrderedEvent& left, const OrderedEvent& right) {
                     if (left.event.frame != right.event.frame)
                       return left.event.frame < right.event.frame;
                     return left.order < right.order;
                   });
  document.events.reserve(converted.size());
  for (const OrderedEvent& event : converted) {
    if (mol_sequence_validate_event(&event.event) == MOL_OK) document.events.push_back(event.event);
  }
  return document;
}

void export_midi(const std::string& path, const SequenceDocument& document) {
  if (mol_sequence_validate_config(&document.config) != MOL_OK)
    throw std::runtime_error("cannot export invalid sequence configuration");
  std::vector<std::uint8_t> track;
  std::uint32_t tempo_us = tempo_to_microseconds(document.config.initial_state.tempo);
  std::uint64_t timeline_frame = 0u;
  std::uint64_t timeline_tick = 0u;
  std::uint64_t emitted_tick = 0u;
  append_meta(track, 0u, 0x51u,
              {static_cast<std::uint8_t>(tempo_us >> 16u),
               static_cast<std::uint8_t>(tempo_us >> 8u), static_cast<std::uint8_t>(tempo_us)});
  std::uint8_t denominator_power = 0u;
  std::uint8_t denominator = document.config.initial_state.time_signature_denominator;
  while (denominator > 1u) {
    denominator >>= 1u;
    ++denominator_power;
  }
  append_meta(track, 0u, 0x58u,
              {document.config.initial_state.time_signature_numerator, denominator_power, 24u, 8u});
  append_channel(track, 0u, 0xC0u, preset_to_gm(document.config.initial_state.preset), -1);

  for (const mol_sequence_event_t& event : document.events) {
    if (event.frame < timeline_frame) throw std::runtime_error("sequence events are not ordered");
    timeline_tick +=
        frames_to_ticks(event.frame - timeline_frame, document.config.time_base, tempo_us);
    timeline_frame = event.frame;
    const std::uint64_t delta = timeline_tick - emitted_tick;
    bool emitted = true;
    switch (event.command_type) {
      case MOL_COMMAND_NOTE_ON:
        append_channel(track, delta, 0x90u, event.payload.note.note,
                       static_cast<int>(std::lround(event.payload.note.velocity * 127.0f)));
        break;
      case MOL_COMMAND_NOTE_OFF:
        append_channel(track, delta, 0x80u, event.payload.note.note,
                       static_cast<int>(std::lround(event.payload.note.velocity * 127.0f)));
        break;
      case MOL_COMMAND_POLY_PRESSURE:
        append_channel(track, delta, 0xA0u, event.payload.note.note,
                       static_cast<int>(std::lround(event.payload.note.velocity * 127.0f)));
        break;
      case MOL_COMMAND_PITCH_BEND: {
        const float bend = event.payload.scalar.value;
        const int wheel = bend < 0.0f ? static_cast<int>(std::lround(8192.0f + bend * 8192.0f))
                                      : static_cast<int>(std::lround(8192.0f + bend * 8191.0f));
        append_channel(track, delta, 0xE0u, static_cast<std::uint8_t>(wheel & 0x7F),
                       (wheel >> 7) & 0x7F);
        break;
      }
      case MOL_COMMAND_SUSTAIN:
        append_channel(track, delta, 0xB0u, 64u,
                       static_cast<int>(std::lround(event.payload.scalar.value * 127.0f)));
        break;
      case MOL_COMMAND_SET_PRESET:
        append_channel(track, delta, 0xC0u, preset_to_gm(event.payload.preset.preset), -1);
        break;
      case MOL_COMMAND_SET_TEMPO: {
        const std::uint32_t next_tempo = tempo_to_microseconds(event.payload.scalar.value);
        append_meta(
            track, delta, 0x51u,
            {static_cast<std::uint8_t>(next_tempo >> 16u),
             static_cast<std::uint8_t>(next_tempo >> 8u), static_cast<std::uint8_t>(next_tempo)});
        tempo_us = next_tempo;
        break;
      }
      case MOL_COMMAND_SET_TIME_SIGNATURE: {
        std::uint8_t power = 0u;
        std::uint8_t value = event.payload.time_signature.denominator;
        while (value > 1u) {
          value >>= 1u;
          ++power;
        }
        append_meta(track, delta, 0x58u, {event.payload.time_signature.numerator, power, 24u, 8u});
        break;
      }
      default:
        emitted = false;
        break;
    }
    if (emitted) emitted_tick = timeline_tick;
  }
  append_meta(track, 0u, 0x2Fu, {});
  if (track.size() > UINT32_MAX) throw std::runtime_error("MIDI track exceeds 4 GiB");
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) throw std::runtime_error("cannot open MIDI output: " + path);
  stream.write("MThd", 4);
  write_u32(stream, 6u);
  write_u16(stream, 0u);
  write_u16(stream, 1u);
  write_u16(stream, static_cast<std::uint16_t>(kExportPpqn));
  stream.write("MTrk", 4);
  write_u32(stream, static_cast<std::uint32_t>(track.size()));
  stream.write(reinterpret_cast<const char*>(track.data()),
               static_cast<std::streamsize>(track.size()));
  stream.close();
  if (!stream) {
    (void)std::remove(path.c_str());
    throw std::runtime_error("cannot finalize MIDI output");
  }
}

}  // namespace molseq
