// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_TESTS_HARMONY_NAPI_SIM_H
#define MOL_TESTS_HARMONY_NAPI_SIM_H

#include <napi/native_api.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mol::harmony::test {

napi_env create_environment();
void destroy_environment(napi_env environment);
napi_value initialize_registered_module(napi_env environment);

napi_value make_int32(napi_env environment, std::int32_t value);
napi_value make_int64(napi_env environment, std::int64_t value);
napi_value make_double(napi_env environment, double value);
napi_value make_boolean(napi_env environment, bool value);
napi_value make_arraybuffer(napi_env environment, const std::vector<std::uint8_t>& bytes);

napi_value call(napi_env environment, napi_value exports, const std::string& method,
                const std::vector<napi_value>& arguments);
[[nodiscard]] bool has_method(napi_value exports, const std::string& method);
[[nodiscard]] std::size_t method_count(napi_value exports);
[[nodiscard]] bool has_exception(napi_env environment);
[[nodiscard]] std::string exception_message(napi_env environment);
void clear_exception(napi_env environment);

[[nodiscard]] bool is_undefined(napi_value value);
[[nodiscard]] std::int64_t integer_value(napi_value value);
[[nodiscard]] bool boolean_value(napi_value value);
[[nodiscard]] napi_value property(napi_value object, const std::string& name);
[[nodiscard]] std::size_t array_length(napi_value array);
[[nodiscard]] napi_value array_element(napi_value array, std::size_t index);
[[nodiscard]] std::vector<std::uint8_t> arraybuffer_bytes(napi_value arraybuffer);

}  // namespace mol::harmony::test

#endif
