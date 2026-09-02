// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_CONTROL_LOCAL_IPC_HPP_
#define MOL_CONTROL_LOCAL_IPC_HPP_

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>

namespace molcontrol {

constexpr std::size_t kMaximumIpcMessageBytes = 65536u;

std::string default_local_ipc_endpoint(const std::filesystem::path& state_directory);

class LocalIpcServer {
 public:
  using Handler = std::function<std::string(const std::string&)>;

  bool serve(const std::string& endpoint, const Handler& handler,
             const std::atomic<bool>& stop_requested, std::string& error);
};

bool send_local_ipc_request(const std::string& endpoint, const std::string& request,
                            std::string& response, std::string& error);

}  // namespace molcontrol

#endif  // MOL_CONTROL_LOCAL_IPC_HPP_
