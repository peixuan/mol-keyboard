// SPDX-License-Identifier: Apache-2.0
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "midi_input.hpp"

namespace {

int failures;

#define EXPECT_TRUE(condition)                                                                  \
  do {                                                                                          \
    if (!(condition)) {                                                                         \
      std::fprintf(stderr, "%s:%d: expectation failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                               \
    }                                                                                           \
  } while (false)

void test_notes_and_running_status() {
  std::vector<mol_command_t> commands;
  molkeyboardd::MidiStreamDecoder decoder(
      7u, 0u, [&commands](const mol_command_t& command) {
        commands.push_back(command);
        return MOL_OK;
      });
  const std::uint8_t input[] = {0x90u, 60u, 127u, 61u, 64u, 0xf8u, 60u, 0u,
                                0x80u, 61u, 32u};
  EXPECT_TRUE(decoder.feed(input, sizeof(input)) == MOL_OK);
  EXPECT_TRUE(commands.size() == 4u);
  EXPECT_TRUE(commands[0].command_type == MOL_COMMAND_NOTE_ON &&
              commands[0].payload.note.note == 60u && commands[0].payload.note.velocity == 1.0f);
  EXPECT_TRUE(commands[1].command_type == MOL_COMMAND_NOTE_ON &&
              commands[1].gesture_id != commands[0].gesture_id);
  EXPECT_TRUE(commands[2].command_type == MOL_COMMAND_NOTE_OFF &&
              commands[2].gesture_id == commands[0].gesture_id);
  EXPECT_TRUE(commands[3].command_type == MOL_COMMAND_NOTE_OFF &&
              commands[3].gesture_id == commands[1].gesture_id);
  EXPECT_TRUE(decoder.active_note_count() == 0u);
}

void test_controls_and_program() {
  std::vector<mol_command_t> commands;
  molkeyboardd::MidiStreamDecoder decoder(
      8u, 0u, [&commands](const mol_command_t& command) {
        commands.push_back(command);
        return MOL_OK;
      });
  const std::uint8_t input[] = {0xb2u, 64u, 96u, 1u, 127u, 121u, 0u,
                                0xe2u, 0u, 0u,   0xe2u, 127u, 127u,
                                0xc2u, 40u};
  EXPECT_TRUE(decoder.feed(input, sizeof(input)) == MOL_OK);
  EXPECT_TRUE(commands.size() == 8u);
  EXPECT_TRUE(commands[0].command_type == MOL_COMMAND_SUSTAIN &&
              std::fabs(commands[0].payload.scalar.value - 96.0f / 127.0f) < 0.00001f);
  EXPECT_TRUE(commands[1].command_type == MOL_COMMAND_SET_PARAMETER &&
              commands[1].payload.parameter.parameter == MOL_PARAMETER_MODULATION &&
              commands[1].payload.parameter.value == 1.0f);
  EXPECT_TRUE(commands[2].command_type == MOL_COMMAND_SUSTAIN &&
              commands[3].command_type == MOL_COMMAND_PITCH_BEND &&
              commands[4].command_type == MOL_COMMAND_SET_PARAMETER);
  EXPECT_TRUE(commands[5].command_type == MOL_COMMAND_PITCH_BEND &&
              commands[5].payload.scalar.value == -1.0f);
  EXPECT_TRUE(commands[6].command_type == MOL_COMMAND_PITCH_BEND &&
              commands[6].payload.scalar.value == 1.0f);
  EXPECT_TRUE(commands[7].command_type == MOL_COMMAND_SET_PRESET &&
              commands[7].payload.preset.preset == MOL_PRESET_VIOLIN);
}

void test_filtering_and_system_bytes() {
  std::vector<mol_command_t> commands;
  molkeyboardd::MidiStreamDecoder decoder(
      9u, 2u, [&commands](const mol_command_t& command) {
        commands.push_back(command);
        return MOL_OK;
      });
  const std::uint8_t input[] = {0x90u, 55u, 100u, 0xf0u, 1u, 2u, 0xf8u, 0xf7u,
                                0x91u, 56u, 100u, 0xf2u, 1u, 2u, 0x91u, 56u, 0u};
  EXPECT_TRUE(decoder.feed(input, sizeof(input)) == MOL_OK);
  EXPECT_TRUE(commands.size() == 2u && commands[0].command_type == MOL_COMMAND_NOTE_ON &&
              commands[1].command_type == MOL_COMMAND_NOTE_OFF);
  EXPECT_TRUE(decoder.feed(nullptr, 0u) == MOL_OK);
  EXPECT_TRUE(decoder.feed(nullptr, 1u) == MOL_ERROR_INVALID_ARGUMENT);
  EXPECT_TRUE(!molkeyboardd::MidiStreamDecoder::valid_channel_filter(17u));
}

void test_pressure_mode_messages_and_release() {
  std::vector<mol_command_t> commands;
  molkeyboardd::MidiStreamDecoder decoder(
      10u, 0u, [&commands](const mol_command_t& command) {
        commands.push_back(command);
        return MOL_OK;
      });
  const std::uint8_t input[] = {0x93u, 60u, 100u, 60u, 90u, 0xa3u, 60u, 80u,
                                0xb3u, 123u, 0u, 0x94u, 61u, 100u, 0xb4u, 120u, 0u};
  EXPECT_TRUE(decoder.feed(input, sizeof(input)) == MOL_OK);
  EXPECT_TRUE(commands.size() == 7u);
  EXPECT_TRUE(commands[2].command_type == MOL_COMMAND_POLY_PRESSURE &&
              commands[3].command_type == MOL_COMMAND_POLY_PRESSURE);
  EXPECT_TRUE(commands[4].command_type == MOL_COMMAND_ALL_NOTES_OFF &&
              commands[6].command_type == MOL_COMMAND_ALL_SOUND_OFF);
  EXPECT_TRUE(decoder.active_note_count() == 0u);

  const std::uint8_t held[] = {0x90u, 62u, 127u};
  EXPECT_TRUE(decoder.feed(held, sizeof(held)) == MOL_OK);
  decoder.release_all();
  EXPECT_TRUE(decoder.active_note_count() == 0u);
  EXPECT_TRUE(commands[commands.size() - 4u].command_type == MOL_COMMAND_NOTE_OFF);
  EXPECT_TRUE(commands[commands.size() - 3u].command_type == MOL_COMMAND_SUSTAIN);
  EXPECT_TRUE(commands[commands.size() - 2u].command_type == MOL_COMMAND_PITCH_BEND);
  EXPECT_TRUE(commands.back().command_type == MOL_COMMAND_SET_PARAMETER);
}

void test_repeat_bound_and_sink_failure() {
  std::vector<mol_command_t> commands;
  molkeyboardd::MidiStreamDecoder decoder(
      11u, 0u, [&commands](const mol_command_t& command) {
        commands.push_back(command);
        return MOL_OK;
      });
  const std::uint8_t repeated[] = {0x90u, 60u, 1u, 60u, 2u, 60u, 3u,
                                    60u,   4u,  60u, 5u};
  EXPECT_TRUE(decoder.feed(repeated, sizeof(repeated)) == MOL_OK);
  EXPECT_TRUE(commands.size() == 6u && decoder.active_note_count() == 4u);
  EXPECT_TRUE(commands[4].command_type == MOL_COMMAND_NOTE_OFF &&
              commands[4].gesture_id == commands[0].gesture_id &&
              commands[5].command_type == MOL_COMMAND_NOTE_ON);
  const std::uint8_t releases[] = {0x80u, 60u, 0u, 60u, 0u, 60u, 0u, 60u, 0u};
  EXPECT_TRUE(decoder.feed(releases, sizeof(releases)) == MOL_OK);
  EXPECT_TRUE(decoder.active_note_count() == 0u);

  molkeyboardd::MidiStreamDecoder rejecting(
      12u, 0u, [](const mol_command_t&) { return MOL_ERROR_QUEUE_FULL; });
  const std::uint8_t note[] = {0x90u, 64u, 127u};
  EXPECT_TRUE(rejecting.feed(note, sizeof(note)) == MOL_ERROR_QUEUE_FULL);
  EXPECT_TRUE(rejecting.active_note_count() == 0u);
  rejecting.release_all();
}

}  // namespace

int main() {
  test_notes_and_running_status();
  test_controls_and_program();
  test_filtering_and_system_bytes();
  test_pressure_mode_messages_and_release();
  test_repeat_bound_and_sink_failure();
  if (failures != 0) {
    std::fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
