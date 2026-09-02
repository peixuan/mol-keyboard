/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_RESULT_H_
#define MOL_RESULT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t mol_result_t;

enum {
  MOL_OK = 0,
  MOL_ERROR_INVALID_ARGUMENT = 1,
  MOL_ERROR_UNSUPPORTED_VERSION = 2,
  MOL_ERROR_INSUFFICIENT_MEMORY = 3,
  MOL_ERROR_MISALIGNED_MEMORY = 4,
  MOL_ERROR_INVALID_STATE = 5,
  MOL_ERROR_QUEUE_FULL = 6,
  MOL_ERROR_BUFFER_TOO_SMALL = 7,
  MOL_ERROR_UNSUPPORTED = 8,
  MOL_ERROR_OVERFLOW = 9,
  MOL_ERROR_INTERNAL = 10
};

/** Returns a stable ASCII description for a result code. */
const char* mol_result_string(mol_result_t result);

#ifdef __cplusplus
}
#endif

#endif  /* MOL_RESULT_H_ */
