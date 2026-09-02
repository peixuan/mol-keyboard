/* SPDX-License-Identifier: Apache-2.0 */
#include "mol/mol.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"

#define MOL_ESP32_RENDER_FRAMES 128u

typedef union mol_esp32_engine_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[16384];
} mol_esp32_engine_storage_t;

static const char* const kTag = "mol-keyboard";
static mol_esp32_engine_storage_t engine_memory;
static float render_buffer[MOL_ESP32_RENDER_FRAMES];

static mol_command_t make_note_on(void) {
  mol_command_t command;
  memset(&command, 0, sizeof(command));
  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = MOL_COMMAND_NOTE_ON;
  command.target_frame = MOL_FRAME_IMMEDIATE;
  command.gesture_id = 1u;
  command.payload.note.note = 60u;
  command.payload.note.velocity = 0.8f;
  return command;
}

void app_main(void) {
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  mol_command_t note_on = make_note_on();
  mol_result_t result;
  float peak = 0.0f;
  uint32_t index;

  config.sample_rate = 32000u;
  config.channel_count = 1u;
  config.max_voices = 8u;
  config.command_capacity = 32u;
  config.event_capacity = 32u;
  if (mol_engine_query_memory(&config) > sizeof(engine_memory.bytes)) {
    ESP_LOGE(kTag, "Tiny engine exceeds the static memory budget");
    return;
  }
  result = mol_engine_init(engine_memory.bytes, sizeof(engine_memory.bytes), &config, &engine);
  if (result != MOL_OK) {
    ESP_LOGE(kTag, "Engine initialization failed: %s", mol_result_string(result));
    return;
  }
  result = mol_engine_submit(engine, &note_on);
  if (result == MOL_OK) {
    result = mol_engine_render_interleaved_f32(engine, render_buffer,
                                                MOL_ESP32_RENDER_FRAMES, 1u);
  }
  if (result != MOL_OK) {
    ESP_LOGE(kTag, "Core render self-test failed: %s", mol_result_string(result));
    mol_engine_shutdown(engine);
    return;
  }
  for (index = 0u; index < MOL_ESP32_RENDER_FRAMES; ++index) {
    float magnitude = fabsf(render_buffer[index]);
    if (!isfinite(render_buffer[index])) {
      ESP_LOGE(kTag, "Core render self-test produced a non-finite sample");
      mol_engine_shutdown(engine);
      return;
    }
    if (magnitude > peak) {
      peak = magnitude;
    }
  }
  if (peak <= 0.0f) {
    ESP_LOGE(kTag, "Core render self-test produced silence");
    mol_engine_shutdown(engine);
    return;
  }
  ESP_LOGI(kTag, "Tiny core C4 self-test passed; peak=%.6f", (double)peak);
  mol_engine_shutdown(engine);
}
