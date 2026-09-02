// SPDX-License-Identifier: Apache-2.0
#include <string.h>

#include "mol_platform/audio_runtime.h"

enum {
  MOL_PLATFORM_AUDIO_MAX_VOICES = 8,
  MOL_PLATFORM_AUDIO_COMMAND_CAPACITY = 64,
  MOL_PLATFORM_AUDIO_EVENT_CAPACITY = 64
};

mol_result_t mol_platform_audio_init(mol_platform_audio_runtime_t* runtime, uint32_t sample_rate,
                                     uint32_t channel_count) {
  mol_engine_config_t config;
  size_t required_memory;
  mol_result_t result;

  if (runtime == NULL || sample_rate == 0U || channel_count == 0U) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }

  memset(runtime, 0, sizeof(*runtime));
  config = mol_engine_config_default();
  config.sample_rate = sample_rate;
  config.channel_count = channel_count;
  config.max_voices = MOL_PLATFORM_AUDIO_MAX_VOICES;
  config.command_capacity = MOL_PLATFORM_AUDIO_COMMAND_CAPACITY;
  config.event_capacity = MOL_PLATFORM_AUDIO_EVENT_CAPACITY;
  required_memory = mol_engine_query_memory(&config);
  if (required_memory == 0U || required_memory > sizeof(runtime->memory.bytes)) {
    return MOL_ERROR_INSUFFICIENT_MEMORY;
  }

  result = mol_engine_init(runtime->memory.bytes, sizeof(runtime->memory.bytes), &config,
                           &runtime->engine);
  if (result != MOL_OK) {
    return result;
  }
  runtime->sample_rate = sample_rate;
  runtime->channel_count = channel_count;
  return MOL_OK;
}

void mol_platform_audio_shutdown(mol_platform_audio_runtime_t* runtime) {
  if (runtime == NULL) {
    return;
  }
  if (runtime->engine != NULL) {
    mol_engine_shutdown(runtime->engine);
  }
  runtime->engine = NULL;
  runtime->sample_rate = 0U;
  runtime->channel_count = 0U;
}

mol_result_t mol_platform_audio_submit(mol_platform_audio_runtime_t* runtime,
                                       const mol_command_t* command) {
  if (runtime == NULL || runtime->engine == NULL || command == NULL) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  return mol_engine_submit(runtime->engine, command);
}

mol_result_t mol_platform_audio_render_f32(mol_platform_audio_runtime_t* runtime,
                                           float* interleaved, uint32_t frame_count) {
  if (runtime == NULL || runtime->engine == NULL || interleaved == NULL || frame_count == 0U) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  return mol_engine_render_interleaved_f32(runtime->engine, interleaved, frame_count,
                                           runtime->channel_count);
}
