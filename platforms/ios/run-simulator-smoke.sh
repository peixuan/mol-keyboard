#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd "$script_dir/../.." && pwd)"
artifact_dir="${MOL_IOS_SMOKE_ARTIFACT_DIR:-$repository_root/build/ios-simulator-smoke}"

command -v xcrun >/dev/null ||
  { echo "Xcode command-line tools are required." >&2; exit 1; }
command -v python3 >/dev/null ||
  { echo "Python 3 is required to select an iOS Simulator." >&2; exit 1; }
xcrun --sdk iphonesimulator --show-sdk-path >/dev/null

if [[ "${MOL_SKIP_IOS_BUILD:-0}" != "1" ]]; then
  "$script_dir/build-app.sh" Simulator
fi

app_path="$(find "$repository_root/build/ios-simulator" -type d -name MoLKeyboard.app -print -quit)"
if [[ -z "$app_path" || ! -x "$app_path/MoLKeyboard" ]]; then
  echo "A built iOS Simulator application is required." >&2
  exit 1
fi
bundle_identifier="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$app_path/Info.plist")"
if [[ -z "$bundle_identifier" ]]; then
  echo "The Simulator application has no bundle identifier." >&2
  exit 1
fi

device_record="$({ xcrun simctl list devices available --json; } | python3 -c '
import json
import sys

catalog = json.load(sys.stdin)
for runtime in sorted(catalog.get("devices", {}), reverse=True):
    if ".SimRuntime.iOS-" not in runtime:
        continue
    for device in catalog["devices"][runtime]:
        if device.get("isAvailable", False) and "iPhone" in device.get("name", ""):
            udid = device.get("udid", "")
            state = device.get("state", "Shutdown")
            print("{}\t{}".format(udid, state))
            raise SystemExit(0)
raise SystemExit("No available iPhone Simulator was found")
')"
IFS=$'\t' read -r device_udid device_state <<< "$device_record"
if [[ -z "$device_udid" ]]; then
  echo "The selected iOS Simulator has no UDID." >&2
  exit 1
fi

mkdir -p "$artifact_dir"
stdout_log="$artifact_dir/stdout.log"
stderr_log="$artifact_dir/stderr.log"
: > "$stdout_log"
: > "$stderr_log"

booted_here=0
cleanup() {
  xcrun simctl terminate "$device_udid" "$bundle_identifier" >/dev/null 2>&1 || true
  xcrun simctl uninstall "$device_udid" "$bundle_identifier" >/dev/null 2>&1 || true
  if [[ "$booted_here" == "1" ]]; then
    xcrun simctl shutdown "$device_udid" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

if [[ "$device_state" != "Booted" ]]; then
  xcrun simctl boot "$device_udid"
  booted_here=1
fi
xcrun simctl bootstatus "$device_udid" -b
xcrun simctl install "$device_udid" "$app_path"
xcrun simctl launch --terminate-running-process \
  --stdout="$stdout_log" \
  --stderr="$stderr_log" \
  "$device_udid" "$bundle_identifier" --mol-simulator-smoke

deadline=$((SECONDS + 60))
while (( SECONDS < deadline )); do
  if grep -Fq "MOL_IOS_SIMULATOR_SMOKE_PASS" "$stderr_log"; then
    xcrun simctl io "$device_udid" screenshot "$artifact_dir/screenshot.png"
    cat "$stdout_log" "$stderr_log"
    echo "iOS Simulator packaged UI and native bridge smoke passed on $device_udid."
    exit 0
  fi
  if grep -Fq "MOL_IOS_SIMULATOR_SMOKE_FAIL" "$stderr_log"; then
    cat "$stdout_log" "$stderr_log" >&2
    exit 1
  fi
  sleep 1
done

cat "$stdout_log" "$stderr_log" >&2
echo "Timed out waiting for the iOS Simulator smoke result." >&2
exit 1
