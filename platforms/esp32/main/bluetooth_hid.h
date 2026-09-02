/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_ESP32_BLUETOOTH_HID_H_
#define MOL_ESP32_BLUETOOTH_HID_H_

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct mol_bluetooth_hid_stats {
  uint32_t ble_scans;
  uint32_t classic_scans;
  uint32_t open_attempts;
  uint32_t connections;
  uint32_t disconnects;
  uint32_t reports;
  uint32_t invalid_reports;
  uint32_t delivery_failures;
  uint32_t stack_high_water;
} mol_bluetooth_hid_stats_t;

/** Starts the non-blocking BLE/Classic HID host and its reconnect scanner. */
esp_err_t mol_bluetooth_hid_start(const uint8_t preferred_address[6], bool preferred_valid);

mol_bluetooth_hid_stats_t mol_bluetooth_hid_stats(void);

#endif /* MOL_ESP32_BLUETOOTH_HID_H_ */
