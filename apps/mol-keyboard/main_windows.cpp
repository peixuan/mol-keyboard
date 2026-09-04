// SPDX-License-Identifier: Apache-2.0
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// shellapi.h depends on the base Windows declarations above.
#include <shellapi.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "desktop_web_server.hpp"

#ifndef MOL_DEVELOPMENT_WEB_ROOT
#define MOL_DEVELOPMENT_WEB_ROOT ""
#endif

namespace {

std::wstring Utf8ToWide(const std::string& value) {
  if (value.empty()) return {};
  const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
  if (length <= 0) return L"Unknown error";
  std::wstring result(static_cast<std::size_t>(length), L'\0');
  (void)MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), result.data(), length);
  return result;
}

void ShowError(const std::wstring& message) {
  MessageBoxW(nullptr, message.c_str(), L"MoL Keyboard", MB_OK | MB_ICONERROR);
}

std::filesystem::path ExecutableDirectory() {
  std::vector<wchar_t> path(1024U);
  for (;;) {
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0U) return {};
    if (length < path.size() - 1U) {
      return std::filesystem::path(std::wstring(path.data(), length)).parent_path();
    }
    path.resize(path.size() * 2U);
  }
}

std::filesystem::path EnvironmentPath(const wchar_t* name) {
  const DWORD required = GetEnvironmentVariableW(name, nullptr, 0U);
  if (required == 0U) return {};
  std::wstring value(required, L'\0');
  const DWORD written = GetEnvironmentVariableW(name, value.data(), required);
  if (written == 0U || written >= required) return {};
  value.resize(written);
  return std::filesystem::path(value);
}

bool HasWebUi(const std::filesystem::path& root) {
  std::error_code error;
  return std::filesystem::is_regular_file(root / "index.html", error) &&
         std::filesystem::is_regular_file(root / "generated/mol_audio_worklet_core.wasm", error);
}

std::filesystem::path FindWebRoot(const std::filesystem::path& override_path) {
  if (!override_path.empty()) return override_path;
  const auto installed = ExecutableDirectory().parent_path() / "share/mol-keyboard/web";
  if (HasWebUi(installed)) return installed;
  const std::filesystem::path development(MOL_DEVELOPMENT_WEB_ROOT);
  return HasWebUi(development) ? development : installed;
}

std::filesystem::path FindEdge(const std::filesystem::path& override_path) {
  if (!override_path.empty()) return override_path;
  const std::vector<std::filesystem::path> candidates = {
      EnvironmentPath(L"ProgramFiles(x86)") / "Microsoft/Edge/Application/msedge.exe",
      EnvironmentPath(L"ProgramFiles") / "Microsoft/Edge/Application/msedge.exe",
      EnvironmentPath(L"LOCALAPPDATA") / "Microsoft/Edge/Application/msedge.exe"};
  std::error_code error;
  for (const auto& candidate : candidates) {
    if (std::filesystem::is_regular_file(candidate, error)) return candidate;
  }
  return {};
}

std::wstring Quote(const std::wstring& value) { return L"\"" + value + L"\""; }

struct WindowSearch {
  std::filesystem::path browser;
  std::vector<HWND> windows;
};

BOOL CALLBACK CollectBrowserWindow(HWND window, LPARAM context) {
  if (IsWindowVisible(window) == FALSE) return TRUE;
  wchar_t class_name[64]{};
  if (GetClassNameW(window, class_name, static_cast<int>(std::size(class_name))) <= 0 ||
      std::wstring(class_name) != L"Chrome_WidgetWin_1")
    return TRUE;
  DWORD process_id = 0U;
  (void)GetWindowThreadProcessId(window, &process_id);
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
  if (process == nullptr) return TRUE;
  std::wstring image_path(32768U, L'\0');
  DWORD image_length = static_cast<DWORD>(image_path.size());
  const BOOL found = QueryFullProcessImageNameW(process, 0U, image_path.data(), &image_length);
  CloseHandle(process);
  if (found == FALSE) return TRUE;
  image_path.resize(image_length);
  auto* search = reinterpret_cast<WindowSearch*>(context);
  std::error_code path_error;
  if (std::filesystem::equivalent(std::filesystem::path(image_path), search->browser, path_error) &&
      !path_error)
    search->windows.push_back(window);
  return TRUE;
}

std::vector<HWND> BrowserWindows(const std::filesystem::path& browser) {
  WindowSearch search{browser, {}};
  (void)EnumWindows(CollectBrowserWindow, reinterpret_cast<LPARAM>(&search));
  return search.windows;
}

HWND WaitForNewBrowserWindow(const std::filesystem::path& browser,
                             const std::vector<HWND>& existing) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
  while (std::chrono::steady_clock::now() < deadline) {
    for (HWND window : BrowserWindows(browser)) {
      if (std::find(existing.begin(), existing.end(), window) == existing.end()) return window;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  return nullptr;
}

struct Options {
  std::filesystem::path web_root;
  std::filesystem::path browser;
};

bool ParseOptions(Options& options, std::wstring& error) {
  int argument_count = 0;
  wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
  if (arguments == nullptr) {
    error = L"The command line could not be read.";
    return false;
  }
  for (int index = 1; index < argument_count; ++index) {
    const std::wstring argument(arguments[index]);
    if (argument == L"--web-root" || argument == L"--browser") {
      if (++index >= argument_count) {
        error = argument + L" requires a path.";
        LocalFree(arguments);
        return false;
      }
      (argument == L"--web-root" ? options.web_root : options.browser) = arguments[index];
    } else {
      error = L"Unknown option: " + argument;
      LocalFree(arguments);
      return false;
    }
  }
  LocalFree(arguments);
  return true;
}

std::filesystem::path ProfileDirectory() {
  auto base = EnvironmentPath(L"LOCALAPPDATA");
  if (base.empty()) base = EnvironmentPath(L"TEMP");
  return base / "MoL Keyboard/DesktopUiProfile";
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  HANDLE instance_mutex =
      CreateMutexW(nullptr, FALSE, L"Local\\cn.zhangpeixuan.molkeyboard.desktop-ui");
  if (instance_mutex == nullptr) {
    ShowError(L"MoL Keyboard could not create its single-instance guard.");
    return 1;
  }
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    MessageBoxW(nullptr, L"MoL Keyboard is already running.", L"MoL Keyboard",
                MB_OK | MB_ICONINFORMATION);
    CloseHandle(instance_mutex);
    return 8;
  }

  Options options;
  std::wstring option_error;
  if (!ParseOptions(options, option_error)) {
    ShowError(option_error);
    CloseHandle(instance_mutex);
    return 2;
  }
  const auto web_root = FindWebRoot(options.web_root);
  const auto browser = FindEdge(options.browser);
  if (browser.empty()) {
    ShowError(L"Microsoft Edge was not found. Install Edge or pass --browser <path>.");
    CloseHandle(instance_mutex);
    return 3;
  }

  moldesktop::WebServer server;
  std::string server_error;
  if (!server.Start(web_root, 0U, server_error)) {
    ShowError(L"The local interface could not start.\n\n" + Utf8ToWide(server_error) +
              L"\n\nWeb UI: " + web_root.wstring());
    CloseHandle(instance_mutex);
    return 4;
  }

  const std::wstring url = L"http://127.0.0.1:" + std::to_wstring(server.port()) + L"/";
  const auto profile = ProfileDirectory();
  std::error_code directory_error;
  std::filesystem::create_directories(profile, directory_error);
  if (directory_error) {
    ShowError(L"The private desktop profile directory could not be created.");
    CloseHandle(instance_mutex);
    return 5;
  }
  std::wstring command = Quote(browser.wstring()) + L" --app=" + Quote(url) + L" --user-data-dir=" +
                         Quote(profile.wstring()) +
                         L" --disable-background-mode --no-first-run "
                         L"--disable-features=msEdgeFirstRunExperience";
  std::vector<wchar_t> command_line(command.begin(), command.end());
  command_line.push_back(L'\0');
  const auto existing_windows = BrowserWindows(browser);
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (CreateProcessW(browser.c_str(), command_line.data(), nullptr, nullptr, FALSE,
                     CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &startup, &process) == FALSE) {
    ShowError(L"The MoL Keyboard application window could not be opened.");
    CloseHandle(instance_mutex);
    return 6;
  }
  CloseHandle(process.hThread);
  const HWND application_window = WaitForNewBrowserWindow(browser, existing_windows);
  if (application_window == nullptr) {
    ShowError(L"Microsoft Edge started, but the MoL Keyboard window did not appear.");
    CloseHandle(process.hProcess);
    CloseHandle(instance_mutex);
    return 7;
  }
  while (IsWindow(application_window) != FALSE) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  DWORD browser_exit_code = 0U;
  if (!options.browser.empty() && WaitForSingleObject(process.hProcess, 5000U) == WAIT_OBJECT_0) {
    (void)GetExitCodeProcess(process.hProcess, &browser_exit_code);
  }
  CloseHandle(process.hProcess);
  CloseHandle(instance_mutex);
  return browser_exit_code == 0U ? 0 : 9;
}
