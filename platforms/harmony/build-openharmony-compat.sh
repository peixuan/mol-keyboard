#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

build_mode="${1:-release}"
case "$build_mode" in
  debug|release) ;;
  *)
    echo "usage: $0 [debug|release]" >&2
    exit 2
    ;;
esac

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$script_dir/app"
source_main="$project_dir/entry/src/main"
compat_main="$project_dir/entry-openharmony/src/main"

: "${HVIGORW:?HVIGORW must point to an OpenHarmony-compatible hvigorw executable}"
: "${OHOS_BASE_SDK_HOME:?OHOS_BASE_SDK_HOME must point to an API 12 or newer OpenHarmony SDK root}"
[[ -x "$HVIGORW" ]] || {
  echo "HVIGORW is not executable: $HVIGORW" >&2
  exit 1
}
[[ -d "$OHOS_BASE_SDK_HOME" ]] || {
  echo "OHOS_BASE_SDK_HOME does not exist: $OHOS_BASE_SDK_HOME" >&2
  exit 1
}
command -v java >/dev/null 2>&1 || {
  echo "A JDK must be available on PATH for HAP packaging." >&2
  exit 1
}
command -v unzip >/dev/null 2>&1 || {
  echo "unzip is required to audit the HAP contents." >&2
  exit 1
}

for name in ets resources cpp; do
  source_dir="$source_main/$name"
  destination="$compat_main/$name"
  [[ -d "$source_dir" ]] || {
    echo "Shared Harmony source directory is missing: $source_dir" >&2
    exit 1
  }
  rm -rf -- "$destination"
  mkdir -p -- "$destination"
  cp -R -- "$source_dir/." "$destination/"
done

(
  cd "$project_dir"
  "$HVIGORW" clean --mode module \
    -p product=openharmony \
    -p module=entryOpenHarmony@default \
    --no-daemon
  "$HVIGORW" assembleHap --mode module \
    -p product=openharmony \
    -p module=entryOpenHarmony@default \
    -p buildMode="$build_mode" \
    --no-daemon
)

hap_path="$(find "$project_dir/entry-openharmony/build" -type f -name '*.hap' -print -quit)"
if [[ -z "$hap_path" ]]; then
  echo "Hvigor completed without producing an OpenHarmony compatibility HAP." >&2
  exit 1
fi

for required in \
  module.json \
  resources.index \
  ets/modules.abc \
  libs/arm64-v8a/libmol_harmony_audio.so \
  libs/x86_64/libmol_harmony_audio.so; do
  unzip -Z1 "$hap_path" | grep -Fxq "$required" || {
    echo "HAP is missing required entry: $required" >&2
    exit 1
  }
done

echo "OpenHarmony compatibility HAP: $hap_path"
if command -v sha256sum >/dev/null 2>&1; then
  sha256sum "$hap_path"
  bytecode_hash="$(unzip -p "$hap_path" ets/modules.abc | sha256sum | cut -d ' ' -f 1)"
  echo "HAP bytecode SHA256: $bytecode_hash"
fi
