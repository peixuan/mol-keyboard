// SPDX-License-Identifier: Apache-2.0
#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>

#include "json.hpp"
#include "service_backend.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size > 65536u) return 0;
  std::string source;
  if (size != 0u) source.assign(reinterpret_cast<const char*>(data), size);
  try {
    const molseq::Json first = molcontrol::parse_service_config(source);
    const std::string normalized = molseq::write_json(first);
    const molseq::Json second = molcontrol::parse_service_config(normalized);
    if (molseq::write_json(second) != normalized) __builtin_trap();
  } catch (const std::exception&) {
  }
  return 0;
}
