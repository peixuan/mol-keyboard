/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_EVENT_H_
#define MOL_EVENT_H_

#include <stdint.h>

#include "mol/command.h"

typedef uint32_t mol_event_type_t;

enum {
  MOL_EVENT_NOTE_STARTED = 1u,
  MOL_EVENT_NOTE_RELEASED = 2u,
  MOL_EVENT_NOTE_ENDED = 3u,
  MOL_EVENT_GESTURE_MAPPED = 4u,
  MOL_EVENT_PRESET_CHANGED = 5u,
  MOL_EVENT_TRANSPORT_CHANGED = 6u,
  MOL_EVENT_RECORDING_CHANGED = 7u,
  MOL_EVENT_VOICE_STOLEN = 8u,
  MOL_EVENT_XRUN_REPORTED = 9u,
  MOL_EVENT_COMMAND_DROPPED = 10u,
  MOL_EVENT_DEVICE_STATE_CHANGED = 11u,
  MOL_EVENT_ERROR_REPORTED = 12u,
  MOL_EVENT_METRONOME_TICK = 13u
};

enum {
  MOL_EVENT_PAYLOAD_NOTE = 0u,
  MOL_EVENT_PAYLOAD_INPUT_NOTE = 1u,
  MOL_EVENT_PAYLOAD_MAPPED_INDEX = 2u,
  MOL_EVENT_PAYLOAD_MAPPED_COUNT = 3u
};

enum { MOL_EVENT_PAYLOAD_METRONOME_ACCENT = 0u, MOL_EVENT_PAYLOAD_METRONOME_BEAT = 1u };

enum {
  MOL_MUSIC_ERROR_NOTE_OUT_OF_RANGE = 1u,
  MOL_MUSIC_ERROR_SCALE_MAPPING = 2u,
  MOL_MUSIC_ERROR_DUPLICATE_GESTURE = 3u
};

typedef struct mol_event {
  uint32_t struct_size;
  uint32_t api_version;
  mol_event_type_t event_type;
  uint32_t source_id;
  mol_frame_index_t frame;
  mol_gesture_id_t gesture_id;
  uint8_t payload[32];
} mol_event_t;

#endif /* MOL_EVENT_H_ */
