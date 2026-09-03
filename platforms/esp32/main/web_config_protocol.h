/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_ESP32_WEB_CONFIG_PROTOCOL_H_
#define MOL_ESP32_WEB_CONFIG_PROTOCOL_H_

#include <stdbool.h>
#include <stddef.h>

#include "device_settings.h"

#define MOL_WEB_FORM_MAX_BODY_SIZE 512u
#define MOL_WEB_FORM_TOKEN_HEX_LENGTH 32u

typedef enum mol_web_form_result {
  MOL_WEB_FORM_OK = 0,
  MOL_WEB_FORM_INVALID_ARGUMENT,
  MOL_WEB_FORM_TOO_LARGE,
  MOL_WEB_FORM_MALFORMED,
  MOL_WEB_FORM_UNAUTHORIZED,
  MOL_WEB_FORM_DUPLICATE_FIELD,
  MOL_WEB_FORM_UNKNOWN_FIELD,
  MOL_WEB_FORM_INVALID_VALUE,
  MOL_WEB_FORM_NO_SETTINGS
} mol_web_form_result_t;

/**
 * Applies one bounded application/x-www-form-urlencoded settings patch.
 *
 * The form must contain exactly one token field matching expected_token and at
 * least one recognized setting. Unknown and duplicate fields are rejected. On
 * failure, output is left unchanged.
 */
mol_web_form_result_t mol_web_form_apply(const char* body, size_t body_size,
                                         const char* expected_token, bool a2dp_supported,
                                         const mol_device_settings_t* current,
                                         mol_device_settings_t* output);

const char* mol_web_form_result_string(mol_web_form_result_t result);

#endif /* MOL_ESP32_WEB_CONFIG_PROTOCOL_H_ */
