// SPDX-License-Identifier: Apache-2.0
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "midi_input.hpp"

namespace {

using Commands = std::vector<mol_command_t>;

void append(Commands& commands, const mol_command_t& command) {
  if (command.struct_size != sizeof(command) || command.api_version != MOL_API_VERSION)
    __builtin_trap();
  commands.push_back(command);
}

void require_equal(const Commands& left, const Commands& right) {
  if (left.size() != right.size()) __builtin_trap();
  if (!left.empty() &&
      std::memcmp(left.data(), right.data(), left.size() * sizeof(mol_command_t)) != 0)
    __builtin_trap();
}

void exercise(const std::uint8_t* data, std::size_t size, bool seed_running_status) {
  Commands contiguous_commands;
  Commands chunked_commands;
  const auto channel_filter = static_cast<std::uint8_t>(size % 17u);
  molkeyboardd::MidiStreamDecoder contiguous(
      0x4d494449u, channel_filter,
      [&contiguous_commands](const mol_command_t& command) {
        append(contiguous_commands, command);
        return MOL_OK;
      });
  molkeyboardd::MidiStreamDecoder chunked(
      0x4d494449u, channel_filter,
      [&chunked_commands](const mol_command_t& command) {
        append(chunked_commands, command);
        return MOL_OK;
      });

  constexpr std::uint8_t prefix[] = {0x90u, 60u};
  if (seed_running_status) {
    if (contiguous.feed(prefix, sizeof(prefix)) != MOL_OK ||
        chunked.feed(prefix, sizeof(prefix)) != MOL_OK)
      __builtin_trap();
  }
  if (contiguous.feed(data, size) != MOL_OK) __builtin_trap();
  for (std::size_t offset = 0u; offset < size;) {
    const std::size_t remaining = size - offset;
    const std::size_t requested = static_cast<std::size_t>(data[offset] & 0x0fu) + 1u;
    const std::size_t chunk_size = requested < remaining ? requested : remaining;
    if (chunked.feed(data + offset, chunk_size) != MOL_OK) __builtin_trap();
    offset += chunk_size;
  }

  if (contiguous.active_note_count() > 16u * 128u * 4u ||
      contiguous.active_note_count() != chunked.active_note_count())
    __builtin_trap();
  require_equal(contiguous_commands, chunked_commands);
  contiguous.release_all();
  chunked.release_all();
  if (contiguous.active_note_count() != 0u || chunked.active_note_count() != 0u)
    __builtin_trap();
  require_equal(contiguous_commands, chunked_commands);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size > 65536u) return 0;
  exercise(data, size, false);
  exercise(data, size, true);
  return 0;
}
