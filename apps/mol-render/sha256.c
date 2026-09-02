/* SPDX-License-Identifier: Apache-2.0 */
#include "sha256.h"

#include <string.h>

static uint32_t mol_rotr(uint32_t value, uint32_t bits) {
  return (value >> bits) | (value << (32u - bits));
}

static uint32_t mol_read_be32(const uint8_t* data) {
  return ((uint32_t)data[0] << 24u) | ((uint32_t)data[1] << 16u) | ((uint32_t)data[2] << 8u) |
         data[3];
}

static void mol_sha256_transform(mol_sha256_t* hash, const uint8_t block[64]) {
  static const uint32_t constants[64] = {
      UINT32_C(0x428a2f98), UINT32_C(0x71374491), UINT32_C(0xb5c0fbcf), UINT32_C(0xe9b5dba5),
      UINT32_C(0x3956c25b), UINT32_C(0x59f111f1), UINT32_C(0x923f82a4), UINT32_C(0xab1c5ed5),
      UINT32_C(0xd807aa98), UINT32_C(0x12835b01), UINT32_C(0x243185be), UINT32_C(0x550c7dc3),
      UINT32_C(0x72be5d74), UINT32_C(0x80deb1fe), UINT32_C(0x9bdc06a7), UINT32_C(0xc19bf174),
      UINT32_C(0xe49b69c1), UINT32_C(0xefbe4786), UINT32_C(0x0fc19dc6), UINT32_C(0x240ca1cc),
      UINT32_C(0x2de92c6f), UINT32_C(0x4a7484aa), UINT32_C(0x5cb0a9dc), UINT32_C(0x76f988da),
      UINT32_C(0x983e5152), UINT32_C(0xa831c66d), UINT32_C(0xb00327c8), UINT32_C(0xbf597fc7),
      UINT32_C(0xc6e00bf3), UINT32_C(0xd5a79147), UINT32_C(0x06ca6351), UINT32_C(0x14292967),
      UINT32_C(0x27b70a85), UINT32_C(0x2e1b2138), UINT32_C(0x4d2c6dfc), UINT32_C(0x53380d13),
      UINT32_C(0x650a7354), UINT32_C(0x766a0abb), UINT32_C(0x81c2c92e), UINT32_C(0x92722c85),
      UINT32_C(0xa2bfe8a1), UINT32_C(0xa81a664b), UINT32_C(0xc24b8b70), UINT32_C(0xc76c51a3),
      UINT32_C(0xd192e819), UINT32_C(0xd6990624), UINT32_C(0xf40e3585), UINT32_C(0x106aa070),
      UINT32_C(0x19a4c116), UINT32_C(0x1e376c08), UINT32_C(0x2748774c), UINT32_C(0x34b0bcb5),
      UINT32_C(0x391c0cb3), UINT32_C(0x4ed8aa4a), UINT32_C(0x5b9cca4f), UINT32_C(0x682e6ff3),
      UINT32_C(0x748f82ee), UINT32_C(0x78a5636f), UINT32_C(0x84c87814), UINT32_C(0x8cc70208),
      UINT32_C(0x90befffa), UINT32_C(0xa4506ceb), UINT32_C(0xbef9a3f7), UINT32_C(0xc67178f2)};
  uint32_t words[64];
  uint32_t a;
  uint32_t b;
  uint32_t c;
  uint32_t d;
  uint32_t e;
  uint32_t f;
  uint32_t g;
  uint32_t h;
  for (uint32_t index = 0u; index < 16u; ++index) words[index] = mol_read_be32(block + index * 4u);
  for (uint32_t index = 16u; index < 64u; ++index) {
    uint32_t s0 = mol_rotr(words[index - 15u], 7u) ^ mol_rotr(words[index - 15u], 18u) ^
                  (words[index - 15u] >> 3u);
    uint32_t s1 = mol_rotr(words[index - 2u], 17u) ^ mol_rotr(words[index - 2u], 19u) ^
                  (words[index - 2u] >> 10u);
    words[index] = words[index - 16u] + s0 + words[index - 7u] + s1;
  }
  a = hash->state[0];
  b = hash->state[1];
  c = hash->state[2];
  d = hash->state[3];
  e = hash->state[4];
  f = hash->state[5];
  g = hash->state[6];
  h = hash->state[7];
  for (uint32_t index = 0u; index < 64u; ++index) {
    uint32_t sum1 = mol_rotr(e, 6u) ^ mol_rotr(e, 11u) ^ mol_rotr(e, 25u);
    uint32_t choice = (e & f) ^ ((~e) & g);
    uint32_t temporary1 = h + sum1 + choice + constants[index] + words[index];
    uint32_t sum0 = mol_rotr(a, 2u) ^ mol_rotr(a, 13u) ^ mol_rotr(a, 22u);
    uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
    uint32_t temporary2 = sum0 + majority;
    h = g;
    g = f;
    f = e;
    e = d + temporary1;
    d = c;
    c = b;
    b = a;
    a = temporary1 + temporary2;
  }
  hash->state[0] += a;
  hash->state[1] += b;
  hash->state[2] += c;
  hash->state[3] += d;
  hash->state[4] += e;
  hash->state[5] += f;
  hash->state[6] += g;
  hash->state[7] += h;
}

void mol_sha256_init(mol_sha256_t* hash) {
  static const uint32_t initial[8] = {
      UINT32_C(0x6a09e667), UINT32_C(0xbb67ae85), UINT32_C(0x3c6ef372), UINT32_C(0xa54ff53a),
      UINT32_C(0x510e527f), UINT32_C(0x9b05688c), UINT32_C(0x1f83d9ab), UINT32_C(0x5be0cd19)};
  memset(hash, 0, sizeof(*hash));
  memcpy(hash->state, initial, sizeof(initial));
}

void mol_sha256_update(mol_sha256_t* hash, const void* data, size_t size) {
  const uint8_t* bytes = (const uint8_t*)data;
  hash->bit_count += (uint64_t)size * 8u;
  while (size != 0u) {
    size_t available = sizeof(hash->block) - hash->block_size;
    size_t count = size < available ? size : available;
    memcpy(hash->block + hash->block_size, bytes, count);
    hash->block_size += count;
    bytes += count;
    size -= count;
    if (hash->block_size == sizeof(hash->block)) {
      mol_sha256_transform(hash, hash->block);
      hash->block_size = 0u;
    }
  }
}

void mol_sha256_finish(mol_sha256_t* hash, uint8_t digest[32]) {
  uint64_t bit_count = hash->bit_count;
  uint8_t padding[128] = {0x80u};
  size_t padding_size = hash->block_size < 56u ? 56u - hash->block_size : 120u - hash->block_size;
  mol_sha256_update(hash, padding, padding_size);
  for (uint32_t index = 0u; index < 8u; ++index)
    hash->block[56u + index] = (uint8_t)(bit_count >> ((7u - index) * 8u));
  mol_sha256_transform(hash, hash->block);
  for (uint32_t index = 0u; index < 8u; ++index) {
    digest[index * 4u] = (uint8_t)(hash->state[index] >> 24u);
    digest[index * 4u + 1u] = (uint8_t)(hash->state[index] >> 16u);
    digest[index * 4u + 2u] = (uint8_t)(hash->state[index] >> 8u);
    digest[index * 4u + 3u] = (uint8_t)hash->state[index];
  }
}
