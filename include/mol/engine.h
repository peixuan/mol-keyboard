/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_ENGINE_H_
#define MOL_ENGINE_H_

#include <stddef.h>
#include <stdint.h>

#include "mol/capabilities.h"
#include "mol/command.h"
#include "mol/event.h"
#include "mol/result.h"
#include "mol/version.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mol_engine mol_engine_t;

typedef struct mol_engine_config {
  uint32_t struct_size;
  uint32_t api_version;
  uint32_t sample_rate;
  uint32_t channel_count;
  uint32_t max_voices;
  uint32_t command_capacity;
  uint32_t event_capacity;
  uint32_t random_seed;
} mol_engine_config_t;

typedef struct mol_engine_state {
  uint32_t struct_size;
  uint32_t api_version;
  mol_frame_index_t current_frame;
  uint32_t sample_rate;
  uint32_t channel_count;
  uint32_t max_voices;
  uint32_t active_voices;
  int32_t octave_shift;
  int32_t transpose;
  uint32_t scale_type;
  uint32_t scale_tonic;
  uint32_t scale_mapping;
  uint32_t chord_mode;
  float sustain;
  mol_frame_index_t transport_frame;
  float tempo;
  uint8_t time_signature_numerator;
  uint8_t time_signature_denominator;
  uint8_t transport_running;
  uint8_t metronome_enabled;
} mol_engine_state_t;

/** Returns a portable default Standard-profile configuration. */
mol_engine_config_t mol_engine_config_default(void);

/** Returns the minimum required alignment for caller-owned engine memory. */
size_t mol_engine_memory_alignment(void);

/** Returns required bytes, or zero when the configuration is invalid. */
size_t mol_engine_query_memory(const mol_engine_config_t* config);

/** Initializes an engine entirely inside caller-owned memory. */
mol_result_t mol_engine_init(void* memory, size_t memory_size, const mol_engine_config_t* config,
                             mol_engine_t** out_engine);

/** Invalidates the engine without releasing caller-owned memory. */
void mol_engine_shutdown(mol_engine_t* engine);

/** Restores deterministic initial state while retaining configuration. */
void mol_engine_reset(mol_engine_t* engine);

/** Submits a versioned music command. */
mol_result_t mol_engine_submit(mol_engine_t* engine, const mol_command_t* command);

/** Renders interleaved floating-point PCM without allocation. */
mol_result_t mol_engine_render_interleaved_f32(mol_engine_t* engine, float* output,
                                               uint32_t frame_count, uint32_t channel_count);

/** Renders planar floating-point PCM without allocation. */
mol_result_t mol_engine_render_planar_f32(mol_engine_t* engine, float* const* output_channels,
                                          uint32_t frame_count, uint32_t channel_count);

/** Copies at most capacity queued events into caller storage. */
uint32_t mol_engine_poll_events(mol_engine_t* engine, mol_event_t* events, uint32_t capacity);

/** Copies a read-only state snapshot into caller storage. */
mol_result_t mol_engine_get_state(const mol_engine_t* engine, mol_engine_state_t* state);

/** Returns capabilities actually implemented by this engine build. */
mol_capability_flags_t mol_engine_get_capabilities(const mol_engine_t* engine);

#ifdef __cplusplus
}
#endif

#endif /* MOL_ENGINE_H_ */
