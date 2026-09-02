/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_ESP32_INPUT_QUEUE_H_
#define MOL_ESP32_INPUT_QUEUE_H_

#include <stdbool.h>
#include <stdint.h>

#include "mol/mol.h"

typedef struct mol_input_queue_stats {
  uint32_t queued;
  uint32_t dropped;
  uint32_t rejected;
  uint32_t high_water;
} mol_input_queue_stats_t;

void mol_input_queue_init(void);
bool mol_input_submit(const mol_command_t* command);
uint32_t mol_input_drain(mol_engine_t* engine);
void mol_input_request_config_mode(void);
bool mol_input_take_config_mode_request(void);
void mol_input_request_clear_pairing(void);
bool mol_input_take_clear_pairing_request(void);
void mol_input_request_factory_reset(void);
bool mol_input_take_factory_reset_request(void);
mol_input_queue_stats_t mol_input_queue_stats(void);

#endif /* MOL_ESP32_INPUT_QUEUE_H_ */
