/* SPDX-License-Identifier: Apache-2.0 */
#include <stddef.h>
#include <stdint.h>

#include "device_settings.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  mol_device_settings_t settings;
  uint8_t encoded[MOL_DEVICE_SETTINGS_RECORD_SIZE];
  if (mol_device_settings_decode(data, size, &settings) == MOL_OK) {
    mol_device_settings_t roundtrip;
    if (mol_device_settings_validate(&settings) != MOL_OK ||
        mol_device_settings_encode(&settings, encoded) != MOL_OK ||
        mol_device_settings_decode(encoded, sizeof(encoded), &roundtrip) != MOL_OK) {
      __builtin_trap();
    }
  }
  return 0;
}
