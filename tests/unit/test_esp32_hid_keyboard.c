/* SPDX-License-Identifier: Apache-2.0 */
#include <stdio.h>
#include <string.h>

#include "hid_keyboard.h"

static int failures;

#define EXPECT_TRUE(condition)                                                           \
  do {                                                                                   \
    if (!(condition)) {                                                                  \
      fprintf(stderr, "%s:%d expectation failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                        \
    }                                                                                    \
  } while (0)

static mol_hid_keyboard_result_t process(mol_hid_keyboard_state_t* state,
                                         const uint8_t keys[MOL_HID_BOOT_KEY_COUNT],
                                         mol_command_t* commands, size_t capacity, size_t* count) {
  uint8_t report[MOL_HID_BOOT_REPORT_SIZE] = {0u};
  memcpy(&report[2], keys, MOL_HID_BOOT_KEY_COUNT);
  return mol_hid_keyboard_process(state, report, sizeof(report), commands, capacity, count);
}

static void test_press_repeat_release_preserves_gesture(void) {
  mol_hid_keyboard_state_t state;
  mol_command_t commands[MOL_HID_MAX_REPORT_COMMANDS];
  const uint8_t pressed[MOL_HID_BOOT_KEY_COUNT] = {0x1du};
  const uint8_t released[MOL_HID_BOOT_KEY_COUNT] = {0u};
  mol_gesture_id_t gesture;
  size_t count = 0u;
  mol_hid_keyboard_init(&state, 7u);

  EXPECT_TRUE(process(&state, pressed, commands, MOL_HID_MAX_REPORT_COMMANDS, &count) ==
              MOL_HID_KEYBOARD_OK);
  EXPECT_TRUE(count == 1u && commands[0].command_type == MOL_COMMAND_NOTE_ON);
  EXPECT_TRUE(commands[0].payload.note.note == 60u && commands[0].payload.note.velocity == 0.8f);
  EXPECT_TRUE(commands[0].source_id == 7u && commands[0].target_frame == MOL_FRAME_IMMEDIATE);
  gesture = commands[0].gesture_id;
  EXPECT_TRUE(gesture == ((UINT64_C(7) << 32u) | UINT64_C(1)));

  memset(commands, 0xa5, sizeof(commands));
  EXPECT_TRUE(process(&state, pressed, commands, MOL_HID_MAX_REPORT_COMMANDS, &count) ==
              MOL_HID_KEYBOARD_OK);
  EXPECT_TRUE(count == 0u);
  EXPECT_TRUE(process(&state, released, commands, MOL_HID_MAX_REPORT_COMMANDS, &count) ==
              MOL_HID_KEYBOARD_OK);
  EXPECT_TRUE(count == 1u && commands[0].command_type == MOL_COMMAND_NOTE_OFF);
  EXPECT_TRUE(commands[0].gesture_id == gesture && commands[0].payload.note.note == 60u);
}

static void test_release_before_press_and_sustain(void) {
  mol_hid_keyboard_state_t state;
  mol_command_t commands[MOL_HID_MAX_REPORT_COMMANDS];
  const uint8_t first[MOL_HID_BOOT_KEY_COUNT] = {0x1du, MOL_HID_SPACE_USAGE};
  const uint8_t second[MOL_HID_BOOT_KEY_COUNT] = {0x16u};
  size_t count = 0u;
  mol_hid_keyboard_init(&state, 9u);
  EXPECT_TRUE(process(&state, first, commands, MOL_HID_MAX_REPORT_COMMANDS, &count) ==
              MOL_HID_KEYBOARD_OK);
  EXPECT_TRUE(count == 2u && commands[0].command_type == MOL_COMMAND_NOTE_ON);
  EXPECT_TRUE(commands[1].command_type == MOL_COMMAND_SUSTAIN &&
              commands[1].payload.scalar.value == 1.0f);

  EXPECT_TRUE(process(&state, second, commands, MOL_HID_MAX_REPORT_COMMANDS, &count) ==
              MOL_HID_KEYBOARD_OK);
  EXPECT_TRUE(count == 3u);
  EXPECT_TRUE(commands[0].command_type == MOL_COMMAND_NOTE_OFF &&
              commands[0].payload.note.note == 60u);
  EXPECT_TRUE(commands[1].command_type == MOL_COMMAND_SUSTAIN &&
              commands[1].payload.scalar.value == 0.0f);
  EXPECT_TRUE(commands[2].command_type == MOL_COMMAND_NOTE_ON &&
              commands[2].payload.note.note == 61u);
}

static void test_invalid_reports_are_transactional(void) {
  mol_hid_keyboard_state_t state;
  mol_hid_keyboard_state_t before;
  mol_command_t commands[MOL_HID_MAX_REPORT_COMMANDS];
  const uint8_t pressed[MOL_HID_BOOT_KEY_COUNT] = {0x1du};
  const uint8_t rollover[MOL_HID_BOOT_KEY_COUNT] = {0x01u};
  const uint8_t duplicate[MOL_HID_BOOT_KEY_COUNT] = {0x05u, 0x05u};
  size_t count = 0u;
  mol_hid_keyboard_init(&state, 3u);
  EXPECT_TRUE(process(&state, pressed, commands, MOL_HID_MAX_REPORT_COMMANDS, &count) ==
              MOL_HID_KEYBOARD_OK);
  before = state;
  EXPECT_TRUE(process(&state, rollover, commands, MOL_HID_MAX_REPORT_COMMANDS, &count) ==
              MOL_HID_KEYBOARD_INVALID_REPORT);
  EXPECT_TRUE(memcmp(&state, &before, sizeof(state)) == 0);
  EXPECT_TRUE(process(&state, duplicate, commands, MOL_HID_MAX_REPORT_COMMANDS, &count) ==
              MOL_HID_KEYBOARD_INVALID_REPORT);
  EXPECT_TRUE(memcmp(&state, &before, sizeof(state)) == 0);
}

static void test_small_buffer_is_transactional(void) {
  mol_hid_keyboard_state_t state;
  mol_hid_keyboard_state_t before;
  mol_command_t command;
  const uint8_t two_keys[MOL_HID_BOOT_KEY_COUNT] = {0x1du, 0x16u};
  size_t count = 0u;
  mol_hid_keyboard_init(&state, 1u);
  before = state;
  memset(&command, 0x5a, sizeof(command));
  EXPECT_TRUE(process(&state, two_keys, &command, 1u, &count) == MOL_HID_KEYBOARD_BUFFER_TOO_SMALL);
  EXPECT_TRUE(count == 2u && memcmp(&state, &before, sizeof(state)) == 0);
  {
    mol_command_t untouched;
    memset(&untouched, 0x5a, sizeof(untouched));
    EXPECT_TRUE(memcmp(&command, &untouched, sizeof(command)) == 0);
  }
}

static void test_unmapped_usage_is_tracked_without_commands(void) {
  mol_hid_keyboard_state_t state;
  mol_command_t commands[MOL_HID_MAX_REPORT_COMMANDS];
  const uint8_t media_key[MOL_HID_BOOT_KEY_COUNT] = {0x65u};
  const uint8_t none[MOL_HID_BOOT_KEY_COUNT] = {0u};
  size_t count = 99u;
  mol_hid_keyboard_init(&state, 1u);
  EXPECT_TRUE(process(&state, media_key, commands, MOL_HID_MAX_REPORT_COMMANDS, &count) ==
              MOL_HID_KEYBOARD_OK);
  EXPECT_TRUE(count == 0u && state.keys[0] == 0x65u);
  EXPECT_TRUE(process(&state, none, commands, MOL_HID_MAX_REPORT_COMMANDS, &count) ==
              MOL_HID_KEYBOARD_OK);
  EXPECT_TRUE(count == 0u && state.keys[0] == 0u);
}

static void test_disconnect_clears_sustain_and_all_notes(void) {
  mol_hid_keyboard_state_t state;
  mol_hid_keyboard_state_t before;
  mol_command_t commands[2];
  const uint8_t pressed[MOL_HID_BOOT_KEY_COUNT] = {0x1du, MOL_HID_SPACE_USAGE};
  size_t count = 0u;
  mol_hid_keyboard_init(&state, 12u);
  EXPECT_TRUE(process(&state, pressed, commands, 2u, &count) == MOL_HID_KEYBOARD_OK);
  before = state;
  EXPECT_TRUE(mol_hid_keyboard_disconnect(&state, commands, 1u, &count) ==
              MOL_HID_KEYBOARD_BUFFER_TOO_SMALL);
  EXPECT_TRUE(count == 2u && memcmp(&state, &before, sizeof(state)) == 0);
  EXPECT_TRUE(mol_hid_keyboard_disconnect(&state, commands, 2u, &count) == MOL_HID_KEYBOARD_OK);
  EXPECT_TRUE(count == 2u && commands[0].command_type == MOL_COMMAND_SUSTAIN);
  EXPECT_TRUE(commands[1].command_type == MOL_COMMAND_ALL_NOTES_OFF &&
              commands[1].source_id == 12u);
  EXPECT_TRUE(state.keys[0] == 0u && state.keys[1] == 0u);
}

int main(void) {
  test_press_repeat_release_preserves_gesture();
  test_release_before_press_and_sustain();
  test_invalid_reports_are_transactional();
  test_small_buffer_is_transactional();
  test_unmapped_usage_is_tracked_without_commands();
  test_disconnect_clears_sustain_and_all_notes();
  if (failures != 0) {
    fprintf(stderr, "%d ESP32 HID keyboard test(s) failed\n", failures);
    return 1;
  }
  puts("ESP32 HID keyboard tests passed");
  return 0;
}
