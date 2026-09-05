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

app_path="$repository_root/build/$preset/platforms/ios/Release-$sdk/MoLKeyboard.app"
if [[ ! -d "$app_path" ]]; then
  echo "The expected $variant application bundle is missing: $app_path" >&2
  exit 1
fi
cmake -E copy_directory "$repository_root/apps/web/dist" "$app_path/web"
cmake -E copy_if_different \
  "$script_dir/PrivacyInfo.xcprivacy" "$app_path/PrivacyInfo.xcprivacy"
cmake -E copy_directory "$script_dir/en.lproj" "$app_path/en.lproj"
cmake -E copy_directory "$script_dir/zh-Hans.lproj" "$app_path/zh-Hans.lproj"
if [[ ! -x "$app_path/MoLKeyboard" ]]; then
  echo "The $variant application executable is missing or not executable." >&2
  exit 1
fi
for relative_path in Assets.car PrivacyInfo.xcprivacy web/index.html; do
  if [[ ! -f "$app_path/$relative_path" ]]; then
    echo "The $variant application is missing $relative_path." >&2
    exit 1
  fi
done
for plist_path in "$app_path/Info.plist" "$app_path/PrivacyInfo.xcprivacy"; do
  if ! plutil -lint "$plist_path"; then
    echo "The $variant application contains an invalid property list: $plist_path" >&2
    exit 1
  fi
done

if [[ "$variant" == "Device" && -n "${MOL_APPLE_SIGNING_IDENTITY:-}" ]]; then
  codesign --force --sign "$MOL_APPLE_SIGNING_IDENTITY" --timestamp=none "$app_path"
  codesign --verify --deep --strict "$app_path"
fi

echo "$app_path"
