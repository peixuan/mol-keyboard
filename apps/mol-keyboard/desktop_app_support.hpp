// SPDX-License-Identifier: Apache-2.0
#ifndef MOL_APPS_DESKTOP_APP_SUPPORT_HPP
#define MOL_APPS_DESKTOP_APP_SUPPORT_HPP

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

namespace moldesktop {

bool IsCompleteWebUi(const std::filesystem::path& root);
std::filesystem::path FindWebRoot(
    const std::filesystem::path& requested,
    const std::vector<std::filesystem::path>& fallback_candidates);
bool IsTrustedNavigation(std::string_view url, std::uint16_t port);

}  // namespace moldesktop

#endif
