// SPDX-License-Identifier: Apache-2.0
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "service_rpc.hpp"

namespace {

class RecordingBackend final : public molcontrol::RpcBackend {
 public:
  molseq::Json invoke(std::string_view method, const molseq::Json& params) override {
    calls.emplace_back(method);
    molseq::Json::Object result;
    result["method"] = molseq::Json::string(std::string(method));
    result["params"] = params;
    return molseq::Json::object_value(std::move(result));
  }

  std::vector<std::string> calls;
};

}  // namespace

int main() {
  molcontrol::JsonRpcDispatcher dispatcher;
  RecordingBackend backend;
  if (!molcontrol::register_service_methods(dispatcher, backend) ||
      dispatcher.method_count() != molcontrol::kRequiredRpcMethods.size()) {
    std::fprintf(stderr, "Could not register the required RPC method surface\n");
    return 1;
  }
  std::size_t id = 1u;
  for (const std::string_view method : molcontrol::kRequiredRpcMethods) {
    const std::string request = "{\"jsonrpc\":\"2.0\",\"method\":\"" + std::string(method) +
                                "\",\"params\":{},\"id\":" + std::to_string(id++) + "}";
    const molseq::Json response = molseq::parse_json(dispatcher.dispatch(request));
    const molseq::Json& result = molseq::require_member(response, "result");
    if (molseq::json_string(molseq::require_member(result, "method")) != method) {
      std::fprintf(stderr, "RPC method dispatch mismatch: %s\n", std::string(method).c_str());
      return 1;
    }
  }
  if (backend.calls.size() != molcontrol::kRequiredRpcMethods.size()) {
    std::fprintf(stderr, "Expected %zu RPC calls, received %zu\n",
                 molcontrol::kRequiredRpcMethods.size(), backend.calls.size());
    return 1;
  }
  return 0;
}
