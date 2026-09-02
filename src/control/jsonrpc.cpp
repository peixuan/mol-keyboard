// SPDX-License-Identifier: Apache-2.0
#include "jsonrpc.hpp"

#include <optional>
#include <utility>

namespace molcontrol {
namespace {

using Json = molseq::Json;

Json make_error(const Json& id, int code, const std::string& message) {
  Json::Object error;
  Json::Object response;
  error["code"] = Json::number(code);
  error["message"] = Json::string(message);
  response["error"] = Json::object_value(std::move(error));
  response["id"] = id;
  response["jsonrpc"] = Json::string("2.0");
  return Json::object_value(std::move(response));
}

Json make_result(const Json& id, Json result) {
  Json::Object response;
  response["id"] = id;
  response["jsonrpc"] = Json::string("2.0");
  response["result"] = std::move(result);
  return Json::object_value(std::move(response));
}

bool valid_id(const Json& id) {
  return id.type == Json::Type::Null || id.type == Json::Type::String ||
         id.type == Json::Type::Number;
}

bool valid_method(const std::string& method) {
  if (method.empty() || method.size() > kMaxRpcMethodBytes) return false;
  for (const unsigned char character : method)
    if (character < 0x20u) return false;
  return true;
}

bool valid_members(const Json& request) {
  for (const auto& member : request.object) {
    if (member.first != "jsonrpc" && member.first != "method" && member.first != "params" &&
        member.first != "id")
      return false;
  }
  return true;
}

std::optional<Json> dispatch_one(const JsonRpcDispatcher::Handler* handler,
                                 const std::string& method, const Json& params, const Json& id,
                                 bool notification) {
  if (handler == nullptr) {
    if (notification) return std::nullopt;
    return make_error(id, -32601, "Method not found: " + method);
  }
  try {
    Json result = (*handler)(params);
    if (notification) return std::nullopt;
    return make_result(id, std::move(result));
  } catch (const RpcError& error) {
    if (notification) return std::nullopt;
    return make_error(id, error.code(), error.what());
  } catch (const std::exception&) {
    if (notification) return std::nullopt;
    return make_error(id, -32603, "Internal error");
  }
}

std::optional<Json> process_request(
    const Json& request, const std::map<std::string, JsonRpcDispatcher::Handler>& methods) {
  Json null_id;
  Json id;
  bool notification = true;
  if (request.type != Json::Type::Object) return make_error(null_id, -32600, "Invalid Request");
  const Json* id_member = molseq::optional_member(request, "id");
  if (id_member != nullptr && valid_id(*id_member)) {
    id = *id_member;
    notification = false;
  }
  const Json* version = molseq::optional_member(request, "jsonrpc");
  const Json* method_value = molseq::optional_member(request, "method");
  const Json* params_value = molseq::optional_member(request, "params");
  if (!valid_members(request) || version == nullptr || method_value == nullptr ||
      version->type != Json::Type::String || version->text != "2.0" ||
      method_value->type != Json::Type::String || !valid_method(method_value->text) ||
      (id_member != nullptr && !valid_id(*id_member)) ||
      (params_value != nullptr && params_value->type != Json::Type::Object &&
       params_value->type != Json::Type::Array)) {
    return make_error(notification ? null_id : id, -32600, "Invalid Request");
  }
  Json empty_params = Json::object_value({});
  const Json& params = params_value == nullptr ? empty_params : *params_value;
  const auto found = methods.find(method_value->text);
  const JsonRpcDispatcher::Handler* handler = found == methods.end() ? nullptr : &found->second;
  return dispatch_one(handler, method_value->text, params, id, notification);
}

}  // namespace

bool JsonRpcDispatcher::add_method(const std::string& method, Handler handler) {
  if (!valid_method(method) || !handler || methods_.size() >= kMaxRpcMethods) return false;
  return methods_.emplace(method, std::move(handler)).second;
}

std::string JsonRpcDispatcher::dispatch(const std::string& request) const {
  Json root;
  if (request.size() > kMaxRpcRequestBytes)
    return molseq::write_json(make_error(Json{}, -32600, "Request too large"));
  try {
    root = molseq::parse_json(request);
  } catch (const std::exception&) {
    return molseq::write_json(make_error(Json{}, -32700, "Parse error"));
  }
  if (root.type != Json::Type::Array) {
    std::optional<Json> response = process_request(root, methods_);
    return response.has_value() ? molseq::write_json(*response) : std::string{};
  }
  if (root.array.empty() || root.array.size() > kMaxRpcBatchSize)
    return molseq::write_json(make_error(Json{}, -32600, "Invalid Request"));
  Json::Array responses;
  for (const Json& item : root.array) {
    std::optional<Json> response = process_request(item, methods_);
    if (response.has_value()) responses.push_back(std::move(*response));
  }
  return responses.empty() ? std::string{}
                           : molseq::write_json(Json::array_value(std::move(responses)));
}

}  // namespace molcontrol
