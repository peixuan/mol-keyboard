// SPDX-License-Identifier: Apache-2.0
#include "harmony_napi_sim.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <utility>

struct napi_value__ {
  enum class Kind { undefined, number, boolean, external, object, array, arraybuffer };

  Kind kind = Kind::undefined;
  double number = 0.0;
  bool boolean = false;
  void* external = nullptr;
  napi_finalize finalizer = nullptr;
  void* finalize_hint = nullptr;
  std::map<std::string, napi_value> properties{};
  std::map<std::string, napi_callback> methods{};
  std::vector<napi_value> elements{};
  std::vector<std::uint8_t> bytes{};
};

struct napi_env__ {
  std::vector<std::unique_ptr<napi_value__>> values{};
  std::string exception{};
};

struct napi_callback_info__ {
  std::vector<napi_value> arguments{};
};

namespace {

napi_module* g_registered_module = nullptr;

napi_status failure_status() { return static_cast<napi_status>(1); }

napi_value allocate_value(napi_env environment, napi_value__::Kind kind) {
  if (environment == nullptr) return nullptr;
  auto value = std::make_unique<napi_value__>();
  value->kind = kind;
  napi_value result = value.get();
  environment->values.push_back(std::move(value));
  return result;
}

bool is_number(napi_value value) {
  return value != nullptr && value->kind == napi_value__::Kind::number &&
         std::isfinite(value->number);
}

}  // namespace

extern "C" void register_mol_harmony_audio();

namespace mol::harmony::test {

napi_env create_environment() { return new (std::nothrow) napi_env__(); }

void destroy_environment(napi_env environment) {
  if (environment == nullptr) return;
  for (const auto& value : environment->values) {
    if (value->kind == napi_value__::Kind::external && value->external != nullptr &&
        value->finalizer != nullptr) {
      value->finalizer(environment, value->external, value->finalize_hint);
      value->external = nullptr;
    }
  }
  delete environment;
}

napi_value initialize_registered_module(napi_env environment) {
  register_mol_harmony_audio();
  if (environment == nullptr || g_registered_module == nullptr ||
      g_registered_module->nm_register_func == nullptr) {
    return nullptr;
  }
  napi_value exports = allocate_value(environment, napi_value__::Kind::object);
  return g_registered_module->nm_register_func(environment, exports);
}

napi_value make_int32(napi_env environment, std::int32_t value) {
  napi_value result = nullptr;
  return napi_create_int32(environment, value, &result) == napi_ok ? result : nullptr;
}

napi_value make_int64(napi_env environment, std::int64_t value) {
  napi_value result = nullptr;
  return napi_create_int64(environment, value, &result) == napi_ok ? result : nullptr;
}

napi_value make_double(napi_env environment, double value) {
  napi_value result = allocate_value(environment, napi_value__::Kind::number);
  if (result != nullptr) result->number = value;
  return result;
}

napi_value make_boolean(napi_env environment, bool value) {
  napi_value result = nullptr;
  return napi_get_boolean(environment, value, &result) == napi_ok ? result : nullptr;
}

napi_value make_arraybuffer(napi_env environment, const std::vector<std::uint8_t>& bytes) {
  napi_value result = allocate_value(environment, napi_value__::Kind::arraybuffer);
  if (result != nullptr) result->bytes = bytes;
  return result;
}

napi_value call(napi_env environment, napi_value exports, const std::string& method,
                const std::vector<napi_value>& arguments) {
  if (environment == nullptr || exports == nullptr || exports->kind != napi_value__::Kind::object) {
    return nullptr;
  }
  const auto found = exports->methods.find(method);
  if (found == exports->methods.end() || found->second == nullptr) return nullptr;
  napi_callback_info__ info{};
  info.arguments = arguments;
  return found->second(environment, &info);
}

bool has_method(napi_value exports, const std::string& method) {
  return exports != nullptr && exports->kind == napi_value__::Kind::object &&
         exports->methods.find(method) != exports->methods.end();
}

std::size_t method_count(napi_value exports) {
  return exports != nullptr && exports->kind == napi_value__::Kind::object ? exports->methods.size()
                                                                           : 0U;
}

bool has_exception(napi_env environment) {
  return environment != nullptr && !environment->exception.empty();
}

std::string exception_message(napi_env environment) {
  return environment == nullptr ? std::string{} : environment->exception;
}

void clear_exception(napi_env environment) {
  if (environment != nullptr) environment->exception.clear();
}

bool is_undefined(napi_value value) {
  return value != nullptr && value->kind == napi_value__::Kind::undefined;
}

std::int64_t integer_value(napi_value value) {
  return is_number(value) ? static_cast<std::int64_t>(value->number) : 0;
}

bool boolean_value(napi_value value) {
  return value != nullptr && value->kind == napi_value__::Kind::boolean && value->boolean;
}

napi_value property(napi_value object, const std::string& name) {
  if (object == nullptr || object->kind != napi_value__::Kind::object) return nullptr;
  const auto found = object->properties.find(name);
  return found == object->properties.end() ? nullptr : found->second;
}

std::size_t array_length(napi_value array) {
  return array != nullptr && array->kind == napi_value__::Kind::array ? array->elements.size() : 0U;
}

napi_value array_element(napi_value array, std::size_t index) {
  if (array == nullptr || array->kind != napi_value__::Kind::array ||
      index >= array->elements.size()) {
    return nullptr;
  }
  return array->elements[index];
}

std::vector<std::uint8_t> arraybuffer_bytes(napi_value arraybuffer) {
  return arraybuffer != nullptr && arraybuffer->kind == napi_value__::Kind::arraybuffer
             ? arraybuffer->bytes
             : std::vector<std::uint8_t>{};
}

}  // namespace mol::harmony::test

extern "C" {

napi_status napi_get_undefined(napi_env environment, napi_value* result) {
  if (environment == nullptr || result == nullptr) return failure_status();
  *result = allocate_value(environment, napi_value__::Kind::undefined);
  return *result == nullptr ? failure_status() : napi_ok;
}

napi_status napi_get_cb_info(napi_env, napi_callback_info info, std::size_t* argument_count,
                             napi_value* arguments, napi_value*, void**) {
  if (info == nullptr || argument_count == nullptr) return failure_status();
  const std::size_t capacity = *argument_count;
  const std::size_t copy_count = std::min(capacity, info->arguments.size());
  if (arguments != nullptr) {
    std::copy_n(info->arguments.begin(), copy_count, arguments);
  }
  *argument_count = info->arguments.size();
  return napi_ok;
}

napi_status napi_throw_type_error(napi_env environment, const char*, const char* message) {
  if (environment == nullptr) return failure_status();
  environment->exception = message == nullptr ? "type error" : message;
  return napi_ok;
}

napi_status napi_throw_error(napi_env environment, const char*, const char* message) {
  if (environment == nullptr) return failure_status();
  environment->exception = message == nullptr ? "error" : message;
  return napi_ok;
}

napi_status napi_get_value_external(napi_env, napi_value value, void** result) {
  if (value == nullptr || result == nullptr || value->kind != napi_value__::Kind::external) {
    return failure_status();
  }
  *result = value->external;
  return napi_ok;
}

napi_status napi_get_value_int32(napi_env, napi_value value, std::int32_t* result) {
  if (!is_number(value) || result == nullptr ||
      value->number < static_cast<double>(std::numeric_limits<std::int32_t>::min()) ||
      value->number > static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
    return failure_status();
  }
  *result = static_cast<std::int32_t>(value->number);
  return napi_ok;
}

napi_status napi_get_value_int64(napi_env, napi_value value, std::int64_t* result) {
  if (!is_number(value) || result == nullptr ||
      value->number < static_cast<double>(std::numeric_limits<std::int64_t>::min()) ||
      value->number > static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
    return failure_status();
  }
  *result = static_cast<std::int64_t>(value->number);
  return napi_ok;
}

napi_status napi_get_value_double(napi_env, napi_value value, double* result) {
  if (!is_number(value) || result == nullptr) return failure_status();
  *result = value->number;
  return napi_ok;
}

napi_status napi_create_int32(napi_env environment, std::int32_t value, napi_value* result) {
  if (environment == nullptr || result == nullptr) return failure_status();
  *result = allocate_value(environment, napi_value__::Kind::number);
  if (*result == nullptr) return failure_status();
  (*result)->number = static_cast<double>(value);
  return napi_ok;
}

napi_status napi_create_int64(napi_env environment, std::int64_t value, napi_value* result) {
  if (environment == nullptr || result == nullptr) return failure_status();
  *result = allocate_value(environment, napi_value__::Kind::number);
  if (*result == nullptr) return failure_status();
  (*result)->number = static_cast<double>(value);
  return napi_ok;
}

napi_status napi_get_boolean(napi_env environment, bool value, napi_value* result) {
  if (environment == nullptr || result == nullptr) return failure_status();
  *result = allocate_value(environment, napi_value__::Kind::boolean);
  if (*result == nullptr) return failure_status();
  (*result)->boolean = value;
  return napi_ok;
}

napi_status napi_set_named_property(napi_env, napi_value object, const char* name,
                                    napi_value value) {
  if (object == nullptr || object->kind != napi_value__::Kind::object || name == nullptr ||
      value == nullptr) {
    return failure_status();
  }
  object->properties[name] = value;
  return napi_ok;
}

napi_status napi_create_external(napi_env environment, void* data, napi_finalize finalizer,
                                 void* finalize_hint, napi_value* result) {
  if (environment == nullptr || data == nullptr || result == nullptr) return failure_status();
  *result = allocate_value(environment, napi_value__::Kind::external);
  if (*result == nullptr) return failure_status();
  (*result)->external = data;
  (*result)->finalizer = finalizer;
  (*result)->finalize_hint = finalize_hint;
  return napi_ok;
}

napi_status napi_create_object(napi_env environment, napi_value* result) {
  if (environment == nullptr || result == nullptr) return failure_status();
  *result = allocate_value(environment, napi_value__::Kind::object);
  return *result == nullptr ? failure_status() : napi_ok;
}

napi_status napi_create_array_with_length(napi_env environment, std::size_t length,
                                          napi_value* result) {
  if (environment == nullptr || result == nullptr) return failure_status();
  *result = allocate_value(environment, napi_value__::Kind::array);
  if (*result == nullptr) return failure_status();
  (*result)->elements.resize(length, nullptr);
  return napi_ok;
}

napi_status napi_set_element(napi_env, napi_value object, std::uint32_t index, napi_value value) {
  if (object == nullptr || object->kind != napi_value__::Kind::array ||
      index >= object->elements.size() || value == nullptr) {
    return failure_status();
  }
  object->elements[index] = value;
  return napi_ok;
}

napi_status napi_create_arraybuffer(napi_env environment, std::size_t byte_length, void** data,
                                    napi_value* result) {
  if (environment == nullptr || data == nullptr || result == nullptr) return failure_status();
  *result = allocate_value(environment, napi_value__::Kind::arraybuffer);
  if (*result == nullptr) return failure_status();
  (*result)->bytes.resize(byte_length);
  *data = byte_length == 0U ? nullptr : (*result)->bytes.data();
  return napi_ok;
}

napi_status napi_get_arraybuffer_info(napi_env, napi_value arraybuffer, void** data,
                                      std::size_t* byte_length) {
  if (arraybuffer == nullptr || arraybuffer->kind != napi_value__::Kind::arraybuffer ||
      data == nullptr || byte_length == nullptr) {
    return failure_status();
  }
  *byte_length = arraybuffer->bytes.size();
  *data = arraybuffer->bytes.empty() ? nullptr : arraybuffer->bytes.data();
  return napi_ok;
}

napi_status napi_define_properties(napi_env, napi_value object, std::size_t property_count,
                                   const napi_property_descriptor* properties) {
  if (object == nullptr || object->kind != napi_value__::Kind::object || properties == nullptr) {
    return failure_status();
  }
  for (std::size_t index = 0U; index < property_count; ++index) {
    if (properties[index].utf8name == nullptr || properties[index].method == nullptr) {
      return failure_status();
    }
    object->methods[properties[index].utf8name] = properties[index].method;
  }
  return napi_ok;
}

void napi_module_register(napi_module* module) { g_registered_module = module; }

}  // extern "C"
