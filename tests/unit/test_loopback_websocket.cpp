// SPDX-License-Identifier: Apache-2.0
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "loopback_websocket.hpp"

namespace {

#if defined(_WIN32)
using Socket = SOCKET;
constexpr Socket kInvalidSocket = INVALID_SOCKET;
#else
using Socket = int;
constexpr Socket kInvalidSocket = -1;
#endif

void close_socket(Socket socket) {
#if defined(_WIN32)
  if (socket != kInvalidSocket) (void)closesocket(socket);
#else
  if (socket != kInvalidSocket) (void)::close(socket);
#endif
}

bool send_all(Socket socket, const void* input, std::size_t size) {
  const auto* bytes = static_cast<const char*>(input);
  while (size != 0u) {
    const int sent = ::send(socket, bytes, static_cast<int>(size), 0);
    if (sent <= 0) return false;
    bytes += sent;
    size -= static_cast<std::size_t>(sent);
  }
  return true;
}

bool receive_all(Socket socket, void* output, std::size_t size) {
  auto* bytes = static_cast<char*>(output);
  while (size != 0u) {
    const int received = ::recv(socket, bytes, static_cast<int>(size), 0);
    if (received <= 0) return false;
    bytes += received;
    size -= static_cast<std::size_t>(received);
  }
  return true;
}

Socket connect_to(std::uint16_t port) {
  const Socket socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket == kInvalidSocket) return kInvalidSocket;
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(port);
  if (::connect(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
    close_socket(socket);
    return kInvalidSocket;
  }
  return socket;
}

std::string handshake(Socket socket, std::uint16_t port, std::string_view token,
                      std::string_view origin) {
  const std::string request =
      "GET /control?token=" + std::string(token) +
      " HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port) +
      "\r\nUpgrade: websocket\r\nConnection: keep-alive, Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\nOrigin: " +
      std::string(origin) + "\r\n\r\n";
  if (!send_all(socket, request.data(), request.size())) return {};
  std::string response;
  while (response.find("\r\n\r\n") == std::string::npos && response.size() < 8192u) {
    char character = '\0';
    if (!receive_all(socket, &character, 1u)) return response;
    response.push_back(character);
  }
  return response;
}

bool send_masked_frame(Socket socket, unsigned char opcode, std::string_view payload) {
  if (payload.size() > 125u) return false;
  std::vector<unsigned char> frame(6u + payload.size());
  frame[0] = static_cast<unsigned char>(0x80u | opcode);
  frame[1] = static_cast<unsigned char>(0x80u | payload.size());
  constexpr std::array<unsigned char, 4u> mask = {0x12u, 0x34u, 0x56u, 0x78u};
  std::memcpy(frame.data() + 2u, mask.data(), mask.size());
  for (std::size_t index = 0u; index < payload.size(); ++index)
    frame[6u + index] = static_cast<unsigned char>(payload[index]) ^ mask[index % mask.size()];
  return send_all(socket, frame.data(), frame.size());
}

bool receive_frame(Socket socket, unsigned char& opcode, std::string& payload) {
  std::array<unsigned char, 2u> header{};
  if (!receive_all(socket, header.data(), header.size()) || (header[0] & 0x80u) == 0u ||
      (header[1] & 0x80u) != 0u)
    return false;
  opcode = static_cast<unsigned char>(header[0] & 0x0fu);
  std::size_t size = header[1] & 0x7fu;
  if (size == 126u) {
    std::array<unsigned char, 2u> extended{};
    if (!receive_all(socket, extended.data(), extended.size())) return false;
    size = (static_cast<std::size_t>(extended[0]) << 8u) | extended[1];
  } else if (size == 127u) {
    return false;
  }
  payload.assign(size, '\0');
  return payload.empty() || receive_all(socket, payload.data(), payload.size());
}

}  // namespace

int main() {
  const std::string generated_token = molcontrol::generate_websocket_token();
  if (generated_token.size() != 64u ||
      generated_token.find_first_not_of("0123456789abcdef") != std::string::npos)
    return 1;
  std::atomic<bool> stop_requested{false};
  std::atomic<bool> notification_pending{true};
  molcontrol::LoopbackWebSocketOptions options;
  options.port = 0u;
  options.token = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  options.allowed_origins = {"http://127.0.0.1:4173"};
  molcontrol::LoopbackWebSocketServer server;
  std::string error;
  if (!server.start(
          options, [](const std::string& request) { return "reply:" + request; },
          [&notification_pending] {
            return notification_pending.exchange(false) ? std::string("notification")
                                                        : std::string();
          },
          stop_requested, error)) {
    std::fprintf(stderr, "WebSocket test server failed: %s\n", error.c_str());
    return 1;
  }

  Socket client = connect_to(server.port());
  const std::string accepted =
      handshake(client, server.port(), options.token, "http://127.0.0.1:4173");
  if (accepted.find("101 Switching Protocols") == std::string::npos ||
      accepted.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") == std::string::npos ||
      !send_masked_frame(client, 0x01u, "hello")) {
    close_socket(client);
    return 1;
  }
  bool received_reply = false;
  bool received_notification = false;
  for (int index = 0; index < 2; ++index) {
    unsigned char opcode = 0u;
    std::string payload;
    if (!receive_frame(client, opcode, payload) || opcode != 0x01u) return 1;
    received_reply = received_reply || payload == "reply:hello";
    received_notification = received_notification || payload == "notification";
  }
  if (!received_reply || !received_notification || !send_masked_frame(client, 0x08u, {})) return 1;
  close_socket(client);

  client = connect_to(server.port());
  const std::string unauthorized =
      handshake(client, server.port(), std::string(64u, 'f'), "http://127.0.0.1:4173");
  close_socket(client);
  if (unauthorized.find("401 Unauthorized") == std::string::npos) return 1;

  client = connect_to(server.port());
  const std::string forbidden =
      handshake(client, server.port(), options.token, "http://malicious.invalid");
  close_socket(client);
  if (forbidden.find("403 Forbidden") == std::string::npos) return 1;

  server.stop();
  return 0;
}
