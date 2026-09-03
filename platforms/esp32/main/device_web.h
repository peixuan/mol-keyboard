/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_ESP32_DEVICE_WEB_H_
#define MOL_ESP32_DEVICE_WEB_H_

#include <stdbool.h>

#include "esp_err.h"

/** Starts or extends the physically authorized, AP-only configuration session. */
esp_err_t mol_device_web_start(void);

/** Stops an expired session and returns whether the local service remains active. */
bool mol_device_web_poll(void);

void mol_device_web_stop(void);

/** Erases the persisted AP password and request token during a physical factory reset. */
esp_err_t mol_device_web_erase_credentials(void);

#endif /* MOL_ESP32_DEVICE_WEB_H_ */
