#!/usr/bin/env sh
# SPDX-License-Identifier: Apache-2.0
set -eu

variant="${1:-Debug}"
case "$variant" in
  Debug|Release|DebugAndroidTest) ;;
  *) echo "Usage: $0 [Debug|Release|DebugAndroidTest]" >&2; exit 2 ;;
esac

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_root=$(CDPATH= cd -- "$script_dir/../.." && pwd)

: "${EMSDK:?EMSDK must point to the pinned Emscripten SDK}"
if [ -z "${ANDROID_HOME:-}" ] && [ -z "${ANDROID_SDK_ROOT:-}" ]; then
  echo "ANDROID_HOME or ANDROID_SDK_ROOT must point to Android SDK 36." >&2
  exit 2
fi

if [ "${MOL_SKIP_WEB_BUILD:-0}" != "1" ]; then
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

set -- ":app:assemble$variant" --no-daemon --no-configuration-cache
if [ -n "${MOL_ANDROID_AAPT2_OVERRIDE:-}" ]; then
  set -- "$@" "-Pandroid.aapt2FromMavenOverride=$MOL_ANDROID_AAPT2_OVERRIDE"
fi
cd "$script_dir"
exec ./gradlew "$@"
