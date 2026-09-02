// SPDX-License-Identifier: Apache-2.0
#include "service_paths.hpp"

#include <cstdlib>
#include <filesystem>

namespace molcontrol {

std::filesystem::path default_service_state_directory() {
#if defined(_WIN32)
  const char* local = std::getenv("LOCALAPPDATA");
  if (local != nullptr && local[0] != '\0') return std::filesystem::path(local) / "MolKeyboard";
  const char* profile = std::getenv("USERPROFILE");
  if (profile != nullptr && profile[0] != '\0')
    return std::filesystem::path(profile) / "AppData" / "Local" / "MolKeyboard";
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
