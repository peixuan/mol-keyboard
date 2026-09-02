/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_ESP32_DEVICE_STORAGE_H_
#define MOL_ESP32_DEVICE_STORAGE_H_

#include <stdint.h>

#include "device_settings.h"

typedef uint32_t mol_device_settings_source_t;
enum {
  MOL_DEVICE_SETTINGS_FROM_NVS = 0u,
  MOL_DEVICE_SETTINGS_DEFAULT_MISSING = 1u,
  MOL_DEVICE_SETTINGS_DEFAULT_CORRUPT = 2u,
  MOL_DEVICE_SETTINGS_DEFAULT_IO_ERROR = 3u
};

typedef struct mol_device_storage_stats {
  uint32_t settings_loads;
  uint32_t settings_saves;
  uint32_t missing_records;
  uint32_t corrupt_records;
  uint32_t io_failures;
} mol_device_storage_stats_t;

mol_result_t mol_device_storage_initialize(void);
mol_result_t mol_device_storage_load_settings(mol_device_settings_t* settings,
                                              mol_device_settings_source_t* source);
mol_result_t mol_device_storage_save_settings(const mol_device_settings_t* settings);
mol_device_storage_stats_t mol_device_storage_stats(void);

#endif /* MOL_ESP32_DEVICE_STORAGE_H_ */
