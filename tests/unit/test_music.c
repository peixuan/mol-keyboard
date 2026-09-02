/* SPDX-License-Identifier: Apache-2.0 */
#include <stdint.h>
#include <stdio.h>

#include "mol/mol.h"

static int failures = 0;

#define EXPECT_TRUE(condition)                                                                  \
  do {                                                                                          \
    if (!(condition)) {                                                                         \
      (void)fprintf(stderr, "%s:%d: expectation failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures;                                                                               \
    }                                                                                           \
  } while (0)

static void test_default_keyboard(void) {
  static const uint16_t usages[30] = {
      0x001Du, 0x0016u, 0x001Bu, 0x0007u, 0x0006u, 0x0019u, 0x000Au, 0x0005u, 0x000Bu, 0x0011u,
      0x000Du, 0x0010u, 0x0014u, 0x001Fu, 0x001Au, 0x0020u, 0x0008u, 0x0015u, 0x0022u, 0x0017u,
      0x0023u, 0x001Cu, 0x0024u, 0x0018u, 0x000Cu, 0x0026u, 0x0012u, 0x0027u, 0x0013u, 0x002Fu};
  uint32_t index;
  uint8_t note = 0u;
  for (index = 0u; index < 30u; ++index) {
    EXPECT_TRUE(mol_keyboard_note_from_hid_usage(usages[index], &note) == MOL_OK);
    EXPECT_TRUE(note == (uint8_t)(60u + index));
  }
  EXPECT_TRUE(mol_keyboard_note_from_hid_usage(0x0004u, &note) == MOL_ERROR_UNSUPPORTED);
  EXPECT_TRUE(mol_keyboard_note_from_hid_usage(usages[0], NULL) == MOL_ERROR_INVALID_ARGUMENT);
}

static int note_is_allowed(uint8_t note, uint8_t tonic, uint16_t mask) {
  uint32_t relative = ((uint32_t)note + 12u - tonic) % 12u;
  return (mask & ((uint16_t)1u << relative)) != 0u;
}

static void test_scales(void) {
  static const uint16_t masks[MOL_SCALE_TYPE_COUNT] = {0x0FFFu, 0x0AB5u, 0x05ADu, 0x0295u,
                                                       0x04A9u, 0x04E9u, 0x06ADu, 0x06B5u};
  uint32_t scale;
  uint32_t tonic;
  uint32_t note;
  uint8_t mapped = 0u;

  EXPECT_TRUE(mol_scale_map_note(61u, 0u, MOL_SCALE_MAJOR, MOL_SCALE_MAP_NEAREST, &mapped) ==
              MOL_OK);
  EXPECT_TRUE(mapped == 60u);
  EXPECT_TRUE(mol_scale_map_note(61u, 0u, MOL_SCALE_MAJOR, MOL_SCALE_MAP_UP, &mapped) == MOL_OK);
  EXPECT_TRUE(mapped == 62u);
  EXPECT_TRUE(mol_scale_map_note(61u, 0u, MOL_SCALE_MAJOR, MOL_SCALE_MAP_DOWN, &mapped) == MOL_OK);
  EXPECT_TRUE(mapped == 60u);

  for (scale = 0u; scale < MOL_SCALE_TYPE_COUNT; ++scale) {
    for (tonic = 0u; tonic < 12u; ++tonic) {
      for (note = 0u; note < 128u; ++note) {
        mol_scale_mapping_t mapping;
        for (mapping = MOL_SCALE_MAP_NEAREST; mapping < MOL_SCALE_MAPPING_COUNT; ++mapping) {
          mol_result_t result =
              mol_scale_map_note((uint8_t)note, (uint8_t)tonic, scale, mapping, &mapped);
          if (result == MOL_OK) {
            EXPECT_TRUE(note_is_allowed(mapped, (uint8_t)tonic, masks[scale]));
            if (mapping == MOL_SCALE_MAP_DOWN) {
              EXPECT_TRUE(mapped <= note);
            } else if (mapping == MOL_SCALE_MAP_UP) {
              EXPECT_TRUE(mapped >= note);
            }
          } else {
            EXPECT_TRUE(result == MOL_ERROR_INVALID_STATE);
            EXPECT_TRUE(mapping != MOL_SCALE_MAP_NEAREST);
          }
        }
      }
    }
  }
  EXPECT_TRUE(mol_scale_map_note(60u, 12u, MOL_SCALE_MAJOR, MOL_SCALE_MAP_NEAREST, &mapped) ==
              MOL_ERROR_INVALID_ARGUMENT);
}

static void test_chords(void) {
  static const uint8_t expected[MOL_CHORD_MODE_COUNT][4] = {
      {60u, 0u, 0u, 0u},   {60u, 64u, 67u, 0u},  {60u, 63u, 67u, 0u},  {60u, 62u, 67u, 0u},
      {60u, 65u, 67u, 0u}, {60u, 64u, 67u, 70u}, {60u, 64u, 67u, 71u}, {60u, 63u, 67u, 70u},
      {60u, 67u, 0u, 0u},  {60u, 72u, 0u, 0u}};
  static const uint32_t expected_count[MOL_CHORD_MODE_COUNT] = {1u, 3u, 3u, 3u, 3u,
                                                                4u, 4u, 4u, 2u, 2u};
  uint32_t chord;
  for (chord = 0u; chord < MOL_CHORD_MODE_COUNT; ++chord) {
    uint8_t notes[4] = {0u};
    uint32_t count = 0u;
    uint32_t index;
    EXPECT_TRUE(mol_chord_expand(60u, chord, notes, 4u, &count) == MOL_OK);
    EXPECT_TRUE(count == expected_count[chord]);
    for (index = 0u; index < count; ++index) {
      EXPECT_TRUE(notes[index] == expected[chord][index]);
    }
  }
  {
    uint8_t notes[4] = {0u};
    uint32_t count = 0u;
    EXPECT_TRUE(mol_chord_expand(125u, MOL_CHORD_MAJOR_7, notes, 4u, &count) == MOL_OK);
    EXPECT_TRUE(count == 1u && notes[0] == 125u);
    EXPECT_TRUE(mol_chord_expand(60u, MOL_CHORD_MAJOR_7, notes, 3u, &count) ==
                MOL_ERROR_BUFFER_TOO_SMALL);
  }
}

int main(void) {
  test_default_keyboard();
  test_scales();
  test_chords();
  if (failures != 0) {
    (void)fprintf(stderr, "%d test expectations failed\n", failures);
    return 1;
  }
  return 0;
}
