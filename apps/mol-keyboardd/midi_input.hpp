// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_KEYBOARDD_MIDI_INPUT_HPP_
#define MOL_KEYBOARDD_MIDI_INPUT_HPP_

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

#include "mol/mol.h"

namespace molkeyboardd {

class MidiStreamDecoder {
 public:
  using CommandSink = std::function<mol_result_t(const mol_command_t&)>;

  // A filter of zero accepts every MIDI channel; 1 through 16 select one channel.
  MidiStreamDecoder(std::uint32_t source_id, std::uint8_t channel_filter, CommandSink sink);

  static bool valid_channel_filter(std::uint8_t channel_filter) noexcept;
  mol_result_t feed(const std::uint8_t* data, std::size_t size);
  void release_all();
  [[nodiscard]] std::size_t active_note_count() const noexcept;

 private:
  static constexpr std::size_t kChannelCount = 16u;
  static constexpr std::size_t kNoteCount = 128u;
  static constexpr std::size_t kMaximumRepeatedNotes = 4u;

  mol_command_t command(mol_command_type_t type) const noexcept;
  mol_result_t handle_message(std::uint8_t status, const std::uint8_t* bytes);
  mol_result_t note_on(std::uint8_t channel, std::uint8_t note, std::uint8_t velocity);
  mol_result_t note_off(std::uint8_t channel, std::uint8_t note, std::uint8_t velocity);
  mol_result_t silence(mol_command_type_t type);
  mol_result_t reset_controllers();
  std::size_t key_index(std::uint8_t channel, std::uint8_t note) const noexcept;
  std::size_t gesture_index(std::size_t key, std::size_t depth) const noexcept;
  bool accepts(std::uint8_t channel) const noexcept;
  std::uint64_t next_gesture() noexcept;

  CommandSink sink_;
  std::array<std::uint64_t, kChannelCount * kNoteCount * kMaximumRepeatedNotes> gestures_{};
  std::array<std::uint8_t, kChannelCount * kNoteCount> gesture_counts_{};
  std::uint32_t source_id_ = 0u;
  std::uint64_t gesture_serial_ = 1u;
  std::uint8_t channel_filter_ = 0u;
  std::uint8_t running_status_ = 0u;
  std::uint8_t data_count_ = 0u;
  std::uint8_t expected_data_ = 0u;
  std::uint8_t system_remaining_ = 0u;
  std::array<std::uint8_t, 2u> data_{};
  bool in_sysex_ = false;
};

}  // namespace molkeyboardd

#endif  // MOL_KEYBOARDD_MIDI_INPUT_HPP_
