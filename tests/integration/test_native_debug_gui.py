#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise the native debugger against a real null-backend daemon process."""

from __future__ import annotations

import argparse
import pathlib
import shutil
import subprocess
import sys


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--daemon", required=True)
    parser.add_argument("--gui", required=True)
    parser.add_argument("--state-dir", required=True)
    parser.add_argument("--endpoint", required=True)
    parser.add_argument("--report", required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    state_dir = pathlib.Path(args.state_dir).resolve()
    report = pathlib.Path(args.report).resolve()
    shutil.rmtree(state_dir, ignore_errors=True)
    state_dir.mkdir(parents=True)
    report.unlink(missing_ok=True)
    daemon_flags = subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0
    daemon = subprocess.Popen(
        [
            str(pathlib.Path(args.daemon).resolve()),
            "--null-backend",
            "--state-dir",
            str(state_dir),
            "--endpoint",
            args.endpoint,
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        creationflags=daemon_flags,
    )
    try:
        debugger = subprocess.run(
            [
                str(pathlib.Path(args.gui).resolve()),
                "--endpoint",
                args.endpoint,
                "--acceptance-output",
                str(report),
            ],
            check=False,
            timeout=15,
        )
        if debugger.returncode != 0:
            print(f"native debugger exited with {debugger.returncode}", file=sys.stderr)
            return 1
        try:
            daemon_stdout, daemon_stderr = daemon.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            print("daemon did not stop after the debugger requested shutdown", file=sys.stderr)
            return 1
        if daemon.returncode != 0:
            print(daemon_stdout, file=sys.stderr)
            print(daemon_stderr, file=sys.stderr)
            return 1
        if not report.is_file() or "passed=true" not in report.read_text(encoding="utf-8"):
            print("native debugger did not produce a passing report", file=sys.stderr)
            return 1
        return 0
    finally:
        if daemon.poll() is None:
            daemon.terminate()
            try:
                daemon.wait(timeout=3)
            except subprocess.TimeoutExpired:
                daemon.kill()
                daemon.wait(timeout=3)
        shutil.rmtree(state_dir, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
