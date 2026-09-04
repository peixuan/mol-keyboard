// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_APPS_DESKTOP_WEB_SERVER_HPP
#define MOL_APPS_DESKTOP_WEB_SERVER_HPP

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace moldesktop {

class WebServer {
 public:
  WebServer();
  ~WebServer();

  WebServer(const WebServer&) = delete;
  WebServer& operator=(const WebServer&) = delete;

  bool Start(const std::filesystem::path& web_root, std::uint16_t requested_port,
             std::string& error);
  void Stop();
  std::uint16_t port() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace moldesktop

#endif
