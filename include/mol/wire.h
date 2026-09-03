/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_WIRE_H_
#define MOL_WIRE_H_

#include <stddef.h>
#include <stdint.h>

#include "mol/command.h"
#include "mol/export.h"
#include "mol/result.h"
#include "mol/version.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOL_WIRE_EVENT_V1_SIZE 48u
#define MOL_WIRE_FORMAT_VERSION 1u

typedef uint16_t mol_wire_type_t;
enum {
  MOL_WIRE_NOTE_ON = 1u,
  MOL_WIRE_NOTE_OFF = 2u,
  MOL_WIRE_CONTROL = 3u,
  MOL_WIRE_PITCH_BEND = 4u,
  MOL_WIRE_ALL_NOTES_OFF = 5u,
  MOL_WIRE_ALL_SOUND_OFF = 6u
};

typedef uint16_t mol_wire_control_t;
enum {
  MOL_WIRE_CONTROL_SUSTAIN = 1u,
  MOL_WIRE_CONTROL_MASTER_GAIN = 2u,
  MOL_WIRE_CONTROL_MODULATION = 3u
};

typedef union mol_wire_payload {
  struct {
    uint8_t note;
    uint8_t reserved[3];
    float velocity;
  } note;
  struct {
    mol_wire_control_t control;
    uint16_t reserved;
    float value;
  } control;
  struct {
    float value;
    uint32_t reserved;
  } pitch_bend;
  uint8_t bytes[8];
} mol_wire_payload_t;

typedef struct mol_wire_event_v1 {
  uint32_t struct_size;
  uint32_t api_version;
  mol_wire_type_t type;
  uint16_t flags;
  uint32_t sequence;
  uint64_t timestamp;
  uint32_t source_id;
  mol_gesture_id_t gesture_id;
  mol_wire_payload_t payload;
} mol_wire_event_v1_t;

/** Encodes one canonical fixed-size little-endian MolWireEventV1 packet. */
MOL_API mol_result_t mol_wire_event_v1_encode(const mol_wire_event_v1_t* event, uint8_t* output,
                                              size_t capacity);

/** Decodes exactly one fixed-size MolWireEventV1 packet with strict validation. */
MOL_API mol_result_t mol_wire_event_v1_decode(const uint8_t* data, size_t size,
                                              mol_wire_event_v1_t* event);

/** Converts a validated packet to a core command when the control is supported. */
MOL_API mol_result_t mol_wire_event_v1_to_command(const mol_wire_event_v1_t* event,
                                                  mol_command_t* command);

#ifdef __cplusplus
}
#endif

#endif /* MOL_WIRE_H_ */
