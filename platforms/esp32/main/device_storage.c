/* SPDX-License-Identifier: Apache-2.0 */
#include "device_storage.h"

#include <stdatomic.h>
#include <stdbool.h>

#include "nvs.h"
#include "nvs_flash.h"

static const char* const kSettingsNamespace = "mol-settings";
static const char* const kSettingsKey = "current";
static atomic_uint_least32_t settings_loads;
static atomic_uint_least32_t settings_saves;
static atomic_uint_least32_t missing_records;
static atomic_uint_least32_t corrupt_records;
static atomic_uint_least32_t io_failures;

mol_result_t mol_device_storage_initialize(void) {
  esp_err_t result = nvs_flash_init();
  if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    result = nvs_flash_erase();
    if (result == ESP_OK) {
      result = nvs_flash_init();
    }
  }
  if (result != ESP_OK) {
    atomic_fetch_add_explicit(&io_failures, 1u, memory_order_relaxed);
    return MOL_ERROR_IO;
  }
  return MOL_OK;
}

mol_result_t mol_device_storage_load_settings(mol_device_settings_t* settings,
                                              mol_device_settings_source_t* source) {
  uint8_t encoded[MOL_DEVICE_SETTINGS_RECORD_SIZE];
  size_t encoded_size = 0u;
  nvs_handle_t handle;
  esp_err_t result;
  mol_result_t decode_result;
  if (settings == NULL || source == NULL) {
    return MOL_ERROR_INVALID_ARGUMENT;
  }
  result = nvs_open(kSettingsNamespace, NVS_READONLY, &handle);
  if (result == ESP_ERR_NVS_NOT_FOUND) {
    *settings = mol_device_settings_default();
    *source = MOL_DEVICE_SETTINGS_DEFAULT_MISSING;
    atomic_fetch_add_explicit(&missing_records, 1u, memory_order_relaxed);
    atomic_fetch_add_explicit(&settings_loads, 1u, memory_order_relaxed);
    return MOL_OK;
  }
  if (result != ESP_OK) {
    atomic_fetch_add_explicit(&io_failures, 1u, memory_order_relaxed);
    return MOL_ERROR_IO;
  }
  result = nvs_get_blob(handle, kSettingsKey, NULL, &encoded_size);
  if (result == ESP_ERR_NVS_NOT_FOUND) {
    nvs_close(handle);
    *settings = mol_device_settings_default();
    *source = MOL_DEVICE_SETTINGS_DEFAULT_MISSING;
    atomic_fetch_add_explicit(&missing_records, 1u, memory_order_relaxed);
    atomic_fetch_add_explicit(&settings_loads, 1u, memory_order_relaxed);
    return MOL_OK;
  }
  if (result != ESP_OK) {
    nvs_close(handle);
    atomic_fetch_add_explicit(&io_failures, 1u, memory_order_relaxed);
    return MOL_ERROR_IO;
  }
  if (encoded_size != sizeof(encoded)) {
    nvs_close(handle);
    *settings = mol_device_settings_default();
    *source = MOL_DEVICE_SETTINGS_DEFAULT_CORRUPT;
    atomic_fetch_add_explicit(&corrupt_records, 1u, memory_order_relaxed);
    atomic_fetch_add_explicit(&settings_loads, 1u, memory_order_relaxed);
    return MOL_OK;
  }
  result = nvs_get_blob(handle, kSettingsKey, encoded, &encoded_size);
  nvs_close(handle);
  if (result != ESP_OK) {
    atomic_fetch_add_explicit(&io_failures, 1u, memory_order_relaxed);
    return MOL_ERROR_IO;
  }
  decode_result = mol_device_settings_decode(encoded, encoded_size, settings);
  if (decode_result != MOL_OK) {
    *settings = mol_device_settings_default();
    *source = MOL_DEVICE_SETTINGS_DEFAULT_CORRUPT;
    atomic_fetch_add_explicit(&corrupt_records, 1u, memory_order_relaxed);
  } else {
    *source = MOL_DEVICE_SETTINGS_FROM_NVS;
  }
  atomic_fetch_add_explicit(&settings_loads, 1u, memory_order_relaxed);
  return MOL_OK;
}

mol_result_t mol_device_storage_save_settings(const mol_device_settings_t* settings) {
  uint8_t encoded[MOL_DEVICE_SETTINGS_RECORD_SIZE];
  nvs_handle_t handle;
  bool opened = false;
  esp_err_t result;
  mol_result_t encode_result = mol_device_settings_encode(settings, encoded);
  if (encode_result != MOL_OK) {
    return encode_result;
  }
  result = nvs_open(kSettingsNamespace, NVS_READWRITE, &handle);
  if (result == ESP_OK) {
    opened = true;
    result = nvs_set_blob(handle, kSettingsKey, encoded, sizeof(encoded));
  }
  if (result == ESP_OK) {
    result = nvs_commit(handle);
  }
  if (opened) {
    nvs_close(handle);
  }
  if (result == ESP_OK) {
    atomic_fetch_add_explicit(&settings_saves, 1u, memory_order_relaxed);
    return MOL_OK;
  }
  atomic_fetch_add_explicit(&io_failures, 1u, memory_order_relaxed);
  return MOL_ERROR_IO;
}

mol_device_storage_stats_t mol_device_storage_stats(void) {
  mol_device_storage_stats_t stats;
  stats.settings_loads = (uint32_t)atomic_load_explicit(&settings_loads, memory_order_relaxed);
  stats.settings_saves = (uint32_t)atomic_load_explicit(&settings_saves, memory_order_relaxed);
  stats.missing_records = (uint32_t)atomic_load_explicit(&missing_records, memory_order_relaxed);
  stats.corrupt_records = (uint32_t)atomic_load_explicit(&corrupt_records, memory_order_relaxed);
  stats.io_failures = (uint32_t)atomic_load_explicit(&io_failures, memory_order_relaxed);
  return stats;
}
