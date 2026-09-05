// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_DESKTOP_RPC_CLIENT_HPP_
#define MOL_DESKTOP_RPC_CLIENT_HPP_

#include <cstdint>
#include <string>

#include "json.hpp"

namespace moldesktop {

struct RpcResult {
  bool ok = false;
  molseq::Json value;
  std::string response;
  std::string error;
};

class RpcClient {
 public:
  explicit RpcClient(std::string endpoint = {});

  void set_endpoint(std::string endpoint);
  const std::string& endpoint() const;
  RpcResult Call(const std::string& method,
                 const molseq::Json& params = molseq::Json::object_value({}));

 private:
  std::string endpoint_;
  std::uint64_t next_id_ = 1u;
};

}  // namespace moldesktop

#endif  // MOL_DESKTOP_RPC_CLIENT_HPP_
