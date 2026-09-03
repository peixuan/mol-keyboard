/* SPDX-License-Identifier: Apache-2.0 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mol/wire.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  mol_wire_event_v1_t event = {0};
  mol_wire_event_v1_t decoded = {0};
  mol_command_t command;
  uint8_t encoded[MOL_WIRE_EVENT_V1_SIZE];
  event.struct_size = (uint32_t)sizeof(event);
  event.api_version = MOL_API_VERSION;
  decoded.struct_size = (uint32_t)sizeof(decoded);
  decoded.api_version = MOL_API_VERSION;
  if (mol_wire_event_v1_decode(data, size, &decoded) == MOL_OK) {
    if (mol_wire_event_v1_encode(&decoded, encoded, sizeof(encoded)) != MOL_OK ||
        memcmp(data, encoded, sizeof(encoded)) != 0) {
      __builtin_trap();
    }
    if (mol_wire_event_v1_to_command(&decoded, &command) != MOL_OK) {
      __builtin_trap();
    }
  }
  if (size >= 4u) {
    event.type = (mol_wire_type_t)(data[0] % 6u + 1u);
    event.sequence = data[1];
    event.source_id = data[2];
    event.gesture_id = data[3];
    if (event.type == MOL_WIRE_NOTE_ON || event.type == MOL_WIRE_NOTE_OFF) {
      event.payload.note.note = (uint8_t)(data[2] & 0x7fu);
      event.payload.note.velocity = (float)data[3] / 255.0f;
    } else if (event.type == MOL_WIRE_CONTROL) {
      event.payload.control.control = (mol_wire_control_t)(data[2] % 3u + 1u);
      event.payload.control.value = (float)data[3] / 255.0f;
    } else if (event.type == MOL_WIRE_PITCH_BEND) {
      event.payload.pitch_bend.value = (float)data[3] / 127.5f - 1.0f;
    }
    if (mol_wire_event_v1_encode(&event, encoded, sizeof(encoded)) != MOL_OK ||
        mol_wire_event_v1_decode(encoded, sizeof(encoded), &decoded) != MOL_OK) {
      __builtin_trap();
    }
  }
  return 0;
}
