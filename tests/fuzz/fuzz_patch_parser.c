/* SPDX-License-Identifier: Apache-2.0 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mol/patch.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  mol_patch_t patch;
  uint8_t encoded[MOL_PATCH_BINARY_SIZE];
  size_t encoded_size = 0u;

  memset(&patch, 0, sizeof(patch));
  patch.struct_size = (uint32_t)sizeof(patch);
  if (mol_patch_decode(data, size, &patch) == MOL_OK) {
    if (mol_patch_encode(&patch, encoded, sizeof(encoded), &encoded_size) != MOL_OK ||
        encoded_size != MOL_PATCH_BINARY_SIZE) {
      __builtin_trap();
    }
  }

  memset(&patch, 0, sizeof(patch));
  patch.struct_size = (uint32_t)sizeof(patch);
  if (size <= MOL_PATCH_MAX_JSON_SIZE &&
      mol_patch_compile_json((const char*)data, size, &patch) == MOL_OK) {
    mol_patch_t decoded;
    memset(&decoded, 0, sizeof(decoded));
    decoded.struct_size = (uint32_t)sizeof(decoded);
    if (mol_patch_encode(&patch, encoded, sizeof(encoded), &encoded_size) != MOL_OK ||
        mol_patch_decode(encoded, encoded_size, &decoded) != MOL_OK ||
        memcmp(&patch, &decoded, sizeof(patch)) != 0) {
      __builtin_trap();
    }
  }
  return 0;
}
