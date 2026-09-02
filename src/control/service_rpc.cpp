// SPDX-License-Identifier: Apache-2.0
#include "service_rpc.hpp"

#include <utility>

namespace molcontrol {

bool register_service_methods(JsonRpcDispatcher& dispatcher, RpcBackend& backend) {
  for (const std::string_view method : kRequiredRpcMethods) {
    const std::string owned_method(method);
    if (!dispatcher.add_method(owned_method, [&backend, owned_method](const molseq::Json& params) {
          return backend.invoke(owned_method, params);
        })) {
      return false;
    }
  }
  return true;
}

}  // namespace molcontrol
