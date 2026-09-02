/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_RENDER_SHA256_H_
#define MOL_RENDER_SHA256_H_

#include <stddef.h>
#include <stdint.h>

typedef struct mol_sha256 {
  uint32_t state[8];
  uint64_t bit_count;
  uint8_t block[64];
  size_t block_size;
} mol_sha256_t;

void mol_sha256_init(mol_sha256_t* hash);
void mol_sha256_update(mol_sha256_t* hash, const void* data, size_t size);
void mol_sha256_finish(mol_sha256_t* hash, uint8_t digest[32]);

#endif /* MOL_RENDER_SHA256_H_ */
