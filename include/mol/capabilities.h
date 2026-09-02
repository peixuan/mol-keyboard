/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_CAPABILITIES_H_
#define MOL_CAPABILITIES_H_

#include <stdint.h>

typedef uint64_t mol_capability_flags_t;

enum {
  MOL_CAPABILITY_CALLER_MEMORY = UINT64_C(1) << 0u,
  MOL_CAPABILITY_INTERLEAVED_F32 = UINT64_C(1) << 1u,
  MOL_CAPABILITY_PLANAR_F32 = UINT64_C(1) << 2u
};

#endif  /* MOL_CAPABILITIES_H_ */
