/* SPDX-License-Identifier: Apache-2.0 */
#include "bluetooth_hid.h"

#include <inttypes.h>
#include <stdatomic.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"
#include "esp_hidh.h"
#include "esp_hidh_bluedroid.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "hid_keyboard.h"
#include "input_queue.h"
#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32
#include "esp_gap_bt_api.h"
#endif

#define MOL_HID_SOURCE_ID UINT32_C(0x424c0001)
#define MOL_HID_SCAN_SECONDS 5u
#define MOL_HID_BLE_PARAMS_READY BIT0
#define MOL_HID_BLE_SCAN_DONE BIT1
#define MOL_HID_CLASSIC_SCAN_DONE BIT2
#define MOL_HID_CONNECTION_CHANGED BIT3

typedef struct mol_bluetooth_hid_atomic_stats {
  atomic_uint_least32_t ble_scans;
  atomic_uint_least32_t classic_scans;
  atomic_uint_least32_t open_attempts;
  atomic_uint_least32_t connections;
  atomic_uint_least32_t disconnects;
  atomic_uint_least32_t reports;
  atomic_uint_least32_t invalid_reports;
  atomic_uint_least32_t delivery_failures;
} mol_bluetooth_hid_atomic_stats_t;

static const char* const kTag = "mol-bt-hid";
static StaticEventGroup_t scan_events_control;
static EventGroupHandle_t scan_events;
static StaticTask_t scan_task_control;
static StackType_t scan_task_stack[CONFIG_MOL_BLUETOOTH_HID_TASK_STACK_SIZE];
static TaskHandle_t scan_task_handle;
static atomic_bool connected;
static atomic_bool opening;
static bool preferred_peer_valid;
static uint8_t preferred_peer[6];
static mol_hid_keyboard_state_t keyboard_state;
static mol_bluetooth_hid_atomic_stats_t stats;

static esp_ble_scan_params_t ble_scan_parameters = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x30,
    .scan_duplicate = BLE_SCAN_DUPLICATE_ENABLE,
};

static bool peer_is_allowed(const uint8_t address[6]) {
  return !preferred_peer_valid || memcmp(address, preferred_peer, sizeof(preferred_peer)) == 0;
}

static bool advertisement_has_hid_service(const uint8_t* data, uint8_t data_length) {
  uint8_t field_length = 0u;
  uint8_t* field = esp_ble_resolve_adv_data_by_type((uint8_t*)data, data_length,
                                                    ESP_BLE_AD_TYPE_16SRV_CMPL, &field_length);
  size_t index;
  if (field == NULL) {
    field = esp_ble_resolve_adv_data_by_type((uint8_t*)data, data_length,
                                             ESP_BLE_AD_TYPE_16SRV_PART, &field_length);
  }
  for (index = 0u; field != NULL && index + 1u < field_length; index += 2u) {
    if (((uint16_t)field[index] | ((uint16_t)field[index + 1u] << 8u)) == ESP_GATT_UUID_HID_SVC) {
      return true;
    }
  }
  return false;
}

static void open_device(const uint8_t address[6], esp_hid_transport_t transport,
                        uint8_t address_type) {
  bool expected = false;
  if (!peer_is_allowed(address) || atomic_load_explicit(&connected, memory_order_acquire) ||
      !atomic_compare_exchange_strong_explicit(&opening, &expected, true, memory_order_acq_rel,
                                               memory_order_acquire)) {
    return;
  }
  atomic_fetch_add_explicit(&stats.open_attempts, 1u, memory_order_relaxed);
  if (esp_hidh_dev_open((uint8_t*)address, transport, address_type) == NULL) {
    atomic_store_explicit(&opening, false, memory_order_release);
  }
}

static void submit_keyboard_commands(const mol_command_t* commands, size_t command_count) {
  size_t index;
  for (index = 0u; index < command_count; ++index) {
    if (!mol_input_submit(&commands[index])) {
      atomic_fetch_add_explicit(&stats.delivery_failures, 1u, memory_order_relaxed);
    }
  }
}

static void hidh_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id,
                               void* event_data) {
  esp_hidh_event_data_t* event = (esp_hidh_event_data_t*)event_data;
  (void)handler_args;
  (void)base;
  switch ((esp_hidh_event_t)event_id) {
    case ESP_HIDH_OPEN_EVENT:
      if (event->open.status == ESP_OK) {
        const uint8_t* address = esp_hidh_dev_bda_get(event->open.dev);
        atomic_store_explicit(&connected, true, memory_order_release);
        atomic_fetch_add_explicit(&stats.connections, 1u, memory_order_relaxed);
        if (address != NULL) {
          ESP_LOGI(kTag, "keyboard connected: %02x:%02x:%02x:%02x:%02x:%02x transport=%d",
                   address[0], address[1], address[2], address[3], address[4], address[5],
                   (int)esp_hidh_dev_transport_get(event->open.dev));
        }
      } else {
        atomic_fetch_add_explicit(&stats.delivery_failures, 1u, memory_order_relaxed);
      }
      atomic_store_explicit(&opening, false, memory_order_release);
      xEventGroupSetBits(scan_events, MOL_HID_CONNECTION_CHANGED);
      break;
    case ESP_HIDH_INPUT_EVENT:
      if (event->input.usage == ESP_HID_USAGE_KEYBOARD) {
        mol_command_t commands[MOL_HID_MAX_REPORT_COMMANDS];
        size_t command_count = 0u;
        mol_hid_keyboard_result_t result =
            mol_hid_keyboard_process(&keyboard_state, event->input.data, event->input.length,
                                     commands, MOL_HID_MAX_REPORT_COMMANDS, &command_count);
        atomic_fetch_add_explicit(&stats.reports, 1u, memory_order_relaxed);
        if (result == MOL_HID_KEYBOARD_OK) {
          submit_keyboard_commands(commands, command_count);
        } else {
          atomic_fetch_add_explicit(&stats.invalid_reports, 1u, memory_order_relaxed);
        }
      }
      break;
    case ESP_HIDH_CLOSE_EVENT: {
      mol_command_t commands[2];
      size_t command_count = 0u;
      if (mol_hid_keyboard_disconnect(&keyboard_state, commands, 2u, &command_count) ==
          MOL_HID_KEYBOARD_OK) {
        submit_keyboard_commands(commands, command_count);
      }
      atomic_store_explicit(&connected, false, memory_order_release);
      atomic_store_explicit(&opening, false, memory_order_release);
      atomic_fetch_add_explicit(&stats.disconnects, 1u, memory_order_relaxed);
      ESP_LOGW(kTag, "keyboard disconnected: reason=%d status=%s", event->close.reason,
               esp_err_to_name(event->close.status));
      (void)esp_hidh_dev_free(event->close.dev);
      xEventGroupSetBits(scan_events, MOL_HID_CONNECTION_CHANGED);
      break;
    }
    default:
      break;
  }
}

static void ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* parameter) {
  if (event == ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT) {
    xEventGroupSetBits(scan_events, MOL_HID_BLE_PARAMS_READY);
  } else if (event == ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT) {
    xEventGroupSetBits(scan_events, MOL_HID_BLE_SCAN_DONE);
  } else if (event == ESP_GAP_BLE_SCAN_RESULT_EVT) {
    if (parameter->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
      xEventGroupSetBits(scan_events, MOL_HID_BLE_SCAN_DONE);
    } else if (parameter->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT &&
               advertisement_has_hid_service(
                   parameter->scan_rst.ble_adv,
                   parameter->scan_rst.adv_data_len + parameter->scan_rst.scan_rsp_len)) {
      open_device(parameter->scan_rst.bda, ESP_HID_TRANSPORT_BLE,
                  (uint8_t)parameter->scan_rst.ble_addr_type);
      if (atomic_load_explicit(&opening, memory_order_acquire)) {
        (void)esp_ble_gap_stop_scanning();
      }
    }
  } else if (event == ESP_GAP_BLE_SEC_REQ_EVT) {
    (void)esp_ble_gap_security_rsp(parameter->ble_security.ble_req.bd_addr, true);
  } else if (event == ESP_GAP_BLE_NC_REQ_EVT) {
    (void)esp_ble_confirm_reply(parameter->ble_security.key_notif.bd_addr, true);
  } else if (event == ESP_GAP_BLE_AUTH_CMPL_EVT && !parameter->ble_security.auth_cmpl.success) {
    ESP_LOGW(kTag, "BLE authentication failed: 0x%x",
             parameter->ble_security.auth_cmpl.fail_reason);
  }
}

#if CONFIG_IDF_TARGET_ESP32
static bool classic_result_is_keyboard(const esp_bt_gap_cb_param_t* parameter) {
  int property_index;
  for (property_index = 0; property_index < parameter->disc_res.num_prop; ++property_index) {
    const esp_bt_gap_dev_prop_t* property = &parameter->disc_res.prop[property_index];
    if (property->type == ESP_BT_GAP_DEV_PROP_COD && property->len == sizeof(uint32_t)) {
      const uint32_t cod = *(const uint32_t*)property->val;
      const uint32_t minor = esp_bt_gap_get_cod_minor_dev(cod);
      return esp_bt_gap_get_cod_major_dev(cod) == ESP_BT_COD_MAJOR_DEV_PERIPHERAL &&
             (minor & ESP_BT_COD_MINOR_PERIPHERAL_KEYBOARD) != 0u;
    }
  }
  return false;
}

static void classic_gap_event_handler(esp_bt_gap_cb_event_t event,
                                      esp_bt_gap_cb_param_t* parameter) {
  if (event == ESP_BT_GAP_DISC_RES_EVT && classic_result_is_keyboard(parameter)) {
    open_device(parameter->disc_res.bda, ESP_HID_TRANSPORT_BT, 0u);
    if (atomic_load_explicit(&opening, memory_order_acquire)) {
      (void)esp_bt_gap_cancel_discovery();
    }
  } else if (event == ESP_BT_GAP_DISC_STATE_CHANGED_EVT &&
             parameter->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
    xEventGroupSetBits(scan_events, MOL_HID_CLASSIC_SCAN_DONE);
  } else if (event == ESP_BT_GAP_CFM_REQ_EVT) {
    (void)esp_bt_gap_ssp_confirm_reply(parameter->cfm_req.bda, true);
  } else if (event == ESP_BT_GAP_PIN_REQ_EVT) {
    esp_bt_pin_code_t pin = {0u};
    uint32_t pin_value = UINT32_C(100000) + esp_random() % UINT32_C(900000);
    size_t index;
    ESP_LOGI(kTag, "type pairing PIN %06" PRIu32 " on the keyboard, then press Enter", pin_value);
    for (index = 0u; index < 6u; ++index) {
      pin[5u - index] = (uint8_t)('0' + pin_value % 10u);
      pin_value /= 10u;
    }
    (void)esp_bt_gap_pin_reply(parameter->pin_req.bda, true, 6u, pin);
  }
}
#endif

static void scan_task(void* context) {
  (void)context;
  for (;;) {
    if (atomic_load_explicit(&connected, memory_order_acquire) ||
        atomic_load_explicit(&opening, memory_order_acquire)) {
      (void)xEventGroupWaitBits(scan_events, MOL_HID_CONNECTION_CHANGED, pdTRUE, pdFALSE,
                                pdMS_TO_TICKS(1000u));
      continue;
    }

    xEventGroupClearBits(scan_events, MOL_HID_BLE_PARAMS_READY | MOL_HID_BLE_SCAN_DONE);
    if (esp_ble_gap_set_scan_params(&ble_scan_parameters) == ESP_OK &&
        (xEventGroupWaitBits(scan_events, MOL_HID_BLE_PARAMS_READY, pdTRUE, pdTRUE,
                             pdMS_TO_TICKS(2000u)) &
         MOL_HID_BLE_PARAMS_READY) != 0u &&
        esp_ble_gap_start_scanning(MOL_HID_SCAN_SECONDS) == ESP_OK) {
      atomic_fetch_add_explicit(&stats.ble_scans, 1u, memory_order_relaxed);
      (void)xEventGroupWaitBits(scan_events, MOL_HID_BLE_SCAN_DONE, pdTRUE, pdTRUE,
                                pdMS_TO_TICKS((MOL_HID_SCAN_SECONDS + 2u) * 1000u));
    } else {
      atomic_fetch_add_explicit(&stats.delivery_failures, 1u, memory_order_relaxed);
    }

#if CONFIG_IDF_TARGET_ESP32
    if (!atomic_load_explicit(&connected, memory_order_acquire) &&
        !atomic_load_explicit(&opening, memory_order_acquire)) {
      xEventGroupClearBits(scan_events, MOL_HID_CLASSIC_SCAN_DONE);
      if (esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 4u, 0u) == ESP_OK) {
        atomic_fetch_add_explicit(&stats.classic_scans, 1u, memory_order_relaxed);
        (void)xEventGroupWaitBits(scan_events, MOL_HID_CLASSIC_SCAN_DONE, pdTRUE, pdTRUE,
                                  pdMS_TO_TICKS(7000u));
      } else {
        atomic_fetch_add_explicit(&stats.delivery_failures, 1u, memory_order_relaxed);
      }
    }
#endif
    vTaskDelay(pdMS_TO_TICKS(500u));
  }
}

static esp_err_t configure_security(void) {
  esp_ble_auth_req_t authentication = ESP_LE_AUTH_BOND;
  esp_ble_io_cap_t io_capability = ESP_IO_CAP_NONE;
  uint8_t key_mask = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t key_size = 16u;
  esp_err_t result;
  result = esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &authentication,
                                          sizeof(authentication));
  if (result == ESP_OK) {
    result = esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &io_capability,
                                            sizeof(io_capability));
  }
  if (result == ESP_OK) {
    result = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &key_mask, sizeof(key_mask));
  }
  if (result == ESP_OK) {
    result = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &key_mask, sizeof(key_mask));
  }
  if (result == ESP_OK) {
    result = esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(key_size));
  }
  return result;
}

esp_err_t mol_bluetooth_hid_start(const uint8_t preferred_address[6], bool preferred_valid) {
  esp_bt_controller_config_t controller_config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  esp_bluedroid_config_t bluedroid_config = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
  esp_hidh_config_t hidh_config = {
      .callback = hidh_event_handler,
      .event_stack_size = CONFIG_MOL_BLUETOOTH_HID_EVENT_STACK_SIZE,
      .callback_arg = NULL,
  };
  esp_err_t result;

  if (preferred_valid && preferred_address == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  preferred_peer_valid = preferred_valid;
  if (preferred_valid) {
    memcpy(preferred_peer, preferred_address, sizeof(preferred_peer));
  } else {
    memset(preferred_peer, 0, sizeof(preferred_peer));
  }
  mol_hid_keyboard_init(&keyboard_state, MOL_HID_SOURCE_ID);
  scan_events = xEventGroupCreateStatic(&scan_events_control);
  if (scan_events == NULL) {
    return ESP_ERR_NO_MEM;
  }

#if CONFIG_IDF_TARGET_ESP32
  controller_config.mode = ESP_BT_MODE_BTDM;
#endif
  result = esp_bt_controller_init(&controller_config);
  if (result != ESP_OK) {
    return result;
  }
#if CONFIG_IDF_TARGET_ESP32
  result = esp_bt_controller_enable(ESP_BT_MODE_BTDM);
#else
  result = esp_bt_controller_enable(ESP_BT_MODE_BLE);
#endif
  if (result != ESP_OK) {
    return result;
  }
  result = esp_bluedroid_init_with_cfg(&bluedroid_config);
  if (result != ESP_OK) {
    return result;
  }
  result = esp_bluedroid_enable();
  if (result != ESP_OK) {
    return result;
  }
  result = esp_ble_gap_register_callback(ble_gap_event_handler);
  if (result != ESP_OK) {
    return result;
  }
#if CONFIG_IDF_TARGET_ESP32
  result = esp_bt_gap_register_callback(classic_gap_event_handler);
  if (result != ESP_OK) {
    return result;
  }
  result = esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
  if (result != ESP_OK) {
    return result;
  }
#endif
  result = configure_security();
  if (result != ESP_OK) {
    return result;
  }
  result = esp_ble_gattc_register_callback(esp_hidh_gattc_event_handler);
  if (result != ESP_OK) {
    return result;
  }
  result = esp_hidh_init(&hidh_config);
  if (result != ESP_OK) {
    return result;
  }
  scan_task_handle = xTaskCreateStaticPinnedToCore(
      scan_task, "mol-bt-hid", CONFIG_MOL_BLUETOOTH_HID_TASK_STACK_SIZE, NULL,
      CONFIG_MOL_BLUETOOTH_HID_TASK_PRIORITY, scan_task_stack, &scan_task_control,
      CONFIG_MOL_BLUETOOTH_HID_TASK_CORE);
  return scan_task_handle != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

mol_bluetooth_hid_stats_t mol_bluetooth_hid_stats(void) {
  mol_bluetooth_hid_stats_t snapshot;
  snapshot.ble_scans = (uint32_t)atomic_load_explicit(&stats.ble_scans, memory_order_relaxed);
  snapshot.classic_scans =
      (uint32_t)atomic_load_explicit(&stats.classic_scans, memory_order_relaxed);
  snapshot.open_attempts =
      (uint32_t)atomic_load_explicit(&stats.open_attempts, memory_order_relaxed);
  snapshot.connections = (uint32_t)atomic_load_explicit(&stats.connections, memory_order_relaxed);
  snapshot.disconnects = (uint32_t)atomic_load_explicit(&stats.disconnects, memory_order_relaxed);
  snapshot.reports = (uint32_t)atomic_load_explicit(&stats.reports, memory_order_relaxed);
  snapshot.invalid_reports =
      (uint32_t)atomic_load_explicit(&stats.invalid_reports, memory_order_relaxed);
  snapshot.delivery_failures =
      (uint32_t)atomic_load_explicit(&stats.delivery_failures, memory_order_relaxed);
  snapshot.stack_high_water =
      scan_task_handle != NULL ? (uint32_t)uxTaskGetStackHighWaterMark(scan_task_handle) : 0u;
  return snapshot;
}
