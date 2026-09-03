// SPDX-License-Identifier: Apache-2.0
#include <stdint.h>
#include <stdio.h>

#include "mol_ios_hardware_keys.h"

#define CHECK(condition)                                                                       \
  do {                                                                                         \
    if (!(condition)) {                                                                        \
      fprintf(stderr, "iOS hardware key check failed at line %d: %s\n", __LINE__, #condition); \
      return 1;                                                                                \
    }                                                                                          \
  } while (0)

int main(void) {
  static const uint32_t k_note_usages[30] = {
      0x1du, 0x16u, 0x1bu, 0x07u, 0x06u, 0x19u, 0x0au, 0x05u, 0x0bu, 0x11u,
      0x0du, 0x10u, 0x14u, 0x1fu, 0x1au, 0x20u, 0x08u, 0x15u, 0x22u, 0x17u,
      0x23u, 0x1cu, 0x24u, 0x18u, 0x0cu, 0x26u, 0x12u, 0x27u, 0x13u, 0x2fu,
  };
  mol_ios_hardware_keys_t keys;
  mol_ios_hardware_key_action_t action;
  mol_ios_hardware_key_action_t releases[MOL_IOS_HARDWARE_KEY_CAPACITY];
  size_t index;
  size_t count;

  mol_ios_hardware_keys_init(&keys);
  CHECK(mol_ios_hardware_keys_active_count(&keys) == 0u);
  CHECK(!mol_ios_hardware_keys_process(&keys, 0x04u, true, &action));
  CHECK(action.type == MOL_IOS_HARDWARE_KEY_ACTION_NONE);
  CHECK(!mol_ios_hardware_keys_process(&keys, k_note_usages[0], false, &action));

  for (index = 0u; index < 30u; ++index) {
    CHECK(mol_ios_hardware_keys_process(&keys, k_note_usages[index], true, &action));
    CHECK(action.type == MOL_IOS_HARDWARE_KEY_ACTION_NOTE);
    CHECK(action.pressed);
    CHECK(action.note == (uint8_t)(60u + index));
    CHECK(action.gesture_id == (MOL_IOS_HARDWARE_GESTURE_PREFIX | (uint64_t)k_note_usages[index]));
    CHECK(mol_ios_hardware_keys_process(&keys, k_note_usages[index], true, &action));
    CHECK(action.type == MOL_IOS_HARDWARE_KEY_ACTION_NONE);
  }
  CHECK(mol_ios_hardware_keys_active_count(&keys) == 30u);
  CHECK(mol_ios_hardware_keys_process(&keys, 0x2cu, true, &action));
  CHECK(action.type == MOL_IOS_HARDWARE_KEY_ACTION_SUSTAIN && action.pressed);
  CHECK(action.gesture_id == (MOL_IOS_HARDWARE_GESTURE_PREFIX | UINT64_C(0x2c)));
  CHECK(mol_ios_hardware_keys_active_count(&keys) == MOL_IOS_HARDWARE_KEY_CAPACITY);

  mol_ios_hardware_keys_cancel_press(&keys, k_note_usages[0]);
  CHECK(mol_ios_hardware_keys_active_count(&keys) == 30u);
  CHECK(!mol_ios_hardware_keys_process(&keys, k_note_usages[0], false, &action));
  CHECK(mol_ios_hardware_keys_process(&keys, k_note_usages[0], true, &action));
  CHECK(mol_ios_hardware_keys_process(&keys, k_note_usages[2], false, &action));
  CHECK(action.type == MOL_IOS_HARDWARE_KEY_ACTION_NOTE && !action.pressed && action.note == 62u);

  count = mol_ios_hardware_keys_release_all(&keys, releases, MOL_IOS_HARDWARE_KEY_CAPACITY);
  CHECK(count == 30u);
  for (index = 0u; index < count; ++index) {
    CHECK(releases[index].type != MOL_IOS_HARDWARE_KEY_ACTION_NONE);
    CHECK(!releases[index].pressed);
  }
  CHECK(mol_ios_hardware_keys_active_count(&keys) == 0u);
  CHECK(mol_ios_hardware_keys_release_all(&keys, releases, MOL_IOS_HARDWARE_KEY_CAPACITY) == 0u);

  for (index = 0u; index < 3u; ++index) {
    CHECK(mol_ios_hardware_keys_process(&keys, k_note_usages[index], true, &action));
  }
  CHECK(mol_ios_hardware_keys_release_all(&keys, releases, 2u) == 2u);
  CHECK(mol_ios_hardware_keys_active_count(&keys) == 1u);
  CHECK(mol_ios_hardware_keys_release_all(&keys, releases, 2u) == 1u);
  CHECK(mol_ios_hardware_keys_active_count(&keys) == 0u);

  mol_ios_hardware_keys_init(NULL);
  mol_ios_hardware_keys_cancel_press(NULL, k_note_usages[0]);
  mol_ios_hardware_keys_cancel_press(&keys, 0xffffffffu);
  CHECK(!mol_ios_hardware_keys_process(NULL, k_note_usages[0], true, &action));
  CHECK(!mol_ios_hardware_keys_process(&keys, k_note_usages[0], true, NULL));
  CHECK(mol_ios_hardware_keys_release_all(NULL, releases, 1u) == 0u);
  CHECK(mol_ios_hardware_keys_release_all(&keys, NULL, 1u) == 0u);
  CHECK(mol_ios_hardware_keys_active_count(NULL) == 0u);
  return 0;
}
