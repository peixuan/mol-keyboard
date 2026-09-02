/* SPDX-License-Identifier: Apache-2.0 */
#include <stddef.h>
#include <stdint.h>

#include "mol/patch.h"

extern const uint8_t mol_builtin_grand_piano[MOL_PATCH_BINARY_SIZE];
extern const uint8_t mol_builtin_electric_piano[MOL_PATCH_BINARY_SIZE];
extern const uint8_t mol_builtin_harpsichord[MOL_PATCH_BINARY_SIZE];
extern const uint8_t mol_builtin_church_organ[MOL_PATCH_BINARY_SIZE];
extern const uint8_t mol_builtin_jazz_organ[MOL_PATCH_BINARY_SIZE];
extern const uint8_t mol_builtin_nylon_guitar[MOL_PATCH_BINARY_SIZE];
extern const uint8_t mol_builtin_steel_guitar[MOL_PATCH_BINARY_SIZE];
extern const uint8_t mol_builtin_violin[MOL_PATCH_BINARY_SIZE];
extern const uint8_t mol_builtin_cello[MOL_PATCH_BINARY_SIZE];
extern const uint8_t mol_builtin_flute[MOL_PATCH_BINARY_SIZE];
extern const uint8_t mol_builtin_clarinet[MOL_PATCH_BINARY_SIZE];
extern const uint8_t mol_builtin_synth_lead[MOL_PATCH_BINARY_SIZE];
extern const uint8_t mol_builtin_synth_pad[MOL_PATCH_BINARY_SIZE];
extern const uint8_t mol_builtin_synth_bass[MOL_PATCH_BINARY_SIZE];
extern const uint8_t mol_builtin_choir[MOL_PATCH_BINARY_SIZE];
extern const uint8_t mol_builtin_vibraphone[MOL_PATCH_BINARY_SIZE];
extern const uint8_t mol_builtin_harp[MOL_PATCH_BINARY_SIZE];
extern const uint8_t mol_builtin_music_box[MOL_PATCH_BINARY_SIZE];

static const uint8_t* const mol_builtin_patch_data[MOL_PRESET_COUNT] = {
    mol_builtin_grand_piano,  mol_builtin_electric_piano, mol_builtin_harpsichord,
    mol_builtin_church_organ, mol_builtin_jazz_organ,     mol_builtin_nylon_guitar,
    mol_builtin_steel_guitar, mol_builtin_violin,         mol_builtin_cello,
    mol_builtin_flute,        mol_builtin_clarinet,       mol_builtin_synth_lead,
    mol_builtin_synth_pad,    mol_builtin_synth_bass,     mol_builtin_choir,
    mol_builtin_vibraphone,   mol_builtin_harp,           mol_builtin_music_box};

const uint8_t* mol_builtin_patch_binary(mol_preset_id_t preset, size_t* out_size) {
  if (out_size == NULL || preset >= MOL_PRESET_COUNT) {
    return NULL;
  }
  *out_size = MOL_PATCH_BINARY_SIZE;
  return mol_builtin_patch_data[preset];
}

mol_result_t mol_builtin_patch_load(mol_preset_id_t preset, mol_patch_t* out_patch) {
  size_t size = 0u;
  const uint8_t* data = mol_builtin_patch_binary(preset, &size);
  if (data == NULL) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  return mol_patch_decode(data, size, out_patch);
}
