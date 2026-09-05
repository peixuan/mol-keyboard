#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run a GUI acceptance mode and reject missing or failed evidence reports."""

from __future__ import annotations

import argparse
import pathlib
import subprocess


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True)
    parser.add_argument("--report", required=True)
    parser.add_argument("application_arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    report = pathlib.Path(args.report).resolve()
    report.unlink(missing_ok=True)
    application_arguments = args.application_arguments
    if application_arguments and application_arguments[0] == "--":
        application_arguments = application_arguments[1:]
    completed = subprocess.run(
        [str(pathlib.Path(args.executable).resolve()), *application_arguments],
        check=False,
        timeout=20,
    )
    if completed.returncode != 0:
        raise RuntimeError(f"GUI acceptance process exited with {completed.returncode}")
    if not report.is_file():
        raise RuntimeError("GUI acceptance report is missing")
    fields = dict(
        line.split("=", 1)
        for line in report.read_text(encoding="utf-8").splitlines()
        if "=" in line
    )
    if fields.get("passed") != "true":
        raise RuntimeError(f"GUI acceptance failed: {fields.get('detail', 'no detail')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
