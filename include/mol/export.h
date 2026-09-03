/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MOL_EXPORT_H_
#define MOL_EXPORT_H_

#if defined(_WIN32) && defined(MOL_CORE_SHARED)
#if defined(MOL_CORE_BUILDING_LIBRARY)
#define MOL_API __declspec(dllexport)
#else
#define MOL_API __declspec(dllimport)
#endif
#elif defined(MOL_CORE_SHARED) && (defined(__GNUC__) || defined(__clang__))
#define MOL_API __attribute__((visibility("default")))
#else
#define MOL_API
#endif

#endif /* MOL_EXPORT_H_ */
