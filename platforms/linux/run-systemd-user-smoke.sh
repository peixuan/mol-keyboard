#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "The systemd user-service smoke requires Linux." >&2
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
template="$repository_root/packaging/systemd/mol-keyboardd.service"

for command in systemctl systemd-analyze python3; do
  command -v "$command" >/dev/null || {
    echo "$command is required for the systemd user-service smoke." >&2
    exit 77
  }
done
if ! systemctl --user is-system-running >/dev/null 2>&1; then
  echo "A running systemd user manager is unavailable; service smoke skipped." >&2
  exit 77
fi
if [[ ! -x "$daemon" || ! -x "$controller" ]]; then
  echo "Built mol-keyboardd and molctl executables are required." >&2
  exit 1
fi

user_id="$(id -u)"
runtime_root="${XDG_RUNTIME_DIR:-/run/user/$user_id}"
if [[ ! -d "$runtime_root" || ! -w "$runtime_root" ]]; then
  echo "A writable per-user runtime directory is unavailable; service smoke skipped." >&2
  exit 77
fi

artifact_dir="$(mktemp -d "$runtime_root/mol-keyboard-systemd.XXXXXX")"
case "$artifact_dir" in
  "$runtime_root"/mol-keyboard-systemd.*) ;;
  *)
    echo "Refusing to use an unexpected systemd smoke artifact path." >&2
    exit 1
    ;;
esac
state_dir="$artifact_dir/state"
endpoint="$artifact_dir/service.sock"
unit_name="mol-keyboardd-ci-$user_id-$$.service"
runtime_unit="$artifact_dir/$unit_name"
job_log="$artifact_dir/systemd-state.txt"
linked=0
started=0

cleanup() {
  if [[ "$started" == "1" ]]; then
    systemctl --user stop "$unit_name" >/dev/null 2>&1 || true
  fi
  if [[ "$linked" == "1" ]]; then
    systemctl --user --runtime disable "$unit_name" >/dev/null 2>&1 || true
    systemctl --user daemon-reload >/dev/null 2>&1 || true
  fi
  if [[ -S "$endpoint" ]]; then
    rm -f -- "$endpoint"
  fi
  rm -rf -- "$artifact_dir"
}
trap cleanup EXIT

python3 - "$template" "$runtime_unit" "$daemon" "$state_dir" "$endpoint" \
  "$artifact_dir" <<'PY'
from pathlib import Path
import sys

(template_text, output_text, daemon, state, endpoint, writable) = sys.argv[1:]
template = Path(template_text)
output = Path(output_text)
source = template.read_text(encoding="utf-8")
required = [
    "[Unit]",
    "After=graphical-session.target sound.target",
    "[Service]",
    "Type=simple",
    "ExecStart=%h/.local/bin/mol-keyboardd",
    "Restart=on-failure",
    "RestartSec=2",
    "NoNewPrivileges=true",
    "PrivateTmp=true",
    "ProtectSystem=strict",
    "ProtectHome=read-only",
    "ReadWritePaths=%h/.local/state/mol-keyboard",
    "RestrictAddressFamilies=AF_UNIX",
    "[Install]",
    "WantedBy=default.target",
]
for token in required:
    if source.count(token) != 1:
        raise SystemExit("systemd template contract is missing or duplicated: " + token)


def quote(value: str) -> str:
    if not value or "\n" in value or "\r" in value or "\0" in value:
        raise SystemExit("systemd runtime argument is invalid")
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


command = " ".join(
    quote(value)
    for value in [daemon, "--null-backend", "--state-dir", state, "--endpoint", endpoint]
)
runtime = source.replace(
    "ExecStart=%h/.local/bin/mol-keyboardd", "ExecStart=" + command
).replace(
    "ReadWritePaths=%h/.local/state/mol-keyboard", "ReadWritePaths=" + quote(writable)
)
output.write_text(runtime, encoding="utf-8")
PY

systemd-analyze --user verify "$runtime_unit"
systemctl --user --runtime link "$runtime_unit"
linked=1
systemctl --user daemon-reload
systemctl --user start "$unit_name"
started=1

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
  systemctl --user status "$unit_name" --no-pager >&2 || true
  journalctl --user-unit "$unit_name" --no-pager -n 100 >&2 || true
  echo "The systemd-managed daemon did not become ready." >&2
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
run_controller "$artifact_dir/note-on.json" note on 60 --velocity 0.8 --gesture 7002
sleep 0.1
run_controller "$artifact_dir/note-off.json" note off --gesture 7002
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
    raise SystemExit("systemd daemon engine state is invalid")
if str(audio.get("backend", "")).lower() != "null" or audio.get("null_sink") is not True:
    raise SystemExit("systemd daemon did not use the null backend")
if self_test.get("ok") is not True or doctor.get("ok") is not True:
    raise SystemExit("systemd daemon diagnostics failed")
if benchmark.get("frames") != 4096 or benchmark.get("non_finite_samples") != 0:
    raise SystemExit("systemd daemon benchmark failed")
PY

run_controller "$artifact_dir/shutdown.json" shutdown

stopped=0
for _ in {1..100}; do
  systemctl --user show "$unit_name" --property=ActiveState --property=Result \
    --property=ExecMainStatus > "$job_log"
  if [[ ! -S "$endpoint" ]] && grep -Fxq "ActiveState=inactive" "$job_log" && \
      grep -Fxq "Result=success" "$job_log" && grep -Fxq "ExecMainStatus=0" "$job_log"; then
    stopped=1
    break
  fi
  sleep 0.05
done
if [[ "$stopped" != "1" ]]; then
  cat "$job_log" >&2 || true
  systemctl --user status "$unit_name" --no-pager >&2 || true
  echo "The systemd-managed daemon did not exit cleanly." >&2
  exit 1
fi

systemctl --user --runtime disable "$unit_name"
linked=0
systemctl --user daemon-reload
started=0
if systemctl --user cat "$unit_name" >/dev/null 2>&1; then
  echo "The temporary systemd user unit remains installed." >&2
  exit 1
fi

cat "$artifact_dir/status.json" "$artifact_dir/audio.json" \
  "$artifact_dir/self-test.json" "$artifact_dir/doctor.json" \
  "$artifact_dir/benchmark.json"
echo "MOL_LINUX_SYSTEMD_USER_SMOKE_PASS"
