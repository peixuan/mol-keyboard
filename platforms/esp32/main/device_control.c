/* SPDX-License-Identifier: Apache-2.0 */
#include "device_control.h"

#include <inttypes.h>
#include <stdatomic.h>
#include <string.h>

#if CONFIG_MOL_A2DP_ENABLE
#include "a2dp_source.h"
#endif
#include "bluetooth_hid.h"
#include "device_storage.h"
#if CONFIG_MOL_DEVICE_WEB_UI_ENABLE
#include "device_web.h"
#endif
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "input_queue.h"
#include "sequence_storage.h"

#define MOL_DEVICE_CONTROL_SOURCE_ID UINT32_C(0x43464731)

typedef struct mol_device_control_atomic_stats {
  atomic_uint_least32_t configuration_entries;
  atomic_uint_least32_t settings_applied;
  atomic_uint_least32_t settings_rejected;
  atomic_uint_least32_t queue_rejections;
  atomic_uint_least32_t settings_saves;
  atomic_uint_least32_t persistence_failures;
  atomic_uint_least32_t peer_updates;
  atomic_uint_least32_t clear_pairing_operations;
  atomic_uint_least32_t factory_reset_operations;
  atomic_uint_least32_t bond_removal_requests;
} mol_device_control_atomic_stats_t;

static const char* const kTag = "mol-control";
static StaticQueue_t settings_queue_control;
static uint8_t
    settings_queue_storage[CONFIG_MOL_DEVICE_CONTROL_QUEUE_LENGTH * sizeof(mol_device_settings_t)]
    __attribute__((aligned(portBYTE_ALIGNMENT)));
static QueueHandle_t settings_queue;
static StaticTask_t control_task_control;
static StackType_t control_task_stack[CONFIG_MOL_DEVICE_CONTROL_TASK_STACK_SIZE];
static TaskHandle_t control_task_handle;
static portMUX_TYPE settings_lock = portMUX_INITIALIZER_UNLOCKED;
static mol_device_settings_t current_settings;
static bool storage_is_ready;
static bool sequence_storage_is_ready;
static bool bluetooth_is_ready;
static atomic_bool started;
static atomic_bool configuration_active;
static mol_device_control_atomic_stats_t stats;

static uint32_t next_generation(uint32_t generation) {
  return generation == UINT32_MAX ? 1u : generation + 1u;
}

static mol_command_t all_sound_off_command(void) {
  mol_command_t command;
  memset(&command, 0, sizeof(command));
  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = MOL_COMMAND_ALL_SOUND_OFF;
  command.source_id = MOL_DEVICE_CONTROL_SOURCE_ID;
  command.target_frame = MOL_FRAME_IMMEDIATE;
  return command;
}

static void publish_settings(const mol_device_settings_t* settings) {
  portENTER_CRITICAL(&settings_lock);
  current_settings = *settings;
  portEXIT_CRITICAL(&settings_lock);
}

static void save_settings(const mol_device_settings_t* settings) {
  if (!storage_is_ready) {
    return;
  }
  if (mol_device_storage_save_settings(settings) == MOL_OK) {
    atomic_fetch_add_explicit(&stats.settings_saves, 1u, memory_order_relaxed);
  } else {
    atomic_fetch_add_explicit(&stats.persistence_failures, 1u, memory_order_relaxed);
    ESP_LOGW(kTag, "Could not persist settings generation=%" PRIu32, settings->generation);
  }
}

static bool queue_settings_commands(const mol_device_settings_t* settings) {
  mol_command_t commands[MOL_DEVICE_SETTINGS_COMMAND_COUNT];
  size_t count = 0u;
  size_t index;
  if (mol_device_settings_compile_commands(settings, commands, MOL_DEVICE_SETTINGS_COMMAND_COUNT,
                                           &count) != MOL_OK) {
    return false;
  }
  for (index = 0u; index < count; ++index) {
    if (!mol_input_submit(&commands[index])) {
      return false;
    }
  }
  return true;
}

static void apply_candidate(const mol_device_settings_t* requested) {
  mol_device_settings_t applied = *requested;
  mol_device_settings_t previous;
  (void)mol_device_control_get_settings(&previous);
#if !CONFIG_MOL_A2DP_ENABLE
  if (applied.output_mode == MOL_DEVICE_OUTPUT_A2DP) {
    atomic_fetch_add_explicit(&stats.settings_rejected, 1u, memory_order_relaxed);
    ESP_LOGW(kTag, "Rejected A2DP output on a target without A2DP Source support");
    return;
  }
#endif
  applied.generation = next_generation(previous.generation);
  applied.paired_peer_valid = previous.paired_peer_valid;
  memcpy(applied.paired_peer_address, previous.paired_peer_address,
         sizeof(applied.paired_peer_address));
  applied.a2dp_sink_valid = previous.a2dp_sink_valid;
  memcpy(applied.a2dp_sink_address, previous.a2dp_sink_address, sizeof(applied.a2dp_sink_address));
  if (mol_device_settings_validate(&applied) != MOL_OK || !queue_settings_commands(&applied)) {
    atomic_fetch_add_explicit(&stats.settings_rejected, 1u, memory_order_relaxed);
    ESP_LOGW(kTag, "Rejected invalid settings or a saturated audio command queue");
    return;
  }
#if CONFIG_MOL_A2DP_ENABLE
  mol_a2dp_source_set_enabled(applied.output_mode == MOL_DEVICE_OUTPUT_A2DP);
#endif
  publish_settings(&applied);
  save_settings(&applied);
  atomic_fetch_add_explicit(&stats.settings_applied, 1u, memory_order_relaxed);
  ESP_LOGI(kTag, "Applied settings generation=%" PRIu32, applied.generation);
}

static void update_peer(const uint8_t address[6], bool a2dp) {
  mol_device_settings_t updated;
  (void)mol_device_control_get_settings(&updated);
  if (a2dp) {
    updated.a2dp_sink_valid = 1u;
    memcpy(updated.a2dp_sink_address, address, sizeof(updated.a2dp_sink_address));
  } else {
    updated.paired_peer_valid = 1u;
    memcpy(updated.paired_peer_address, address, sizeof(updated.paired_peer_address));
  }
  updated.generation = next_generation(updated.generation);
  publish_settings(&updated);
  save_settings(&updated);
  atomic_fetch_add_explicit(&stats.peer_updates, 1u, memory_order_relaxed);
  ESP_LOGI(kTag, "Persisted %s peer %02x:%02x:%02x:%02x:%02x:%02x generation=%" PRIu32,
           a2dp ? "A2DP" : "HID", address[0], address[1], address[2], address[3], address[4],
           address[5], updated.generation);
}

static void forget_pairing(void) {
  mol_device_settings_t updated;
  uint32_t removals = 0u;
  esp_err_t result = ESP_OK;
  const mol_command_t silence = all_sound_off_command();
  (void)mol_input_submit(&silence);
  if (bluetooth_is_ready) {
    result = mol_bluetooth_hid_clear_bonds(&removals);
  }
  mol_bluetooth_hid_forget_preferred();
#if CONFIG_MOL_A2DP_ENABLE
  mol_a2dp_source_forget_preferred();
#endif
  (void)mol_device_control_get_settings(&updated);
  updated.paired_peer_valid = 0u;
  memset(updated.paired_peer_address, 0, sizeof(updated.paired_peer_address));
  updated.a2dp_sink_valid = 0u;
  memset(updated.a2dp_sink_address, 0, sizeof(updated.a2dp_sink_address));
  updated.generation = next_generation(updated.generation);
  publish_settings(&updated);
  save_settings(&updated);
  atomic_fetch_add_explicit(&stats.clear_pairing_operations, 1u, memory_order_relaxed);
  atomic_fetch_add_explicit(&stats.bond_removal_requests, removals, memory_order_relaxed);
  if (result == ESP_OK) {
    ESP_LOGI(kTag, "Physical clear-pairing completed; removal requests=%" PRIu32, removals);
  } else {
    ESP_LOGW(kTag, "Physical clear-pairing completed with Bluetooth error: %s",
             esp_err_to_name(result));
  }
}

static void restore_factory_defaults(void) {
  const mol_device_settings_t defaults = mol_device_settings_default();
  uint32_t ignored_removals = 0u;
  const mol_command_t silence = all_sound_off_command();
  (void)mol_input_submit(&silence);
  if (bluetooth_is_ready) {
    (void)mol_bluetooth_hid_clear_bonds(&ignored_removals);
  }
  mol_bluetooth_hid_forget_preferred();
#if CONFIG_MOL_A2DP_ENABLE
  mol_a2dp_source_forget_preferred();
#endif
#if CONFIG_MOL_DEVICE_WEB_UI_ENABLE
  if (mol_device_web_erase_credentials() != ESP_OK) {
    atomic_fetch_add_explicit(&stats.persistence_failures, 1u, memory_order_relaxed);
  }
#endif
  if (storage_is_ready && mol_device_storage_erase_settings() != MOL_OK) {
    atomic_fetch_add_explicit(&stats.persistence_failures, 1u, memory_order_relaxed);
  }
  if (sequence_storage_is_ready && mol_sequence_storage_erase_all() != MOL_OK) {
    atomic_fetch_add_explicit(&stats.persistence_failures, 1u, memory_order_relaxed);
  }
  publish_settings(&defaults);
  atomic_fetch_add_explicit(&stats.factory_reset_operations, 1u, memory_order_relaxed);
  atomic_fetch_add_explicit(&stats.bond_removal_requests, ignored_removals, memory_order_relaxed);
  ESP_LOGW(kTag, "Physical factory reset completed; restarting into safe defaults");
  vTaskDelay(pdMS_TO_TICKS(250u));
  esp_restart();
}

static void control_task(void* context) {
  (void)context;
  for (;;) {
    mol_device_settings_t candidate;
    uint8_t address[6];
    if (mol_input_take_factory_reset_request()) {
      restore_factory_defaults();
    }
    if (mol_input_take_clear_pairing_request()) {
      forget_pairing();
    }
    if (mol_input_take_config_mode_request()) {
      atomic_store_explicit(&configuration_active, true, memory_order_release);
      atomic_fetch_add_explicit(&stats.configuration_entries, 1u, memory_order_relaxed);
#if CONFIG_MOL_DEVICE_WEB_UI_ENABLE
      {
        mol_device_settings_t settings;
        (void)mol_device_control_get_settings(&settings);
        if (settings.web_ui_enabled == 0u || mol_device_web_start() != ESP_OK) {
          atomic_store_explicit(&configuration_active, false, memory_order_release);
        }
      }
#else
      atomic_store_explicit(&configuration_active, false, memory_order_release);
      ESP_LOGI(kTag, "Physical configuration request received; Web UI is not in this build");
#endif
    }
    if (bluetooth_is_ready && mol_bluetooth_hid_take_new_peer(address)) {
      update_peer(address, false);
    }
#if CONFIG_MOL_A2DP_ENABLE
    if (bluetooth_is_ready && mol_a2dp_source_take_new_peer(address)) {
      update_peer(address, true);
    }
#endif
    if (xQueueReceive(settings_queue, &candidate, pdMS_TO_TICKS(50u)) == pdPASS) {
      apply_candidate(&candidate);
    }
#if CONFIG_MOL_DEVICE_WEB_UI_ENABLE
    if (atomic_load_explicit(&configuration_active, memory_order_acquire) &&
        !mol_device_web_poll()) {
      atomic_store_explicit(&configuration_active, false, memory_order_release);
    }
#endif
  }
}

esp_err_t mol_device_control_start(const mol_device_settings_t* initial_settings,
                                   bool storage_ready, bool sequence_storage_ready,
                                   bool bluetooth_ready) {
  bool expected = false;
  if (initial_settings == NULL || mol_device_settings_validate(initial_settings) != MOL_OK) {
    return ESP_ERR_INVALID_ARG;
  }
  if (!atomic_compare_exchange_strong_explicit(&started, &expected, true, memory_order_acq_rel,
                                               memory_order_acquire)) {
    return ESP_ERR_INVALID_STATE;
  }
  storage_is_ready = storage_ready;
  sequence_storage_is_ready = sequence_storage_ready;
  bluetooth_is_ready = bluetooth_ready;
  publish_settings(initial_settings);
  settings_queue =
      xQueueCreateStatic(CONFIG_MOL_DEVICE_CONTROL_QUEUE_LENGTH, sizeof(mol_device_settings_t),
                         settings_queue_storage, &settings_queue_control);
  if (settings_queue == NULL) {
    atomic_store_explicit(&started, false, memory_order_release);
    return ESP_ERR_NO_MEM;
  }
  control_task_handle = xTaskCreateStaticPinnedToCore(
      control_task, "mol-control", CONFIG_MOL_DEVICE_CONTROL_TASK_STACK_SIZE, NULL,
      CONFIG_MOL_DEVICE_CONTROL_TASK_PRIORITY, control_task_stack, &control_task_control,
      CONFIG_MOL_DEVICE_CONTROL_TASK_CORE);
  if (control_task_handle == NULL) {
    atomic_store_explicit(&started, false, memory_order_release);
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

bool mol_device_control_submit_settings(const mol_device_settings_t* candidate) {
  if (candidate == NULL || settings_queue == NULL ||
      xQueueSendToBack(settings_queue, candidate, 0u) != pdPASS) {
    atomic_fetch_add_explicit(&stats.queue_rejections, 1u, memory_order_relaxed);
    return false;
  }
  return true;
}

bool mol_device_control_get_settings(mol_device_settings_t* settings) {
  if (settings == NULL || !atomic_load_explicit(&started, memory_order_acquire)) {
    return false;
  }
  portENTER_CRITICAL(&settings_lock);
  *settings = current_settings;
  portEXIT_CRITICAL(&settings_lock);
  return true;
}

bool mol_device_control_configuration_active(void) {
  return atomic_load_explicit(&configuration_active, memory_order_acquire);
}

mol_device_control_stats_t mol_device_control_stats(void) {
  mol_device_control_stats_t snapshot;
  snapshot.configuration_entries =
      (uint32_t)atomic_load_explicit(&stats.configuration_entries, memory_order_relaxed);
  snapshot.settings_applied =
      (uint32_t)atomic_load_explicit(&stats.settings_applied, memory_order_relaxed);
  snapshot.settings_rejected =
      (uint32_t)atomic_load_explicit(&stats.settings_rejected, memory_order_relaxed);
  snapshot.queue_rejections =
      (uint32_t)atomic_load_explicit(&stats.queue_rejections, memory_order_relaxed);
  snapshot.settings_saves =
      (uint32_t)atomic_load_explicit(&stats.settings_saves, memory_order_relaxed);
  snapshot.persistence_failures =
      (uint32_t)atomic_load_explicit(&stats.persistence_failures, memory_order_relaxed);
  snapshot.peer_updates = (uint32_t)atomic_load_explicit(&stats.peer_updates, memory_order_relaxed);
  snapshot.clear_pairing_operations =
      (uint32_t)atomic_load_explicit(&stats.clear_pairing_operations, memory_order_relaxed);
  snapshot.factory_reset_operations =
      (uint32_t)atomic_load_explicit(&stats.factory_reset_operations, memory_order_relaxed);
  snapshot.bond_removal_requests =
      (uint32_t)atomic_load_explicit(&stats.bond_removal_requests, memory_order_relaxed);
  snapshot.stack_high_water =
      control_task_handle != NULL ? (uint32_t)uxTaskGetStackHighWaterMark(control_task_handle) : 0u;
  return snapshot;
}
