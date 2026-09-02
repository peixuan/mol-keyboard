// SPDX-License-Identifier: Apache-2.0
#include "local_ipc.hpp"

#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#endif

namespace molcontrol {
namespace {

std::array<unsigned char, 4u> encode_size(std::size_t size) {
  return {static_cast<unsigned char>(size & 0xffu),
          static_cast<unsigned char>((size >> 8u) & 0xffu),
          static_cast<unsigned char>((size >> 16u) & 0xffu),
          static_cast<unsigned char>((size >> 24u) & 0xffu)};
}

std::size_t decode_size(const std::array<unsigned char, 4u>& bytes) {
  return static_cast<std::size_t>(bytes[0]) | (static_cast<std::size_t>(bytes[1]) << 8u) |
         (static_cast<std::size_t>(bytes[2]) << 16u) | (static_cast<std::size_t>(bytes[3]) << 24u);
}

#if defined(_WIN32)

constexpr const char* kPipePrefix = R"(\\.\pipe\)";

std::string windows_error(const char* operation) {
  return std::string(operation) + " failed with Windows error " +
         std::to_string(static_cast<unsigned long>(GetLastError()));
}

bool valid_endpoint(const std::string& endpoint) {
  if (endpoint.rfind(kPipePrefix, 0u) != 0u) return false;
  const std::string name = endpoint.substr(std::strlen(kPipePrefix));
  if (name.empty() || name.size() > 128u) return false;
  for (const unsigned char character : name)
    if (!(std::isalnum(character) != 0 || character == '-' || character == '_' || character == '.'))
      return false;
  return true;
}

bool read_exact(HANDLE handle, void* output, std::size_t size) {
  auto* bytes = static_cast<unsigned char*>(output);
  while (size != 0u) {
    DWORD received = 0u;
    const DWORD chunk = static_cast<DWORD>(
        size > static_cast<std::size_t>(MAXDWORD) ? MAXDWORD : static_cast<DWORD>(size));
    OVERLAPPED overlapped{};
    overlapped.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (overlapped.hEvent == nullptr) return false;
    bool success = ReadFile(handle, bytes, chunk, &received, &overlapped) != FALSE;
    if (!success && GetLastError() == ERROR_IO_PENDING) {
      if (WaitForSingleObject(overlapped.hEvent, 2000u) == WAIT_OBJECT_0)
        success = GetOverlappedResult(handle, &overlapped, &received, FALSE) != FALSE;
      else {
        (void)CancelIoEx(handle, &overlapped);
        (void)WaitForSingleObject(overlapped.hEvent, INFINITE);
      }
    }
    CloseHandle(overlapped.hEvent);
    if (!success || received == 0u) return false;
    bytes += received;
    size -= received;
  }
  return true;
}

bool write_exact(HANDLE handle, const void* input, std::size_t size) {
  const auto* bytes = static_cast<const unsigned char*>(input);
  while (size != 0u) {
    DWORD sent = 0u;
    const DWORD chunk = static_cast<DWORD>(
        size > static_cast<std::size_t>(MAXDWORD) ? MAXDWORD : static_cast<DWORD>(size));
    OVERLAPPED overlapped{};
    overlapped.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (overlapped.hEvent == nullptr) return false;
    bool success = WriteFile(handle, bytes, chunk, &sent, &overlapped) != FALSE;
    if (!success && GetLastError() == ERROR_IO_PENDING) {
      if (WaitForSingleObject(overlapped.hEvent, 2000u) == WAIT_OBJECT_0)
        success = GetOverlappedResult(handle, &overlapped, &sent, FALSE) != FALSE;
      else {
        (void)CancelIoEx(handle, &overlapped);
        (void)WaitForSingleObject(overlapped.hEvent, INFINITE);
      }
    }
    CloseHandle(overlapped.hEvent);
    if (!success || sent == 0u) return false;
    bytes += sent;
    size -= sent;
  }
  return true;
}

#else

std::string posix_error(const char* operation) {
  return std::string(operation) + " failed: " + std::strerror(errno);
}

bool valid_endpoint(const std::string& endpoint) {
  return !endpoint.empty() && endpoint.size() < sizeof(sockaddr_un{}.sun_path) &&
         endpoint.front() == '/';
}

bool read_exact(int descriptor, void* output, std::size_t size) {
  auto* bytes = static_cast<unsigned char*>(output);
  while (size != 0u) {
    const ssize_t received = ::read(descriptor, bytes, size);
    if (received <= 0) return false;
    bytes += received;
    size -= static_cast<std::size_t>(received);
  }
  return true;
}

bool write_exact(int descriptor, const void* input, std::size_t size) {
  const auto* bytes = static_cast<const unsigned char*>(input);
  while (size != 0u) {
#if defined(MSG_NOSIGNAL)
    const ssize_t sent = ::send(descriptor, bytes, size, MSG_NOSIGNAL);
#else
    const ssize_t sent = ::send(descriptor, bytes, size, 0);
#endif
    if (sent <= 0) return false;
    bytes += sent;
    size -= static_cast<std::size_t>(sent);
  }
  return true;
}

#endif

template <typename Handle>
bool read_message(Handle handle, std::string& message, bool allow_empty = false) {
  std::array<unsigned char, 4u> bytes{};
  if (!read_exact(handle, bytes.data(), bytes.size())) return false;
  const std::size_t size = decode_size(bytes);
  if ((!allow_empty && size == 0u) || size > kMaximumIpcMessageBytes) return false;
  message.assign(size, '\0');
  return message.empty() || read_exact(handle, message.data(), message.size());
}

template <typename Handle>
bool write_message(Handle handle, const std::string& message) {
  if (message.size() > kMaximumIpcMessageBytes) return false;
  const std::array<unsigned char, 4u> bytes = encode_size(message.size());
  return write_exact(handle, bytes.data(), bytes.size()) &&
         (message.empty() || write_exact(handle, message.data(), message.size()));
}

}  // namespace

std::string default_local_ipc_endpoint(const std::filesystem::path& state_directory) {
#if defined(_WIN32)
  (void)state_directory;
  return R"(\\.\pipe\mol-keyboard-v1)";
#else
  const char* runtime = std::getenv("XDG_RUNTIME_DIR");
  if (runtime != nullptr && runtime[0] == '/')
    return (std::filesystem::path(runtime) / "mol-keyboard-v1.sock").string();
  return (state_directory / "mol-keyboard-v1.sock").string();
#endif
}

bool LocalIpcServer::serve(const std::string& endpoint, const Handler& handler,
                           const std::atomic<bool>& stop_requested, std::string& error) {
  if (!valid_endpoint(endpoint) || !handler) {
    error = "invalid local IPC endpoint or handler";
    return false;
  }
#if defined(_WIN32)
  while (!stop_requested.load(std::memory_order_acquire)) {
    HANDLE pipe = CreateNamedPipeA(
        endpoint.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1u,
        static_cast<DWORD>(kMaximumIpcMessageBytes + 4u),
        static_cast<DWORD>(kMaximumIpcMessageBytes + 4u), 0u, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
      error = windows_error("CreateNamedPipe");
      return false;
    }
    OVERLAPPED overlapped{};
    overlapped.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (overlapped.hEvent == nullptr) {
      error = windows_error("CreateEvent");
      CloseHandle(pipe);
      return false;
    }
    bool connected = ConnectNamedPipe(pipe, &overlapped) != FALSE;
    if (!connected) {
      const DWORD connect_error = GetLastError();
      if (connect_error == ERROR_PIPE_CONNECTED) {
        connected = true;
      } else if (connect_error == ERROR_IO_PENDING) {
        while (!stop_requested.load(std::memory_order_acquire)) {
          const DWORD wait_result = WaitForSingleObject(overlapped.hEvent, 100u);
          if (wait_result == WAIT_OBJECT_0) {
            DWORD ignored = 0u;
            connected = GetOverlappedResult(pipe, &overlapped, &ignored, FALSE) != FALSE;
            break;
          }
          if (wait_result != WAIT_TIMEOUT) break;
        }
      }
    }
    if (!connected) CancelIoEx(pipe, &overlapped);
    CloseHandle(overlapped.hEvent);
    if (connected) {
      std::string request;
      if (read_message(pipe, request)) {
        const std::string response = handler(request);
        if (write_message(pipe, response)) {
          unsigned char client_closed = 0u;
          (void)read_exact(pipe, &client_closed, 1u);
        }
      }
      (void)DisconnectNamedPipe(pipe);
    }
    CloseHandle(pipe);
  }
  return true;
#else
  struct SocketCleanup {
    int descriptor = -1;
    std::string path;
    ~SocketCleanup() {
      if (descriptor >= 0) ::close(descriptor);
      struct stat status{};
      if (!path.empty() && ::lstat(path.c_str(), &status) == 0 && S_ISSOCK(status.st_mode))
        (void)::unlink(path.c_str());
    }
  } cleanup;
  struct stat existing{};
  if (::lstat(endpoint.c_str(), &existing) == 0) {
    if (!S_ISSOCK(existing.st_mode) || ::unlink(endpoint.c_str()) != 0) {
      error = "IPC endpoint exists and is not a removable socket";
      return false;
    }
  } else if (errno != ENOENT) {
    error = posix_error("lstat IPC endpoint");
    return false;
  }
  cleanup.descriptor = ::socket(AF_UNIX, SOCK_STREAM, 0);
  cleanup.path = endpoint;
  if (cleanup.descriptor < 0) {
    error = posix_error("socket");
    return false;
  }
  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  std::memcpy(address.sun_path, endpoint.c_str(), endpoint.size() + 1u);
  if (::bind(cleanup.descriptor, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) !=
      0) {
    error = posix_error("bind local IPC socket");
    return false;
  }
  if (::chmod(endpoint.c_str(), S_IRUSR | S_IWUSR) != 0 || ::listen(cleanup.descriptor, 8) != 0) {
    error = posix_error("secure or listen on local IPC socket");
    return false;
  }
  while (!stop_requested.load(std::memory_order_acquire)) {
    pollfd ready{cleanup.descriptor, POLLIN, 0};
    const int poll_result = ::poll(&ready, 1u, 100);
    if (poll_result < 0) {
      if (errno == EINTR) continue;
      error = posix_error("poll local IPC socket");
      return false;
    }
    if (poll_result == 0 || (ready.revents & POLLIN) == 0) continue;
    const int client = ::accept(cleanup.descriptor, nullptr, nullptr);
    if (client < 0) {
      if (errno == EINTR) continue;
      error = posix_error("accept local IPC client");
      return false;
    }
    timeval timeout{2, 0};
    (void)::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)::setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    std::string request;
    bool success = read_message(client, request);
    if (success) success = write_message(client, handler(request));
    (void)::close(client);
    (void)success;
  }
  return true;
#endif
}

bool send_local_ipc_request(const std::string& endpoint, const std::string& request,
                            std::string& response, std::string& error) {
  response.clear();
  if (!valid_endpoint(endpoint) || request.empty() || request.size() > kMaximumIpcMessageBytes) {
    error = "invalid local IPC endpoint or request size";
    return false;
  }
#if defined(_WIN32)
  HANDLE pipe = INVALID_HANDLE_VALUE;
  const ULONGLONG deadline = GetTickCount64() + 2000u;
  while (GetTickCount64() < deadline) {
    if (WaitNamedPipeA(endpoint.c_str(), 50u) != FALSE) {
      pipe = CreateFileA(endpoint.c_str(), GENERIC_READ | GENERIC_WRITE, 0u, nullptr, OPEN_EXISTING,
                         FILE_FLAG_OVERLAPPED, nullptr);
      if (pipe != INVALID_HANDLE_VALUE) break;
    }
    Sleep(5u);
  }
  if (pipe == INVALID_HANDLE_VALUE) {
    error = windows_error("connect to named pipe");
    return false;
  }
  const bool success = write_message(pipe, request) && read_message(pipe, response, true);
  if (!success) error = windows_error("named pipe exchange");
  CloseHandle(pipe);
  return success;
#else
  const int descriptor = ::socket(AF_UNIX, SOCK_STREAM, 0);
  if (descriptor < 0) {
    error = posix_error("socket");
    return false;
  }
  sockaddr_un address{};
  timeval timeout{2, 0};
  (void)::setsockopt(descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  (void)::setsockopt(descriptor, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  address.sun_family = AF_UNIX;
  std::memcpy(address.sun_path, endpoint.c_str(), endpoint.size() + 1u);
  bool success =
      ::connect(descriptor, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
  if (success) success = write_message(descriptor, request);
  if (success) success = read_message(descriptor, response, true);
  if (!success) error = posix_error("local IPC exchange");
  (void)::close(descriptor);
  return success;
#endif
}

}  // namespace molcontrol
