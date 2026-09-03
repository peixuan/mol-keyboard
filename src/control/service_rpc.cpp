// SPDX-License-Identifier: Apache-2.0
#include "service_rpc.hpp"

#include <utility>

namespace molcontrol {

bool register_service_methods(JsonRpcDispatcher& dispatcher, RpcBackend& backend) {
  for (const std::string_view method : kRequiredRpcMethods) {
    if (!dispatcher.add_method(std::string(method), [&backend, method](const molseq::Json& params) {
          return backend.invoke(method, params);
        })) {
      return false;
    }
  }
  return true;
}

}  // namespace molcontrol
