// SPDX-License-Identifier: Apache-2.0
#include "midi_input.hpp"

#include <algorithm>
#include <utility>

namespace molkeyboardd {
namespace {

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

std::uint8_t channel_message_size(std::uint8_t status) {
  const std::uint8_t type = status & 0xf0u;
  return type == 0xc0u || type == 0xd0u ? 1u : 2u;
}

std::uint8_t system_message_size(std::uint8_t status) {
  if (status == 0xf1u || status == 0xf3u) return 1u;
  if (status == 0xf2u) return 2u;
  return 0u;
}

}  // namespace

MidiStreamDecoder::MidiStreamDecoder(std::uint32_t source_id, std::uint8_t channel_filter,
                                     CommandSink sink)
    : sink_(std::move(sink)), source_id_(source_id), channel_filter_(channel_filter) {}

bool MidiStreamDecoder::valid_channel_filter(std::uint8_t channel_filter) noexcept {
  return channel_filter <= kChannelCount;
}

mol_command_t MidiStreamDecoder::command(mol_command_type_t type) const noexcept {
  mol_command_t result{};
  result.struct_size = static_cast<std::uint32_t>(sizeof(result));
  result.api_version = MOL_API_VERSION;
  result.command_type = type;
  result.source_id = source_id_;
  result.target_frame = MOL_FRAME_IMMEDIATE;
  return result;
}

std::size_t MidiStreamDecoder::key_index(std::uint8_t channel, std::uint8_t note) const noexcept {
  return static_cast<std::size_t>(channel) * kNoteCount + note;
}

std::size_t MidiStreamDecoder::gesture_index(std::size_t key, std::size_t depth) const noexcept {
  return key * kMaximumRepeatedNotes + depth;
}

bool MidiStreamDecoder::accepts(std::uint8_t channel) const noexcept {
  return channel_filter_ == 0u || channel_filter_ == static_cast<std::uint8_t>(channel + 1u);
}

std::uint64_t MidiStreamDecoder::next_gesture() noexcept {
  std::uint64_t result = (static_cast<std::uint64_t>(source_id_) << 32u) | gesture_serial_++;
  if (gesture_serial_ > UINT32_MAX) gesture_serial_ = 1u;
  if (result == 0u) result = gesture_serial_++;
  return result;
}

mol_result_t MidiStreamDecoder::note_on(std::uint8_t channel, std::uint8_t note,
                                        std::uint8_t velocity) {
  const std::size_t key = key_index(channel, note);
  std::uint8_t& count = gesture_counts_[key];
  if (count == kMaximumRepeatedNotes) {
    mol_command_t oldest = command(MOL_COMMAND_NOTE_OFF);
    oldest.gesture_id = gestures_[gesture_index(key, 0u)];
    oldest.payload.note.note = note;
    if (sink_(oldest) != MOL_OK) return MOL_ERROR_QUEUE_FULL;
    for (std::size_t depth = 1u; depth < kMaximumRepeatedNotes; ++depth)
      gestures_[gesture_index(key, depth - 1u)] = gestures_[gesture_index(key, depth)];
    --count;
  }
  mol_command_t value = command(MOL_COMMAND_NOTE_ON);
  value.gesture_id = next_gesture();
  value.payload.note.note = note;
  value.payload.note.velocity = static_cast<float>(velocity) / 127.0f;
  const mol_result_t result = sink_(value);
  if (result == MOL_OK) gestures_[gesture_index(key, count++)] = value.gesture_id;
  return result;
}

mol_result_t MidiStreamDecoder::note_off(std::uint8_t channel, std::uint8_t note,
                                         std::uint8_t velocity) {
  const std::size_t key = key_index(channel, note);
  std::uint8_t& count = gesture_counts_[key];
  if (count == 0u) return MOL_OK;
  mol_command_t value = command(MOL_COMMAND_NOTE_OFF);
  value.gesture_id = gestures_[gesture_index(key, count - 1u)];
  value.payload.note.note = note;
  value.payload.note.velocity = static_cast<float>(velocity) / 127.0f;
  const mol_result_t result = sink_(value);
  if (result == MOL_OK) {
    gestures_[gesture_index(key, count - 1u)] = 0u;
    --count;
  }
  return result;
}

mol_result_t MidiStreamDecoder::silence(mol_command_type_t type) {
  mol_command_t value = command(type);
  const mol_result_t result = sink_(value);
  if (result != MOL_OK) return result;
  gestures_.fill(0u);
  gesture_counts_.fill(0u);
  return MOL_OK;
}

mol_result_t MidiStreamDecoder::reset_controllers() {
  mol_command_t value = command(MOL_COMMAND_SUSTAIN);
  value.payload.scalar.value = 0.0f;
  mol_result_t result = sink_(value);
  value = command(MOL_COMMAND_PITCH_BEND);
  value.payload.scalar.value = 0.0f;
  if (sink_(value) != MOL_OK && result == MOL_OK) result = MOL_ERROR_QUEUE_FULL;
  value = command(MOL_COMMAND_SET_PARAMETER);
  value.payload.parameter.parameter = MOL_PARAMETER_MODULATION;
  value.payload.parameter.value = 0.0f;
  if (sink_(value) != MOL_OK && result == MOL_OK) result = MOL_ERROR_QUEUE_FULL;
  return result;
}

mol_result_t MidiStreamDecoder::handle_message(std::uint8_t status, const std::uint8_t* bytes) {
  const std::uint8_t channel = status & 0x0fu;
  if (!accepts(channel)) return MOL_OK;
  const std::uint8_t type = status & 0xf0u;
  if (type == 0x90u)
    return bytes[1] == 0u ? note_off(channel, bytes[0], 0u)
                           : note_on(channel, bytes[0], bytes[1]);
  if (type == 0x80u) return note_off(channel, bytes[0], bytes[1]);
  if (type == 0xa0u) {
    const std::size_t key = key_index(channel, bytes[0]);
    mol_result_t result = MOL_OK;
    for (std::size_t depth = 0u; depth < gesture_counts_[key]; ++depth) {
      mol_command_t value = command(MOL_COMMAND_POLY_PRESSURE);
      value.gesture_id = gestures_[gesture_index(key, depth)];
      value.payload.note.note = bytes[0];
      value.payload.note.velocity = static_cast<float>(bytes[1]) / 127.0f;
      if (sink_(value) != MOL_OK) result = MOL_ERROR_QUEUE_FULL;
    }
    return result;
  }
  if (type == 0xb0u) {
    if (bytes[0] == 1u) {
      mol_command_t value = command(MOL_COMMAND_SET_PARAMETER);
      value.payload.parameter.parameter = MOL_PARAMETER_MODULATION;
      value.payload.parameter.value = static_cast<float>(bytes[1]) / 127.0f;
      return sink_(value);
    }
    if (bytes[0] == 64u) {
      mol_command_t value = command(MOL_COMMAND_SUSTAIN);
      value.payload.scalar.value = static_cast<float>(bytes[1]) / 127.0f;
      return sink_(value);
    }
    if (bytes[0] == 120u) return silence(MOL_COMMAND_ALL_SOUND_OFF);
    if (bytes[0] == 121u) return reset_controllers();
    if (bytes[0] == 123u) return silence(MOL_COMMAND_ALL_NOTES_OFF);
    return MOL_OK;
  }
  if (type == 0xc0u) {
    mol_command_t value = command(MOL_COMMAND_SET_PRESET);
    value.payload.preset.preset = gm_to_preset(bytes[0]);
    return sink_(value);
  }
  if (type == 0xe0u) {
    const std::int32_t wheel = static_cast<std::int32_t>(bytes[0]) |
                               (static_cast<std::int32_t>(bytes[1]) << 7u);
    mol_command_t value = command(MOL_COMMAND_PITCH_BEND);
    value.payload.scalar.value = wheel < 8192 ? static_cast<float>(wheel - 8192) / 8192.0f
                                               : static_cast<float>(wheel - 8192) / 8191.0f;
    return sink_(value);
  }
  return MOL_OK;
}

mol_result_t MidiStreamDecoder::feed(const std::uint8_t* bytes, std::size_t size) {
  if ((!valid_channel_filter(channel_filter_)) || !sink_ || (bytes == nullptr && size != 0u))
    return MOL_ERROR_INVALID_ARGUMENT;
  mol_result_t result = MOL_OK;
  for (std::size_t index = 0u; index < size; ++index) {
    const std::uint8_t byte = bytes[index];
    if (byte >= 0xf8u) {
      if (byte == 0xffu && reset_controllers() != MOL_OK) result = MOL_ERROR_QUEUE_FULL;
      continue;
    }
    if ((byte & 0x80u) != 0u) {
      data_count_ = 0u;
      if (byte < 0xf0u) {
        in_sysex_ = false;
        system_remaining_ = 0u;
        running_status_ = byte;
        expected_data_ = channel_message_size(byte);
      } else {
        running_status_ = 0u;
        expected_data_ = 0u;
        in_sysex_ = byte == 0xf0u;
        system_remaining_ = in_sysex_ ? 0u : system_message_size(byte);
      }
      continue;
    }
    if (in_sysex_) continue;
    if (system_remaining_ != 0u) {
      --system_remaining_;
      continue;
    }
    if (running_status_ == 0u) continue;
    data_[data_count_++] = byte;
    if (data_count_ == expected_data_) {
      const mol_result_t message_result = handle_message(running_status_, data_.data());
      if (message_result != MOL_OK && result == MOL_OK) result = message_result;
      data_count_ = 0u;
    }
  }
  return result;
}

void MidiStreamDecoder::release_all() {
  for (std::uint8_t channel = 0u; channel < kChannelCount; ++channel) {
    for (std::uint8_t note = 0u; note < kNoteCount; ++note) {
      const std::size_t key = key_index(channel, note);
      while (gesture_counts_[key] != 0u) {
        if (note_off(channel, note, 0u) != MOL_OK) {
          gesture_counts_[key] = 0u;
          for (std::size_t depth = 0u; depth < kMaximumRepeatedNotes; ++depth)
            gestures_[gesture_index(key, depth)] = 0u;
        }
      }
    }
  }
  (void)reset_controllers();
  running_status_ = 0u;
  data_count_ = 0u;
  expected_data_ = 0u;
  system_remaining_ = 0u;
  in_sysex_ = false;
}

std::size_t MidiStreamDecoder::active_note_count() const noexcept {
  std::size_t result = 0u;
  for (std::uint8_t count : gesture_counts_) result += count;
  return result;
}

}  // namespace molkeyboardd
