// SPDX-License-Identifier: Apache-2.0
#include "desktop_app_support.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

bool Write(const std::filesystem::path& path, std::string_view value) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  output << value;
  return output.good();
}

bool CreateWebUi(const std::filesystem::path& root) {
  return Write(root / "index.html", "ui") && Write(root / "manifest.webmanifest", "{}") &&
         Write(root / "sw.js", "") &&
         Write(root / "generated/mol_audio_worklet_core.js", "") &&
         Write(root / "generated/mol_audio_worklet_core.wasm", "wasm");
}

}  // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("mol-desktop-support-" + std::to_string(
                                                  std::chrono::steady_clock::now()
                                                      .time_since_epoch()
                                                      .count()));
  const auto missing = root / "missing";
  const auto complete = root / "complete";
  if (!CreateWebUi(complete)) return 1;

  const bool paths_ok = !moldesktop::IsCompleteWebUi(missing) &&
                        moldesktop::IsCompleteWebUi(complete) &&
                        moldesktop::FindWebRoot({}, {missing, complete}) == complete &&
                        moldesktop::FindWebRoot(missing, {complete}) == missing;
  const bool navigation_ok =
      moldesktop::IsTrustedNavigation("about:blank", 43121) &&
      moldesktop::IsTrustedNavigation("http://127.0.0.1:43121/", 43121) &&
      moldesktop::IsTrustedNavigation("http://127.0.0.1:43121/assets/app.js?q=1", 43121) &&
      !moldesktop::IsTrustedNavigation("https://127.0.0.1:43121/", 43121) &&
      !moldesktop::IsTrustedNavigation("http://localhost:43121/", 43121) &&
      !moldesktop::IsTrustedNavigation("http://127.0.0.1:43122/", 43121) &&
      !moldesktop::IsTrustedNavigation("http://127.0.0.1:43121.evil/", 43121) &&
      !moldesktop::IsTrustedNavigation("javascript:alert(1)", 43121);
  std::error_code ignored;
  std::filesystem::remove_all(root, ignored);
  return paths_ok && navigation_ok ? 0 : 1;
}
