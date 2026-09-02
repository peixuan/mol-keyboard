/* SPDX-License-Identifier: Apache-2.0 */
#include "mol/engine.h"

#include <stdint.h>
#include <string.h>

#define MOL_ENGINE_MAGIC UINT32_C(0x4D4F4C45)

struct mol_engine {
  uint32_t magic;
  mol_engine_config_t config;
  mol_frame_index_t current_frame;
};

static int mol_engine_config_is_valid(const mol_engine_config_t* config) {
  if (config == NULL || config->struct_size < sizeof(*config) ||
      config->api_version != MOL_API_VERSION) {
    return 0;
  }
  if (config->sample_rate != 32000u && config->sample_rate != 44100u &&
      config->sample_rate != 48000u) {
    return 0;
  }
  if ((config->channel_count != 1u && config->channel_count != 2u) ||
      config->max_voices == 0u || config->max_voices > MOL_PROFILE_MAX_VOICES ||
      config->command_capacity == 0u || config->event_capacity == 0u) {
    return 0;
  }
  return 1;
}

static int mol_engine_is_valid(const mol_engine_t* engine) {
  return engine != NULL && engine->magic == MOL_ENGINE_MAGIC;
}

static mol_result_t mol_engine_advance(mol_engine_t* engine, uint32_t frame_count) {
  if (UINT64_MAX - engine->current_frame < frame_count) {
    return MOL_ERROR_OVERFLOW;
  }
  engine->current_frame += frame_count;
  return MOL_OK;
}

uint32_t mol_get_api_version(void) { return MOL_API_VERSION; }

const char* mol_get_version_string(void) { return "0.1.0"; }

const char* mol_result_string(mol_result_t result) {
  switch (result) {
    case MOL_OK:
      return "ok";
    case MOL_ERROR_INVALID_ARGUMENT:
      return "invalid argument";
    case MOL_ERROR_UNSUPPORTED_VERSION:
      return "unsupported version";
    case MOL_ERROR_INSUFFICIENT_MEMORY:
      return "insufficient memory";
    case MOL_ERROR_MISALIGNED_MEMORY:
      return "misaligned memory";
    case MOL_ERROR_INVALID_STATE:
      return "invalid state";
    case MOL_ERROR_QUEUE_FULL:
      return "queue full";
    case MOL_ERROR_BUFFER_TOO_SMALL:
      return "buffer too small";
    case MOL_ERROR_UNSUPPORTED:
      return "unsupported";
    case MOL_ERROR_OVERFLOW:
      return "overflow";
    case MOL_ERROR_INTERNAL:
      return "internal error";
    default:
      return "unknown result";
  }
}

mol_engine_config_t mol_engine_config_default(void) {
  mol_engine_config_t config;
  config.struct_size = (uint32_t)sizeof(config);
  config.api_version = MOL_API_VERSION;
  config.sample_rate = 48000u;
  config.channel_count = 2u;
  config.max_voices = MOL_PROFILE_MAX_VOICES;
  config.command_capacity = 256u;
  config.event_capacity = 256u;
  config.random_seed = UINT32_C(0x4D4F4C31);
  return config;
}

size_t mol_engine_memory_alignment(void) { return _Alignof(mol_engine_t); }

size_t mol_engine_query_memory(const mol_engine_config_t* config) {
  const size_t alignment = mol_engine_memory_alignment();
  if (!mol_engine_config_is_valid(config) || sizeof(mol_engine_t) > SIZE_MAX - (alignment - 1u)) {
    return 0u;
  }
  return (sizeof(mol_engine_t) + alignment - 1u) & ~(alignment - 1u);
}

mol_result_t mol_engine_init(void* memory, size_t memory_size,
                             const mol_engine_config_t* config,
                             mol_engine_t** out_engine) {
  size_t required;
  mol_engine_t* engine;

  if (out_engine == NULL) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  *out_engine = NULL;
  if (config == NULL || memory == NULL) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  if (config->struct_size < sizeof(*config)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  if (config->api_version != MOL_API_VERSION) {
    return MOL_ERROR_UNSUPPORTED_VERSION;
  }
  required = mol_engine_query_memory(config);
  if (required == 0u) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  if (((uintptr_t)memory % mol_engine_memory_alignment()) != 0u) {
    return MOL_ERROR_MISALIGNED_MEMORY;
  }
  if (memory_size < required) {
    return MOL_ERROR_INSUFFICIENT_MEMORY;
  }

  memset(memory, 0, required);
  engine = (mol_engine_t*)memory;
  engine->config = *config;
  engine->magic = MOL_ENGINE_MAGIC;
  *out_engine = engine;
  return MOL_OK;
}

void mol_engine_shutdown(mol_engine_t* engine) {
  if (mol_engine_is_valid(engine)) {
    memset(engine, 0, sizeof(*engine));
  }
}

void mol_engine_reset(mol_engine_t* engine) {
  if (mol_engine_is_valid(engine)) {
    engine->current_frame = 0u;
  }
}

mol_result_t mol_engine_submit(mol_engine_t* engine, const mol_command_t* command) {
  if (!mol_engine_is_valid(engine) || command == NULL ||
      command->struct_size < sizeof(*command)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  if (command->api_version != MOL_API_VERSION) {
    return MOL_ERROR_UNSUPPORTED_VERSION;
  }
  switch (command->command_type) {
    case MOL_COMMAND_ALL_NOTES_OFF:
    case MOL_COMMAND_ALL_SOUND_OFF:
      return MOL_OK;
    case MOL_COMMAND_RESET_ENGINE:
      mol_engine_reset(engine);
      return MOL_OK;
    default:
      return MOL_ERROR_UNSUPPORTED;
  }
}

mol_result_t mol_engine_render_interleaved_f32(mol_engine_t* engine, float* output,
                                                uint32_t frame_count,
                                                uint32_t channel_count) {
  size_t sample_count;
  if (!mol_engine_is_valid(engine) || output == NULL || channel_count == 0u ||
      channel_count != engine->config.channel_count) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  if ((size_t)channel_count > SIZE_MAX / sizeof(*output) ||
      (size_t)frame_count > SIZE_MAX / ((size_t)channel_count * sizeof(*output))) {
    return MOL_ERROR_OVERFLOW;
  }
  sample_count = (size_t)frame_count * (size_t)channel_count;
  memset(output, 0, sample_count * sizeof(*output));
  return mol_engine_advance(engine, frame_count);
}

mol_result_t mol_engine_render_planar_f32(mol_engine_t* engine,
                                          float* const* output_channels,
                                          uint32_t frame_count,
                                          uint32_t channel_count) {
  uint32_t channel;
  if (!mol_engine_is_valid(engine) || output_channels == NULL || channel_count == 0u ||
      channel_count != engine->config.channel_count ||
      (size_t)frame_count > SIZE_MAX / sizeof(float)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  for (channel = 0u; channel < channel_count; ++channel) {
    if (output_channels[channel] == NULL) {
      return MOL_ERROR_INVALID_ARGUMENT;
    }
  }
  for (channel = 0u; channel < channel_count; ++channel) {
    memset(output_channels[channel], 0, (size_t)frame_count * sizeof(float));
  }
  return mol_engine_advance(engine, frame_count);
}

uint32_t mol_engine_poll_events(mol_engine_t* engine, mol_event_t* events,
                                uint32_t capacity) {
  if (!mol_engine_is_valid(engine) || (events == NULL && capacity != 0u)) {
    return 0u;
  }
  return 0u;
}

mol_result_t mol_engine_get_state(const mol_engine_t* engine, mol_engine_state_t* state) {
  if (!mol_engine_is_valid(engine) || state == NULL || state->struct_size < sizeof(*state)) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  state->struct_size = (uint32_t)sizeof(*state);
  state->api_version = MOL_API_VERSION;
  state->current_frame = engine->current_frame;
  state->sample_rate = engine->config.sample_rate;
  state->channel_count = engine->config.channel_count;
  state->max_voices = engine->config.max_voices;
  state->active_voices = 0u;
  return MOL_OK;
}

mol_capability_flags_t mol_engine_get_capabilities(const mol_engine_t* engine) {
  if (!mol_engine_is_valid(engine)) {
    return 0u;
  }
  return MOL_CAPABILITY_CALLER_MEMORY | MOL_CAPABILITY_INTERLEAVED_F32 |
         MOL_CAPABILITY_PLANAR_F32;
}
