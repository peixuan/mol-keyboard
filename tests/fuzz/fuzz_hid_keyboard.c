/* SPDX-License-Identifier: Apache-2.0 */
#include <stddef.h>
#include <stdint.h>

#include "hid_keyboard.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  mol_hid_keyboard_state_t state;
  mol_command_t commands[MOL_HID_MAX_REPORT_COMMANDS];
  size_t command_count = 0u;
  mol_hid_keyboard_result_t result;
  mol_hid_keyboard_init(&state, size > 0u ? data[0] : 0u);
  result = mol_hid_keyboard_process(&state, data, size, commands, MOL_HID_MAX_REPORT_COMMANDS,
                                    &command_count);
  if (result == MOL_HID_KEYBOARD_OK && command_count > MOL_HID_MAX_REPORT_COMMANDS) {
    __builtin_trap();
  }
  return 0;
}
