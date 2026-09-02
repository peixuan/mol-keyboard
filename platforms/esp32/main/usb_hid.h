/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_ESP32_USB_HID_H_
#define MOL_ESP32_USB_HID_H_

#include <stdint.h>

#include "esp_err.h"

typedef struct mol_usb_hid_stats {
  uint32_t interfaces_seen;
  uint32_t keyboards_opened;
  uint32_t rejected_interfaces;
  uint32_t disconnects;
  uint32_t reports;
  uint32_t invalid_reports;
  uint32_t transfer_errors;
  uint32_t delivery_failures;
  uint32_t driver_failures;
  uint32_t event_queue_overflows;
  uint32_t host_stack_high_water;
  uint32_t hid_stack_high_water;
} mol_usb_hid_stats_t;

/** Starts the ESP32-S3 USB Host library and boot-keyboard HID class driver. */
esp_err_t mol_usb_hid_start(void);

mol_usb_hid_stats_t mol_usb_hid_stats(void);

#endif /* MOL_ESP32_USB_HID_H_ */
