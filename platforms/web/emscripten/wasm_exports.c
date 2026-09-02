/* SPDX-License-Identifier: Apache-2.0 */
#include "mol/mol.h"

#include <emscripten/emscripten.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MOL_WASM_MAX_FRAMES 128u

typedef union mol_wasm_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[65536];
} mol_wasm_storage_t;

static mol_wasm_storage_t mol_wasm_memory;
static mol_engine_t* mol_wasm_engine;
static float mol_wasm_output[MOL_WASM_MAX_FRAMES * 2u];

static mol_command_t mol_wasm_note_command(mol_command_type_t type, uint8_t note,
                                           float velocity, mol_gesture_id_t gesture_id) {
  mol_command_t command;
  memset(&command, 0, sizeof(command));
  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = type;
  command.target_frame = MOL_FRAME_IMMEDIATE;
  command.gesture_id = gesture_id;
  command.payload.note.note = note;
  command.payload.note.velocity = velocity;
  return command;
}

EMSCRIPTEN_KEEPALIVE int mol_wasm_initialize(uint32_t sample_rate, uint32_t channel_count,
                                             uint32_t max_voices) {
  mol_engine_config_t config = mol_engine_config_default();
  if (mol_wasm_engine != NULL) {
    mol_engine_shutdown(mol_wasm_engine);
    mol_wasm_engine = NULL;
  }
  config.sample_rate = sample_rate;
  config.channel_count = channel_count;
  config.max_voices = max_voices;
  config.command_capacity = 64u;
  config.event_capacity = 64u;
  if (mol_engine_query_memory(&config) > sizeof(mol_wasm_memory.bytes)) {
    return MOL_ERROR_INSUFFICIENT_MEMORY;
  }
  return mol_engine_init(mol_wasm_memory.bytes, sizeof(mol_wasm_memory.bytes), &config,
                         &mol_wasm_engine);
}

EMSCRIPTEN_KEEPALIVE int mol_wasm_note_on(uint32_t note, float velocity,
                                          uint32_t gesture_id) {
  mol_command_t command;
  if (note > 127u || mol_wasm_engine == NULL) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  command = mol_wasm_note_command(MOL_COMMAND_NOTE_ON, (uint8_t)note, velocity,
                                  (mol_gesture_id_t)gesture_id);
  return mol_engine_submit(mol_wasm_engine, &command);
}

EMSCRIPTEN_KEEPALIVE int mol_wasm_note_off(uint32_t gesture_id) {
  mol_command_t command;
  if (mol_wasm_engine == NULL) {
    return MOL_ERROR_INVALID_STATE;
  }
  command = mol_wasm_note_command(MOL_COMMAND_NOTE_OFF, 0u, 0.0f,
                                  (mol_gesture_id_t)gesture_id);
  return mol_engine_submit(mol_wasm_engine, &command);
}

EMSCRIPTEN_KEEPALIVE const float* mol_wasm_render(uint32_t frame_count,
                                                  uint32_t channel_count) {
  if (mol_wasm_engine == NULL || frame_count > MOL_WASM_MAX_FRAMES ||
      channel_count == 0u || channel_count > 2u) {
    return NULL;
  }
  if (mol_engine_render_interleaved_f32(mol_wasm_engine, mol_wasm_output, frame_count,
                                         channel_count) != MOL_OK) {
    return NULL;
  }
  return mol_wasm_output;
}
