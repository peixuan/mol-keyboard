/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_ESP32_DEVICE_CONTROL_H_
#define MOL_ESP32_DEVICE_CONTROL_H_

#include <stdbool.h>
#include <stdint.h>

#include "device_settings.h"
#include "esp_err.h"

typedef struct mol_device_control_stats {
  uint32_t configuration_entries;
  uint32_t settings_applied;
  uint32_t settings_rejected;
  uint32_t queue_rejections;
  uint32_t settings_saves;
  uint32_t persistence_failures;
  uint32_t peer_updates;
  uint32_t clear_pairing_operations;
  uint32_t factory_reset_operations;
  uint32_t bond_removal_requests;
  uint32_t stack_high_water;
} mol_device_control_stats_t;

/** Starts the static, low-priority owner of mutable settings and destructive operations. */
esp_err_t mol_device_control_start(const mol_device_settings_t* initial_settings,
                                   bool storage_ready, bool sequence_storage_ready,
                                   bool bluetooth_ready);

/** Queues a complete candidate settings snapshot without waiting. Pair addresses are preserved. */
bool mol_device_control_submit_settings(const mol_device_settings_t* candidate);

/** Copies the current settings through a short cross-core critical section. */
bool mol_device_control_get_settings(mol_device_settings_t* settings);

bool mol_device_control_configuration_active(void);
mol_device_control_stats_t mol_device_control_stats(void);

#endif /* MOL_ESP32_DEVICE_CONTROL_H_ */
