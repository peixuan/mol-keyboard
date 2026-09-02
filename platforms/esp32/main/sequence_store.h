/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_ESP32_SEQUENCE_STORE_H_
#define MOL_ESP32_SEQUENCE_STORE_H_

#include <stddef.h>
#include <stdint.h>

#include "mol/mol.h"

#define MOL_SEQUENCE_STORE_MAX_NAME 32u
#define MOL_SEQUENCE_STORE_MAX_EVENTS 256u

int mol_sequence_store_name_is_valid(const char* name);
mol_result_t mol_sequence_store_save(const char* base_path, const char* name,
                                     const mol_sequence_config_t* config,
                                     const mol_sequence_event_t* events, uint32_t event_count);
mol_result_t mol_sequence_store_load(const char* base_path, const char* name,
                                     mol_sequence_config_t* config, mol_sequence_event_t* events,
                                     uint32_t capacity, uint32_t* event_count);

#endif /* MOL_ESP32_SEQUENCE_STORE_H_ */
