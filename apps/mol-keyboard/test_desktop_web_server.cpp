// SPDX-License-Identifier: Apache-2.0
#include "desktop_web_server.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

#if defined(_WIN32)
using Socket = SOCKET;
constexpr Socket kInvalidSocket = INVALID_SOCKET;
void CloseSocket(Socket socket_handle) { closesocket(socket_handle); }
#else
using Socket = int;
constexpr Socket kInvalidSocket = -1;
void CloseSocket(Socket socket_handle) { close(socket_handle); }
#endif

bool Write(const std::filesystem::path& path, const std::string& value) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  output << value;
  return output.good();
}

std::string Request(std::uint16_t port, const std::string& request) {
  const Socket client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (client == kInvalidSocket) return {};
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(port);
  if (connect(client, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
    CloseSocket(client);
    return {};
  }
  if (send(client, request.data(), static_cast<int>(request.size()), 0) <= 0) {
    CloseSocket(client);
    return {};
  }
  std::string response;
  char buffer[2048];
  for (;;) {
    const int received = recv(client, buffer, static_cast<int>(sizeof(buffer)), 0);
    if (received <= 0) break;
    response.append(buffer, static_cast<std::size_t>(received));
  }
  CloseSocket(client);
  return response;
}

bool Contains(const std::string& value, const std::string& expected) {
  if (value.find(expected) != value.npos) return true;
  std::cerr << "missing response fragment: " << expected << '\n' << value << '\n';
  return false;
}

}  // namespace

int main() {
  const auto suffix = std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count());
  const auto root = std::filesystem::temp_directory_path() / ("mol-desktop-web-" + suffix);
  const bool fixture_ok = Write(root / "index.html", "desktop gui") &&
                          Write(root / "manifest.webmanifest", "{}") &&
                          Write(root / "sw.js", "") &&
                          Write(root / "generated/mol_audio_worklet_core.js", "") &&
                          Write(root / "generated/mol_audio_worklet_core.wasm", "wasm") &&
                          Write(root / "assets/app.js", "console.log('mol');");
  if (!fixture_ok) return 1;

  moldesktop::WebServer server;
  std::string error;
  if (!server.Start(root, 0U, error)) {
    std::cerr << error << '\n';
    return 1;
  }
  const auto index = Request(server.port(), "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
  const auto script =
      Request(server.port(), "HEAD /assets/app.js HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
  const auto traversal =
      Request(server.port(), "GET /../secret HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
  const auto encoded =
      Request(server.port(), "GET /%2e%2e/secret HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
  const auto post = Request(server.port(), "POST / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
  server.Stop();
  std::filesystem::remove_all(root);

  const bool passed = Contains(index, "HTTP/1.1 200 OK") && Contains(index, "desktop gui") &&
                      Contains(index, "Cross-Origin-Opener-Policy: same-origin") &&
                      Contains(index, "Cross-Origin-Embedder-Policy: require-corp") &&
                      Contains(index, "Content-Security-Policy:") &&
                      Contains(script, "Content-Type: text/javascript") &&
                      script.find("console.log") == script.npos &&
                      Contains(traversal, "HTTP/1.1 400 Bad Request") &&
                      Contains(encoded, "HTTP/1.1 400 Bad Request") &&
                      Contains(post, "HTTP/1.1 405 Method Not Allowed");
  return passed ? 0 : 1;
}
