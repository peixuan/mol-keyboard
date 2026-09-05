// SPDX-License-Identifier: Apache-2.0
#include "desktop_app_support.hpp"

#include <array>
#include <string>

namespace moldesktop {

bool IsCompleteWebUi(const std::filesystem::path& root) {
  constexpr std::array<std::string_view, 5> required = {
      "index.html", "manifest.webmanifest", "sw.js",
      "generated/mol_audio_worklet_core.js", "generated/mol_audio_worklet_core.wasm"};
  std::error_code error;
  for (const auto relative : required) {
    if (!std::filesystem::is_regular_file(root / relative, error) || error) return false;
  }
  return true;
}

std::filesystem::path FindWebRoot(
    const std::filesystem::path& requested,
    const std::vector<std::filesystem::path>& fallback_candidates) {
  if (!requested.empty()) return requested;
  for (const auto& candidate : fallback_candidates) {
    if (IsCompleteWebUi(candidate)) return candidate;
  }
  return fallback_candidates.empty() ? std::filesystem::path{} : fallback_candidates.front();
}

bool IsTrustedNavigation(std::string_view url, std::uint16_t port) {
  if (url == "about:blank") return true;
  const std::string origin = "http://127.0.0.1:" + std::to_string(port);
  return url.size() > origin.size() && url.compare(0, origin.size(), origin) == 0 &&
         url[origin.size()] == '/';
}

}  // namespace moldesktop
