#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

variant="${1:-Simulator}"
case "$variant" in
  Simulator)
    preset="ios-simulator"
    sdk="iphonesimulator"
    ;;
  Device)
    preset="ios-device"
    sdk="iphoneos"
    ;;
  *)
    echo "usage: $0 [Simulator|Device]" >&2
    exit 2
    ;;
esac

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd "$script_dir/../.." && pwd)"

command -v xcrun >/dev/null ||
  { echo "Xcode command-line tools are required." >&2; exit 1; }
xcrun --sdk "$sdk" --show-sdk-path >/dev/null

if [[ "${MOL_SKIP_WEB_BUILD:-0}" != "1" ]]; then
  if [[ -z "${EMSDK:-}" || ! -f "$EMSDK/emsdk_env.sh" ]]; then
    echo "EMSDK must point to the pinned Emscripten SDK." >&2
    exit 1
  fi
  # shellcheck disable=SC1091
  source "$EMSDK/emsdk_env.sh"
  (
    cd "$repository_root"
    cmake --preset wasm-release
    cmake --build --preset wasm-release
    ctest --preset wasm-release
  )
  (
    cd "$repository_root/apps/web"
    npm ci
    npm test
    npm run build
  )
fi

cmake_arguments=(--preset "$preset")
if [[ -n "${MOL_APPLE_BUNDLE_IDENTIFIER:-}" ]]; then
  cmake_arguments+=("-DMOL_APPLE_BUNDLE_IDENTIFIER=$MOL_APPLE_BUNDLE_IDENTIFIER")
fi
if [[ -n "${MOL_APPLE_DEVELOPMENT_TEAM:-}" ]]; then
  cmake_arguments+=("-DMOL_APPLE_DEVELOPMENT_TEAM=$MOL_APPLE_DEVELOPMENT_TEAM")
fi
(
  cd "$repository_root"
  cmake "${cmake_arguments[@]}"
  cmake --build --preset "$preset"
)

app_path="$(find "$repository_root/build/$preset" -type d -name MoLKeyboard.app -print -quit)"
test -n "$app_path"
test -x "$app_path/MoLKeyboard"
test -f "$app_path/Assets.car"
test -f "$app_path/PrivacyInfo.xcprivacy"
test -f "$app_path/web/index.html"
plutil -lint "$app_path/Info.plist" "$app_path/PrivacyInfo.xcprivacy"

if [[ "$variant" == "Device" && -n "${MOL_APPLE_SIGNING_IDENTITY:-}" ]]; then
  codesign --force --sign "$MOL_APPLE_SIGNING_IDENTITY" --timestamp=none "$app_path"
  codesign --verify --deep --strict "$app_path"
fi

echo "$app_path"
