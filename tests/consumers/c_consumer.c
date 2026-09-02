/* SPDX-License-Identifier: Apache-2.0 */
#include "mol/mol.h"

#include <stddef.h>
#include <stdint.h>

typedef union consumer_storage {
  long double floating_alignment;
  void* pointer_alignment;
  uint64_t integer_alignment;
  unsigned char bytes[262144];
} consumer_storage_t;

int main(void) {
  static consumer_storage_t storage;
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = NULL;
  if (mol_engine_init(storage.bytes, sizeof(storage.bytes), &config, &engine) != MOL_OK) {
    return 1;
  }
  mol_engine_shutdown(engine);
  return 0;
}
