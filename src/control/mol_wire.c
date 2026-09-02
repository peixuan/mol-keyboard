/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <string.h>

#include "mol/wire.h"

static uint16_t read_u16(const uint8_t* data) {
  return (uint16_t)((uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8u));
}

static uint32_t read_u32(const uint8_t* data) {
  return (uint32_t)data[0] | (uint32_t)data[1] << 8u | (uint32_t)data[2] << 16u |
         (uint32_t)data[3] << 24u;
}

static uint64_t read_u64(const uint8_t* data) {
  return (uint64_t)read_u32(data) | (uint64_t)read_u32(data + 4u) << 32u;
}

static void write_u16(uint8_t* data, uint16_t value) {
  data[0] = (uint8_t)value;
  data[1] = (uint8_t)(value >> 8u);
}

static void write_u32(uint8_t* data, uint32_t value) {
  for (uint32_t index = 0u; index < 4u; ++index) data[index] = (uint8_t)(value >> (index * 8u));
}

static void write_u64(uint8_t* data, uint64_t value) {
  write_u32(data, (uint32_t)value);
  write_u32(data + 4u, (uint32_t)(value >> 32u));
}

static float read_float(const uint8_t* data) {
  uint32_t bits = read_u32(data);
  float value;
  memcpy(&value, &bits, sizeof(value));
  return value;
}

static void write_float(uint8_t* data, float value) {
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));
  write_u32(data, bits);
}

static mol_result_t validate(const mol_wire_event_v1_t* event) {
  float value = 0.0f;
  if (event == NULL || event->struct_size != sizeof(*event) ||
      event->api_version != MOL_API_VERSION || event->flags != 0u) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  switch (event->type) {
    case MOL_WIRE_NOTE_ON:
    case MOL_WIRE_NOTE_OFF:
      value = event->payload.note.velocity;
      if (event->payload.note.note > 127u || !isfinite(value) || value < 0.0f || value > 1.0f)
        return MOL_ERROR_INVALID_ARGUMENT;
      break;
    case MOL_WIRE_CONTROL:
      value = event->payload.control.value;
      if (event->payload.control.control < MOL_WIRE_CONTROL_SUSTAIN ||
          event->payload.control.control > MOL_WIRE_CONTROL_MODULATION || !isfinite(value) ||
          value < 0.0f || value > 1.0f)
        return MOL_ERROR_INVALID_ARGUMENT;
      break;
    case MOL_WIRE_PITCH_BEND:
      value = event->payload.pitch_bend.value;
      if (!isfinite(value) || value < -1.0f || value > 1.0f) return MOL_ERROR_INVALID_ARGUMENT;
      break;
    case MOL_WIRE_ALL_NOTES_OFF:
    case MOL_WIRE_ALL_SOUND_OFF:
      break;
    default:
      return MOL_ERROR_UNSUPPORTED;
  }
  return MOL_OK;
}

mol_result_t mol_wire_event_v1_encode(const mol_wire_event_v1_t* event, uint8_t* output,
                                      size_t capacity) {
  mol_result_t result = validate(event);
  if (result != MOL_OK) return result;
  if (output == NULL || capacity < MOL_WIRE_EVENT_V1_SIZE) return MOL_ERROR_BUFFER_TOO_SMALL;
  memset(output, 0, MOL_WIRE_EVENT_V1_SIZE);
  memcpy(output, "MOLW", 4u);
  write_u16(output + 4u, MOL_WIRE_FORMAT_VERSION);
  write_u16(output + 6u, event->type);
  write_u16(output + 8u, MOL_WIRE_EVENT_V1_SIZE);
  write_u16(output + 10u, event->flags);
  write_u32(output + 12u, event->sequence);
  write_u64(output + 16u, event->timestamp);
  write_u32(output + 24u, event->source_id);
  write_u64(output + 32u, event->gesture_id);
  if (event->type == MOL_WIRE_NOTE_ON || event->type == MOL_WIRE_NOTE_OFF) {
    output[40] = event->payload.note.note;
    write_float(output + 44u, event->payload.note.velocity);
  } else if (event->type == MOL_WIRE_CONTROL) {
    write_u16(output + 40u, event->payload.control.control);
    write_float(output + 44u, event->payload.control.value);
  } else if (event->type == MOL_WIRE_PITCH_BEND) {
    write_float(output + 40u, event->payload.pitch_bend.value);
  }
  return MOL_OK;
}

mol_result_t mol_wire_event_v1_decode(const uint8_t* data, size_t size,
                                      mol_wire_event_v1_t* event) {
  if (data == NULL || event == NULL || event->struct_size != sizeof(*event) ||
      event->api_version != MOL_API_VERSION)
    return MOL_ERROR_INVALID_ARGUMENT;
  if (size != MOL_WIRE_EVENT_V1_SIZE || memcmp(data, "MOLW", 4u) != 0)
    return MOL_ERROR_CORRUPT_DATA;
  if (read_u16(data + 4u) != MOL_WIRE_FORMAT_VERSION) return MOL_ERROR_UNSUPPORTED_VERSION;
  if (read_u16(data + 8u) != MOL_WIRE_EVENT_V1_SIZE || read_u32(data + 28u) != 0u)
    return MOL_ERROR_CORRUPT_DATA;
  memset(event->payload.bytes, 0, sizeof(event->payload.bytes));
  event->type = read_u16(data + 6u);
  event->flags = read_u16(data + 10u);
  event->sequence = read_u32(data + 12u);
  event->timestamp = read_u64(data + 16u);
  event->source_id = read_u32(data + 24u);
  event->gesture_id = read_u64(data + 32u);
  if (event->type == MOL_WIRE_NOTE_ON || event->type == MOL_WIRE_NOTE_OFF) {
    event->payload.note.note = data[40];
    if (data[41] != 0u || data[42] != 0u || data[43] != 0u) return MOL_ERROR_CORRUPT_DATA;
    event->payload.note.velocity = read_float(data + 44u);
  } else if (event->type == MOL_WIRE_CONTROL) {
    event->payload.control.control = read_u16(data + 40u);
    if (read_u16(data + 42u) != 0u) return MOL_ERROR_CORRUPT_DATA;
    event->payload.control.value = read_float(data + 44u);
  } else if (event->type == MOL_WIRE_PITCH_BEND) {
    event->payload.pitch_bend.value = read_float(data + 40u);
    if (read_u32(data + 44u) != 0u) return MOL_ERROR_CORRUPT_DATA;
  } else if (read_u64(data + 40u) != 0u) {
    return MOL_ERROR_CORRUPT_DATA;
  }
  return validate(event);
}

mol_result_t mol_wire_event_v1_to_command(const mol_wire_event_v1_t* event,
                                          mol_command_t* command) {
  mol_result_t result = validate(event);
  if (result != MOL_OK || command == NULL)
    return result != MOL_OK ? result : MOL_ERROR_INVALID_ARGUMENT;
  memset(command, 0, sizeof(*command));
  command->struct_size = (uint32_t)sizeof(*command);
  command->api_version = MOL_API_VERSION;
  command->source_id = event->source_id;
  command->gesture_id = event->gesture_id;
  command->target_frame = event->timestamp;
  switch (event->type) {
    case MOL_WIRE_NOTE_ON:
      command->command_type = MOL_COMMAND_NOTE_ON;
      command->payload.note.note = event->payload.note.note;
      command->payload.note.velocity = event->payload.note.velocity;
      break;
    case MOL_WIRE_NOTE_OFF:
      command->command_type = MOL_COMMAND_NOTE_OFF;
      command->payload.note.note = event->payload.note.note;
      command->payload.note.velocity = event->payload.note.velocity;
      break;
    case MOL_WIRE_CONTROL:
      if (event->payload.control.control == MOL_WIRE_CONTROL_SUSTAIN)
        command->command_type = MOL_COMMAND_SUSTAIN;
      else if (event->payload.control.control == MOL_WIRE_CONTROL_MASTER_GAIN)
        command->command_type = MOL_COMMAND_SET_MASTER_GAIN;
      else
        return MOL_ERROR_UNSUPPORTED;
      command->payload.scalar.value = event->payload.control.value;
      break;
    case MOL_WIRE_PITCH_BEND:
      command->command_type = MOL_COMMAND_PITCH_BEND;
      command->payload.scalar.value = event->payload.pitch_bend.value;
      break;
    case MOL_WIRE_ALL_NOTES_OFF:
      command->command_type = MOL_COMMAND_ALL_NOTES_OFF;
      break;
    case MOL_WIRE_ALL_SOUND_OFF:
      command->command_type = MOL_COMMAND_ALL_SOUND_OFF;
      break;
    default:
      return MOL_ERROR_UNSUPPORTED;
  }
  return MOL_OK;
}
