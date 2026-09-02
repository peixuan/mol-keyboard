/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_COMMAND_H_
#define MOL_COMMAND_H_

#include <stdint.h>

#include "mol/version.h"

typedef uint64_t mol_frame_index_t;
typedef uint64_t mol_gesture_id_t;

#define MOL_FRAME_IMMEDIATE UINT64_MAX

typedef uint32_t mol_command_type_t;

enum {
  MOL_COMMAND_NOTE_ON = 1u,
  MOL_COMMAND_NOTE_OFF = 2u,
  MOL_COMMAND_POLY_PRESSURE = 3u,
  MOL_COMMAND_PITCH_BEND = 4u,
  MOL_COMMAND_SUSTAIN = 5u,
  MOL_COMMAND_ALL_NOTES_OFF = 6u,
  MOL_COMMAND_ALL_SOUND_OFF = 7u,
  MOL_COMMAND_SET_MASTER_GAIN = 8u,
  MOL_COMMAND_SET_PRESET = 9u,
  MOL_COMMAND_SET_PARAMETER = 10u,
  MOL_COMMAND_SET_OCTAVE_SHIFT = 11u,
  MOL_COMMAND_SET_TRANSPOSE = 12u,
  MOL_COMMAND_SET_SCALE = 13u,
  MOL_COMMAND_SET_CHORD_MODE = 14u,
  MOL_COMMAND_SET_ARPEGGIATOR = 15u,
  MOL_COMMAND_SET_TEMPO = 16u,
  MOL_COMMAND_SET_TIME_SIGNATURE = 17u,
  MOL_COMMAND_TRANSPORT_START = 18u,
  MOL_COMMAND_TRANSPORT_STOP = 19u,
  MOL_COMMAND_TRANSPORT_SEEK = 20u,
  MOL_COMMAND_RECORD_START = 21u,
  MOL_COMMAND_RECORD_STOP = 22u,
  MOL_COMMAND_PLAYBACK_START = 23u,
  MOL_COMMAND_PLAYBACK_STOP = 24u,
  MOL_COMMAND_LOAD_SEQUENCE = 25u,
  MOL_COMMAND_RESET_ENGINE = 26u,
  MOL_COMMAND_SET_METRONOME = 27u,
  MOL_COMMAND_SET_PORTAMENTO = 28u
};

typedef union mol_command_payload {
  struct {
    uint8_t note;
    uint8_t reserved[3];
    float velocity;
  } note;
  struct {
    float value;
  } scalar;
  struct {
    int32_t value;
  } integer;
  struct {
    uint32_t type;
    uint8_t tonic;
    uint8_t mapping;
    uint8_t reserved[2];
  } scale;
  struct {
    uint8_t numerator;
    uint8_t denominator;
    uint8_t reserved[2];
  } time_signature;
  struct {
    uint64_t frame;
  } transport;
  struct {
    float level;
    uint8_t enabled;
    uint8_t reserved[3];
  } metronome;
  struct {
    uint32_t mode;
    uint32_t rate;
    float gate;
    uint32_t random_seed;
    uint8_t octaves;
    uint8_t reserved[3];
  } arpeggiator;
  struct {
    uint32_t mode;
    float time_ms;
  } portamento;
  uint8_t bytes[64];
} mol_command_payload_t;

typedef struct mol_command {
  uint32_t struct_size;
  uint32_t api_version;
  mol_command_type_t command_type;
  uint32_t source_id;
  mol_frame_index_t target_frame;
  mol_gesture_id_t gesture_id;
  mol_command_payload_t payload;
} mol_command_t;

#endif /* MOL_COMMAND_H_ */
