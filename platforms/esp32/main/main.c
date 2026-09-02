/* SPDX-License-Identifier: Apache-2.0 */
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "driver/i2s_std.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mol/mol.h"
#include "sequence_fixture.h"

#define MOL_ESP32_RENDER_FRAMES 128u
#define MOL_ESP32_CHANNEL_COUNT 2u
#define MOL_ESP32_ENGINE_BYTES 131072u
#define MOL_ESP32_WRITE_TIMEOUT_MS 100u
#define MOL_ESP32_DIAGNOSTIC_PERIOD_MS 10000u
#define MOL_ESP32_C4_HZ 261.6256f

typedef union mol_esp32_engine_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[MOL_ESP32_ENGINE_BYTES];
} mol_esp32_engine_storage_t;

typedef struct mol_esp32_audio_stats {
  volatile uint32_t dma_queue_overflows;
  uint32_t render_failures;
  uint32_t write_failures;
  uint32_t partial_writes;
  uint32_t render_deadline_misses;
  uint32_t watchdog_failures;
  uint32_t max_render_time_us;
  uint64_t rendered_frames;
} mol_esp32_audio_stats_t;

static const char* const kTag = "mol-keyboard";
static mol_esp32_engine_storage_t engine_memory;
static float render_buffer[MOL_ESP32_RENDER_FRAMES * MOL_ESP32_CHANNEL_COUNT];
static int16_t pcm_buffer[MOL_ESP32_RENDER_FRAMES * MOL_ESP32_CHANNEL_COUNT];
static StackType_t audio_task_stack[CONFIG_MOL_AUDIO_TASK_STACK_SIZE];
static StaticTask_t audio_task_control;
static TaskHandle_t audio_task_handle;
static mol_engine_t* engine;
static i2s_chan_handle_t i2s_tx_channel;
static mol_esp32_audio_stats_t audio_stats;

_Static_assert(CONFIG_MOL_AUDIO_TASK_PRIORITY < configMAX_PRIORITIES,
               "Audio task priority must be below configMAX_PRIORITIES");

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

static bool verify_c4(mol_engine_t* test_engine, uint32_t sample_rate, float* measured_frequency,
                      float* measured_peak) {
  const uint32_t analysis_start = sample_rate / 10u;
  const uint32_t analysis_frames = (sample_rate * 8u) / 10u;
  const uint32_t analysis_end = analysis_start + analysis_frames;
  const uint32_t block_count =
      (sample_rate + MOL_ESP32_RENDER_FRAMES - 1u) / MOL_ESP32_RENDER_FRAMES;
  float previous = 0.0f;
  float peak = 0.0f;
  uint32_t crossings = 0u;
  uint32_t block;

  for (block = 0u; block < block_count; ++block) {
    uint32_t frame;
    if (mol_engine_render_interleaved_f32(test_engine, render_buffer, MOL_ESP32_RENDER_FRAMES,
                                          MOL_ESP32_CHANNEL_COUNT) != MOL_OK) {
      return false;
    }
    for (frame = 0u; frame < MOL_ESP32_RENDER_FRAMES; ++frame) {
      const uint32_t absolute_frame = block * MOL_ESP32_RENDER_FRAMES + frame;
      const float sample = render_buffer[frame * MOL_ESP32_CHANNEL_COUNT];
      const float magnitude = fabsf(sample);
      if (!isfinite(sample)) {
        return false;
      }
      if (magnitude > peak) {
        peak = magnitude;
      }
      if (absolute_frame >= analysis_start && absolute_frame < analysis_end && previous <= 0.0f &&
          sample > 0.0f) {
        ++crossings;
      }
      previous = sample;
    }
  }

  *measured_frequency = (float)crossings * (float)sample_rate / (float)analysis_frames;
  *measured_peak = peak;
  return peak > 0.01f && fabsf(*measured_frequency - MOL_ESP32_C4_HZ) < 1.0f;
}

#if CONFIG_MOL_I2S_ENABLE_DITHER
static uint32_t dither_state = 0x6d6f6c31u;

static float triangular_dither(void) {
  uint32_t first;
  uint32_t second;
  dither_state = dither_state * 1664525u + 1013904223u;
  first = dither_state >> 16u;
  dither_state = dither_state * 1664525u + 1013904223u;
  second = dither_state >> 16u;
  return (float)((int32_t)first - (int32_t)second) / 65536.0f;
}
#endif

static int16_t float_to_pcm16(float sample) {
  float scaled = sample >= 0.0f ? sample * 32767.0f : sample * 32768.0f;
  int32_t rounded;
#if CONFIG_MOL_I2S_ENABLE_DITHER
  scaled += triangular_dither();
#endif
  if (scaled >= 32767.0f) {
    return INT16_MAX;
  }
  if (scaled <= -32768.0f) {
    return INT16_MIN;
  }
  rounded = (int32_t)(scaled + (scaled >= 0.0f ? 0.5f : -0.5f));
  return (int16_t)rounded;
}

static bool IRAM_ATTR on_send_queue_overflow(i2s_chan_handle_t handle, i2s_event_data_t* event,
                                             void* user_context) {
  mol_esp32_audio_stats_t* stats = (mol_esp32_audio_stats_t*)user_context;
  (void)handle;
  (void)event;
  ++stats->dma_queue_overflows;
  return false;
}

static void initialize_i2s(void) {
  i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  i2s_std_config_t standard_config = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(CONFIG_MOL_I2S_SAMPLE_RATE),
      .slot_cfg =
          I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
      .gpio_cfg =
          {
              .mclk = CONFIG_MOL_I2S_MCLK_GPIO < 0 ? I2S_GPIO_UNUSED
                                                   : (gpio_num_t)CONFIG_MOL_I2S_MCLK_GPIO,
              .bclk = (gpio_num_t)CONFIG_MOL_I2S_BCLK_GPIO,
              .ws = (gpio_num_t)CONFIG_MOL_I2S_WS_GPIO,
              .dout = (gpio_num_t)CONFIG_MOL_I2S_DOUT_GPIO,
              .din = I2S_GPIO_UNUSED,
              .invert_flags =
                  {
                      .mclk_inv = false,
                      .bclk_inv = false,
                      .ws_inv = false,
                  },
          },
  };
  const i2s_event_callbacks_t callbacks = {
      .on_recv = NULL,
      .on_recv_q_ovf = NULL,
      .on_sent = NULL,
      .on_send_q_ovf = on_send_queue_overflow,
  };

  channel_config.dma_desc_num = CONFIG_MOL_I2S_DMA_DESCRIPTOR_COUNT;
  channel_config.dma_frame_num = MOL_ESP32_RENDER_FRAMES;
  channel_config.auto_clear_after_cb = true;
  channel_config.intr_priority = 3;
  ESP_ERROR_CHECK(i2s_new_channel(&channel_config, &i2s_tx_channel, NULL));
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(i2s_tx_channel, &standard_config));
  ESP_ERROR_CHECK(i2s_channel_register_event_callback(i2s_tx_channel, &callbacks, &audio_stats));
  ESP_ERROR_CHECK(i2s_channel_enable(i2s_tx_channel));
}

static void audio_render_task(void* context) {
  const uint32_t render_deadline_us =
      (MOL_ESP32_RENDER_FRAMES * 1000000u) / CONFIG_MOL_I2S_SAMPLE_RATE;
  (void)context;

#if CONFIG_ESP_TASK_WDT_EN
  if (esp_task_wdt_add(NULL) != ESP_OK) {
    ++audio_stats.watchdog_failures;
  }
#endif

  for (;;) {
    const int64_t render_start = esp_timer_get_time();
    size_t bytes_written = 0u;
    esp_err_t write_result;
    uint32_t index;
    mol_result_t render_result = mol_engine_render_interleaved_f32(
        engine, render_buffer, MOL_ESP32_RENDER_FRAMES, MOL_ESP32_CHANNEL_COUNT);

    if (render_result == MOL_OK) {
      for (index = 0u; index < MOL_ESP32_RENDER_FRAMES * MOL_ESP32_CHANNEL_COUNT; ++index) {
        pcm_buffer[index] = float_to_pcm16(render_buffer[index]);
      }
    } else {
      ++audio_stats.render_failures;
      memset(pcm_buffer, 0, sizeof(pcm_buffer));
    }

    {
      const uint32_t render_time_us = (uint32_t)(esp_timer_get_time() - render_start);
      if (render_time_us > audio_stats.max_render_time_us) {
        audio_stats.max_render_time_us = render_time_us;
      }
      if (render_time_us > render_deadline_us) {
        ++audio_stats.render_deadline_misses;
      }
    }

    write_result = i2s_channel_write(i2s_tx_channel, pcm_buffer, sizeof(pcm_buffer), &bytes_written,
                                     MOL_ESP32_WRITE_TIMEOUT_MS);
    if (write_result != ESP_OK) {
      ++audio_stats.write_failures;
    } else if (bytes_written != sizeof(pcm_buffer)) {
      ++audio_stats.partial_writes;
    }
    audio_stats.rendered_frames += MOL_ESP32_RENDER_FRAMES;

#if CONFIG_ESP_TASK_WDT_EN
    if (esp_task_wdt_reset() != ESP_OK) {
      ++audio_stats.watchdog_failures;
    }
#endif
  }
}

void app_main(void) {
  mol_engine_config_t config = mol_engine_config_default();
  mol_command_t note_on = make_note_on();
  mol_sequence_fixture_summary_t sequence_summary;
  mol_result_t result;
  float frequency = 0.0f;
  float peak = 0.0f;

  ESP_LOGI(kTag, "Reset reason=%d", (int)esp_reset_reason());
  if (!mol_sequence_fixture_verify(&sequence_summary)) {
    ESP_LOGE(kTag, "Shared Mol Sequence fixture conformance failed");
    return;
  }
  ESP_LOGI(kTag, "Shared Mol Sequence passed: events=%" PRIu32 " final=%" PRIu64,
           sequence_summary.event_count, sequence_summary.final_frame);
  config.sample_rate = CONFIG_MOL_I2S_SAMPLE_RATE;
  config.channel_count = MOL_ESP32_CHANNEL_COUNT;
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
  if (result != MOL_OK || !verify_c4(engine, CONFIG_MOL_I2S_SAMPLE_RATE, &frequency, &peak)) {
    ESP_LOGE(kTag, "Core C4 conformance failed");
    mol_engine_shutdown(engine);
    return;
  }
  ESP_LOGI(kTag, "Tiny core C4 passed: frequency=%.4f Hz peak=%.6f", (double)frequency,
           (double)peak);

  initialize_i2s();
  audio_task_handle = xTaskCreateStaticPinnedToCore(
      audio_render_task, "mol-audio", CONFIG_MOL_AUDIO_TASK_STACK_SIZE, NULL,
      CONFIG_MOL_AUDIO_TASK_PRIORITY, audio_task_stack, &audio_task_control,
      CONFIG_MOL_AUDIO_TASK_CORE);
  ESP_ERROR_CHECK(audio_task_handle != NULL ? ESP_OK : ESP_ERR_NO_MEM);
  ESP_LOGI(kTag,
           "I2S active: %" PRIu32
           " Hz, BCLK=%d WS=%d DOUT=%d, DMA=%d x %u, "
           "audio priority=%d core=%d",
           (uint32_t)CONFIG_MOL_I2S_SAMPLE_RATE, CONFIG_MOL_I2S_BCLK_GPIO, CONFIG_MOL_I2S_WS_GPIO,
           CONFIG_MOL_I2S_DOUT_GPIO, CONFIG_MOL_I2S_DMA_DESCRIPTOR_COUNT, MOL_ESP32_RENDER_FRAMES,
           CONFIG_MOL_AUDIO_TASK_PRIORITY, CONFIG_MOL_AUDIO_TASK_CORE);

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(MOL_ESP32_DIAGNOSTIC_PERIOD_MS));
    ESP_LOGI(kTag,
             "audio frames=%" PRIu64 " render_fail=%" PRIu32 " write_fail=%" PRIu32
             " partial=%" PRIu32 " dma_q_ovf=%" PRIu32 " deadline_miss=%" PRIu32
             " max_render_us=%" PRIu32 " wdt_fail=%" PRIu32 " stack_min=%u internal_heap_min=%u",
             audio_stats.rendered_frames, audio_stats.render_failures, audio_stats.write_failures,
             audio_stats.partial_writes, audio_stats.dma_queue_overflows,
             audio_stats.render_deadline_misses, audio_stats.max_render_time_us,
             audio_stats.watchdog_failures,
             (unsigned int)uxTaskGetStackHighWaterMark(audio_task_handle),
             (unsigned int)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
  }
}
