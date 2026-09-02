// SPDX-License-Identifier: Apache-2.0
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#endif

#include "local_ipc.hpp"

namespace {

bool send_incomplete_frame(const std::string& endpoint) {
  const std::array<unsigned char, 2u> fragment{8u, 0u};
#if defined(_WIN32)
  HANDLE pipe = INVALID_HANDLE_VALUE;
  const ULONGLONG deadline = GetTickCount64() + 2000u;
  while (GetTickCount64() < deadline) {
    if (WaitNamedPipeA(endpoint.c_str(), 50u) != FALSE) {
      pipe = CreateFileA(endpoint.c_str(), GENERIC_WRITE, 0u, nullptr, OPEN_EXISTING, 0u, nullptr);
      if (pipe != INVALID_HANDLE_VALUE) break;
    }
    Sleep(5u);
  }
  if (pipe == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0u;
  const bool success = WriteFile(pipe, fragment.data(), static_cast<DWORD>(fragment.size()),
                                 &written, nullptr) != FALSE &&
                       written == fragment.size();
  CloseHandle(pipe);
  return success;
#else
  const int descriptor = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (descriptor < 0) return false;
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  std::memcpy(address.sun_path, endpoint.c_str(), endpoint.size() + 1u);
  const bool success =
      ::connect(descriptor, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0 &&
      ::send(descriptor, fragment.data(), fragment.size(), 0) ==
          static_cast<ssize_t>(fragment.size());
  (void)::close(descriptor);
  return success;
#endif
}

}  // namespace

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
  const bool initial = exchanged && response == "response:hello";
  const bool bad_sent = initial && send_incomplete_frame(endpoint);
  const bool recovered =
      bad_sent &&
      molcontrol::send_local_ipc_request(endpoint, "after-bad-client", response, client_error) &&
      response == "response:after-bad-client";
  const bool stopped =
      recovered && molcontrol::send_local_ipc_request(endpoint, "stop", response, client_error) &&
      response == "response:stop";
  if (!initial || !bad_sent || !recovered || !stopped) {
    stop.store(true, std::memory_order_release);
    server.join();
    std::fprintf(stderr,
                 "IPC exchange failed (initial=%d bad=%d recovered=%d stopped=%d): %s; server: "
                 "%s\n",
                 initial ? 1 : 0, bad_sent ? 1 : 0, recovered ? 1 : 0, stopped ? 1 : 0,
                 client_error.c_str(), server_error.c_str());
    return 1;
  }
  server.join();
  if (!server_result) {
    std::fprintf(stderr, "IPC server failed: %s\n", server_error.c_str());
    return 1;
  }
  return 0;
}
