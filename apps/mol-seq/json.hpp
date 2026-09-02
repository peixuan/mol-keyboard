// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_SEQ_JSON_HPP_
#define MOL_SEQ_JSON_HPP_

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

namespace molseq {

struct Json {
  enum class Type { Null, Boolean, Number, String, Array, Object };
  using Array = std::vector<Json>;
  using Object = std::map<std::string, Json>;

  Type type = Type::Null;
  bool boolean = false;
  std::string text;
  Array array;
  Object object;

  static Json boolean_value(bool value);
  static Json number(std::string value);
  template <typename Integer,
            typename = std::enable_if_t<std::is_integral_v<Integer> &&
                                        !std::is_same_v<std::remove_cv_t<Integer>, bool>>>
  static Json number(Integer value) {
    if constexpr (std::is_signed_v<Integer>)
      return number(std::to_string(static_cast<std::int64_t>(value)));
    else
      return number(std::to_string(static_cast<std::uint64_t>(value)));
  }
  static Json number(double value);
  static Json string(std::string value);
  static Json array_value(Array value);
  static Json object_value(Object value);
};

Json parse_json(const std::string& source);
std::string write_json(const Json& value);

const Json& require_member(const Json& object, const std::string& name);
const Json* optional_member(const Json& object, const std::string& name);
std::uint64_t json_u64(const Json& value, std::uint64_t maximum);
std::int64_t json_i64(const Json& value, std::int64_t minimum, std::int64_t maximum);
double json_double(const Json& value, double minimum, double maximum);
bool json_bool(const Json& value);
const std::string& json_string(const Json& value);

}  // namespace molseq

#endif  // MOL_SEQ_JSON_HPP_
