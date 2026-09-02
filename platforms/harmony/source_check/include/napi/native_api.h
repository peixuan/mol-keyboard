// SPDX-License-Identifier: Apache-2.0
// Compile-only subset of the OpenHarmony Node-API declarations used by MoL Keyboard.
#ifndef MOL_SOURCE_CHECK_NAPI_NATIVE_API_H
#define MOL_SOURCE_CHECK_NAPI_NATIVE_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct napi_env__* napi_env;
typedef struct napi_value__* napi_value;
typedef struct napi_callback_info__* napi_callback_info;

typedef enum { napi_ok = 0 } napi_status;
typedef enum { napi_default = 0 } napi_property_attributes;

typedef napi_value (*napi_callback)(napi_env env, napi_callback_info info);
typedef void (*napi_finalize)(napi_env env, void* finalizeData, void* finalizeHint);

typedef struct {
  const char* utf8name;
  napi_value name;
  napi_callback method;
  napi_callback getter;
  napi_callback setter;
  napi_value value;
  napi_property_attributes attributes;
  void* data;
} napi_property_descriptor;

typedef struct napi_module {
  int nm_version;
  unsigned int nm_flags;
  const char* nm_filename;
  napi_value (*nm_register_func)(napi_env env, napi_value exports);
  const char* nm_modname;
  void* nm_priv;
  void* reserved[4];
} napi_module;

napi_status napi_get_undefined(napi_env env, napi_value* result);
napi_status napi_get_cb_info(napi_env env, napi_callback_info cbinfo, size_t* argc,
                             napi_value* argv, napi_value* thisArg, void** data);
napi_status napi_throw_type_error(napi_env env, const char* code, const char* msg);
napi_status napi_throw_error(napi_env env, const char* code, const char* msg);
napi_status napi_get_value_external(napi_env env, napi_value value, void** result);
napi_status napi_get_value_int32(napi_env env, napi_value value, int32_t* result);
napi_status napi_get_value_int64(napi_env env, napi_value value, int64_t* result);
napi_status napi_get_value_double(napi_env env, napi_value value, double* result);
napi_status napi_create_int32(napi_env env, int32_t value, napi_value* result);
napi_status napi_create_int64(napi_env env, int64_t value, napi_value* result);
napi_status napi_get_boolean(napi_env env, bool value, napi_value* result);
napi_status napi_set_named_property(napi_env env, napi_value object, const char* utf8name,
                                    napi_value value);
napi_status napi_create_external(napi_env env, void* data, napi_finalize finalizeCb,
                                 void* finalizeHint, napi_value* result);
napi_status napi_create_object(napi_env env, napi_value* result);
napi_status napi_define_properties(napi_env env, napi_value object, size_t propertyCount,
                                   const napi_property_descriptor* properties);
void napi_module_register(napi_module* module);

#ifdef __cplusplus
}
#endif

#endif
