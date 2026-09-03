#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "The launchd service smoke requires macOS." >&2
  exit 2
fi
if [[ "$#" != "2" ]]; then
  echo "usage: $0 MOL_KEYBOARDD MOLCTL" >&2
  exit 2
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd "$script_dir/../.." && pwd)"
daemon="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
controller="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
template="$repository_root/packaging/launchd/cn.zhangpeixuan.molkeyboard.daemon.plist"

for command in launchctl plutil python3; do
  command -v "$command" >/dev/null ||
    { echo "$command is required for the launchd service smoke." >&2; exit 1; }
done
if [[ ! -x "$daemon" || ! -x "$controller" ]]; then
  echo "Built mol-keyboardd and molctl executables are required." >&2
  exit 1
fi
plutil -lint "$template"

artifact_dir="$(mktemp -d "${TMPDIR:-/tmp}/mol-keyboard-launchd.XXXXXX")"
state_dir="$artifact_dir/state"
runtime_plist="$artifact_dir/cn.zhangpeixuan.molkeyboard.daemon.ci.plist"
stdout_log="$artifact_dir/daemon.stdout.log"
stderr_log="$artifact_dir/daemon.stderr.log"
job_log="$artifact_dir/launchctl.txt"
user_id="$(id -u)"
label="cn.zhangpeixuan.molkeyboard.daemon.ci.$user_id.$$"
domain="gui/$user_id"
service_target="$domain/$label"
endpoint="/tmp/mol-keyboard-launchd-$user_id-$$.sock"

if [[ -e "$endpoint" ]]; then
  echo "Refusing to replace the existing IPC endpoint $endpoint." >&2
  exit 1
fi
if ! launchctl print "$domain" >/dev/null 2>&1; then
  echo "The current macOS user has no launchd GUI domain." >&2
  exit 1
fi

python3 - "$template" "$runtime_plist" "$label" "$daemon" "$state_dir" "$endpoint" \
  "$stdout_log" "$stderr_log" <<'PY'
import plistlib
import sys

(template, output, label, daemon, state, endpoint, stdout, stderr) = sys.argv[1:]
with open(template, "rb") as source:
    job = plistlib.load(source)
if job.get("Label") != "cn.zhangpeixuan.molkeyboard.daemon":
    raise SystemExit("launchd template label is invalid")
if job.get("ProgramArguments") != ["/usr/local/bin/mol-keyboardd"]:
    raise SystemExit("launchd template program arguments are invalid")
if job.get("RunAtLoad") is not True or not isinstance(job.get("KeepAlive"), dict):
    raise SystemExit("launchd template lifecycle policy is invalid")
job["Label"] = label
job["ProgramArguments"] = [
    daemon,
    "--null-backend",
    "--state-dir",
    state,
    "--endpoint",
    endpoint,
]
job["StandardOutPath"] = stdout
job["StandardErrorPath"] = stderr
with open(output, "wb") as destination:
    plistlib.dump(job, destination, sort_keys=False)
PY
plutil -lint "$runtime_plist"

bootstrapped=0
cleanup() {
  if [[ "$bootstrapped" == "1" ]]; then
    launchctl bootout "$domain" "$runtime_plist" >/dev/null 2>&1 || true
  fi
  if [[ -S "$endpoint" ]]; then
    rm -f -- "$endpoint"
  fi
  rm -rf -- "$artifact_dir"
}
trap cleanup EXIT

launchctl bootstrap "$domain" "$runtime_plist"
bootstrapped=1

ready=0
for _ in {1..100}; do
  if "$controller" --json --endpoint "$endpoint" --state-dir "$state_dir" status \
      > "$artifact_dir/status.json" 2>/dev/null; then
    ready=1
    break
  fi
  sleep 0.05
done
if [[ "$ready" != "1" ]]; then
  cat "$stdout_log" "$stderr_log" >&2 || true
  echo "The launchd-managed daemon did not become ready." >&2
  exit 1
fi

run_controller() {
  local output="$1"
  shift
  "$controller" --json --endpoint "$endpoint" --state-dir "$state_dir" "$@" > "$output"
}

run_controller "$artifact_dir/capabilities.json" capabilities
run_controller "$artifact_dir/preset.json" preset set violin
run_controller "$artifact_dir/tempo.json" tempo 123
run_controller "$artifact_dir/record-start.json" record start
run_controller "$artifact_dir/note-on.json" note on 60 --velocity 0.8 --gesture 7001
sleep 0.1
run_controller "$artifact_dir/note-off.json" note off --gesture 7001
run_controller "$artifact_dir/record-stop.json" record stop --output "$artifact_dir/take.molseq"
test -s "$artifact_dir/take.molseq"
run_controller "$artifact_dir/play.json" play "$artifact_dir/take.molseq"
run_controller "$artifact_dir/play-stop.json" rpc playback.stop '{}'
run_controller "$artifact_dir/audio.json" rpc audio.getLatency '{}'
run_controller "$artifact_dir/self-test.json" self-test
run_controller "$artifact_dir/doctor.json" doctor
run_controller "$artifact_dir/benchmark.json" rpc diagnostics.benchmark '{"frames":4096}'
run_controller "$artifact_dir/all-notes-off.json" all-notes-off

python3 - "$artifact_dir/status.json" "$artifact_dir/audio.json" \
  "$artifact_dir/self-test.json" "$artifact_dir/doctor.json" \
  "$artifact_dir/benchmark.json" <<'PY'
import json
import sys

documents = []
for path in sys.argv[1:]:
    with open(path, encoding="utf-8") as source:
        document = json.load(source)
    if "error" in document or "result" not in document:
        raise SystemExit("invalid or failed molctl response: {}".format(path))
    documents.append(document["result"])
status, audio, self_test, doctor, benchmark = documents
if status.get("sample_rate") != 48000 or status.get("channel_count") != 2:
    raise SystemExit("launchd daemon engine state is invalid")
if audio.get("backend") != "null" or audio.get("null_sink") is not True:
    raise SystemExit("launchd daemon did not use the null backend")
if self_test.get("ok") is not True or doctor.get("ok") is not True:
    raise SystemExit("launchd daemon diagnostics failed")
if benchmark.get("frames") != 4096 or benchmark.get("non_finite_samples") != 0:
    raise SystemExit("launchd daemon benchmark failed")
PY

run_controller "$artifact_dir/shutdown.json" shutdown

stopped=0
for _ in {1..100}; do
  if launchctl print "$service_target" > "$job_log" 2>&1 && \
      [[ ! -S "$endpoint" ]] && grep -Fq "last exit code = 0" "$job_log"; then
    stopped=1
    break
  fi
  sleep 0.05
done
if [[ "$stopped" != "1" ]]; then
  cat "$job_log" "$stdout_log" "$stderr_log" >&2 || true
  echo "The launchd-managed daemon did not exit cleanly." >&2
  exit 1
fi

cat "$artifact_dir/status.json" "$artifact_dir/audio.json" \
  "$artifact_dir/self-test.json" "$artifact_dir/doctor.json" \
  "$artifact_dir/benchmark.json"
echo "MOL_MACOS_LAUNCHD_SMOKE_PASS"
