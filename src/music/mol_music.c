/* SPDX-License-Identifier: Apache-2.0 */
#include <stddef.h>

#include "mol/music.h"

typedef struct mol_key_note {
  uint16_t usage;
  uint8_t note;
} mol_key_note_t;

typedef struct mol_scale_definition {
  uint16_t pitch_class_mask;
} mol_scale_definition_t;

typedef struct mol_chord_definition {
  uint8_t intervals[4];
  uint8_t count;
} mol_chord_definition_t;

static const mol_key_note_t mol_default_keyboard[30] = {
    {0x001Du, 60u}, {0x0016u, 61u}, {0x001Bu, 62u}, {0x0007u, 63u}, {0x0006u, 64u}, {0x0019u, 65u},
    {0x000Au, 66u}, {0x0005u, 67u}, {0x000Bu, 68u}, {0x0011u, 69u}, {0x000Du, 70u}, {0x0010u, 71u},
    {0x0014u, 72u}, {0x001Fu, 73u}, {0x001Au, 74u}, {0x0020u, 75u}, {0x0008u, 76u}, {0x0015u, 77u},
    {0x0022u, 78u}, {0x0017u, 79u}, {0x0023u, 80u}, {0x001Cu, 81u}, {0x0024u, 82u}, {0x0018u, 83u},
    {0x000Cu, 84u}, {0x0026u, 85u}, {0x0012u, 86u}, {0x0027u, 87u}, {0x0013u, 88u}, {0x002Fu, 89u}};

static const mol_scale_definition_t mol_scales[MOL_SCALE_TYPE_COUNT] = {
    {0x0FFFu}, /* Chromatic */
    {0x0AB5u}, /* Major: 0, 2, 4, 5, 7, 9, 11 */
    {0x05ADu}, /* Natural minor: 0, 2, 3, 5, 7, 8, 10 */
    {0x0295u}, /* Major pentatonic: 0, 2, 4, 7, 9 */
    {0x04A9u}, /* Minor pentatonic: 0, 3, 5, 7, 10 */
    {0x04E9u}, /* Blues: 0, 3, 5, 6, 7, 10 */
    {0x06ADu}, /* Dorian: 0, 2, 3, 5, 7, 9, 10 */
    {0x06B5u}  /* Mixolydian: 0, 2, 4, 5, 7, 9, 10 */
};

static const mol_chord_definition_t mol_chords[MOL_CHORD_MODE_COUNT] = {
    {{0u, 0u, 0u, 0u}, 1u},  {{0u, 4u, 7u, 0u}, 3u},  {{0u, 3u, 7u, 0u}, 3u},
    {{0u, 2u, 7u, 0u}, 3u},  {{0u, 5u, 7u, 0u}, 3u},  {{0u, 4u, 7u, 10u}, 4u},
    {{0u, 4u, 7u, 11u}, 4u}, {{0u, 3u, 7u, 10u}, 4u}, {{0u, 7u, 0u, 0u}, 2u},
    {{0u, 12u, 0u, 0u}, 2u}};

static int mol_note_is_in_scale(int note, uint8_t tonic, uint16_t mask) {
  int pitch_class;
  if (note < 0 || note > 127) {
    return 0;
  }
  pitch_class = (note - (int)tonic) % 12;
  if (pitch_class < 0) {
    pitch_class += 12;
  }
  return (mask & ((uint16_t)1u << (uint32_t)pitch_class)) != 0u;
}

mol_result_t mol_keyboard_note_from_hid_usage(uint16_t usage, uint8_t* out_note) {
  size_t index;
  if (out_note == NULL) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  for (index = 0u; index < sizeof(mol_default_keyboard) / sizeof(mol_default_keyboard[0]);
       ++index) {
    if (mol_default_keyboard[index].usage == usage) {
      *out_note = mol_default_keyboard[index].note;
      return MOL_OK;
    }
  }
  return MOL_ERROR_UNSUPPORTED;
}

mol_result_t mol_scale_map_note(uint8_t note, uint8_t tonic, mol_scale_type_t scale,
                                mol_scale_mapping_t mapping, uint8_t* out_note) {
  uint16_t mask;
  int distance;
  if (out_note == NULL || tonic > 11u || scale >= MOL_SCALE_TYPE_COUNT ||
      mapping >= MOL_SCALE_MAPPING_COUNT) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  mask = mol_scales[scale].pitch_class_mask;
  if (mol_note_is_in_scale((int)note, tonic, mask)) {
    *out_note = note;
    return MOL_OK;
  }
  for (distance = 1; distance < 12; ++distance) {
    int lower = (int)note - distance;
    int upper = (int)note + distance;
    int lower_allowed = mol_note_is_in_scale(lower, tonic, mask);
    int upper_allowed = mol_note_is_in_scale(upper, tonic, mask);
    if (mapping == MOL_SCALE_MAP_DOWN && lower_allowed) {
      *out_note = (uint8_t)lower;
      return MOL_OK;
    }
    if (mapping == MOL_SCALE_MAP_UP && upper_allowed) {
      *out_note = (uint8_t)upper;
      return MOL_OK;
    }
    if (mapping == MOL_SCALE_MAP_NEAREST) {
      if (lower_allowed) {
        *out_note = (uint8_t)lower;
        return MOL_OK;
      }
      if (upper_allowed) {
        *out_note = (uint8_t)upper;
        return MOL_OK;
      }
    }
  }
  return MOL_ERROR_INVALID_STATE;
}

mol_result_t mol_chord_expand(uint8_t root, mol_chord_mode_t chord, uint8_t* out_notes,
                              uint32_t capacity, uint32_t* out_count) {
  const mol_chord_definition_t* definition;
  uint32_t produced = 0u;
  uint32_t index;
  if (out_count == NULL || chord >= MOL_CHORD_MODE_COUNT || (out_notes == NULL && capacity != 0u)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  definition = &mol_chords[chord];
  for (index = 0u; index < definition->count; ++index) {
    uint32_t note = (uint32_t)root + definition->intervals[index];
    if (note <= 127u) {
      if (produced >= capacity) {
        *out_count = produced;
        return MOL_ERROR_BUFFER_TOO_SMALL;
      }
      out_notes[produced++] = (uint8_t)note;
    }
  }
  *out_count = produced;
  return MOL_OK;
}
