// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_PLATFORM_AUDIO_RUNTIME_H
#define MOL_PLATFORM_AUDIO_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#include "mol/mol.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOL_PLATFORM_AUDIO_MEMORY_BYTES 2097152U

typedef union mol_platform_audio_memory {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[MOL_PLATFORM_AUDIO_MEMORY_BYTES];
} mol_platform_audio_memory_t;

typedef struct mol_platform_audio_runtime {
  mol_platform_audio_memory_t memory;
  mol_engine_t* engine;
  uint32_t sample_rate;
  uint32_t channel_count;
} mol_platform_audio_runtime_t;

mol_result_t mol_platform_audio_init(mol_platform_audio_runtime_t* runtime, uint32_t sample_rate,
                                     uint32_t channel_count);
void mol_platform_audio_shutdown(mol_platform_audio_runtime_t* runtime);
mol_result_t mol_platform_audio_submit(mol_platform_audio_runtime_t* runtime,
                                       const mol_command_t* command);
mol_result_t mol_platform_audio_render_f32(mol_platform_audio_runtime_t* runtime,
                                           float* interleaved, uint32_t frame_count);

#ifdef __cplusplus
}
#endif

#endif
