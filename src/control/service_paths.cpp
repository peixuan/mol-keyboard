// SPDX-License-Identifier: Apache-2.0
#include "service_paths.hpp"

#include <cstdlib>
#include <filesystem>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace molcontrol {

#if defined(_WIN32)
namespace {

std::filesystem::path environment_path(const wchar_t* name) {
  const DWORD required = GetEnvironmentVariableW(name, nullptr, 0u);
  if (required == 0u) return {};
  std::vector<wchar_t> value(required, L'\0');
  const DWORD written = GetEnvironmentVariableW(name, value.data(), required);
  if (written == 0u || written >= required) return {};
  return std::filesystem::path(value.data());
}

}  // namespace
#endif

std::filesystem::path default_service_state_directory() {
#if defined(_WIN32)
  const std::filesystem::path local = environment_path(L"LOCALAPPDATA");
  if (!local.empty()) return local / "MolKeyboard";
  const std::filesystem::path profile = environment_path(L"USERPROFILE");
  if (!profile.empty()) return profile / "AppData" / "Local" / "MolKeyboard";
#else
  const char* state_home = std::getenv("XDG_STATE_HOME");
  if (state_home != nullptr && state_home[0] == '/')
    return std::filesystem::path(state_home) / "mol-keyboard";
  const char* home = std::getenv("HOME");
  if (home != nullptr && home[0] == '/')
    return std::filesystem::path(home) / ".local" / "state" / "mol-keyboard";
#endif
  return std::filesystem::current_path() / ".mol-keyboard-state";
}

}  // namespace molcontrol
