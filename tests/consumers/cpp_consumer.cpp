/* SPDX-License-Identifier: Apache-2.0 */
#include "mol/mol.h"

#include <array>
#include <cstddef>

int main() {
  alignas(std::max_align_t) std::array<std::byte, 65536> storage{};
  mol_engine_config_t config = mol_engine_config_default();
  mol_engine_t* engine = nullptr;
  if (mol_engine_init(storage.data(), storage.size(), &config, &engine) != MOL_OK) {
    return 1;
  }
  mol_engine_shutdown(engine);
  return 0;
}
