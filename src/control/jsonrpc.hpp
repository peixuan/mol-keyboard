// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_CONTROL_JSONRPC_HPP_
#define MOL_CONTROL_JSONRPC_HPP_

#include <cstddef>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>

#include "json.hpp"

namespace molcontrol {

constexpr std::size_t kMaxRpcRequestBytes = 65536u;
constexpr std::size_t kMaxRpcBatchSize = 64u;
constexpr std::size_t kMaxRpcMethodBytes = 96u;
constexpr std::size_t kMaxRpcMethods = 64u;

class RpcError : public std::runtime_error {
 public:
  RpcError(int code, const std::string& message) : std::runtime_error(message), code_(code) {}
  [[nodiscard]] int code() const noexcept { return code_; }

 private:
  int code_;
};

class JsonRpcDispatcher {
 public:
  using Handler = std::function<molseq::Json(const molseq::Json& params)>;

  bool add_method(const std::string& method, Handler handler);
  [[nodiscard]] std::string dispatch(const std::string& request) const;
  [[nodiscard]] std::size_t method_count() const noexcept { return methods_.size(); }

 private:
  std::map<std::string, Handler> methods_;
};

}  // namespace molcontrol

#endif  // MOL_CONTROL_JSONRPC_HPP_
