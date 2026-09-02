// SPDX-License-Identifier: Apache-2.0
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

#include "jsonrpc.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  molcontrol::JsonRpcDispatcher dispatcher;
  (void)dispatcher.add_method("echo", [](const molseq::Json& params) { return params; });
  (void)dispatcher.add_method("invalid", [](const molseq::Json&) -> molseq::Json {
    throw molcontrol::RpcError(-32602, "Invalid params");
  });
  (void)dispatcher.add_method("failure", [](const molseq::Json&) -> molseq::Json {
    throw std::runtime_error("private detail");
  });
  const std::string request(reinterpret_cast<const char*>(data), size);
  const std::string first = dispatcher.dispatch(request);
  const std::string second = dispatcher.dispatch(request);
  if (first != second || first.size() > molcontrol::kMaxRpcRequestBytes * 4u) __builtin_trap();
  if (!first.empty()) {
    try {
      const molseq::Json response = molseq::parse_json(first);
      if (response.type != molseq::Json::Type::Object &&
          response.type != molseq::Json::Type::Array) {
        __builtin_trap();
      }
    } catch (const std::exception&) {
      __builtin_trap();
    }
  }
  return 0;
}
