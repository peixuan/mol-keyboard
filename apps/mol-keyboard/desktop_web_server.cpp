// SPDX-License-Identifier: Apache-2.0
#include "desktop_web_server.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <array>
#include <atomic>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string_view>
#include <thread>

namespace moldesktop {
namespace {

constexpr std::size_t kMaximumRequestBytes = 8192U;
constexpr std::size_t kMaximumAssetBytes = 16U * 1024U * 1024U;

#if defined(_WIN32)
using Socket = SOCKET;
constexpr Socket kInvalidSocket = INVALID_SOCKET;
constexpr int kShutdownBoth = SD_BOTH;
#else
using Socket = int;
constexpr Socket kInvalidSocket = -1;
constexpr int kShutdownBoth = SHUT_RDWR;
#endif

void CloseSocket(Socket socket_handle) {
#if defined(_WIN32)
  closesocket(socket_handle);
#else
  close(socket_handle);
#endif
}

bool StartSocketRuntime() {
#if defined(_WIN32)
  WSADATA data{};
  return WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
  return true;
#endif
}

void StopSocketRuntime() {
#if defined(_WIN32)
  WSACleanup();
#endif
}

bool SendAll(Socket client, std::string_view bytes) {
  while (!bytes.empty()) {
    const int flags =
#if defined(MSG_NOSIGNAL)
        MSG_NOSIGNAL;
#else
        0;
#endif
    const auto chunk = send(client, bytes.data(), static_cast<int>(bytes.size()), flags);
    if (chunk <= 0) return false;
    bytes.remove_prefix(static_cast<std::size_t>(chunk));
  }
  return true;
}

std::string_view MimeType(const std::filesystem::path& path) {
  const auto extension = path.extension().string();
  if (extension == ".html") return "text/html; charset=utf-8";
  if (extension == ".js" || extension == ".mjs") return "text/javascript; charset=utf-8";
  if (extension == ".css") return "text/css; charset=utf-8";
  if (extension == ".wasm") return "application/wasm";
  if (extension == ".json" || extension == ".webmanifest") return "application/json";
  if (extension == ".svg") return "image/svg+xml";
  if (extension == ".png") return "image/png";
  return "application/octet-stream";
}

void SendResponse(Socket client, int status, std::string_view reason, std::string_view content_type,
                  std::string_view body, bool head_only) {
  std::ostringstream headers;
  headers << "HTTP/1.1 " << status << ' ' << reason << "\r\n"
          << "Content-Type: " << content_type << "\r\n"
          << "Content-Length: " << body.size() << "\r\n"
          << "Connection: close\r\n"
          << "Cache-Control: no-cache\r\n"
          << "X-Content-Type-Options: nosniff\r\n"
          << "Cross-Origin-Opener-Policy: same-origin\r\n"
          << "Cross-Origin-Embedder-Policy: require-corp\r\n"
          << "Cross-Origin-Resource-Policy: same-origin\r\n"
          << "Content-Security-Policy: default-src 'self'; script-src 'self' 'wasm-unsafe-eval'; "
             "style-src 'self'; img-src 'self' data:; worker-src 'self'; "
             "connect-src 'self' ws: wss:\r\n\r\n";
  const std::string header_bytes = headers.str();
  if (SendAll(client, header_bytes) && !head_only) (void)SendAll(client, body);
}

bool IsSafeTarget(std::string_view target) {
  if (target.empty() || target.front() != '/' || target.find('%') != target.npos ||
      target.find('\\') != target.npos || target.find("..") != target.npos ||
      target.find("//") != target.npos) {
    return false;
  }
  for (const char raw_character : target) {
    const auto character = static_cast<unsigned char>(raw_character);
    if (!(std::isalnum(character) != 0 || character == '/' || character == '.' ||
          character == '-' || character == '_' || character == '?')) {
      return false;
    }
  }
  return true;
}

bool IsInsideRoot(const std::filesystem::path& root, const std::filesystem::path& candidate) {
  auto root_part = root.begin();
  auto candidate_part = candidate.begin();
  for (; root_part != root.end(); ++root_part, ++candidate_part) {
    if (candidate_part == candidate.end() || *root_part != *candidate_part) return false;
  }
  return true;
}

void HandleClient(Socket client, const std::filesystem::path& root) {
#if defined(_WIN32)
  DWORD timeout_ms = 2000U;
  (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms),
                   sizeof(timeout_ms));
#else
  timeval timeout{2, 0};
  (void)setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
  std::string request;
  std::array<char, 2048> buffer{};
  while (request.find("\r\n\r\n") == request.npos && request.size() < kMaximumRequestBytes) {
    const int received = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0);
    if (received <= 0) return;
    request.append(buffer.data(), static_cast<std::size_t>(received));
  }
  if (request.find("\r\n\r\n") == request.npos) {
    SendResponse(client, 431, "Request Header Fields Too Large", "text/plain", "too large\n",
                 false);
    return;
  }

  std::istringstream first_line(request.substr(0, request.find("\r\n")));
  std::string method;
  std::string target;
  std::string protocol;
  first_line >> method >> target >> protocol;
  const bool head_only = method == "HEAD";
  if (method != "GET" && !head_only) {
    SendResponse(client, 405, "Method Not Allowed", "text/plain", "method not allowed\n", false);
    return;
  }
  if ((protocol != "HTTP/1.1" && protocol != "HTTP/1.0") || !IsSafeTarget(target)) {
    SendResponse(client, 400, "Bad Request", "text/plain", "bad request\n", head_only);
    return;
  }

  const std::size_t query = target.find('?');
  target.resize(query == target.npos ? target.size() : query);
  if (target == "/") target = "/index.html";
  std::error_code path_error;
  const auto candidate = std::filesystem::weakly_canonical(root / target.substr(1), path_error);
  if (path_error || !IsInsideRoot(root, candidate) ||
      !std::filesystem::is_regular_file(candidate, path_error)) {
    SendResponse(client, 404, "Not Found", "text/plain", "not found\n", head_only);
    return;
  }
  const auto bytes = std::filesystem::file_size(candidate, path_error);
  if (path_error || bytes > kMaximumAssetBytes) {
    SendResponse(client, 413, "Content Too Large", "text/plain", "asset too large\n", head_only);
    return;
  }
  std::ifstream input(candidate, std::ios::binary);
  const std::string body((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  if (input.bad()) {
    SendResponse(client, 500, "Internal Server Error", "text/plain", "read failed\n", head_only);
    return;
  }
  SendResponse(client, 200, "OK", MimeType(candidate), body, head_only);
}

}  // namespace

struct WebServer::Impl {
  Socket listener = kInvalidSocket;
  std::filesystem::path root;
  std::atomic<bool> running{false};
  std::thread worker;
  std::uint16_t port = 0U;
};

WebServer::WebServer() = default;
WebServer::~WebServer() { Stop(); }

bool WebServer::Start(const std::filesystem::path& web_root, std::uint16_t requested_port,
                      std::string& error) {
  if (impl_) {
    error = "the desktop Web server is already running";
    return false;
  }
  std::error_code path_error;
  const auto root = std::filesystem::canonical(web_root, path_error);
  const std::array required = {"index.html", "manifest.webmanifest", "sw.js",
                               "generated/mol_audio_worklet_core.js",
                               "generated/mol_audio_worklet_core.wasm"};
  if (path_error) {
    error = "the Web UI directory does not exist";
    return false;
  }
  for (const char* relative : required) {
    if (!std::filesystem::is_regular_file(root / relative, path_error)) {
      error = std::string("the Web UI is incomplete: missing ") + relative;
      return false;
    }
  }
  if (!StartSocketRuntime()) {
    error = "the socket runtime could not be initialized";
    return false;
  }
  auto implementation = std::make_unique<Impl>();
  implementation->root = root;
  implementation->listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (implementation->listener == kInvalidSocket) {
    StopSocketRuntime();
    error = "the loopback Web server socket could not be created";
    return false;
  }
#if defined(_WIN32)
  const BOOL exclusive = TRUE;
  (void)setsockopt(implementation->listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
#endif
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(requested_port);
  if (bind(implementation->listener, reinterpret_cast<const sockaddr*>(&address),
           sizeof(address)) != 0 ||
      listen(implementation->listener, SOMAXCONN) != 0) {
    CloseSocket(implementation->listener);
    StopSocketRuntime();
    error = "the loopback Web server could not bind or listen";
    return false;
  }
#if defined(_WIN32)
  int address_size = static_cast<int>(sizeof(address));
  const int name_result = getsockname(implementation->listener,
                                      reinterpret_cast<sockaddr*>(&address),
                                      &address_size);
#else
  socklen_t address_size = sizeof(address);
  const int name_result = getsockname(implementation->listener,
                                      reinterpret_cast<sockaddr*>(&address), &address_size);
#endif
  if (name_result != 0) {
    CloseSocket(implementation->listener);
    StopSocketRuntime();
    error = "the loopback Web server port could not be determined";
    return false;
  }
  implementation->port = ntohs(address.sin_port);
  implementation->running.store(true);
  Impl* const state = implementation.get();
  implementation->worker = std::thread([state]() {
    while (state->running.load()) {
      const Socket client = accept(state->listener, nullptr, nullptr);
      if (client == kInvalidSocket) break;
      HandleClient(client, state->root);
      CloseSocket(client);
    }
  });
  impl_ = std::move(implementation);
  return true;
}

void WebServer::Stop() {
  if (!impl_) return;
  impl_->running.store(false);
  if (impl_->listener != kInvalidSocket) {
    shutdown(impl_->listener, kShutdownBoth);
    CloseSocket(impl_->listener);
    impl_->listener = kInvalidSocket;
  }
  if (impl_->worker.joinable()) impl_->worker.join();
  impl_.reset();
  StopSocketRuntime();
}

std::uint16_t WebServer::port() const { return impl_ ? impl_->port : 0U; }

}  // namespace moldesktop
