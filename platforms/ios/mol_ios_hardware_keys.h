// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_IOS_HARDWARE_KEYS_H
#define MOL_IOS_HARDWARE_KEYS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOL_IOS_HARDWARE_KEY_CAPACITY 31u
#define MOL_IOS_HARDWARE_GESTURE_PREFIX (UINT64_C(1) << 52u)

typedef enum mol_ios_hardware_key_action_type {
  MOL_IOS_HARDWARE_KEY_ACTION_NONE = 0,
  MOL_IOS_HARDWARE_KEY_ACTION_NOTE = 1,
  MOL_IOS_HARDWARE_KEY_ACTION_SUSTAIN = 2
} mol_ios_hardware_key_action_type_t;

typedef struct mol_ios_hardware_key_action {
  mol_ios_hardware_key_action_type_t type;
  bool pressed;
  uint8_t note;
  uint64_t gesture_id;
} mol_ios_hardware_key_action_t;

typedef struct mol_ios_hardware_keys {
  uint32_t active_mask;
} mol_ios_hardware_keys_t;

void mol_ios_hardware_keys_init(mol_ios_hardware_keys_t* keys);

// Returns true when the HID usage belongs to the instrument. Repeated presses
// are consumed with ACTION_NONE; releases without an owned press are unhandled.
bool mol_ios_hardware_keys_process(mol_ios_hardware_keys_t* keys, uint32_t usage, bool pressed,
                                   mol_ios_hardware_key_action_t* action);

// Rolls back an accepted press when the audio controller rejects it.
void mol_ios_hardware_keys_cancel_press(mol_ios_hardware_keys_t* keys, uint32_t usage);

// Emits and clears at most action_capacity owned releases. Call again if the
// returned count fills the output; the production host supplies full capacity.
size_t mol_ios_hardware_keys_release_all(mol_ios_hardware_keys_t* keys,
                                         mol_ios_hardware_key_action_t* actions,
                                         size_t action_capacity);

size_t mol_ios_hardware_keys_active_count(const mol_ios_hardware_keys_t* keys);

#ifdef __cplusplus
}
#endif

#endif
