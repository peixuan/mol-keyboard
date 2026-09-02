/* SPDX-License-Identifier: Apache-2.0 */
#include "gpio_matrix.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "input_queue.h"
#include "matrix_logic.h"
#include "mol/mol.h"
#include "sdkconfig.h"

#define MOL_GPIO_SOURCE_ID UINT32_C(0x4750494f)

typedef struct mol_gpio_runtime_stats {
  atomic_uint_least32_t scans;
  atomic_uint_least32_t transitions;
  atomic_uint_least32_t ghost_scans;
  atomic_uint_least32_t delivery_failures;
  atomic_uint_least32_t config_holds;
  atomic_uint_least32_t clear_pairing_holds;
  atomic_uint_least32_t factory_reset_holds;
  atomic_uint_least32_t stack_high_water;
} mol_gpio_runtime_stats_t;

#if CONFIG_MOL_GPIO_MATRIX_ENABLE
static const gpio_num_t row_pins[MOL_MATRIX_MAX_ROWS] = {
    (gpio_num_t)CONFIG_MOL_GPIO_ROW0, (gpio_num_t)CONFIG_MOL_GPIO_ROW1,
    (gpio_num_t)CONFIG_MOL_GPIO_ROW2, (gpio_num_t)CONFIG_MOL_GPIO_ROW3,
    (gpio_num_t)CONFIG_MOL_GPIO_ROW4,
};
static const gpio_num_t column_pins[MOL_MATRIX_MAX_COLUMNS] = {
    (gpio_num_t)CONFIG_MOL_GPIO_COLUMN0, (gpio_num_t)CONFIG_MOL_GPIO_COLUMN1,
    (gpio_num_t)CONFIG_MOL_GPIO_COLUMN2, (gpio_num_t)CONFIG_MOL_GPIO_COLUMN3,
    (gpio_num_t)CONFIG_MOL_GPIO_COLUMN4, (gpio_num_t)CONFIG_MOL_GPIO_COLUMN5,
};
static StackType_t gpio_task_stack[CONFIG_MOL_GPIO_TASK_STACK_SIZE];
static StaticTask_t gpio_task_control;
static mol_matrix_state_t matrix_state;
static mol_gesture_id_t active_gestures[MOL_MATRIX_MAX_KEYS];
static uint32_t gesture_serial = 1u;
static bool recovery_needed;
#endif
static mol_gpio_runtime_stats_t runtime_stats;

#if CONFIG_MOL_GPIO_MATRIX_ENABLE
static mol_command_t command_base(mol_command_type_t type) {
  mol_command_t command;
  memset(&command, 0, sizeof(command));
  command.struct_size = (uint32_t)sizeof(command);
  command.api_version = MOL_API_VERSION;
  command.command_type = type;
  command.source_id = MOL_GPIO_SOURCE_ID;
  command.target_frame = MOL_FRAME_IMMEDIATE;
  return command;
}

static bool submit_all_notes_off(void) {
  const mol_command_t command = command_base(MOL_COMMAND_ALL_NOTES_OFF);
  if (!mol_input_submit(&command)) {
    atomic_fetch_add_explicit(&runtime_stats.delivery_failures, 1u, memory_order_relaxed);
    return false;
  }
  memset(active_gestures, 0, sizeof(active_gestures));
  return true;
}

static void deliver_matrix_event(const mol_matrix_event_t* event) {
  mol_command_t command;
  if (event->type == MOL_MATRIX_EVENT_CONFIG_HOLD) {
    atomic_fetch_add_explicit(&runtime_stats.config_holds, 1u, memory_order_relaxed);
    mol_input_request_config_mode();
    recovery_needed = !submit_all_notes_off();
    return;
  }
  if (event->type == MOL_MATRIX_EVENT_CLEAR_PAIRING_HOLD) {
    atomic_fetch_add_explicit(&runtime_stats.clear_pairing_holds, 1u, memory_order_relaxed);
    mol_input_request_clear_pairing();
    recovery_needed = !submit_all_notes_off();
    return;
  }
  if (event->type == MOL_MATRIX_EVENT_FACTORY_RESET_HOLD) {
    atomic_fetch_add_explicit(&runtime_stats.factory_reset_holds, 1u, memory_order_relaxed);
    mol_input_request_factory_reset();
    recovery_needed = !submit_all_notes_off();
    return;
  }
  command = command_base(event->type == MOL_MATRIX_EVENT_KEY_DOWN ? MOL_COMMAND_NOTE_ON
                                                                  : MOL_COMMAND_NOTE_OFF);
  command.payload.note.note = (uint8_t)(60u + event->key);
  command.payload.note.velocity = (float)CONFIG_MOL_GPIO_VELOCITY_PERCENT / 100.0f;
  if (event->type == MOL_MATRIX_EVENT_KEY_DOWN) {
    const mol_gesture_id_t gesture =
        ((mol_gesture_id_t)MOL_GPIO_SOURCE_ID << 32u) | (mol_gesture_id_t)gesture_serial;
    ++gesture_serial;
    if (gesture_serial == 0u) {
      gesture_serial = 1u;
    }
    command.gesture_id = gesture;
    if (mol_input_submit(&command)) {
      active_gestures[event->key] = gesture;
    } else {
      recovery_needed = true;
      atomic_fetch_add_explicit(&runtime_stats.delivery_failures, 1u, memory_order_relaxed);
    }
  } else {
    command.gesture_id = active_gestures[event->key];
    active_gestures[event->key] = 0u;
    if (command.gesture_id != 0u && !mol_input_submit(&command)) {
      recovery_needed = true;
      atomic_fetch_add_explicit(&runtime_stats.delivery_failures, 1u, memory_order_relaxed);
    }
  }
}

static uint32_t scan_matrix(void) {
  uint32_t bits = 0u;
  uint32_t row;
  for (row = 0u; row < MOL_MATRIX_MAX_ROWS; ++row) {
    uint32_t column;
    (void)gpio_set_level(row_pins[row], 0u);
    esp_rom_delay_us(CONFIG_MOL_GPIO_SETTLE_US);
    for (column = 0u; column < MOL_MATRIX_MAX_COLUMNS; ++column) {
      if (gpio_get_level(column_pins[column]) == 0) {
        bits |= UINT32_C(1) << (row * MOL_MATRIX_MAX_COLUMNS + column);
      }
    }
    (void)gpio_set_level(row_pins[row], 1u);
  }
  return bits;
}

static bool pin_conflicts_with_audio(gpio_num_t pin) {
  return pin == (gpio_num_t)CONFIG_MOL_I2S_BCLK_GPIO || pin == (gpio_num_t)CONFIG_MOL_I2S_WS_GPIO ||
         pin == (gpio_num_t)CONFIG_MOL_I2S_DOUT_GPIO ||
         (CONFIG_MOL_I2S_MCLK_GPIO >= 0 && pin == (gpio_num_t)CONFIG_MOL_I2S_MCLK_GPIO);
}

static bool pins_are_valid(void) {
  uint32_t index;
  for (index = 0u; index < MOL_MATRIX_MAX_ROWS; ++index) {
    uint32_t previous;
    if (!GPIO_IS_VALID_OUTPUT_GPIO(row_pins[index]) || pin_conflicts_with_audio(row_pins[index])) {
      return false;
    }
    for (previous = 0u; previous < index; ++previous) {
      if (row_pins[index] == row_pins[previous]) {
        return false;
      }
    }
  }
  for (index = 0u; index < MOL_MATRIX_MAX_COLUMNS; ++index) {
    uint32_t previous;
    if (!GPIO_IS_VALID_GPIO(column_pins[index]) || pin_conflicts_with_audio(column_pins[index])) {
      return false;
    }
    for (previous = 0u; previous < MOL_MATRIX_MAX_ROWS; ++previous) {
      if (column_pins[index] == row_pins[previous]) {
        return false;
      }
    }
    for (previous = 0u; previous < index; ++previous) {
      if (column_pins[index] == column_pins[previous]) {
        return false;
      }
    }
  }
  return true;
}

static void gpio_scan_task(void* context) {
  TickType_t last_wake = xTaskGetTickCount();
  (void)context;
  for (;;) {
    mol_matrix_event_t events[MOL_MATRIX_MAX_EVENTS];
    size_t event_count = 0u;
    size_t index;
    bool ghost_detected = false;
    const uint32_t raw_bits = scan_matrix();
    const mol_matrix_result_t result = mol_matrix_process(
        &matrix_state, raw_bits, events, MOL_MATRIX_MAX_EVENTS, &event_count, &ghost_detected);
    atomic_fetch_add_explicit(&runtime_stats.scans, 1u, memory_order_relaxed);
    if (ghost_detected) {
      atomic_fetch_add_explicit(&runtime_stats.ghost_scans, 1u, memory_order_relaxed);
    }
    if (result == MOL_MATRIX_OK) {
      for (index = 0u; index < event_count; ++index) {
        deliver_matrix_event(&events[index]);
        atomic_fetch_add_explicit(&runtime_stats.transitions, 1u, memory_order_relaxed);
      }
    }
    if (recovery_needed && submit_all_notes_off()) {
      recovery_needed = false;
    }
    atomic_store_explicit(&runtime_stats.stack_high_water,
                          (uint_least32_t)uxTaskGetStackHighWaterMark(NULL), memory_order_relaxed);
    xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CONFIG_MOL_GPIO_SCAN_PERIOD_MS));
  }
}

static esp_err_t configure_pins(void) {
  gpio_config_t row_config = {
      .pin_bit_mask = 0u,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config_t column_config = {
      .pin_bit_mask = 0u,
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  uint32_t index;
  esp_err_t result;
  if (!pins_are_valid()) {
    return ESP_ERR_INVALID_ARG;
  }
  for (index = 0u; index < MOL_MATRIX_MAX_ROWS; ++index) {
    row_config.pin_bit_mask |= UINT64_C(1) << (uint32_t)row_pins[index];
  }
  for (index = 0u; index < MOL_MATRIX_MAX_COLUMNS; ++index) {
    column_config.pin_bit_mask |= UINT64_C(1) << (uint32_t)column_pins[index];
  }
  result = gpio_config(&row_config);
  if (result != ESP_OK) {
    return result;
  }
  result = gpio_config(&column_config);
  if (result != ESP_OK) {
    return result;
  }
  for (index = 0u; index < MOL_MATRIX_MAX_ROWS; ++index) {
    result = gpio_set_level(row_pins[index], 1u);
    if (result != ESP_OK) {
      return result;
    }
  }
  return ESP_OK;
}

esp_err_t mol_gpio_matrix_start(void) {
  mol_matrix_config_t config = {
      .rows = MOL_MATRIX_MAX_ROWS,
      .columns = MOL_MATRIX_MAX_COLUMNS,
      .config_key = CONFIG_MOL_GPIO_CONFIG_KEY,
      .clear_pairing_key = CONFIG_MOL_GPIO_CLEAR_PAIRING_KEY,
      .factory_reset_key = CONFIG_MOL_GPIO_FACTORY_RESET_KEY,
#if CONFIG_MOL_GPIO_GHOST_ALLOW
      .ghost_policy = MOL_MATRIX_GHOST_ALLOW,
#else
      .ghost_policy = MOL_MATRIX_GHOST_SUPPRESS_AMBIGUOUS,
#endif
      .debounce_scans =
          (uint16_t)((CONFIG_MOL_GPIO_DEBOUNCE_MS + CONFIG_MOL_GPIO_SCAN_PERIOD_MS - 1) /
                     CONFIG_MOL_GPIO_SCAN_PERIOD_MS),
      .config_hold_scans =
          (uint16_t)((CONFIG_MOL_GPIO_CONFIG_HOLD_MS + CONFIG_MOL_GPIO_SCAN_PERIOD_MS - 1) /
                     CONFIG_MOL_GPIO_SCAN_PERIOD_MS),
      .clear_pairing_hold_scans =
          (uint16_t)((CONFIG_MOL_GPIO_CLEAR_PAIRING_HOLD_MS + CONFIG_MOL_GPIO_SCAN_PERIOD_MS - 1) /
                     CONFIG_MOL_GPIO_SCAN_PERIOD_MS),
      .factory_reset_hold_scans =
          (uint16_t)((CONFIG_MOL_GPIO_FACTORY_RESET_HOLD_MS + CONFIG_MOL_GPIO_SCAN_PERIOD_MS - 1) /
                     CONFIG_MOL_GPIO_SCAN_PERIOD_MS),
  };
  esp_err_t result = configure_pins();
  if (result != ESP_OK) {
    return result;
  }
  if (mol_matrix_init(&matrix_state, &config) != MOL_MATRIX_OK) {
    return ESP_ERR_INVALID_ARG;
  }
  memset(active_gestures, 0, sizeof(active_gestures));
  if (xTaskCreateStaticPinnedToCore(gpio_scan_task, "mol-gpio", CONFIG_MOL_GPIO_TASK_STACK_SIZE,
                                    NULL, CONFIG_MOL_GPIO_TASK_PRIORITY, gpio_task_stack,
                                    &gpio_task_control, CONFIG_MOL_GPIO_TASK_CORE) == NULL) {
    return ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}
#else
esp_err_t mol_gpio_matrix_start(void) { return ESP_ERR_NOT_SUPPORTED; }
#endif

mol_gpio_matrix_stats_t mol_gpio_matrix_stats(void) {
  mol_gpio_matrix_stats_t stats;
  stats.scans = (uint32_t)atomic_load_explicit(&runtime_stats.scans, memory_order_relaxed);
  stats.transitions =
      (uint32_t)atomic_load_explicit(&runtime_stats.transitions, memory_order_relaxed);
  stats.ghost_scans =
      (uint32_t)atomic_load_explicit(&runtime_stats.ghost_scans, memory_order_relaxed);
  stats.delivery_failures =
      (uint32_t)atomic_load_explicit(&runtime_stats.delivery_failures, memory_order_relaxed);
  stats.config_holds =
      (uint32_t)atomic_load_explicit(&runtime_stats.config_holds, memory_order_relaxed);
  stats.clear_pairing_holds =
      (uint32_t)atomic_load_explicit(&runtime_stats.clear_pairing_holds, memory_order_relaxed);
  stats.factory_reset_holds =
      (uint32_t)atomic_load_explicit(&runtime_stats.factory_reset_holds, memory_order_relaxed);
  stats.stack_high_water =
      (uint32_t)atomic_load_explicit(&runtime_stats.stack_high_water, memory_order_relaxed);
  return stats;
}
