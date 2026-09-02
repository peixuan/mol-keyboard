/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_ESP32_HID_KEYBOARD_H_
#define MOL_ESP32_HID_KEYBOARD_H_

#include <stddef.h>
#include <stdint.h>

#include "mol/command.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOL_HID_BOOT_REPORT_SIZE 8u
#define MOL_HID_BOOT_KEY_COUNT 6u
#define MOL_HID_MAX_REPORT_COMMANDS (MOL_HID_BOOT_KEY_COUNT * 2u)
#define MOL_HID_SPACE_USAGE 0x2cu

typedef enum mol_hid_keyboard_result {
  MOL_HID_KEYBOARD_OK = 0,
  MOL_HID_KEYBOARD_INVALID_ARGUMENT = 1,
  MOL_HID_KEYBOARD_INVALID_REPORT = 2,
  MOL_HID_KEYBOARD_BUFFER_TOO_SMALL = 3
} mol_hid_keyboard_result_t;

typedef struct mol_hid_keyboard_state {
  uint32_t source_id;
  uint32_t gesture_serial;
  uint8_t keys[MOL_HID_BOOT_KEY_COUNT];
  mol_gesture_id_t gestures[MOL_HID_BOOT_KEY_COUNT];
} mol_hid_keyboard_state_t;

/** Initializes one independent HID keyboard source. */
void mol_hid_keyboard_init(mol_hid_keyboard_state_t* state, uint32_t source_id);

/**
 * Translates an 8-byte USB HID boot-keyboard report into immediate commands.
 *
 * Key releases are emitted before presses. Repeated keys and modifier-only
 * changes do not emit commands. Rollover/error usages and duplicate key usages
 * reject the complete report. On any error the state and output buffer remain
 * unchanged. event_count receives the required capacity for a valid report.
 */
mol_hid_keyboard_result_t mol_hid_keyboard_process(mol_hid_keyboard_state_t* state,
                                                   const uint8_t* report, size_t report_size,
                                                   mol_command_t* commands, size_t command_capacity,
                                                   size_t* command_count);

/**
 * Clears the source after transport loss and emits ALL_NOTES_OFF. A sustain-off
 * command is emitted first when the space bar was held.
 */
mol_hid_keyboard_result_t mol_hid_keyboard_disconnect(mol_hid_keyboard_state_t* state,
                                                      mol_command_t* commands,
                                                      size_t command_capacity,
                                                      size_t* command_count);

#ifdef __cplusplus
}
#endif

#endif /* MOL_ESP32_HID_KEYBOARD_H_ */
