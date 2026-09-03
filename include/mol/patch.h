/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_PATCH_H_
#define MOL_PATCH_H_

#include <stddef.h>
#include <stdint.h>

#include "mol/export.h"
#include "mol/result.h"
#include "mol/version.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOL_PATCH_FORMAT_VERSION 1u
#define MOL_PATCH_BINARY_HEADER_SIZE 32u
#define MOL_PATCH_BINARY_PAYLOAD_SIZE 88u
#define MOL_PATCH_BINARY_SIZE (MOL_PATCH_BINARY_HEADER_SIZE + MOL_PATCH_BINARY_PAYLOAD_SIZE)
#define MOL_PATCH_MAX_JSON_SIZE 16384u

typedef uint32_t mol_preset_id_t;
enum {
  MOL_PRESET_GRAND_PIANO = 0u,
  MOL_PRESET_ELECTRIC_PIANO = 1u,
  MOL_PRESET_HARPSICHORD = 2u,
  MOL_PRESET_CHURCH_ORGAN = 3u,
  MOL_PRESET_JAZZ_ORGAN = 4u,
  MOL_PRESET_NYLON_GUITAR = 5u,
  MOL_PRESET_STEEL_GUITAR = 6u,
  MOL_PRESET_VIOLIN = 7u,
  MOL_PRESET_CELLO = 8u,
  MOL_PRESET_FLUTE = 9u,
  MOL_PRESET_CLARINET = 10u,
  MOL_PRESET_SYNTH_LEAD = 11u,
  MOL_PRESET_SYNTH_PAD = 12u,
  MOL_PRESET_SYNTH_BASS = 13u,
  MOL_PRESET_CHOIR = 14u,
  MOL_PRESET_VIBRAPHONE = 15u,
  MOL_PRESET_HARP = 16u,
  MOL_PRESET_MUSIC_BOX = 17u,
  MOL_PRESET_COUNT = 18u
};

typedef uint32_t mol_synthesis_model_t;
enum {
  MOL_SYNTHESIS_SUBTRACTIVE = 0u,
  MOL_SYNTHESIS_FM2 = 1u,
  MOL_SYNTHESIS_ADDITIVE = 2u,
  MOL_SYNTHESIS_PLUCK = 3u,
  MOL_SYNTHESIS_MODAL = 4u,
  MOL_SYNTHESIS_FORMANT = 5u,
  MOL_SYNTHESIS_MODEL_COUNT = 6u
};

typedef uint32_t mol_waveform_t;
enum {
  MOL_WAVEFORM_SINE = 0u,
  MOL_WAVEFORM_SAW = 1u,
  MOL_WAVEFORM_SQUARE = 2u,
  MOL_WAVEFORM_PULSE = 3u,
  MOL_WAVEFORM_TRIANGLE = 4u,
  MOL_WAVEFORM_NOISE = 5u,
  MOL_WAVEFORM_COUNT = 6u
};

typedef uint32_t mol_patch_feature_flags_t;
enum {
  MOL_PATCH_FEATURE_OSCILLATOR = UINT32_C(1) << 0u,
  MOL_PATCH_FEATURE_FILTER = UINT32_C(1) << 1u,
  MOL_PATCH_FEATURE_NOISE = UINT32_C(1) << 2u,
  MOL_PATCH_FEATURE_FM2 = UINT32_C(1) << 3u,
  MOL_PATCH_FEATURE_ADDITIVE = UINT32_C(1) << 4u,
  MOL_PATCH_FEATURE_PLUCK = UINT32_C(1) << 5u,
  MOL_PATCH_FEATURE_MODAL = UINT32_C(1) << 6u,
  MOL_PATCH_FEATURE_CHORUS = UINT32_C(1) << 7u,
  MOL_PATCH_FEATURE_DELAY = UINT32_C(1) << 8u,
  MOL_PATCH_FEATURE_REVERB = UINT32_C(1) << 9u
};

typedef struct mol_patch {
  uint32_t struct_size;
  uint32_t api_version;
  uint32_t preset_id_hash;
  mol_patch_feature_flags_t feature_flags;
  mol_synthesis_model_t synthesis_model;
  mol_waveform_t waveform;
  int32_t gain_millidb;
  int32_t attack_ms;
  int32_t decay_ms;
  int32_t sustain_milli;
  int32_t release_ms;
  int32_t filter_cutoff_hz;
  int32_t filter_resonance_milli;
  int32_t oscillator_mix_milli;
  int32_t detune_cents;
  int32_t pulse_width_milli;
  int32_t model_parameter_1_milli;
  int32_t model_parameter_2_milli;
  int32_t vibrato_rate_millihz;
  int32_t vibrato_depth_cents;
  int32_t velocity_curve_milli;
  int32_t noise_mix_milli;
  int32_t saturation_milli;
  int32_t chorus_send_milli;
  int32_t delay_send_milli;
  int32_t reverb_send_milli;
} mol_patch_t;

/** Returns the stable FNV-1a hash stored in a compiled patch header. */
MOL_API uint32_t mol_patch_id_hash(const char* stable_id);

/** Validates all fixed-layout parameters and feature flags. */
MOL_API mol_result_t mol_patch_validate(const mol_patch_t* patch);

/** Compiles one strict, flat .molpatch.json document into quantized parameters. */
MOL_API mol_result_t mol_patch_compile_json(const char* json, size_t json_size,
                                            mol_patch_t* out_patch);

/** Encodes the versioned little-endian binary patch representation. */
MOL_API mol_result_t mol_patch_encode(const mol_patch_t* patch, uint8_t* output, size_t capacity,
                                      size_t* out_size);

/** Validates and decodes an exact-size binary patch representation. */
MOL_API mol_result_t mol_patch_decode(const uint8_t* data, size_t size, mol_patch_t* out_patch);

/** Returns metadata for one of the 18 stable built-in preset identifiers. */
MOL_API const char* mol_preset_stable_id(mol_preset_id_t preset);
MOL_API const char* mol_preset_english_name(mol_preset_id_t preset);
MOL_API const char* mol_preset_chinese_name(mol_preset_id_t preset);

/** Returns the immutable compiled bytes for a built-in preset, or NULL. */
MOL_API const uint8_t* mol_builtin_patch_binary(mol_preset_id_t preset, size_t* out_size);

/** Decodes one built-in preset from its flash-safe compiled representation. */
MOL_API mol_result_t mol_builtin_patch_load(mol_preset_id_t preset, mol_patch_t* out_patch);

#ifdef __cplusplus
}
#endif

#endif /* MOL_PATCH_H_ */
