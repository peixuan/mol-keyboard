// SPDX-License-Identifier: Apache-2.0
#include "loopback_websocket.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <random>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#endif

namespace molcontrol {
namespace {

#if defined(_WIN32)
using Socket = SOCKET;
constexpr Socket kInvalidSocket = INVALID_SOCKET;
#else
using Socket = int;
constexpr Socket kInvalidSocket = -1;
#endif

constexpr std::size_t kMaximumHttpHeaderBytes = 8192u;
constexpr std::string_view kWebSocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

void close_socket(Socket socket) {
  if (socket == kInvalidSocket) return;
#if defined(_WIN32)
  (void)closesocket(socket);
#else
  (void)::close(socket);
#endif
}

std::string socket_error(const char* operation) {
#if defined(_WIN32)
  return std::string(operation) + " failed with Winsock error " +
         std::to_string(static_cast<unsigned long>(WSAGetLastError()));
#else
  return std::string(operation) + " failed: " + std::strerror(errno);
#endif
}

int wait_readable(Socket socket, int timeout_ms) {
  fd_set descriptors;
  FD_ZERO(&descriptors);
  FD_SET(socket, &descriptors);
  timeval timeout{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
#if defined(_WIN32)
  return ::select(0, &descriptors, nullptr, nullptr, &timeout);
#else
  return ::select(socket + 1, &descriptors, nullptr, nullptr, &timeout);
#endif
}

bool receive_exact(Socket socket, void* output, std::size_t size) {
  auto* bytes = static_cast<unsigned char*>(output);
  while (size != 0u) {
    const int chunk = static_cast<int>(std::min<std::size_t>(size, 16384u));
    const int received = ::recv(socket, reinterpret_cast<char*>(bytes), chunk, 0);
    if (received <= 0) return false;
    bytes += static_cast<std::size_t>(received);
    size -= static_cast<std::size_t>(received);
  }
  return true;
}

bool send_exact(Socket socket, const void* input, std::size_t size) {
  const auto* bytes = static_cast<const unsigned char*>(input);
  while (size != 0u) {
    const int chunk = static_cast<int>(std::min<std::size_t>(size, 16384u));
#if defined(MSG_NOSIGNAL)
    const int sent = ::send(socket, reinterpret_cast<const char*>(bytes), chunk, MSG_NOSIGNAL);
#else
    const int sent = ::send(socket, reinterpret_cast<const char*>(bytes), chunk, 0);
#endif
    if (sent <= 0) return false;
    bytes += static_cast<std::size_t>(sent);
    size -= static_cast<std::size_t>(sent);
  }
  return true;
}

std::string trim(std::string_view value) {
  while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1u);
  while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1u);
  return std::string(value);
}

std::string ascii_lower(std::string_view value) {
  std::string result(value);
  std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return result;
}

bool contains_header_token(std::string_view value, std::string_view expected) {
  while (!value.empty()) {
    const std::size_t comma = value.find(',');
    const std::string token = ascii_lower(trim(value.substr(0u, comma)));
    if (token == expected) return true;
    if (comma == std::string_view::npos) break;
    value.remove_prefix(comma + 1u);
  }
  return false;
}

std::uint32_t rotate_left(std::uint32_t value, unsigned int count) {
  return (value << count) | (value >> (32u - count));
}

std::array<unsigned char, 20u> sha1(std::string_view source) {
  std::vector<unsigned char> message(source.begin(), source.end());
  const std::uint64_t bit_count = static_cast<std::uint64_t>(message.size()) * 8u;
  message.push_back(0x80u);
  while ((message.size() % 64u) != 56u) message.push_back(0u);
  for (int shift = 56; shift >= 0; shift -= 8)
    message.push_back(static_cast<unsigned char>((bit_count >> shift) & 0xffu));

  std::uint32_t h0 = UINT32_C(0x67452301);
  std::uint32_t h1 = UINT32_C(0xefcdab89);
  std::uint32_t h2 = UINT32_C(0x98badcfe);
  std::uint32_t h3 = UINT32_C(0x10325476);
  std::uint32_t h4 = UINT32_C(0xc3d2e1f0);
  for (std::size_t offset = 0u; offset < message.size(); offset += 64u) {
    std::array<std::uint32_t, 80u> words{};
    for (std::size_t index = 0u; index < 16u; ++index) {
      const std::size_t byte = offset + index * 4u;
      words[index] = (static_cast<std::uint32_t>(message[byte]) << 24u) |
                     (static_cast<std::uint32_t>(message[byte + 1u]) << 16u) |
                     (static_cast<std::uint32_t>(message[byte + 2u]) << 8u) |
                     static_cast<std::uint32_t>(message[byte + 3u]);
    }
    for (std::size_t index = 16u; index < words.size(); ++index)
      words[index] = rotate_left(
          words[index - 3u] ^ words[index - 8u] ^ words[index - 14u] ^ words[index - 16u], 1u);
    std::uint32_t a = h0;
    std::uint32_t b = h1;
    std::uint32_t c = h2;
    std::uint32_t d = h3;
    std::uint32_t e = h4;
    for (std::size_t index = 0u; index < words.size(); ++index) {
      std::uint32_t function = 0u;
      std::uint32_t constant = 0u;
      if (index < 20u) {
        function = (b & c) | ((~b) & d);
        constant = UINT32_C(0x5a827999);
      } else if (index < 40u) {
        function = b ^ c ^ d;
        constant = UINT32_C(0x6ed9eba1);
      } else if (index < 60u) {
        function = (b & c) | (b & d) | (c & d);
        constant = UINT32_C(0x8f1bbcdc);
      } else {
        function = b ^ c ^ d;
        constant = UINT32_C(0xca62c1d6);
      }
      const std::uint32_t temporary = rotate_left(a, 5u) + function + e + constant + words[index];
      e = d;
      d = c;
      c = rotate_left(b, 30u);
      b = a;
      a = temporary;
    }
    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
    h4 += e;
  }
  const std::array<std::uint32_t, 5u> words = {h0, h1, h2, h3, h4};
  std::array<unsigned char, 20u> digest{};
  for (std::size_t index = 0u; index < words.size(); ++index) {
    digest[index * 4u] = static_cast<unsigned char>(words[index] >> 24u);
    digest[index * 4u + 1u] = static_cast<unsigned char>(words[index] >> 16u);
    digest[index * 4u + 2u] = static_cast<unsigned char>(words[index] >> 8u);
    digest[index * 4u + 3u] = static_cast<unsigned char>(words[index]);
  }
  return digest;
}

std::string base64(const unsigned char* input, std::size_t size) {
  constexpr std::string_view alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;
  result.reserve(((size + 2u) / 3u) * 4u);
  for (std::size_t offset = 0u; offset < size; offset += 3u) {
    const std::uint32_t first = input[offset];
    const std::uint32_t second = offset + 1u < size ? input[offset + 1u] : 0u;
    const std::uint32_t third = offset + 2u < size ? input[offset + 2u] : 0u;
    const std::uint32_t value = (first << 16u) | (second << 8u) | third;
    result.push_back(alphabet[(value >> 18u) & 0x3fu]);
    result.push_back(alphabet[(value >> 12u) & 0x3fu]);
    result.push_back(offset + 1u < size ? alphabet[(value >> 6u) & 0x3fu] : '=');
    result.push_back(offset + 2u < size ? alphabet[value & 0x3fu] : '=');
  }
  return result;
}

bool constant_time_equal(std::string_view left, std::string_view right) {
  std::size_t difference = left.size() ^ right.size();
  const std::size_t count = std::max(left.size(), right.size());
  for (std::size_t index = 0u; index < count; ++index) {
    const unsigned char a = index < left.size() ? static_cast<unsigned char>(left[index]) : 0u;
    const unsigned char b = index < right.size() ? static_cast<unsigned char>(right[index]) : 0u;
    difference |= static_cast<std::size_t>(a ^ b);
  }
  return difference == 0u;
}

bool valid_origin(std::string_view origin) {
  const bool scheme = origin.rfind("http://", 0u) == 0u || origin.rfind("https://", 0u) == 0u;
  return scheme && origin.size() <= 256u &&
         origin.find_first_of("\r\n?#") == std::string_view::npos &&
         origin.find('/', origin.find("//") + 2u) == std::string_view::npos;
}

bool valid_utf8(const std::vector<unsigned char>& bytes) {
  std::size_t index = 0u;
  while (index < bytes.size()) {
    const unsigned char first = bytes[index++];
    if (first <= 0x7fu) continue;
    std::size_t continuation_count = 0u;
    std::uint32_t value = 0u;
    if (first >= 0xc2u && first <= 0xdfu) {
      continuation_count = 1u;
      value = first & 0x1fu;
    } else if (first >= 0xe0u && first <= 0xefu) {
      continuation_count = 2u;
      value = first & 0x0fu;
    } else if (first >= 0xf0u && first <= 0xf4u) {
      continuation_count = 3u;
      value = first & 0x07u;
    } else {
      return false;
    }
    if (index + continuation_count > bytes.size()) return false;
    for (std::size_t count = 0u; count < continuation_count; ++count) {
      const unsigned char next = bytes[index++];
      if ((next & 0xc0u) != 0x80u) return false;
      value = (value << 6u) | (next & 0x3fu);
    }
    if ((continuation_count == 2u && value < 0x800u) ||
        (continuation_count == 3u && value < 0x10000u) || value > 0x10ffffu ||
        (value >= 0xd800u && value <= 0xdfffu))
      return false;
  }
  return true;
}

bool send_http_response(Socket socket, std::string_view status) {
  const std::string response = "HTTP/1.1 " + std::string(status) +
                               "\r\nConnection: close\r\nContent-Length: 0\r\n"
                               "Cache-Control: no-store\r\n\r\n";
  return send_exact(socket, response.data(), response.size());
}

bool send_frame(Socket socket, unsigned char opcode, std::string_view payload) {
  if (payload.size() > kMaximumWebSocketMessageBytes) return false;
  std::array<unsigned char, 10u> header{};
  header[0] = static_cast<unsigned char>(0x80u | opcode);
  std::size_t header_size = 2u;
  if (payload.size() <= 125u) {
    header[1] = static_cast<unsigned char>(payload.size());
  } else if (payload.size() <= 65535u) {
    header[1] = 126u;
    header[2] = static_cast<unsigned char>(payload.size() >> 8u);
    header[3] = static_cast<unsigned char>(payload.size());
    header_size = 4u;
  } else {
    header[1] = 127u;
    const std::uint64_t size = static_cast<std::uint64_t>(payload.size());
    for (std::size_t index = 0u; index < 8u; ++index)
      header[2u + index] = static_cast<unsigned char>(size >> ((7u - index) * 8u));
    header_size = 10u;
  }
  return send_exact(socket, header.data(), header_size) &&
         (payload.empty() || send_exact(socket, payload.data(), payload.size()));
}

bool send_close(Socket socket, std::uint16_t code) {
  std::array<char, 2u> payload = {static_cast<char>(code >> 8u), static_cast<char>(code)};
  return send_frame(socket, 0x08u, std::string_view(payload.data(), payload.size()));
}

struct Frame {
  unsigned char opcode = 0u;
  std::vector<unsigned char> payload;
};

bool receive_frame(Socket socket, Frame& frame) {
  std::array<unsigned char, 2u> header{};
  if (!receive_exact(socket, header.data(), header.size())) return false;
  if ((header[0] & 0x80u) == 0u || (header[0] & 0x70u) != 0u || (header[1] & 0x80u) == 0u)
    return false;
  frame.opcode = static_cast<unsigned char>(header[0] & 0x0fu);
  std::uint64_t size = header[1] & 0x7fu;
  if (size == 126u) {
    std::array<unsigned char, 2u> extended{};
    if (!receive_exact(socket, extended.data(), extended.size())) return false;
    size = (static_cast<std::uint64_t>(extended[0]) << 8u) | extended[1];
    if (size < 126u) return false;
  } else if (size == 127u) {
    std::array<unsigned char, 8u> extended{};
    if (!receive_exact(socket, extended.data(), extended.size()) || (extended[0] & 0x80u) != 0u)
      return false;
    size = 0u;
    for (const unsigned char value : extended) size = (size << 8u) | value;
    if (size <= 65535u) return false;
  }
  const bool control = frame.opcode >= 0x08u;
  if (size > kMaximumWebSocketMessageBytes || (control && size > 125u)) return false;
  std::array<unsigned char, 4u> mask{};
  if (!receive_exact(socket, mask.data(), mask.size())) return false;
  frame.payload.assign(static_cast<std::size_t>(size), 0u);
  if (!frame.payload.empty() && !receive_exact(socket, frame.payload.data(), frame.payload.size()))
    return false;
  for (std::size_t index = 0u; index < frame.payload.size(); ++index)
    frame.payload[index] ^= mask[index % mask.size()];
  return true;
}

struct Handshake {
  std::string key;
  std::string origin;
  std::string token;
};

bool receive_handshake(Socket socket, Handshake& handshake, std::string& failure_status) {
  std::string source;
  source.reserve(1024u);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (source.find("\r\n\r\n") == std::string::npos) {
    if (source.size() >= kMaximumHttpHeaderBytes || std::chrono::steady_clock::now() >= deadline) {
      failure_status = "431 Request Header Fields Too Large";
      return false;
    }
    const int ready = wait_readable(socket, 100);
    if (ready < 0) return false;
    if (ready == 0) continue;
    std::array<char, 1024u> bytes{};
    const int received = ::recv(socket, bytes.data(), static_cast<int>(bytes.size()), 0);
    if (received <= 0) return false;
    source.append(bytes.data(), static_cast<std::size_t>(received));
  }
  const std::size_t end = source.find("\r\n\r\n");
  if (end + 4u != source.size()) {
    failure_status = "400 Bad Request";
    return false;
  }
  const std::size_t first_line_end = source.find("\r\n");
  if (first_line_end == std::string::npos) return false;
  const std::string_view first_line(source.data(), first_line_end);
  constexpr std::string_view prefix = "GET ";
  constexpr std::string_view suffix = " HTTP/1.1";
  if (first_line.rfind(prefix, 0u) != 0u || first_line.size() <= prefix.size() + suffix.size() ||
      first_line.substr(first_line.size() - suffix.size()) != suffix) {
    failure_status = "400 Bad Request";
    return false;
  }
  const std::string_view target =
      first_line.substr(prefix.size(), first_line.size() - prefix.size() - suffix.size());
  constexpr std::string_view path = "/control?token=";
  if (target.rfind(path, 0u) != 0u || target.size() <= path.size() ||
      target.find('&', path.size()) != std::string_view::npos) {
    failure_status = "401 Unauthorized";
    return false;
  }
  handshake.token = std::string(target.substr(path.size()));

  std::map<std::string, std::string> headers;
  std::size_t offset = first_line_end + 2u;
  while (offset < end) {
    const std::size_t line_end = source.find("\r\n", offset);
    if (line_end == std::string::npos || line_end > end) return false;
    const std::string_view line(source.data() + offset, line_end - offset);
    const std::size_t colon = line.find(':');
    if (colon == std::string_view::npos) {
      failure_status = "400 Bad Request";
      return false;
    }
    const std::string name = ascii_lower(trim(line.substr(0u, colon)));
    if (name.empty() || headers.find(name) != headers.end()) {
      failure_status = "400 Bad Request";
      return false;
    }
    headers.emplace(name, trim(line.substr(colon + 1u)));
    offset = line_end + 2u;
  }
  const auto upgrade = headers.find("upgrade");
  const auto connection = headers.find("connection");
  const auto version = headers.find("sec-websocket-version");
  const auto key = headers.find("sec-websocket-key");
  const auto origin = headers.find("origin");
  if (upgrade == headers.end() || ascii_lower(upgrade->second) != "websocket" ||
      connection == headers.end() || !contains_header_token(connection->second, "upgrade") ||
      version == headers.end() || version->second != "13" || key == headers.end() ||
      key->second.size() != 24u || origin == headers.end()) {
    failure_status = "400 Bad Request";
    return false;
  }
  for (const unsigned char character : key->second) {
    if (!(std::isalnum(character) != 0 || character == '+' || character == '/' ||
          character == '=')) {
      failure_status = "400 Bad Request";
      return false;
    }
  }
  handshake.key = key->second;
  handshake.origin = origin->second;
  return true;
}

}  // namespace

class LoopbackWebSocketServer::Impl {
 public:
  ~Impl() { stop(); }

  bool start(const LoopbackWebSocketOptions& options, Handler handler,
             NotificationPoller notification_poller, const std::atomic<bool>& stop_requested,
             std::string& error);
  void stop();
  void run();
  void serve_client(Socket client);

  Socket listener = kInvalidSocket;
  std::uint16_t bound_port = 0u;
  std::string token;
  std::vector<std::string> origins;
  Handler handler;
  NotificationPoller notification_poller;
  const std::atomic<bool>* external_stop = nullptr;
  std::atomic<bool> local_stop{false};
  std::thread worker;
#if defined(_WIN32)
  bool winsock_started = false;
#endif
};

bool LoopbackWebSocketServer::Impl::start(const LoopbackWebSocketOptions& options,
                                          Handler request_handler, NotificationPoller poller,
                                          const std::atomic<bool>& stop_requested,
                                          std::string& error) {
  if (listener != kInvalidSocket || !request_handler || options.token.size() < 32u ||
      options.token.size() > 128u || options.allowed_origins.empty() ||
      options.allowed_origins.size() > 16u ||
      !std::all_of(options.allowed_origins.begin(), options.allowed_origins.end(), valid_origin)) {
    error = "invalid WebSocket options, handler, token, or origin allowlist";
    return false;
  }
#if defined(_WIN32)
  WSADATA data{};
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
    error = socket_error("WSAStartup");
    return false;
  }
  winsock_started = true;
#endif
  listener = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == kInvalidSocket) {
    error = socket_error("create WebSocket listener");
    stop();
    return false;
  }
  const int reuse = 1;
  (void)::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse),
                     static_cast<int>(sizeof(reuse)));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(options.port);
  if (::bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0 ||
      ::listen(listener, 8) != 0) {
    error = socket_error("bind or listen on loopback WebSocket");
    stop();
    return false;
  }
  sockaddr_in actual{};
#if defined(_WIN32)
  int actual_size = static_cast<int>(sizeof(actual));
#else
  socklen_t actual_size = sizeof(actual);
#endif
  if (::getsockname(listener, reinterpret_cast<sockaddr*>(&actual), &actual_size) != 0) {
    error = socket_error("read loopback WebSocket port");
    stop();
    return false;
  }
  bound_port = ntohs(actual.sin_port);
  token = options.token;
  origins = options.allowed_origins;
  handler = std::move(request_handler);
  notification_poller = std::move(poller);
  external_stop = &stop_requested;
  local_stop.store(false, std::memory_order_release);
  worker = std::thread(&Impl::run, this);
  return true;
}

void LoopbackWebSocketServer::Impl::stop() {
  local_stop.store(true, std::memory_order_release);
  if (worker.joinable()) worker.join();
  close_socket(listener);
  listener = kInvalidSocket;
  bound_port = 0u;
#if defined(_WIN32)
  if (winsock_started) {
    (void)WSACleanup();
    winsock_started = false;
  }
#endif
}

void LoopbackWebSocketServer::Impl::run() {
  while (!local_stop.load(std::memory_order_acquire) &&
         !external_stop->load(std::memory_order_acquire)) {
    const int ready = wait_readable(listener, 50);
    if (ready <= 0) continue;
    sockaddr_in peer{};
#if defined(_WIN32)
    int peer_size = static_cast<int>(sizeof(peer));
#else
    socklen_t peer_size = sizeof(peer);
#endif
    const Socket client = ::accept(listener, reinterpret_cast<sockaddr*>(&peer), &peer_size);
    if (client == kInvalidSocket) continue;
    if (ntohl(peer.sin_addr.s_addr) == INADDR_LOOPBACK) serve_client(client);
    close_socket(client);
  }
}

void LoopbackWebSocketServer::Impl::serve_client(Socket client) {
#if defined(_WIN32)
  const DWORD timeout = 2000u;
  (void)::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
                     static_cast<int>(sizeof(timeout)));
  (void)::setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout),
                     static_cast<int>(sizeof(timeout)));
#else
  timeval timeout{2, 0};
  (void)::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  (void)::setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
  Handshake handshake;
  std::string failure_status = "400 Bad Request";
  if (!receive_handshake(client, handshake, failure_status)) {
    (void)send_http_response(client, failure_status);
    return;
  }
  if (!constant_time_equal(handshake.token, token)) {
    (void)send_http_response(client, "401 Unauthorized");
    return;
  }
  if (std::find(origins.begin(), origins.end(), handshake.origin) == origins.end()) {
    (void)send_http_response(client, "403 Forbidden");
    return;
  }
  const std::string accept_source = handshake.key + std::string(kWebSocketGuid);
  const std::array<unsigned char, 20u> digest = sha1(accept_source);
  const std::string response =
      "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
      "Sec-WebSocket-Accept: " +
      base64(digest.data(), digest.size()) + "\r\n\r\n";
  if (!send_exact(client, response.data(), response.size())) return;

  while (!local_stop.load(std::memory_order_acquire) &&
         !external_stop->load(std::memory_order_acquire)) {
    if (notification_poller) {
      const std::string notification = notification_poller();
      if (!notification.empty() && !send_frame(client, 0x01u, notification)) return;
    }
    const int ready = wait_readable(client, 10);
    if (ready < 0) return;
    if (ready == 0) continue;
    Frame frame;
    if (!receive_frame(client, frame)) {
      (void)send_close(client, 1002u);
      return;
    }
    if (frame.opcode == 0x08u) {
      (void)send_close(client, 1000u);
      return;
    }
    if (frame.opcode == 0x09u) {
      const std::string_view payload(reinterpret_cast<const char*>(frame.payload.data()),
                                     frame.payload.size());
      if (!send_frame(client, 0x0au, payload)) return;
      continue;
    }
    if (frame.opcode == 0x0au) continue;
    if (frame.opcode != 0x01u || !valid_utf8(frame.payload)) {
      (void)send_close(client, frame.opcode == 0x02u ? 1003u : 1007u);
      return;
    }
    const std::string request(reinterpret_cast<const char*>(frame.payload.data()),
                              frame.payload.size());
    const std::string reply = handler(request);
    if (!reply.empty() && !send_frame(client, 0x01u, reply)) return;
  }
  (void)send_close(client, 1001u);
}

LoopbackWebSocketServer::LoopbackWebSocketServer() : impl_(std::make_unique<Impl>()) {}

LoopbackWebSocketServer::~LoopbackWebSocketServer() = default;

bool LoopbackWebSocketServer::start(const LoopbackWebSocketOptions& options, Handler handler,
                                    NotificationPoller notification_poller,
                                    const std::atomic<bool>& stop_requested, std::string& error) {
  return impl_->start(options, std::move(handler), std::move(notification_poller), stop_requested,
                      error);
}

void LoopbackWebSocketServer::stop() { impl_->stop(); }

std::uint16_t LoopbackWebSocketServer::port() const noexcept { return impl_->bound_port; }

std::string generate_websocket_token() {
  std::random_device random;
  constexpr char alphabet[] = "0123456789abcdef";
  std::string token(64u, '0');
  for (std::size_t index = 0u; index < token.size(); ++index)
    token[index] = alphabet[random() & 0x0fu];
  return token;
}

}  // namespace molcontrol
