// SPDX-License-Identifier: Apache-2.0
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>

#include <cstdint>
#include <string>

int wmain(int argument_count, wchar_t** arguments) {
  std::wstring url;
  for (int index = 1; index < argument_count; ++index) {
    const std::wstring argument(arguments[index]);
    if (argument.rfind(L"--app=", 0U) == 0U) url = argument.substr(6U);
  }
  const auto colon = url.find(L':', 7U);
  const auto slash = url.find(L'/', colon + 1U);
  if (colon == url.npos || slash == url.npos) return 1;
  const auto port =
      static_cast<std::uint16_t>(std::stoul(url.substr(colon + 1U, slash - colon - 1U)));

  WNDCLASSW window_class{};
  window_class.lpfnWndProc = DefWindowProcW;
  window_class.hInstance = GetModuleHandleW(nullptr);
  window_class.lpszClassName = L"Chrome_WidgetWin_1";
  if (RegisterClassW(&window_class) == 0U) return 2;
  const HWND window =
      CreateWindowExW(0U, window_class.lpszClassName, L"MoL Keyboard test", WS_POPUP, 0, 0, 8, 8,
                      nullptr, nullptr, window_class.hInstance, nullptr);
  if (window == nullptr) return 2;
  ShowWindow(window, SW_SHOW);
  UpdateWindow(window);

  WSADATA data{};
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 3;
  const SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (client == INVALID_SOCKET) return 4;
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(port);
  if (connect(client, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) return 5;
  const std::string request = "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n";
  if (send(client, request.data(), static_cast<int>(request.size()), 0) <= 0) return 6;
  std::string response;
  char buffer[4096];
  for (;;) {
    const int received = recv(client, buffer, static_cast<int>(sizeof(buffer)), 0);
    if (received <= 0) break;
    response.append(buffer, static_cast<std::size_t>(received));
  }
  closesocket(client);
  WSACleanup();
  Sleep(500U);
  DestroyWindow(window);
  return response.find("HTTP/1.1 200 OK") != response.npos &&
                 response.find("<mol-keyboard-app>") != response.npos &&
                 response.find("wasm-unsafe-eval") != response.npos
             ? 0
             : 7;
}
