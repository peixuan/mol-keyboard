/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_MUSIC_H_
#define MOL_MUSIC_H_

#include <stdint.h>

#include "mol/result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t mol_scale_type_t;
enum {
  MOL_SCALE_CHROMATIC = 0u,
  MOL_SCALE_MAJOR = 1u,
  MOL_SCALE_NATURAL_MINOR = 2u,
  MOL_SCALE_MAJOR_PENTATONIC = 3u,
  MOL_SCALE_MINOR_PENTATONIC = 4u,
  MOL_SCALE_BLUES = 5u,
  MOL_SCALE_DORIAN = 6u,
  MOL_SCALE_MIXOLYDIAN = 7u,
  MOL_SCALE_TYPE_COUNT = 8u
};

typedef uint32_t mol_scale_mapping_t;
enum {
  MOL_SCALE_MAP_NEAREST = 0u,
  MOL_SCALE_MAP_DOWN = 1u,
  MOL_SCALE_MAP_UP = 2u,
  MOL_SCALE_MAPPING_COUNT = 3u
};

typedef uint32_t mol_chord_mode_t;
enum {
  MOL_CHORD_OFF = 0u,
  MOL_CHORD_MAJOR = 1u,
  MOL_CHORD_MINOR = 2u,
  MOL_CHORD_SUS2 = 3u,
  MOL_CHORD_SUS4 = 4u,
  MOL_CHORD_DOMINANT_7 = 5u,
  MOL_CHORD_MAJOR_7 = 6u,
  MOL_CHORD_MINOR_7 = 7u,
  MOL_CHORD_POWER_5 = 8u,
  MOL_CHORD_OCTAVE = 9u,
  MOL_CHORD_MODE_COUNT = 10u
};

typedef uint32_t mol_arpeggiator_mode_t;
enum {
  MOL_ARPEGGIATOR_OFF = 0u,
  MOL_ARPEGGIATOR_UP = 1u,
  MOL_ARPEGGIATOR_DOWN = 2u,
  MOL_ARPEGGIATOR_UP_DOWN = 3u,
  MOL_ARPEGGIATOR_DOWN_UP = 4u,
  MOL_ARPEGGIATOR_AS_PLAYED = 5u,
  MOL_ARPEGGIATOR_RANDOM_DETERMINISTIC = 6u,
  MOL_ARPEGGIATOR_MODE_COUNT = 7u
};

typedef uint32_t mol_arpeggiator_rate_t;
enum {
  MOL_ARPEGGIATOR_RATE_QUARTER = 0u,
  MOL_ARPEGGIATOR_RATE_EIGHTH = 1u,
  MOL_ARPEGGIATOR_RATE_EIGHTH_TRIPLET = 2u,
  MOL_ARPEGGIATOR_RATE_SIXTEENTH = 3u,
  MOL_ARPEGGIATOR_RATE_SIXTEENTH_TRIPLET = 4u,
  MOL_ARPEGGIATOR_RATE_THIRTY_SECOND = 5u,
  MOL_ARPEGGIATOR_RATE_COUNT = 6u
};

typedef uint32_t mol_portamento_mode_t;
enum {
  MOL_PORTAMENTO_OFF = 0u,
  MOL_PORTAMENTO_LEGATO_ONLY = 1u,
  MOL_PORTAMENTO_ALWAYS = 2u,
  MOL_PORTAMENTO_MODE_COUNT = 3u
};

/** Returns the exact number of steps per quarter note for an arpeggiator rate. */
uint32_t mol_arpeggiator_steps_per_quarter(mol_arpeggiator_rate_t rate);

/** Maps one USB HID keyboard usage to the default C4-F6 MIDI-note range. */
mol_result_t mol_keyboard_note_from_hid_usage(uint16_t usage, uint8_t* out_note);

/** Maps a MIDI note into a scale. Nearest ties resolve downward. */
mol_result_t mol_scale_map_note(uint8_t note, uint8_t tonic, mol_scale_type_t scale,
                                mol_scale_mapping_t mapping, uint8_t* out_note);

/** Expands a root into ascending MIDI notes and safely drops notes above 127. */
mol_result_t mol_chord_expand(uint8_t root, mol_chord_mode_t chord, uint8_t* out_notes,
                              uint32_t capacity, uint32_t* out_count);

#ifdef __cplusplus
}
#endif

#endif /* MOL_MUSIC_H_ */
