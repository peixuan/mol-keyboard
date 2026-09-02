/* SPDX-License-Identifier: Apache-2.0 */
#include "sequence_storage.h"

#include <stdatomic.h>
#include <stdbool.h>

#include "esp_vfs_fat.h"
#include "wear_levelling.h"

static const char* const kMountPath = "/mol";
static const char* const kPartitionLabel = "sequences";
static wl_handle_t wear_levelling_handle = WL_INVALID_HANDLE;
static atomic_uint_least32_t load_count;
static atomic_uint_least32_t save_count;
static atomic_uint_least32_t corrupt_files;
static atomic_uint_least32_t io_failures;

mol_result_t mol_sequence_storage_initialize(void) {
  const esp_vfs_fat_mount_config_t mount_config = {
      .format_if_mount_failed = true,
      .max_files = 3,
      .allocation_unit_size = 4096u,
      .disk_status_check_enable = false,
      .use_one_fat = false,
  };
  if (wear_levelling_handle != WL_INVALID_HANDLE) {
    return MOL_OK;
  }
  if (esp_vfs_fat_spiflash_mount_rw_wl(kMountPath, kPartitionLabel, &mount_config,
                                       &wear_levelling_handle) != ESP_OK) {
    atomic_fetch_add_explicit(&io_failures, 1u, memory_order_relaxed);
    return MOL_ERROR_IO;
  }
  return MOL_OK;
}

mol_result_t mol_sequence_storage_load(const char* name, mol_sequence_config_t* config,
                                       mol_sequence_event_t* events, uint32_t capacity,
                                       uint32_t* event_count) {
  mol_result_t result;
  if (wear_levelling_handle == WL_INVALID_HANDLE) {
    return MOL_ERROR_INVALID_STATE;
  }
  result = mol_sequence_store_load(kMountPath, name, config, events, capacity, event_count);
  if (result == MOL_OK) {
    atomic_fetch_add_explicit(&load_count, 1u, memory_order_relaxed);
  } else if (result == MOL_ERROR_CORRUPT_DATA || result == MOL_ERROR_UNSUPPORTED_VERSION) {
    atomic_fetch_add_explicit(&corrupt_files, 1u, memory_order_relaxed);
  } else if (result == MOL_ERROR_IO) {
    atomic_fetch_add_explicit(&io_failures, 1u, memory_order_relaxed);
  }
  return result;
}

mol_result_t mol_sequence_storage_save(const char* name, const mol_sequence_config_t* config,
                                       const mol_sequence_event_t* events, uint32_t event_count) {
  mol_result_t result;
  if (wear_levelling_handle == WL_INVALID_HANDLE) {
    return MOL_ERROR_INVALID_STATE;
  }
  result = mol_sequence_store_save(kMountPath, name, config, events, event_count);
  if (result == MOL_OK) {
    atomic_fetch_add_explicit(&save_count, 1u, memory_order_relaxed);
  } else if (result == MOL_ERROR_IO) {
    atomic_fetch_add_explicit(&io_failures, 1u, memory_order_relaxed);
  }
  return result;
}

mol_sequence_storage_stats_t mol_sequence_storage_stats(void) {
  mol_sequence_storage_stats_t stats;
  stats.loads = (uint32_t)atomic_load_explicit(&load_count, memory_order_relaxed);
  stats.saves = (uint32_t)atomic_load_explicit(&save_count, memory_order_relaxed);
  stats.corrupt_files = (uint32_t)atomic_load_explicit(&corrupt_files, memory_order_relaxed);
  stats.io_failures = (uint32_t)atomic_load_explicit(&io_failures, memory_order_relaxed);
  return stats;
}
