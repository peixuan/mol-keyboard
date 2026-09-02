/* SPDX-License-Identifier: Apache-2.0 */
#include "hid_keyboard.h"

#include <stdbool.h>
#include <string.h>

#include "mol/music.h"
#include "mol/version.h"

static bool contains_usage(const uint8_t keys[MOL_HID_BOOT_KEY_COUNT], uint8_t usage) {
  size_t index;
  for (index = 0u; index < MOL_HID_BOOT_KEY_COUNT; ++index) {
    if (keys[index] == usage) {
      return true;
    }
  }
  return false;
}

static size_t find_usage(const uint8_t keys[MOL_HID_BOOT_KEY_COUNT], uint8_t usage) {
  size_t index;
  for (index = 0u; index < MOL_HID_BOOT_KEY_COUNT; ++index) {
    if (keys[index] == usage) {
      return index;
    }
  }
  return MOL_HID_BOOT_KEY_COUNT;
}

static bool usage_emits_command(uint8_t usage) {
  uint8_t note;
  return usage == MOL_HID_SPACE_USAGE || mol_keyboard_note_from_hid_usage(usage, &note) == MOL_OK;
}

static mol_command_t make_command(uint32_t source_id, mol_command_type_t type,
                                  mol_gesture_id_t gesture) {
  mol_command_t command;
  memset(&command, 0, sizeof(command));
  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = type;
  command.source_id = source_id;
  command.target_frame = MOL_FRAME_IMMEDIATE;
  command.gesture_id = gesture;
  return command;
}

static void write_key_command(uint32_t source_id, uint8_t usage, bool pressed,
                              mol_gesture_id_t gesture, mol_command_t* command) {
  uint8_t note;
  if (usage == MOL_HID_SPACE_USAGE) {
    *command = make_command(source_id, MOL_COMMAND_SUSTAIN, 0u);
    command->payload.scalar.value = pressed ? 1.0f : 0.0f;
    return;
  }
  (void)mol_keyboard_note_from_hid_usage(usage, &note);
  *command = make_command(source_id, pressed ? MOL_COMMAND_NOTE_ON : MOL_COMMAND_NOTE_OFF, gesture);
  command->payload.note.note = note;
  command->payload.note.velocity = pressed ? 0.8f : 0.0f;
}

void mol_hid_keyboard_init(mol_hid_keyboard_state_t* state, uint32_t source_id) {
  if (state == NULL) {
    return;
  }
  memset(state, 0, sizeof(*state));
  state->source_id = source_id;
}

mol_hid_keyboard_result_t mol_hid_keyboard_process(mol_hid_keyboard_state_t* state,
                                                   const uint8_t* report, size_t report_size,
                                                   mol_command_t* commands, size_t command_capacity,
                                                   size_t* command_count) {
  mol_hid_keyboard_state_t candidate;
  uint8_t next_keys[MOL_HID_BOOT_KEY_COUNT];
  size_t required = 0u;
  size_t index;
  size_t output_index = 0u;

  if (state == NULL || report == NULL || command_count == NULL ||
      report_size != MOL_HID_BOOT_REPORT_SIZE || (commands == NULL && command_capacity != 0u)) {
    return MOL_HID_KEYBOARD_INVALID_ARGUMENT;
  }
  memcpy(next_keys, &report[2], sizeof(next_keys));
  for (index = 0u; index < MOL_HID_BOOT_KEY_COUNT; ++index) {
    size_t other;
    if (next_keys[index] >= 1u && next_keys[index] <= 3u) {
      return MOL_HID_KEYBOARD_INVALID_REPORT;
    }
    if (next_keys[index] == 0u) {
      continue;
    }
    for (other = index + 1u; other < MOL_HID_BOOT_KEY_COUNT; ++other) {
      if (next_keys[index] == next_keys[other]) {
        return MOL_HID_KEYBOARD_INVALID_REPORT;
      }
    }
  }

  for (index = 0u; index < MOL_HID_BOOT_KEY_COUNT; ++index) {
    const uint8_t usage = state->keys[index];
    if (usage != 0u && !contains_usage(next_keys, usage) && usage_emits_command(usage)) {
      ++required;
    }
  }
  for (index = 0u; index < MOL_HID_BOOT_KEY_COUNT; ++index) {
    const uint8_t usage = next_keys[index];
    if (usage != 0u && !contains_usage(state->keys, usage) && usage_emits_command(usage)) {
      ++required;
    }
  }
  *command_count = required;
  if (command_capacity < required) {
    return MOL_HID_KEYBOARD_BUFFER_TOO_SMALL;
  }

  candidate = *state;
  memset(candidate.keys, 0, sizeof(candidate.keys));
  memset(candidate.gestures, 0, sizeof(candidate.gestures));
  for (index = 0u; index < MOL_HID_BOOT_KEY_COUNT; ++index) {
    const uint8_t usage = state->keys[index];
    if (usage != 0u && !contains_usage(next_keys, usage) && usage_emits_command(usage)) {
      write_key_command(state->source_id, usage, false, state->gestures[index],
                        &commands[output_index++]);
    }
  }
  for (index = 0u; index < MOL_HID_BOOT_KEY_COUNT; ++index) {
    const uint8_t usage = next_keys[index];
    const size_t old_index = find_usage(state->keys, usage);
    candidate.keys[index] = usage;
    if (usage == 0u) {
      continue;
    }
    if (old_index < MOL_HID_BOOT_KEY_COUNT) {
      candidate.gestures[index] = state->gestures[old_index];
      continue;
    }
    if (usage == MOL_HID_SPACE_USAGE) {
      write_key_command(state->source_id, usage, true, 0u, &commands[output_index++]);
      continue;
    }
    {
      uint8_t note;
      if (mol_keyboard_note_from_hid_usage(usage, &note) == MOL_OK) {
        ++candidate.gesture_serial;
        if (candidate.gesture_serial == 0u) {
          ++candidate.gesture_serial;
        }
        candidate.gestures[index] =
            ((mol_gesture_id_t)state->source_id << 32u) | candidate.gesture_serial;
        write_key_command(state->source_id, usage, true, candidate.gestures[index],
                          &commands[output_index++]);
      }
    }
  }
  *state = candidate;
  return MOL_HID_KEYBOARD_OK;
}

mol_hid_keyboard_result_t mol_hid_keyboard_disconnect(mol_hid_keyboard_state_t* state,
                                                      mol_command_t* commands,
                                                      size_t command_capacity,
                                                      size_t* command_count) {
  const bool sustain_held =
      state != NULL && contains_usage(state->keys, (uint8_t)MOL_HID_SPACE_USAGE);
  const size_t required = sustain_held ? 2u : 1u;
  if (state == NULL || command_count == NULL || (commands == NULL && command_capacity != 0u)) {
    return MOL_HID_KEYBOARD_INVALID_ARGUMENT;
  }
  *command_count = required;
  if (command_capacity < required) {
    return MOL_HID_KEYBOARD_BUFFER_TOO_SMALL;
  }
  if (sustain_held) {
    commands[0] = make_command(state->source_id, MOL_COMMAND_SUSTAIN, 0u);
    commands[0].payload.scalar.value = 0.0f;
  }
  commands[required - 1u] = make_command(state->source_id, MOL_COMMAND_ALL_NOTES_OFF, 0u);
  memset(state->keys, 0, sizeof(state->keys));
  memset(state->gestures, 0, sizeof(state->gestures));
  return MOL_HID_KEYBOARD_OK;
}
