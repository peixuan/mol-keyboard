// SPDX-License-Identifier: Apache-2.0
#include "json.hpp"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace molseq {

Json Json::boolean_value(bool value) {
  Json result;
  result.type = Type::Boolean;
  result.boolean = value;
  return result;
}

Json Json::number(std::string value) {
  Json result;
  result.type = Type::Number;
  result.text = std::move(value);
  return result;
}

Json Json::number(double value) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::setprecision(9) << value;
  return number(stream.str());
}

Json Json::string(std::string value) {
  Json result;
  result.type = Type::String;
  result.text = std::move(value);
  return result;
}

Json Json::array_value(Array value) {
  Json result;
  result.type = Type::Array;
  result.array = std::move(value);
  return result;
}

Json Json::object_value(Object value) {
  Json result;
  result.type = Type::Object;
  result.object = std::move(value);
  return result;
}

namespace {

class Parser {
 public:
  explicit Parser(const std::string& source) : source_(source) {}

  Json parse() {
    Json value = parse_value(0u);
    skip_space();
    if (position_ != source_.size()) fail("trailing content");
    return value;
  }

 private:
  [[noreturn]] void fail(const char* message) const {
    throw std::runtime_error(std::string("JSON error at byte ") + std::to_string(position_) + ": " +
                             message);
  }

  void skip_space() {
    while (position_ < source_.size()) {
      const char value = source_[position_];
      if (value != ' ' && value != '\t' && value != '\r' && value != '\n') break;
      ++position_;
    }
  }

  bool consume(char expected) {
    skip_space();
    if (position_ >= source_.size() || source_[position_] != expected) return false;
    ++position_;
    return true;
  }

  Json parse_value(std::size_t depth) {
    if (depth > 32u) fail("nesting limit exceeded");
    skip_space();
    if (position_ >= source_.size()) fail("unexpected end of input");
    switch (source_[position_]) {
      case '{':
        return parse_object(depth + 1u);
      case '[':
        return parse_array(depth + 1u);
      case '"':
        return Json::string(parse_string());
      case 't':
        parse_literal("true");
        return Json::boolean_value(true);
      case 'f':
        parse_literal("false");
        return Json::boolean_value(false);
      case 'n':
        parse_literal("null");
        return Json{};
      default:
        if (source_[position_] == '-' || (source_[position_] >= '0' && source_[position_] <= '9')) {
          return Json::number(parse_number());
        }
        fail("expected a value");
    }
  }

  Json parse_object(std::size_t depth) {
    Json::Object object;
    (void)consume('{');
    if (consume('}')) return Json::object_value(std::move(object));
    for (;;) {
      skip_space();
      if (position_ >= source_.size() || source_[position_] != '"') fail("expected object key");
      std::string key = parse_string();
      if (!consume(':')) fail("expected ':'");
      Json value = parse_value(depth);
      if (!object.emplace(std::move(key), std::move(value)).second) fail("duplicate object key");
      if (consume('}')) break;
      if (!consume(',')) fail("expected ',' or '}'");
    }
    return Json::object_value(std::move(object));
  }

  Json parse_array(std::size_t depth) {
    Json::Array array;
    (void)consume('[');
    if (consume(']')) return Json::array_value(std::move(array));
    for (;;) {
      array.push_back(parse_value(depth));
      if (consume(']')) break;
      if (!consume(',')) fail("expected ',' or ']'");
    }
    return Json::array_value(std::move(array));
  }

  static void append_utf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint <= 0x7Fu) {
      output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FFu) {
      output.push_back(static_cast<char>(0xC0u | (codepoint >> 6u)));
      output.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    } else if (codepoint <= 0xFFFFu) {
      output.push_back(static_cast<char>(0xE0u | (codepoint >> 12u)));
      output.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu)));
      output.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    } else {
      output.push_back(static_cast<char>(0xF0u | (codepoint >> 18u)));
      output.push_back(static_cast<char>(0x80u | ((codepoint >> 12u) & 0x3Fu)));
      output.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu)));
      output.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    }
  }

  std::uint32_t parse_hex4() {
    std::uint32_t value = 0u;
    for (std::uint32_t index = 0u; index < 4u; ++index) {
      if (position_ >= source_.size()) fail("truncated Unicode escape");
      const char digit = source_[position_++];
      value <<= 4u;
      if (digit >= '0' && digit <= '9')
        value |= static_cast<std::uint32_t>(digit - '0');
      else if (digit >= 'a' && digit <= 'f')
        value |= static_cast<std::uint32_t>(digit - 'a' + 10);
      else if (digit >= 'A' && digit <= 'F')
        value |= static_cast<std::uint32_t>(digit - 'A' + 10);
      else
        fail("invalid Unicode escape");
    }
    return value;
  }

  std::string parse_string() {
    std::string result;
    ++position_;
    while (position_ < source_.size()) {
      const unsigned char value = static_cast<unsigned char>(source_[position_++]);
      if (value == '"') return result;
      if (value < 0x20u) fail("unescaped control character");
      if (value != '\\') {
        result.push_back(static_cast<char>(value));
        continue;
      }
      if (position_ >= source_.size()) fail("truncated escape");
      switch (source_[position_++]) {
        case '"':
          result.push_back('"');
          break;
        case '\\':
          result.push_back('\\');
          break;
        case '/':
          result.push_back('/');
          break;
        case 'b':
          result.push_back('\b');
          break;
        case 'f':
          result.push_back('\f');
          break;
        case 'n':
          result.push_back('\n');
          break;
        case 'r':
          result.push_back('\r');
          break;
        case 't':
          result.push_back('\t');
          break;
        case 'u': {
          std::uint32_t codepoint = parse_hex4();
          if (codepoint >= 0xD800u && codepoint <= 0xDBFFu) {
            if (position_ + 2u > source_.size() || source_[position_] != '\\' ||
                source_[position_ + 1u] != 'u')
              fail("missing low surrogate");
            position_ += 2u;
            const std::uint32_t low = parse_hex4();
            if (low < 0xDC00u || low > 0xDFFFu) fail("invalid low surrogate");
            codepoint = 0x10000u + ((codepoint - 0xD800u) << 10u) + (low - 0xDC00u);
          } else if (codepoint >= 0xDC00u && codepoint <= 0xDFFFu) {
            fail("unexpected low surrogate");
          }
          append_utf8(result, codepoint);
          break;
        }
        default:
          fail("unknown escape");
      }
    }
    fail("unterminated string");
  }

  std::string parse_number() {
    const std::size_t start = position_;
    if (source_[position_] == '-') ++position_;
    if (position_ >= source_.size()) fail("truncated number");
    if (source_[position_] == '0') {
      ++position_;
    } else {
      if (source_[position_] < '1' || source_[position_] > '9') fail("invalid number");
      while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9')
        ++position_;
    }
    if (position_ < source_.size() && source_[position_] == '.') {
      ++position_;
      const std::size_t digits = position_;
      while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9')
        ++position_;
      if (digits == position_) fail("missing fraction digits");
    }
    if (position_ < source_.size() && (source_[position_] == 'e' || source_[position_] == 'E')) {
      ++position_;
      if (position_ < source_.size() && (source_[position_] == '+' || source_[position_] == '-'))
        ++position_;
      const std::size_t digits = position_;
      while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9')
        ++position_;
      if (digits == position_) fail("missing exponent digits");
    }
    return source_.substr(start, position_ - start);
  }

  void parse_literal(const char* literal) {
    for (std::size_t index = 0u; literal[index] != '\0'; ++index) {
      if (position_ >= source_.size()) fail("invalid literal");
      const char current = source_[position_];
      ++position_;
      if (current != literal[index]) fail("invalid literal");
    }
  }

  const std::string& source_;
  std::size_t position_ = 0u;
};

void write_indent(std::ostringstream& stream, std::size_t depth) {
  for (std::size_t index = 0u; index < depth * 2u; ++index) stream.put(' ');
}

void write_string(std::ostringstream& stream, const std::string& value) {
  stream.put('"');
  for (const unsigned char character : value) {
    switch (character) {
      case '"':
        stream << "\\\"";
        break;
      case '\\':
        stream << "\\\\";
        break;
      case '\b':
        stream << "\\b";
        break;
      case '\f':
        stream << "\\f";
        break;
      case '\n':
        stream << "\\n";
        break;
      case '\r':
        stream << "\\r";
        break;
      case '\t':
        stream << "\\t";
        break;
      default:
        if (character < 0x20u) {
          stream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                 << static_cast<unsigned int>(character) << std::dec << std::setfill(' ');
        } else {
          stream.put(static_cast<char>(character));
        }
        break;
    }
  }
  stream.put('"');
}

void write_value(std::ostringstream& stream, const Json& value, std::size_t depth) {
  switch (value.type) {
    case Json::Type::Null:
      stream << "null";
      break;
    case Json::Type::Boolean:
      stream << (value.boolean ? "true" : "false");
      break;
    case Json::Type::Number:
      stream << value.text;
      break;
    case Json::Type::String:
      write_string(stream, value.text);
      break;
    case Json::Type::Array:
      stream << '[';
      if (!value.array.empty()) {
        stream << '\n';
        for (std::size_t index = 0u; index < value.array.size(); ++index) {
          write_indent(stream, depth + 1u);
          write_value(stream, value.array[index], depth + 1u);
          stream << (index + 1u == value.array.size() ? "\n" : ",\n");
        }
        write_indent(stream, depth);
      }
      stream << ']';
      break;
    case Json::Type::Object:
      stream << '{';
      if (!value.object.empty()) {
        stream << '\n';
        std::size_t index = 0u;
        for (const auto& member : value.object) {
          write_indent(stream, depth + 1u);
          write_string(stream, member.first);
          stream << ": ";
          write_value(stream, member.second, depth + 1u);
          stream << (++index == value.object.size() ? "\n" : ",\n");
        }
        write_indent(stream, depth);
      }
      stream << '}';
      break;
  }
}

}  // namespace

Json parse_json(const std::string& source) { return Parser(source).parse(); }

std::string write_json(const Json& value) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  write_value(stream, value, 0u);
  stream << '\n';
  return stream.str();
}

const Json& require_member(const Json& object, const std::string& name) {
  if (object.type != Json::Type::Object) throw std::runtime_error("expected JSON object");
  const auto found = object.object.find(name);
  if (found == object.object.end()) throw std::runtime_error("missing JSON member: " + name);
  return found->second;
}

const Json* optional_member(const Json& object, const std::string& name) {
  if (object.type != Json::Type::Object) throw std::runtime_error("expected JSON object");
  const auto found = object.object.find(name);
  return found == object.object.end() ? nullptr : &found->second;
}

std::uint64_t json_u64(const Json& value, std::uint64_t maximum) {
  if (value.type != Json::Type::Number || value.text.empty() || value.text[0] == '-' ||
      value.text.find_first_of(".eE") != std::string::npos)
    throw std::runtime_error("expected unsigned integer");
  std::size_t used = 0u;
  const unsigned long long parsed = std::stoull(value.text, &used, 10);
  if (used != value.text.size() || parsed > maximum)
    throw std::runtime_error("integer out of range");
  return static_cast<std::uint64_t>(parsed);
}

std::int64_t json_i64(const Json& value, std::int64_t minimum, std::int64_t maximum) {
  if (value.type != Json::Type::Number || value.text.find_first_of(".eE") != std::string::npos)
    throw std::runtime_error("expected integer");
  std::size_t used = 0u;
  const long long parsed = std::stoll(value.text, &used, 10);
  if (used != value.text.size() || parsed < minimum || parsed > maximum)
    throw std::runtime_error("integer out of range");
  return static_cast<std::int64_t>(parsed);
}

double json_double(const Json& value, double minimum, double maximum) {
  if (value.type != Json::Type::Number) throw std::runtime_error("expected number");
  std::size_t used = 0u;
  const double parsed = std::stod(value.text, &used);
  if (used != value.text.size() || !std::isfinite(parsed) || parsed < minimum || parsed > maximum)
    throw std::runtime_error("number out of range");
  return parsed;
}

bool json_bool(const Json& value) {
  if (value.type != Json::Type::Boolean) throw std::runtime_error("expected boolean");
  return value.boolean;
}

const std::string& json_string(const Json& value) {
  if (value.type != Json::Type::String) throw std::runtime_error("expected string");
  return value.text;
}

}  // namespace molseq
