/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_VERSION_H_
#define MOL_VERSION_H_

#include <stdint.h>

#include "mol/export.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOL_API_VERSION_MAJOR 1u
#define MOL_API_VERSION_MINOR 0u
#define MOL_API_VERSION ((MOL_API_VERSION_MAJOR << 16u) | MOL_API_VERSION_MINOR)

/** Returns the packed public API version. */
MOL_API uint32_t mol_get_api_version(void);

/** Returns the semantic project version as a static UTF-8 string. */
MOL_API const char* mol_get_version_string(void);

#ifdef __cplusplus
}
#endif

#endif  /* MOL_VERSION_H_ */
