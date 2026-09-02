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

if [[ -n "${HVIGORW:-}" ]]; then
  hvigor="$HVIGORW"
elif command -v hvigorw >/dev/null 2>&1; then
  hvigor="$(command -v hvigorw)"
elif [[ -n "${DEVECO_HOME:-}" && -x "$DEVECO_HOME/tools/hvigor/bin/hvigorw" ]]; then
  hvigor="$DEVECO_HOME/tools/hvigor/bin/hvigorw"
else
  echo "A DevEco Studio installation is required; set HVIGORW or DEVECO_HOME." >&2
  exit 1
fi

if command -v ohpm >/dev/null 2>&1; then
  (cd "$project_dir" && ohpm install --all)
fi

(
  cd "$project_dir"
  "$hvigor" assembleHap --mode module \
    -p product=default \
    -p module=entry@default \
    -p buildMode="$build_mode" \
    --no-daemon
)

hap_path="$(find "$project_dir/entry/build" -type f -name '*.hap' -print -quit)"
if [[ -z "$hap_path" ]]; then
  echo "Hvigor completed without producing a HAP." >&2
  exit 1
fi

command -v unzip >/dev/null 2>&1 || {
  echo "unzip is required to audit the HAP contents." >&2
  exit 1
}
unzip -l "$hap_path" | grep -q 'libmol_harmony_audio.so'
unzip -l "$hap_path" | grep -q 'module.json'

echo "$hap_path"
