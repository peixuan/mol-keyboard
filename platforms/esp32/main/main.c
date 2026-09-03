/* SPDX-License-Identifier: Apache-2.0 */
#include <inttypes.h>
#include <math.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bluetooth_hid.h"
#if CONFIG_MOL_A2DP_ENABLE
#include "a2dp_source.h"
#endif
#include "device_control.h"
#include "device_settings.h"
#include "device_storage.h"
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
#include "gpio_matrix.h"
#include "input_queue.h"
#include "mol/mol.h"
#include "sequence_fixture.h"
#include "sequence_storage.h"

#if CONFIG_MOL_USB_HID_ENABLE
#include "usb_hid.h"
#endif

#define MOL_ESP32_RENDER_FRAMES 128u
#define MOL_ESP32_CHANNEL_COUNT 2u
#define MOL_ESP32_ENGINE_BYTES 37888u
#define MOL_ESP32_WRITE_TIMEOUT_MS 100u
#if CONFIG_MOL_QEMU_RUNTIME
#define MOL_ESP32_DIAGNOSTIC_PERIOD_MS 2000u
#define MOL_ESP32_QEMU_REQUIRED_SNAPSHOTS 3u
#else
#define MOL_ESP32_DIAGNOSTIC_PERIOD_MS 10000u
#endif
#define MOL_ESP32_C4_HZ 261.6256f

typedef union mol_esp32_engine_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[MOL_ESP32_ENGINE_BYTES];
} mol_esp32_engine_storage_t;

typedef struct mol_esp32_audio_stats {
  atomic_uint_least32_t dma_queue_overflows;
  atomic_uint_least32_t render_failures;
  atomic_uint_least32_t write_failures;
  atomic_uint_least32_t partial_writes;
  atomic_uint_least32_t render_deadline_misses;
  atomic_uint_least32_t watchdog_failures;
  atomic_uint_least32_t submitted_commands;
  atomic_uint_least32_t max_render_time_us;
  atomic_uint_least32_t rendered_frames;
  atomic_uint_least32_t nonfinite_samples;
  atomic_uint_least32_t nonzero_samples;
} mol_esp32_audio_stats_t;

typedef struct mol_esp32_audio_snapshot {
  uint32_t dma_queue_overflows;
  uint32_t render_failures;
  uint32_t write_failures;
  uint32_t partial_writes;
  uint32_t render_deadline_misses;
  uint32_t watchdog_failures;
  uint32_t submitted_commands;
  uint32_t max_render_time_us;
  uint32_t rendered_frames;
  uint32_t nonfinite_samples;
  uint32_t nonzero_samples;
} mol_esp32_audio_snapshot_t;

static const char* const kTag = "mol-keyboard";
static mol_esp32_engine_storage_t engine_memory;
static float render_buffer[MOL_ESP32_RENDER_FRAMES * MOL_ESP32_CHANNEL_COUNT];
static int16_t pcm_buffer[MOL_ESP32_RENDER_FRAMES * MOL_ESP32_CHANNEL_COUNT];
static StackType_t audio_task_stack[CONFIG_MOL_AUDIO_TASK_STACK_SIZE];
static StaticTask_t audio_task_control;
static TaskHandle_t audio_task_handle;
static mol_engine_t* engine;
#if !CONFIG_MOL_QEMU_RUNTIME
static i2s_chan_handle_t i2s_tx_channel;
#endif
static mol_esp32_audio_stats_t audio_stats;

_Static_assert(CONFIG_MOL_AUDIO_TASK_PRIORITY < configMAX_PRIORITIES,
               "Audio task priority must be below configMAX_PRIORITIES");

static void update_max_render_time(uint32_t value) {
  uint_least32_t observed =
      atomic_load_explicit(&audio_stats.max_render_time_us, memory_order_relaxed);
  while ((uint_least32_t)value > observed &&
         !atomic_compare_exchange_weak_explicit(&audio_stats.max_render_time_us, &observed, value,
                                                memory_order_relaxed, memory_order_relaxed)) {
  }
}

static mol_esp32_audio_snapshot_t audio_stats_snapshot(void) {
  mol_esp32_audio_snapshot_t snapshot;
  snapshot.dma_queue_overflows =
      (uint32_t)atomic_load_explicit(&audio_stats.dma_queue_overflows, memory_order_relaxed);
  snapshot.render_failures =
      (uint32_t)atomic_load_explicit(&audio_stats.render_failures, memory_order_relaxed);
  snapshot.write_failures =
      (uint32_t)atomic_load_explicit(&audio_stats.write_failures, memory_order_relaxed);
  snapshot.partial_writes =
      (uint32_t)atomic_load_explicit(&audio_stats.partial_writes, memory_order_relaxed);
  snapshot.render_deadline_misses =
      (uint32_t)atomic_load_explicit(&audio_stats.render_deadline_misses, memory_order_relaxed);
  snapshot.watchdog_failures =
      (uint32_t)atomic_load_explicit(&audio_stats.watchdog_failures, memory_order_relaxed);
  snapshot.submitted_commands =
      (uint32_t)atomic_load_explicit(&audio_stats.submitted_commands, memory_order_relaxed);
  snapshot.max_render_time_us =
      (uint32_t)atomic_load_explicit(&audio_stats.max_render_time_us, memory_order_relaxed);
  snapshot.rendered_frames =
      (uint32_t)atomic_load_explicit(&audio_stats.rendered_frames, memory_order_relaxed);
  snapshot.nonfinite_samples =
      (uint32_t)atomic_load_explicit(&audio_stats.nonfinite_samples, memory_order_relaxed);
  snapshot.nonzero_samples =
      (uint32_t)atomic_load_explicit(&audio_stats.nonzero_samples, memory_order_relaxed);
  return snapshot;
}

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

#if !CONFIG_MOL_QEMU_RUNTIME
static bool IRAM_ATTR on_send_queue_overflow(i2s_chan_handle_t handle, i2s_event_data_t* event,
                                             void* user_context) {
  mol_esp32_audio_stats_t* stats = (mol_esp32_audio_stats_t*)user_context;
  (void)handle;
  (void)event;
  atomic_fetch_add_explicit(&stats->dma_queue_overflows, 1u, memory_order_relaxed);
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
#endif

static void audio_render_task(void* context) {
  const uint32_t render_deadline_us =
      (MOL_ESP32_RENDER_FRAMES * 1000000u) / CONFIG_MOL_I2S_SAMPLE_RATE;
  (void)context;

#if CONFIG_ESP_TASK_WDT_EN
  if (esp_task_wdt_add(NULL) != ESP_OK) {
    atomic_fetch_add_explicit(&audio_stats.watchdog_failures, 1u, memory_order_relaxed);
  }
#endif

  for (;;) {
    const int64_t render_start = esp_timer_get_time();
    size_t bytes_written = 0u;
    esp_err_t write_result;
    uint32_t index;
    atomic_fetch_add_explicit(&audio_stats.submitted_commands, mol_input_drain(engine),
                              memory_order_relaxed);
    mol_result_t render_result = mol_engine_render_interleaved_f32(
        engine, render_buffer, MOL_ESP32_RENDER_FRAMES, MOL_ESP32_CHANNEL_COUNT);

    if (render_result == MOL_OK) {
      uint32_t nonfinite_samples = 0u;
      uint32_t nonzero_samples = 0u;
      for (index = 0u; index < MOL_ESP32_RENDER_FRAMES * MOL_ESP32_CHANNEL_COUNT; ++index) {
        if (!isfinite(render_buffer[index])) {
          pcm_buffer[index] = 0;
          ++nonfinite_samples;
        } else {
          pcm_buffer[index] = float_to_pcm16(render_buffer[index]);
          if (pcm_buffer[index] != 0) {
            ++nonzero_samples;
          }
        }
      }
      atomic_fetch_add_explicit(&audio_stats.nonfinite_samples, nonfinite_samples,
                                memory_order_relaxed);
      atomic_fetch_add_explicit(&audio_stats.nonzero_samples, nonzero_samples,
                                memory_order_relaxed);
    } else {
      atomic_fetch_add_explicit(&audio_stats.render_failures, 1u, memory_order_relaxed);
      memset(pcm_buffer, 0, sizeof(pcm_buffer));
    }

    {
      const uint32_t render_time_us = (uint32_t)(esp_timer_get_time() - render_start);
      update_max_render_time(render_time_us);
      if (render_time_us > render_deadline_us) {
        atomic_fetch_add_explicit(&audio_stats.render_deadline_misses, 1u, memory_order_relaxed);
      }
    }

#if CONFIG_MOL_A2DP_ENABLE
    mol_a2dp_source_submit_pcm(pcm_buffer, sizeof(pcm_buffer));
#endif
#if CONFIG_MOL_QEMU_RUNTIME
    vTaskDelay(pdMS_TO_TICKS((MOL_ESP32_RENDER_FRAMES * 1000u) / CONFIG_MOL_I2S_SAMPLE_RATE));
    write_result = ESP_OK;
    bytes_written = sizeof(pcm_buffer);
#else
    write_result = i2s_channel_write(i2s_tx_channel, pcm_buffer, sizeof(pcm_buffer), &bytes_written,
                                     MOL_ESP32_WRITE_TIMEOUT_MS);
#endif
    if (write_result != ESP_OK) {
      atomic_fetch_add_explicit(&audio_stats.write_failures, 1u, memory_order_relaxed);
    } else if (bytes_written != sizeof(pcm_buffer)) {
      atomic_fetch_add_explicit(&audio_stats.partial_writes, 1u, memory_order_relaxed);
    }
    atomic_fetch_add_explicit(&audio_stats.rendered_frames, MOL_ESP32_RENDER_FRAMES,
                              memory_order_relaxed);

#if CONFIG_ESP_TASK_WDT_EN
    if (esp_task_wdt_reset() != ESP_OK) {
      atomic_fetch_add_explicit(&audio_stats.watchdog_failures, 1u, memory_order_relaxed);
    }
#endif
  }
}

void app_main(void) {
  mol_engine_config_t config = mol_engine_config_default();
  mol_command_t note_on = make_note_on();
  mol_sequence_fixture_summary_t sequence_summary;
  mol_result_t result;
  mol_device_settings_t device_settings;
  mol_device_settings_source_t settings_source;
  mol_command_t settings_commands[MOL_DEVICE_SETTINGS_COMMAND_COUNT];
  size_t settings_command_count = 0u;
  size_t settings_command_index;
  size_t engine_bytes;
  float frequency = 0.0f;
  float peak = 0.0f;
  bool device_storage_ready = false;
  bool sequence_storage_ready = false;
  bool bluetooth_ready = false;
#if CONFIG_MOL_QEMU_RUNTIME
  uint32_t qemu_diagnostic_count = 0u;
#endif

  ESP_LOGI(kTag, "Reset reason=%d", (int)esp_reset_reason());
  device_settings = mol_device_settings_default();
  settings_source = MOL_DEVICE_SETTINGS_DEFAULT_IO_ERROR;
  result = mol_device_storage_initialize();
  if (result != MOL_OK) {
    ESP_LOGW(kTag, "NVS initialization failed; volatile safe defaults selected");
  } else {
    device_storage_ready = true;
    result = mol_device_storage_load_settings(&device_settings, &settings_source);
    if (result != MOL_OK) {
      device_settings = mol_device_settings_default();
      settings_source = MOL_DEVICE_SETTINGS_DEFAULT_IO_ERROR;
      ESP_LOGW(kTag, "NVS settings read failed; volatile safe defaults selected");
    }
  }
  if (settings_source == MOL_DEVICE_SETTINGS_DEFAULT_CORRUPT) {
    ESP_LOGW(kTag, "Settings record corrupt or unsupported; safe defaults selected");
  } else if (settings_source == MOL_DEVICE_SETTINGS_DEFAULT_MISSING) {
    ESP_LOGI(kTag, "No settings record; safe defaults selected");
  } else if (settings_source == MOL_DEVICE_SETTINGS_FROM_NVS) {
    ESP_LOGI(kTag, "Settings loaded from NVS: generation=%" PRIu32, device_settings.generation);
  }
  result = mol_sequence_storage_initialize();
  if (result != MOL_OK) {
    ESP_LOGW(kTag, "Sequence FAT storage unavailable; live audio remains enabled");
  } else {
    sequence_storage_ready = true;
    ESP_LOGI(kTag, "Sequence FAT storage mounted with transactional recovery");
  }
  if (!mol_sequence_fixture_verify(&sequence_summary)) {
    ESP_LOGE(kTag, "Shared Mol Sequence fixture conformance failed");
    return;
  }
  ESP_LOGI(kTag, "Shared Mol Sequence passed: events=%" PRIu32 " final=%" PRIu64,
           sequence_summary.event_count, sequence_summary.final_frame);
  config.sample_rate = CONFIG_MOL_I2S_SAMPLE_RATE;
  config.channel_count = MOL_ESP32_CHANNEL_COUNT;
  config.max_voices = 8u;
  config.command_capacity = 16u;
  config.event_capacity = 16u;
  config.sequence_capacity = 64u;
  engine_bytes = mol_engine_query_memory(&config);
  ESP_LOGI(kTag, "Tiny engine memory: required=%" PRIu32 " static=%" PRIu32 " bytes",
           (uint32_t)engine_bytes, (uint32_t)sizeof(engine_memory.bytes));
  if (engine_bytes == 0u || engine_bytes > sizeof(engine_memory.bytes)) {
    ESP_LOGE(kTag, "Tiny engine configuration is invalid or exceeds the static memory budget");
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

  mol_engine_shutdown(engine);
  engine = NULL;
  result = mol_engine_init(engine_memory.bytes, sizeof(engine_memory.bytes), &config, &engine);
  if (result != MOL_OK) {
    ESP_LOGE(kTag, "Clean engine initialization failed: %s", mol_result_string(result));
    return;
  }

  mol_input_queue_init();
  result = mol_device_settings_compile_commands(&device_settings, settings_commands,
                                                MOL_DEVICE_SETTINGS_COMMAND_COUNT,
                                                &settings_command_count);
  if (result != MOL_OK) {
    ESP_LOGE(kTag, "Persisted settings could not be compiled: %s", mol_result_string(result));
    return;
  }
  for (settings_command_index = 0u; settings_command_index < settings_command_count;
       ++settings_command_index) {
    if (!mol_input_submit(&settings_commands[settings_command_index])) {
      ESP_LOGE(kTag, "Persisted settings queue capacity is insufficient");
      return;
    }
  }
#if CONFIG_MOL_QEMU_RUNTIME
  if (!mol_input_submit(&note_on)) {
    ESP_LOGE(kTag, "QEMU synthetic C4 command could not be queued");
    return;
  }
  ESP_LOGI(kTag, "QEMU synthetic C4 command queued through the production input path");
#endif
#if CONFIG_MOL_QEMU_RUNTIME
  ESP_LOGI(kTag,
           "QEMU virtual audio sink active: %" PRIu32
           " Hz, block=%u frames; physical I2S is not exercised",
           (uint32_t)CONFIG_MOL_I2S_SAMPLE_RATE, MOL_ESP32_RENDER_FRAMES);
#else
  initialize_i2s();
#endif
  audio_task_handle = xTaskCreateStaticPinnedToCore(
      audio_render_task, "mol-audio", CONFIG_MOL_AUDIO_TASK_STACK_SIZE, NULL,
      CONFIG_MOL_AUDIO_TASK_PRIORITY, audio_task_stack, &audio_task_control,
      CONFIG_MOL_AUDIO_TASK_CORE);
  ESP_ERROR_CHECK(audio_task_handle != NULL ? ESP_OK : ESP_ERR_NO_MEM);
#if CONFIG_MOL_GPIO_MATRIX_ENABLE
  ESP_ERROR_CHECK(mol_gpio_matrix_start());
  ESP_LOGI(kTag,
           "GPIO matrix active: 5x6, debounce=%d ms, ghost=%s, config=%d/%d ms, "
           "clear-pairing=%d+%d/%d ms, factory-reset=%d+%d/%d ms",
           CONFIG_MOL_GPIO_DEBOUNCE_MS,
#if CONFIG_MOL_GPIO_GHOST_ALLOW
           "diode/allow",
#else
           "suppress-ambiguous",
#endif
           CONFIG_MOL_GPIO_CONFIG_KEY, CONFIG_MOL_GPIO_CONFIG_HOLD_MS, CONFIG_MOL_GPIO_CONFIG_KEY,
           CONFIG_MOL_GPIO_CLEAR_PAIRING_KEY, CONFIG_MOL_GPIO_CLEAR_PAIRING_HOLD_MS,
           CONFIG_MOL_GPIO_CONFIG_KEY, CONFIG_MOL_GPIO_FACTORY_RESET_KEY,
           CONFIG_MOL_GPIO_FACTORY_RESET_HOLD_MS);
#else
  ESP_LOGI(kTag, "GPIO matrix capability=unsupported (disabled by build configuration)");
#endif
#if CONFIG_MOL_QEMU_RUNTIME
  ESP_LOGI(kTag, "QEMU runtime excludes physical GPIO, Bluetooth, A2DP, USB, RF, and I2S claims");
#else
  ESP_LOGI(kTag,
           "I2S active: %" PRIu32
           " Hz, BCLK=%d WS=%d DOUT=%d, DMA=%d x %u, "
           "audio priority=%d core=%d",
           (uint32_t)CONFIG_MOL_I2S_SAMPLE_RATE, CONFIG_MOL_I2S_BCLK_GPIO, CONFIG_MOL_I2S_WS_GPIO,
           CONFIG_MOL_I2S_DOUT_GPIO, CONFIG_MOL_I2S_DMA_DESCRIPTOR_COUNT, MOL_ESP32_RENDER_FRAMES,
           CONFIG_MOL_AUDIO_TASK_PRIORITY, CONFIG_MOL_AUDIO_TASK_CORE);
  result = mol_bluetooth_hid_start(device_settings.paired_peer_address,
                                   device_settings.paired_peer_valid != 0u);
  if (result == ESP_OK) {
    bluetooth_ready = true;
#if CONFIG_IDF_TARGET_ESP32
    ESP_LOGI(kTag, "Bluetooth HID host active: BLE + Classic");
#else
    ESP_LOGI(kTag, "Bluetooth HID host active: BLE (Classic unsupported by this SoC)");
#endif
  } else {
    ESP_LOGW(kTag, "Bluetooth HID host unavailable: %s; live audio remains enabled",
             esp_err_to_name(result));
  }
#if CONFIG_MOL_A2DP_ENABLE
  if (bluetooth_ready) {
    result = mol_a2dp_source_start(device_settings.a2dp_sink_address,
                                   device_settings.a2dp_sink_valid != 0u,
                                   device_settings.output_mode == MOL_DEVICE_OUTPUT_A2DP);
    if (result == ESP_OK) {
      ESP_LOGI(kTag, "A2DP Source capability=available mode=%s codec=SBC sample_rate=%d Hz",
               device_settings.output_mode == MOL_DEVICE_OUTPUT_A2DP ? "selected" : "inactive",
               CONFIG_MOL_I2S_SAMPLE_RATE);
    } else {
      ESP_LOGW(kTag, "A2DP Source unavailable: %s; I2S fallback remains active",
               esp_err_to_name(result));
    }
  } else {
    ESP_LOGW(kTag, "A2DP Source unavailable because the shared Bluetooth host did not start");
  }
#elif CONFIG_IDF_TARGET_ESP32
  ESP_LOGI(kTag, "A2DP Source capability=unsupported (disabled by build configuration)");
#else
  ESP_LOGI(kTag, "A2DP Source capability=unsupported (Classic Bluetooth absent on this SoC)");
#endif
#if CONFIG_MOL_USB_HID_ENABLE
  result = mol_usb_hid_start();
  if (result == ESP_OK) {
    ESP_LOGI(kTag,
             "USB HID host active: internal PHY D-=GPIO19 D+=GPIO20; external 5 V VBUS supply "
             "required");
  } else {
    ESP_LOGW(kTag, "USB HID host unavailable: %s; live audio remains enabled",
             esp_err_to_name(result));
  }
#elif CONFIG_IDF_TARGET_ESP32S3
  ESP_LOGI(kTag, "USB HID host capability=unsupported (disabled by build configuration)");
#else
  ESP_LOGI(kTag, "USB HID host capability=unsupported (not supported by this SoC)");
#endif
#endif
  result = mol_device_control_start(&device_settings, device_storage_ready, sequence_storage_ready,
                                    bluetooth_ready);
  if (result != ESP_OK) {
    ESP_LOGE(kTag, "Device control task unavailable: %s", esp_err_to_name(result));
    return;
  }
  ESP_LOGI(kTag, "Device control active: priority=%d core=%d; audio never waits on storage",
           CONFIG_MOL_DEVICE_CONTROL_TASK_PRIORITY, CONFIG_MOL_DEVICE_CONTROL_TASK_CORE);

  for (;;) {
    mol_esp32_audio_snapshot_t audio_snapshot;
    mol_input_queue_stats_t input_stats;
    mol_gpio_matrix_stats_t gpio_stats;
    mol_device_storage_stats_t storage_stats;
    mol_sequence_storage_stats_t sequence_storage_stats;
    mol_bluetooth_hid_stats_t bluetooth_stats;
    mol_device_control_stats_t control_stats;
    vTaskDelay(pdMS_TO_TICKS(MOL_ESP32_DIAGNOSTIC_PERIOD_MS));
    audio_snapshot = audio_stats_snapshot();
    input_stats = mol_input_queue_stats();
    gpio_stats = mol_gpio_matrix_stats();
    storage_stats = mol_device_storage_stats();
    sequence_storage_stats = mol_sequence_storage_stats();
    bluetooth_stats = mol_bluetooth_hid_stats();
    control_stats = mol_device_control_stats();
    ESP_LOGI(
        kTag,
        "audio frames=%" PRIu32 " render_fail=%" PRIu32 " write_fail=%" PRIu32 " partial=%" PRIu32
        " dma_q_ovf=%" PRIu32 " deadline_miss=%" PRIu32 " max_render_us=%" PRIu32
        " wdt_fail=%" PRIu32 " commands=%" PRIu32 " input_queued=%" PRIu32 " input_drop=%" PRIu32
        " input_reject=%" PRIu32 " input_high=%" PRIu32 " gpio_scans=%" PRIu32
        " gpio_events=%" PRIu32 " gpio_ghost=%" PRIu32 " gpio_fail=%" PRIu32 " nvs_load=%" PRIu32
        " nvs_save=%" PRIu32 " nvs_missing=%" PRIu32 " nvs_corrupt=%" PRIu32 " nvs_io_fail=%" PRIu32
        " seq_load=%" PRIu32 " seq_save=%" PRIu32 " seq_corrupt=%" PRIu32 " seq_io_fail=%" PRIu32
        " bt_ble_scan=%" PRIu32 " bt_classic_scan=%" PRIu32 " bt_open=%" PRIu32
        " bt_connect=%" PRIu32 " bt_disconnect=%" PRIu32 " bt_report=%" PRIu32
        " bt_invalid=%" PRIu32 " bt_fail=%" PRIu32 " bt_stack_min=%" PRIu32
        " audio_stack_min=%u gpio_stack_min=%" PRIu32 " internal_heap_min=%u"
        " nonfinite=%" PRIu32 " nonzero=%" PRIu32,
        audio_snapshot.rendered_frames, audio_snapshot.render_failures,
        audio_snapshot.write_failures, audio_snapshot.partial_writes,
        audio_snapshot.dma_queue_overflows, audio_snapshot.render_deadline_misses,
        audio_snapshot.max_render_time_us, audio_snapshot.watchdog_failures,
        audio_snapshot.submitted_commands, input_stats.queued, input_stats.dropped,
        input_stats.rejected, input_stats.high_water, gpio_stats.scans, gpio_stats.transitions,
        gpio_stats.ghost_scans, gpio_stats.delivery_failures, storage_stats.settings_loads,
        storage_stats.settings_saves, storage_stats.missing_records, storage_stats.corrupt_records,
        storage_stats.io_failures, sequence_storage_stats.loads, sequence_storage_stats.saves,
        sequence_storage_stats.corrupt_files, sequence_storage_stats.io_failures,
        bluetooth_stats.ble_scans, bluetooth_stats.classic_scans, bluetooth_stats.open_attempts,
        bluetooth_stats.connections, bluetooth_stats.disconnects, bluetooth_stats.reports,
        bluetooth_stats.invalid_reports, bluetooth_stats.delivery_failures,
        bluetooth_stats.stack_high_water,
        (unsigned int)uxTaskGetStackHighWaterMark(audio_task_handle), gpio_stats.stack_high_water,
        (unsigned int)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
        audio_snapshot.nonfinite_samples, audio_snapshot.nonzero_samples);
    ESP_LOGI(kTag,
             "control_config=%" PRIu32 " control_apply=%" PRIu32 " control_reject=%" PRIu32
             " control_q_reject=%" PRIu32 " control_save=%" PRIu32 " control_io_fail=%" PRIu32
             " control_peer=%" PRIu32 " control_unpair=%" PRIu32 " control_factory=%" PRIu32
             " control_bond_remove=%" PRIu32 " control_stack_min=%" PRIu32,
             control_stats.configuration_entries, control_stats.settings_applied,
             control_stats.settings_rejected, control_stats.queue_rejections,
             control_stats.settings_saves, control_stats.persistence_failures,
             control_stats.peer_updates, control_stats.clear_pairing_operations,
             control_stats.factory_reset_operations, control_stats.bond_removal_requests,
             control_stats.stack_high_water);
#if CONFIG_MOL_A2DP_ENABLE
    {
      const mol_a2dp_source_stats_t a2dp_stats = mol_a2dp_source_stats();
      ESP_LOGI(kTag,
               "a2dp_found=%" PRIu32 " a2dp_attempt=%" PRIu32 " a2dp_connect=%" PRIu32
               " a2dp_disconnect=%" PRIu32 " a2dp_conn_fail=%" PRIu32 " a2dp_codec_reject=%" PRIu32
               " a2dp_start=%" PRIu32 " a2dp_ctrl_fail=%" PRIu32 " a2dp_pcm_bytes=%" PRIu32
               " a2dp_pcm_drop=%" PRIu32 " a2dp_callbacks=%" PRIu32 " a2dp_underrun=%" PRIu32
               " a2dp_silence_bytes=%" PRIu32 " a2dp_buffer=%" PRIu32 "/%" PRIu32
               " avrc_connect=%" PRIu32 " avrc_caps=%" PRIu32 " avrc_events=%" PRIu32
               " a2dp_auth_fail=%" PRIu32 " a2dp_sink_delay_100us=%" PRIu32
               " a2dp_stack_min=%" PRIu32,
               a2dp_stats.discovered_sinks, a2dp_stats.connection_attempts, a2dp_stats.connections,
               a2dp_stats.disconnects, a2dp_stats.connection_failures, a2dp_stats.codec_rejections,
               a2dp_stats.media_start_requests, a2dp_stats.media_control_failures,
               a2dp_stats.pcm_submitted_bytes, a2dp_stats.pcm_dropped_bytes,
               a2dp_stats.pcm_callbacks, a2dp_stats.pcm_underruns, a2dp_stats.pcm_silence_bytes,
               a2dp_stats.pcm_buffer_bytes, a2dp_stats.pcm_buffer_high_water,
               a2dp_stats.avrc_connections, a2dp_stats.avrc_capability_responses,
               a2dp_stats.avrc_events, a2dp_stats.authentication_failures,
               a2dp_stats.sink_delay_tenths_ms, a2dp_stats.control_stack_high_water);
    }
#endif
#if CONFIG_MOL_USB_HID_ENABLE
    {
      const mol_usb_hid_stats_t usb_stats = mol_usb_hid_stats();
      ESP_LOGI(kTag,
               "usb_ifaces=%" PRIu32 " usb_open=%" PRIu32 " usb_reject=%" PRIu32
               " usb_disconnect=%" PRIu32 " usb_report=%" PRIu32 " usb_invalid=%" PRIu32
               " usb_transfer_err=%" PRIu32 " usb_delivery_fail=%" PRIu32
               " usb_driver_fail=%" PRIu32 " usb_queue_ovf=%" PRIu32 " usb_host_stack_min=%" PRIu32
               " usb_hid_stack_min=%" PRIu32,
               usb_stats.interfaces_seen, usb_stats.keyboards_opened, usb_stats.rejected_interfaces,
               usb_stats.disconnects, usb_stats.reports, usb_stats.invalid_reports,
               usb_stats.transfer_errors, usb_stats.delivery_failures, usb_stats.driver_failures,
               usb_stats.event_queue_overflows, usb_stats.host_stack_high_water,
               usb_stats.hid_stack_high_water);
    }
#endif
#if CONFIG_MOL_QEMU_RUNTIME
    ++qemu_diagnostic_count;
    if (qemu_diagnostic_count >= MOL_ESP32_QEMU_REQUIRED_SNAPSHOTS) {
      const bool qemu_passed =
          audio_snapshot.rendered_frames >= CONFIG_MOL_I2S_SAMPLE_RATE &&
          audio_snapshot.submitted_commands >= settings_command_count + 1u &&
          audio_snapshot.render_failures == 0u && audio_snapshot.write_failures == 0u &&
          audio_snapshot.partial_writes == 0u && audio_snapshot.watchdog_failures == 0u &&
          audio_snapshot.nonfinite_samples == 0u && audio_snapshot.nonzero_samples > 0u &&
          input_stats.dropped == 0u && input_stats.rejected == 0u &&
          storage_stats.io_failures == 0u && sequence_storage_stats.io_failures == 0u;
      if (qemu_passed) {
        ESP_LOGI(kTag,
                 "QEMU firmware smoke passed: snapshots=%" PRIu32 " frames=%" PRIu32
                 " commands=%" PRIu32 " nonzero=%" PRIu32,
                 qemu_diagnostic_count, audio_snapshot.rendered_frames,
                 audio_snapshot.submitted_commands, audio_snapshot.nonzero_samples);
      } else {
        ESP_LOGE(kTag,
                 "QEMU firmware smoke failed: snapshots=%" PRIu32 " frames=%" PRIu32
                 " commands=%" PRIu32 " nonfinite=%" PRIu32 " nonzero=%" PRIu32,
                 qemu_diagnostic_count, audio_snapshot.rendered_frames,
                 audio_snapshot.submitted_commands, audio_snapshot.nonfinite_samples,
                 audio_snapshot.nonzero_samples);
      }
      esp_restart();
    }
#endif
  }
}
