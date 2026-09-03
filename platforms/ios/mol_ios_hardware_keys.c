// SPDX-License-Identifier: Apache-2.0
#include "mol_ios_hardware_keys.h"

typedef struct mol_ios_hardware_key_binding {
  uint32_t usage;
  mol_ios_hardware_key_action_type_t type;
  uint8_t note;
} mol_ios_hardware_key_binding_t;

typedef char
    mol_ios_hardware_key_capacity_must_fit_mask[(MOL_IOS_HARDWARE_KEY_CAPACITY <= 32u) ? 1 : -1];

// USB HID keyboard usages used by UIKit's UIKeyboardHIDUsage values.
static const mol_ios_hardware_key_binding_t k_bindings[MOL_IOS_HARDWARE_KEY_CAPACITY] = {
    {0x1du, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 60u},
    {0x16u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 61u},
    {0x1bu, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 62u},
    {0x07u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 63u},
    {0x06u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 64u},
    {0x19u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 65u},
    {0x0au, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 66u},
    {0x05u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 67u},
    {0x0bu, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 68u},
    {0x11u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 69u},
    {0x0du, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 70u},
    {0x10u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 71u},
    {0x14u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 72u},
    {0x1fu, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 73u},
    {0x1au, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 74u},
    {0x20u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 75u},
    {0x08u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 76u},
    {0x15u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 77u},
    {0x22u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 78u},
    {0x17u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 79u},
    {0x23u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 80u},
    {0x1cu, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 81u},
    {0x24u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 82u},
    {0x18u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 83u},
    {0x0cu, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 84u},
    {0x26u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 85u},
    {0x12u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 86u},
    {0x27u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 87u},
    {0x13u, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 88u},
    {0x2fu, MOL_IOS_HARDWARE_KEY_ACTION_NOTE, 89u},
    {0x2cu, MOL_IOS_HARDWARE_KEY_ACTION_SUSTAIN, 0u},
};

static void mol_ios_hardware_key_clear_action(mol_ios_hardware_key_action_t* action) {
  if (action == NULL) return;
  action->type = MOL_IOS_HARDWARE_KEY_ACTION_NONE;
  action->pressed = false;
  action->note = 0u;
  action->gesture_id = 0u;
}

static size_t mol_ios_hardware_key_binding_index(uint32_t usage) {
  size_t index;
  for (index = 0u; index < MOL_IOS_HARDWARE_KEY_CAPACITY; ++index) {
    if (k_bindings[index].usage == usage) return index;
  }
  return MOL_IOS_HARDWARE_KEY_CAPACITY;
}

static void mol_ios_hardware_key_make_action(const mol_ios_hardware_key_binding_t* binding,
                                             bool pressed, mol_ios_hardware_key_action_t* action) {
  action->type = binding->type;
  action->pressed = pressed;
  action->note = binding->note;
  action->gesture_id = MOL_IOS_HARDWARE_GESTURE_PREFIX | (uint64_t)(binding->usage & 0xffffu);
}

void mol_ios_hardware_keys_init(mol_ios_hardware_keys_t* keys) {
  if (keys != NULL) keys->active_mask = 0u;
}

bool mol_ios_hardware_keys_process(mol_ios_hardware_keys_t* keys, uint32_t usage, bool pressed,
                                   mol_ios_hardware_key_action_t* action) {
  size_t index;
  uint32_t bit;
  mol_ios_hardware_key_clear_action(action);
  if (keys == NULL || action == NULL) return false;
  index = mol_ios_hardware_key_binding_index(usage);
  if (index >= MOL_IOS_HARDWARE_KEY_CAPACITY) return false;
  bit = UINT32_C(1) << (uint32_t)index;
  if (pressed) {
    if ((keys->active_mask & bit) != 0u) return true;
    keys->active_mask |= bit;
  } else {
    if ((keys->active_mask & bit) == 0u) return false;
    keys->active_mask &= ~bit;
  }
  mol_ios_hardware_key_make_action(&k_bindings[index], pressed, action);
  return true;
}

void mol_ios_hardware_keys_cancel_press(mol_ios_hardware_keys_t* keys, uint32_t usage) {
  const size_t index = mol_ios_hardware_key_binding_index(usage);
  if (keys == NULL || index >= MOL_IOS_HARDWARE_KEY_CAPACITY) return;
  keys->active_mask &= ~(UINT32_C(1) << (uint32_t)index);
}

size_t mol_ios_hardware_keys_release_all(mol_ios_hardware_keys_t* keys,
                                         mol_ios_hardware_key_action_t* actions,
                                         size_t action_capacity) {
  size_t index;
  size_t count = 0u;
  if (keys == NULL || actions == NULL || action_capacity == 0u) return 0u;
  for (index = 0u; index < MOL_IOS_HARDWARE_KEY_CAPACITY && count < action_capacity; ++index) {
    const uint32_t bit = UINT32_C(1) << (uint32_t)index;
    if ((keys->active_mask & bit) == 0u) continue;
    mol_ios_hardware_key_make_action(&k_bindings[index], false, &actions[count]);
    keys->active_mask &= ~bit;
    ++count;
  }
  return count;
}

size_t mol_ios_hardware_keys_active_count(const mol_ios_hardware_keys_t* keys) {
  uint32_t mask;
  size_t count = 0u;
  if (keys == NULL) return 0u;
  mask = keys->active_mask;
  while (mask != 0u) {
    mask &= mask - 1u;
    ++count;
  }
  return count;
}
