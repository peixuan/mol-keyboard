// SPDX-License-Identifier: Apache-2.0
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>

#include "local_ipc.hpp"

int main() {
  const std::string unique =
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
#if defined(_WIN32)
  const std::string endpoint = R"(\\.\pipe\mol-keyboard-test-)" + unique;
#else
  const std::string endpoint =
      (std::filesystem::temp_directory_path() / ("mol-keyboard-test-" + unique + ".sock")).string();
#endif
  std::atomic<bool> stop{false};
  std::string server_error;
  bool server_result = false;
  std::thread server([&] {
    molcontrol::LocalIpcServer instance;
    server_result = instance.serve(
        endpoint,
        [&stop](const std::string& request) {
          if (request == "stop") stop.store(true, std::memory_order_release);
          return "response:" + request;
        },
        stop, server_error);
  });

  std::string response;
  std::string client_error;
  bool exchanged = false;
  for (int attempt = 0; attempt < 200 && !exchanged; ++attempt) {
    exchanged = molcontrol::send_local_ipc_request(endpoint, "hello", response, client_error);
    if (!exchanged) std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  if (!exchanged || response != "response:hello" ||
      !molcontrol::send_local_ipc_request(endpoint, "stop", response, client_error) ||
      response != "response:stop") {
    stop.store(true, std::memory_order_release);
    server.join();
    std::fprintf(stderr, "IPC exchange failed: %s; server: %s\n", client_error.c_str(),
                 server_error.c_str());
    return 1;
  }
  server.join();
  if (!server_result) {
    std::fprintf(stderr, "IPC server failed: %s\n", server_error.c_str());
    return 1;
  }
  return 0;
}
