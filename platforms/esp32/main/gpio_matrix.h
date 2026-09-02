/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_ESP32_GPIO_MATRIX_H_
#define MOL_ESP32_GPIO_MATRIX_H_

#include <stdint.h>

#include "esp_err.h"

typedef struct mol_gpio_matrix_stats {
  uint32_t scans;
  uint32_t transitions;
  uint32_t ghost_scans;
  uint32_t delivery_failures;
  uint32_t config_holds;
  uint32_t stack_high_water;
} mol_gpio_matrix_stats_t;

esp_err_t mol_gpio_matrix_start(void);
mol_gpio_matrix_stats_t mol_gpio_matrix_stats(void);

#endif /* MOL_ESP32_GPIO_MATRIX_H_ */
