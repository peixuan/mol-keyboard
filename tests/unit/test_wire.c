/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mol/wire.h"

static int failures;

#define EXPECT_TRUE(condition)                                                                  \
  do {                                                                                          \
    if (!(condition)) {                                                                         \
      (void)fprintf(stderr, "%s:%d: expectation failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                               \
    }                                                                                           \
  } while (0)

static mol_wire_event_v1_t make_event(mol_wire_type_t type) {
  mol_wire_event_v1_t event;
  memset(&event, 0, sizeof(event));
  event.struct_size = (uint32_t)sizeof(event);
  event.api_version = MOL_API_VERSION;
  event.type = type;
  event.sequence = UINT32_C(0x12345678);
  event.timestamp = UINT64_C(0x0102030405060708);
  event.source_id = UINT32_C(0x90abcdef);
  event.gesture_id = UINT64_C(0x1122334455667788);
  return event;
}

static void test_note_round_trip(void) {
  mol_wire_event_v1_t input = make_event(MOL_WIRE_NOTE_ON);
  mol_wire_event_v1_t decoded = {0};
  mol_command_t command;
  uint8_t encoded[MOL_WIRE_EVENT_V1_SIZE];
  input.payload.note.note = 64u;
  input.payload.note.velocity = 0.75f;
  decoded.struct_size = (uint32_t)sizeof(decoded);
  decoded.api_version = MOL_API_VERSION;
  EXPECT_TRUE(mol_wire_event_v1_encode(&input, encoded, sizeof(encoded)) == MOL_OK);
  EXPECT_TRUE(memcmp(encoded, "MOLW\x01\x00\x01\x00\x30\x00", 10u) == 0);
  EXPECT_TRUE(encoded[12] == 0x78u && encoded[15] == 0x12u && encoded[40] == 64u);
  EXPECT_TRUE(mol_wire_event_v1_decode(encoded, sizeof(encoded), &decoded) == MOL_OK);
  EXPECT_TRUE(decoded.type == input.type && decoded.sequence == input.sequence &&
              decoded.timestamp == input.timestamp && decoded.source_id == input.source_id &&
              decoded.gesture_id == input.gesture_id && decoded.payload.note.note == 64u &&
              decoded.payload.note.velocity == 0.75f);
  EXPECT_TRUE(mol_wire_event_v1_to_command(&decoded, &command) == MOL_OK);
  EXPECT_TRUE(command.command_type == MOL_COMMAND_NOTE_ON &&
              command.target_frame == input.timestamp && command.payload.note.note == 64u &&
              command.payload.note.velocity == 0.75f);
}

static void test_types_and_controls(void) {
  mol_wire_event_v1_t event = make_event(MOL_WIRE_CONTROL);
  mol_command_t command;
  uint8_t encoded[MOL_WIRE_EVENT_V1_SIZE];
  event.payload.control.control = MOL_WIRE_CONTROL_SUSTAIN;
  event.payload.control.value = 1.0f;
  EXPECT_TRUE(mol_wire_event_v1_encode(&event, encoded, sizeof(encoded)) == MOL_OK);
  EXPECT_TRUE(mol_wire_event_v1_to_command(&event, &command) == MOL_OK);
  EXPECT_TRUE(command.command_type == MOL_COMMAND_SUSTAIN && command.payload.scalar.value == 1.0f);
  event.payload.control.control = MOL_WIRE_CONTROL_MODULATION;
  EXPECT_TRUE(mol_wire_event_v1_encode(&event, encoded, sizeof(encoded)) == MOL_OK);
  EXPECT_TRUE(mol_wire_event_v1_to_command(&event, &command) == MOL_ERROR_UNSUPPORTED);
  event = make_event(MOL_WIRE_PITCH_BEND);
  event.payload.pitch_bend.value = -0.5f;
  EXPECT_TRUE(mol_wire_event_v1_to_command(&event, &command) == MOL_OK);
  EXPECT_TRUE(command.command_type == MOL_COMMAND_PITCH_BEND &&
              command.payload.scalar.value == -0.5f);
  event = make_event(MOL_WIRE_ALL_NOTES_OFF);
  EXPECT_TRUE(mol_wire_event_v1_to_command(&event, &command) == MOL_OK &&
              command.command_type == MOL_COMMAND_ALL_NOTES_OFF);
  event = make_event(MOL_WIRE_ALL_SOUND_OFF);
  EXPECT_TRUE(mol_wire_event_v1_to_command(&event, &command) == MOL_OK &&
              command.command_type == MOL_COMMAND_ALL_SOUND_OFF);
}

static void test_malformed_packets(void) {
  mol_wire_event_v1_t input = make_event(MOL_WIRE_NOTE_OFF);
  mol_wire_event_v1_t output = {0};
  uint8_t encoded[MOL_WIRE_EVENT_V1_SIZE];
  input.payload.note.note = 60u;
  input.payload.note.velocity = 0.0f;
  output.struct_size = (uint32_t)sizeof(output);
  output.api_version = MOL_API_VERSION;
  EXPECT_TRUE(mol_wire_event_v1_encode(&input, encoded, sizeof(encoded)) == MOL_OK);
  for (size_t size = 0u; size < sizeof(encoded); ++size)
    EXPECT_TRUE(mol_wire_event_v1_decode(encoded, size, &output) == MOL_ERROR_CORRUPT_DATA);
  encoded[4] = 2u;
  EXPECT_TRUE(mol_wire_event_v1_decode(encoded, sizeof(encoded), &output) ==
              MOL_ERROR_UNSUPPORTED_VERSION);
  encoded[4] = 1u;
  encoded[28] = 1u;
  EXPECT_TRUE(mol_wire_event_v1_decode(encoded, sizeof(encoded), &output) ==
              MOL_ERROR_CORRUPT_DATA);
  encoded[28] = 0u;
  encoded[41] = 1u;
  EXPECT_TRUE(mol_wire_event_v1_decode(encoded, sizeof(encoded), &output) ==
              MOL_ERROR_CORRUPT_DATA);
  input.payload.note.velocity = NAN;
  EXPECT_TRUE(mol_wire_event_v1_encode(&input, encoded, sizeof(encoded)) ==
              MOL_ERROR_INVALID_ARGUMENT);
  input = make_event(99u);
  EXPECT_TRUE(mol_wire_event_v1_encode(&input, encoded, sizeof(encoded)) == MOL_ERROR_UNSUPPORTED);
  EXPECT_TRUE(mol_wire_event_v1_encode(&input, encoded, sizeof(encoded) - 1u) ==
              MOL_ERROR_UNSUPPORTED);
}

int main(void) {
  test_note_round_trip();
  test_types_and_controls();
  test_malformed_packets();
  if (failures != 0) {
    (void)fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
