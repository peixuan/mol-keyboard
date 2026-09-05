// SPDX-License-Identifier: Apache-2.0
#include "desktop_rpc_client.hpp"

#include <exception>
#include <utility>

#include "local_ipc.hpp"

namespace moldesktop {

RpcClient::RpcClient(std::string endpoint) : endpoint_(std::move(endpoint)) {}

void RpcClient::set_endpoint(std::string endpoint) { endpoint_ = std::move(endpoint); }

const std::string& RpcClient::endpoint() const { return endpoint_; }

RpcResult RpcClient::Call(const std::string& method, const molseq::Json& params) {
  RpcResult result;
  if (endpoint_.empty()) {
    result.error = "the local IPC endpoint is empty";
    return result;
  }
  molseq::Json::Object request;
  request["id"] = molseq::Json::number(next_id_++);
  request["jsonrpc"] = molseq::Json::string("2.0");
  request["method"] = molseq::Json::string(method);
  request["params"] = params;
  const std::string request_text =
      molseq::write_json(molseq::Json::object_value(std::move(request)));
  if (!molcontrol::send_local_ipc_request(endpoint_, request_text, result.response,
                                          result.error)) {
    return result;
  }
  try {
    const molseq::Json response = molseq::parse_json(result.response);
    const molseq::Json* rpc_error = molseq::optional_member(response, "error");
    if (rpc_error != nullptr) {
      result.error = "service error: " + molseq::write_json(*rpc_error);
      return result;
    }
    result.value = molseq::require_member(response, "result");
    result.ok = true;
  } catch (const std::exception& error) {
    result.error = std::string("invalid service response: ") + error.what();
  }
  return result;
}

}  // namespace moldesktop
