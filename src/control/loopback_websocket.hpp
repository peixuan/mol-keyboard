// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_CONTROL_LOOPBACK_WEBSOCKET_HPP_
#define MOL_CONTROL_LOOPBACK_WEBSOCKET_HPP_

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace molcontrol {

constexpr std::size_t kMaximumWebSocketMessageBytes = 65536u;

struct LoopbackWebSocketOptions {
  std::uint16_t port = 0u;
  std::string token;
  std::vector<std::string> allowed_origins;
};

class LoopbackWebSocketServer {
 public:
  using Handler = std::function<std::string(const std::string&)>;
  using NotificationPoller = std::function<std::string()>;

  LoopbackWebSocketServer();
  ~LoopbackWebSocketServer();
  LoopbackWebSocketServer(const LoopbackWebSocketServer&) = delete;
  LoopbackWebSocketServer& operator=(const LoopbackWebSocketServer&) = delete;

  bool start(const LoopbackWebSocketOptions& options, Handler handler,
             NotificationPoller notification_poller, const std::atomic<bool>& stop_requested,
             std::string& error);
  void stop();

  [[nodiscard]] std::uint16_t port() const noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

std::string generate_websocket_token();

}  // namespace molcontrol

#endif  // MOL_CONTROL_LOOPBACK_WEBSOCKET_HPP_
