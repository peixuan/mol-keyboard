/* SPDX-License-Identifier: Apache-2.0 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "web_config_protocol.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static const char token[] = "0123456789abcdef0123456789abcdef";
  mol_device_settings_t current = mol_device_settings_default();
  mol_device_settings_t output;
  mol_device_settings_t before;
  mol_web_form_result_t result;
  if (size > MOL_WEB_FORM_MAX_BODY_SIZE + 1u) return 0;
  memset(&output, 0xa5, sizeof(output));
  before = output;
  result = mol_web_form_apply((const char*)data, size, token, (size & 1u) != 0u, &current, &output);
  if (result == MOL_WEB_FORM_OK) {
    if (mol_device_settings_validate(&output) != MOL_OK) __builtin_trap();
  } else if (memcmp(&output, &before, sizeof(output)) != 0) {
    __builtin_trap();
  }
  return 0;
}
