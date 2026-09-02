/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_ESP32_MATRIX_LOGIC_H_
#define MOL_ESP32_MATRIX_LOGIC_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOL_MATRIX_MAX_ROWS 5u
#define MOL_MATRIX_MAX_COLUMNS 6u
#define MOL_MATRIX_MAX_KEYS (MOL_MATRIX_MAX_ROWS * MOL_MATRIX_MAX_COLUMNS)
#define MOL_MATRIX_MAX_EVENTS (MOL_MATRIX_MAX_KEYS + 1u)

typedef enum mol_matrix_ghost_policy {
  MOL_MATRIX_GHOST_ALLOW = 0,
  MOL_MATRIX_GHOST_SUPPRESS_AMBIGUOUS = 1
} mol_matrix_ghost_policy_t;

typedef enum mol_matrix_event_type {
  MOL_MATRIX_EVENT_KEY_DOWN = 1,
  MOL_MATRIX_EVENT_KEY_UP = 2,
  MOL_MATRIX_EVENT_CONFIG_HOLD = 3
} mol_matrix_event_type_t;

typedef enum mol_matrix_result {
  MOL_MATRIX_OK = 0,
  MOL_MATRIX_INVALID_ARGUMENT = 1,
  MOL_MATRIX_BUFFER_TOO_SMALL = 2
} mol_matrix_result_t;

typedef struct mol_matrix_config {
  uint8_t rows;
  uint8_t columns;
  uint8_t config_key;
  mol_matrix_ghost_policy_t ghost_policy;
  uint16_t debounce_scans;
  uint16_t config_hold_scans;
} mol_matrix_config_t;

typedef struct mol_matrix_state {
  mol_matrix_config_t config;
  uint32_t stable_bits;
  uint32_t candidate_bits;
  uint16_t candidate_age[MOL_MATRIX_MAX_KEYS];
  uint16_t config_hold_age;
  bool config_hold_fired;
} mol_matrix_state_t;

typedef struct mol_matrix_event {
  mol_matrix_event_type_t type;
  uint8_t key;
} mol_matrix_event_t;

mol_matrix_result_t mol_matrix_init(mol_matrix_state_t* state, const mol_matrix_config_t* config);

mol_matrix_result_t mol_matrix_process(mol_matrix_state_t* state, uint32_t raw_bits,
                                       mol_matrix_event_t* events, size_t event_capacity,
                                       size_t* event_count, bool* ghost_detected);

#ifdef __cplusplus
}
#endif

#endif /* MOL_ESP32_MATRIX_LOGIC_H_ */
