/* SPDX-License-Identifier: Apache-2.0 */
#include "usb_hid.h"

#include "sdkconfig.h"

#if CONFIG_MOL_USB_HID_ENABLE

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "hid_keyboard.h"
#include "input_queue.h"
#include "usb/hid.h"
#include "usb/hid_host.h"
#include "usb/usb_host.h"

#define MOL_USB_HID_SOURCE_ID UINT32_C(0x55530001)
#define MOL_USB_EVENT_QUEUE_LENGTH 4u
#define MOL_USB_REPORT_BUFFER_SIZE 64u
#define MOL_USB_HID_EVENT_WAIT_MS 20u

typedef struct mol_usb_device_event {
  hid_host_device_handle_t handle;
  hid_host_driver_event_t event;
} mol_usb_device_event_t;

typedef struct mol_usb_hid_atomic_stats {
  atomic_uint_least32_t interfaces_seen;
  atomic_uint_least32_t keyboards_opened;
  atomic_uint_least32_t rejected_interfaces;
  atomic_uint_least32_t disconnects;
  atomic_uint_least32_t reports;
  atomic_uint_least32_t invalid_reports;
  atomic_uint_least32_t transfer_errors;
  atomic_uint_least32_t delivery_failures;
  atomic_uint_least32_t driver_failures;
  atomic_uint_least32_t event_queue_overflows;
} mol_usb_hid_atomic_stats_t;

static const char* const kTag = "mol-usb-hid";
static StaticQueue_t device_event_queue_control;
static uint8_t
    device_event_queue_storage[MOL_USB_EVENT_QUEUE_LENGTH * sizeof(mol_usb_device_event_t)]
    __attribute__((aligned(portBYTE_ALIGNMENT)));
static QueueHandle_t device_event_queue;
static StaticTask_t host_task_control;
static StackType_t host_task_stack[CONFIG_MOL_USB_HOST_TASK_STACK_SIZE];
static TaskHandle_t host_task_handle;
static StaticTask_t hid_task_control;
static StackType_t hid_task_stack[CONFIG_MOL_USB_HID_TASK_STACK_SIZE];
static TaskHandle_t hid_task_handle;
static hid_host_device_handle_t active_keyboard;
static mol_hid_keyboard_state_t keyboard_state;
static mol_usb_hid_atomic_stats_t stats;

_Static_assert(CONFIG_MOL_USB_HOST_TASK_PRIORITY < CONFIG_MOL_AUDIO_TASK_PRIORITY,
               "USB Host task must not preempt audio");
_Static_assert(CONFIG_MOL_USB_HID_TASK_PRIORITY < CONFIG_MOL_AUDIO_TASK_PRIORITY,
               "USB HID task must not preempt audio");

static void submit_commands(const mol_command_t* commands, size_t command_count) {
  size_t index;
  for (index = 0u; index < command_count; ++index) {
    if (!mol_input_submit(&commands[index])) {
      atomic_fetch_add_explicit(&stats.delivery_failures, 1u, memory_order_relaxed);
    }
  }
}

static void release_keyboard(void) {
  mol_command_t commands[2];
  size_t command_count = 0u;
  if (mol_hid_keyboard_disconnect(&keyboard_state, commands, 2u, &command_count) ==
      MOL_HID_KEYBOARD_OK) {
    submit_commands(commands, command_count);
  } else {
    atomic_fetch_add_explicit(&stats.driver_failures, 1u, memory_order_relaxed);
  }
}

static void interface_event_callback(hid_host_device_handle_t handle,
                                     hid_host_interface_event_t event, void* argument) {
  (void)argument;
  if (handle != active_keyboard) {
    return;
  }
  switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT: {
      uint8_t report[MOL_USB_REPORT_BUFFER_SIZE];
      size_t report_size = 0u;
      mol_command_t commands[MOL_HID_MAX_REPORT_COMMANDS];
      size_t command_count = 0u;
      mol_hid_keyboard_result_t parse_result;
      if (hid_host_device_get_raw_input_report_data(handle, report, sizeof(report), &report_size) !=
          ESP_OK) {
        atomic_fetch_add_explicit(&stats.driver_failures, 1u, memory_order_relaxed);
        return;
      }
      atomic_fetch_add_explicit(&stats.reports, 1u, memory_order_relaxed);
      parse_result = mol_hid_keyboard_process(&keyboard_state, report, report_size, commands,
                                              MOL_HID_MAX_REPORT_COMMANDS, &command_count);
      if (parse_result == MOL_HID_KEYBOARD_OK) {
        submit_commands(commands, command_count);
      } else {
        atomic_fetch_add_explicit(&stats.invalid_reports, 1u, memory_order_relaxed);
      }
      break;
    }
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
      atomic_fetch_add_explicit(&stats.transfer_errors, 1u, memory_order_relaxed);
      break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
      release_keyboard();
      active_keyboard = NULL;
      atomic_fetch_add_explicit(&stats.disconnects, 1u, memory_order_relaxed);
      if (hid_host_device_close(handle) != ESP_OK) {
        atomic_fetch_add_explicit(&stats.driver_failures, 1u, memory_order_relaxed);
      }
      ESP_LOGW(kTag, "USB boot keyboard disconnected");
      break;
#ifdef HID_HOST_SUSPEND_RESUME_API_SUPPORTED
    case HID_HOST_INTERFACE_EVENT_SUSPENDED:
    case HID_HOST_INTERFACE_EVENT_RESUMED:
      break;
#endif
    default:
      break;
  }
}

static void driver_event_callback(hid_host_device_handle_t handle, hid_host_driver_event_t event,
                                  void* argument) {
  const mol_usb_device_event_t queued_event = {
      .handle = handle,
      .event = event,
  };
  (void)argument;
  if (device_event_queue == NULL ||
      xQueueSendToBack(device_event_queue, &queued_event, 0u) != pdPASS) {
    atomic_fetch_add_explicit(&stats.event_queue_overflows, 1u, memory_order_relaxed);
  }
}

static void open_keyboard(hid_host_device_handle_t handle) {
  hid_host_dev_params_t parameters;
  hid_host_dev_info_t information;
  const hid_host_device_config_t device_config = {
      .callback = interface_event_callback,
      .callback_arg = NULL,
  };
  esp_err_t result;

  atomic_fetch_add_explicit(&stats.interfaces_seen, 1u, memory_order_relaxed);
  if (hid_host_device_get_params(handle, &parameters) != ESP_OK) {
    atomic_fetch_add_explicit(&stats.driver_failures, 1u, memory_order_relaxed);
    return;
  }
  if (parameters.sub_class != HID_SUBCLASS_BOOT_INTERFACE ||
      parameters.proto != HID_PROTOCOL_KEYBOARD || active_keyboard != NULL) {
    atomic_fetch_add_explicit(&stats.rejected_interfaces, 1u, memory_order_relaxed);
    return;
  }

  active_keyboard = handle;
  mol_hid_keyboard_init(&keyboard_state, MOL_USB_HID_SOURCE_ID);
  result = hid_host_device_open(handle, &device_config);
  if (result == ESP_OK) {
    result = hid_class_request_set_protocol(handle, HID_REPORT_PROTOCOL_BOOT);
  }
  if (result == ESP_OK) {
    result = hid_class_request_set_idle(handle, 0u, 0u);
  }
  if (result == ESP_OK) {
    result = hid_host_device_start(handle);
  }
  if (result != ESP_OK) {
    active_keyboard = NULL;
    atomic_fetch_add_explicit(&stats.driver_failures, 1u, memory_order_relaxed);
    (void)hid_host_device_close(handle);
    ESP_LOGW(kTag, "USB keyboard open failed: %s", esp_err_to_name(result));
    return;
  }

  atomic_fetch_add_explicit(&stats.keyboards_opened, 1u, memory_order_relaxed);
  if (hid_host_get_device_info(handle, &information) == ESP_OK) {
    ESP_LOGI(kTag, "USB boot keyboard connected: address=%u interface=%u VID=%04x PID=%04x",
             parameters.addr, parameters.iface_num, information.VID, information.PID);
  } else {
    ESP_LOGI(kTag, "USB boot keyboard connected: address=%u interface=%u", parameters.addr,
             parameters.iface_num);
  }
}

static void host_event_task(void* context) {
  (void)context;
  (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  for (;;) {
    uint32_t event_flags = 0u;
    const esp_err_t result = usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    (void)event_flags;
    if (result != ESP_OK) {
      atomic_fetch_add_explicit(&stats.driver_failures, 1u, memory_order_relaxed);
      vTaskDelay(pdMS_TO_TICKS(MOL_USB_HID_EVENT_WAIT_MS));
    }
  }
}

static void hid_event_task(void* context) {
  (void)context;
  (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  for (;;) {
    mol_usb_device_event_t event;
    const esp_err_t result = hid_host_handle_events(pdMS_TO_TICKS(MOL_USB_HID_EVENT_WAIT_MS));
    if (result != ESP_OK && result != ESP_ERR_TIMEOUT) {
      atomic_fetch_add_explicit(&stats.driver_failures, 1u, memory_order_relaxed);
      vTaskDelay(pdMS_TO_TICKS(MOL_USB_HID_EVENT_WAIT_MS));
    }
    while (xQueueReceive(device_event_queue, &event, 0u) == pdPASS) {
      if (event.event == HID_HOST_DRIVER_EVENT_CONNECTED) {
        open_keyboard(event.handle);
      }
    }
  }
}

esp_err_t mol_usb_hid_start(void) {
  const usb_host_config_t host_config = {
      .skip_phy_setup = false,
      .root_port_unpowered = false,
      .intr_flags = ESP_INTR_FLAG_LEVEL1,
      .enum_filter_cb = NULL,
      .fifo_settings_custom = {.nptx_fifo_lines = 0u, .ptx_fifo_lines = 0u, .rx_fifo_lines = 0u},
      .peripheral_map = 0u,
  };
  const hid_host_driver_config_t hid_config = {
      .create_background_task = false,
      .task_priority = CONFIG_MOL_USB_HID_TASK_PRIORITY,
      .stack_size = CONFIG_MOL_USB_HID_TASK_STACK_SIZE,
      .core_id = CONFIG_MOL_USB_HID_TASK_CORE,
      .callback = driver_event_callback,
      .callback_arg = NULL,
  };
  esp_err_t result;

  if (device_event_queue != NULL || host_task_handle != NULL || hid_task_handle != NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  device_event_queue =
      xQueueCreateStatic(MOL_USB_EVENT_QUEUE_LENGTH, sizeof(mol_usb_device_event_t),
                         device_event_queue_storage, &device_event_queue_control);
  if (device_event_queue == NULL) {
    return ESP_ERR_NO_MEM;
  }
  result = usb_host_install(&host_config);
  if (result != ESP_OK) {
    device_event_queue = NULL;
    return result;
  }
  result = hid_host_install(&hid_config);
  if (result != ESP_OK) {
    (void)usb_host_uninstall();
    device_event_queue = NULL;
    return result;
  }

  host_task_handle = xTaskCreateStaticPinnedToCore(
      host_event_task, "mol-usb-host", CONFIG_MOL_USB_HOST_TASK_STACK_SIZE, NULL,
      CONFIG_MOL_USB_HOST_TASK_PRIORITY, host_task_stack, &host_task_control,
      CONFIG_MOL_USB_HID_TASK_CORE);
  hid_task_handle = xTaskCreateStaticPinnedToCore(hid_event_task, "mol-usb-hid",
                                                  CONFIG_MOL_USB_HID_TASK_STACK_SIZE, NULL,
                                                  CONFIG_MOL_USB_HID_TASK_PRIORITY, hid_task_stack,
                                                  &hid_task_control, CONFIG_MOL_USB_HID_TASK_CORE);
  if (host_task_handle == NULL || hid_task_handle == NULL) {
    if (host_task_handle != NULL) {
      vTaskDelete(host_task_handle);
      host_task_handle = NULL;
    }
    if (hid_task_handle != NULL) {
      vTaskDelete(hid_task_handle);
      hid_task_handle = NULL;
    }
    (void)hid_host_uninstall();
    (void)usb_host_uninstall();
    device_event_queue = NULL;
    return ESP_ERR_NO_MEM;
  }

  xTaskNotifyGive(host_task_handle);
  xTaskNotifyGive(hid_task_handle);
  return ESP_OK;
}

mol_usb_hid_stats_t mol_usb_hid_stats(void) {
  mol_usb_hid_stats_t snapshot;
  snapshot.interfaces_seen =
      (uint32_t)atomic_load_explicit(&stats.interfaces_seen, memory_order_relaxed);
  snapshot.keyboards_opened =
      (uint32_t)atomic_load_explicit(&stats.keyboards_opened, memory_order_relaxed);
  snapshot.rejected_interfaces =
      (uint32_t)atomic_load_explicit(&stats.rejected_interfaces, memory_order_relaxed);
  snapshot.disconnects = (uint32_t)atomic_load_explicit(&stats.disconnects, memory_order_relaxed);
  snapshot.reports = (uint32_t)atomic_load_explicit(&stats.reports, memory_order_relaxed);
  snapshot.invalid_reports =
      (uint32_t)atomic_load_explicit(&stats.invalid_reports, memory_order_relaxed);
  snapshot.transfer_errors =
      (uint32_t)atomic_load_explicit(&stats.transfer_errors, memory_order_relaxed);
  snapshot.delivery_failures =
      (uint32_t)atomic_load_explicit(&stats.delivery_failures, memory_order_relaxed);
  snapshot.driver_failures =
      (uint32_t)atomic_load_explicit(&stats.driver_failures, memory_order_relaxed);
  snapshot.event_queue_overflows =
      (uint32_t)atomic_load_explicit(&stats.event_queue_overflows, memory_order_relaxed);
  snapshot.host_stack_high_water =
      host_task_handle != NULL ? (uint32_t)uxTaskGetStackHighWaterMark(host_task_handle) : 0u;
  snapshot.hid_stack_high_water =
      hid_task_handle != NULL ? (uint32_t)uxTaskGetStackHighWaterMark(hid_task_handle) : 0u;
  return snapshot;
}

#endif /* CONFIG_MOL_USB_HID_ENABLE */
