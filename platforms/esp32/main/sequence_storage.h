/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_ESP32_SEQUENCE_STORAGE_H_
#define MOL_ESP32_SEQUENCE_STORAGE_H_

#include <stdint.h>

#include "sequence_store.h"

typedef struct mol_sequence_storage_stats {
  uint32_t loads;
  uint32_t saves;
  uint32_t corrupt_files;
  uint32_t io_failures;
} mol_sequence_storage_stats_t;

mol_result_t mol_sequence_storage_initialize(void);
mol_result_t mol_sequence_storage_load(const char* name, mol_sequence_config_t* config,
                                       mol_sequence_event_t* events, uint32_t capacity,
                                       uint32_t* event_count);
mol_result_t mol_sequence_storage_save(const char* name, const mol_sequence_config_t* config,
                                       const mol_sequence_event_t* events, uint32_t event_count);
mol_result_t mol_sequence_storage_erase_all(void);
mol_sequence_storage_stats_t mol_sequence_storage_stats(void);

#endif /* MOL_ESP32_SEQUENCE_STORAGE_H_ */
