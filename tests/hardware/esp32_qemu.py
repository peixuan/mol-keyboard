#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Execute and validate the real ESP-IDF firmware under Espressif QEMU."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import signal
import subprocess
import sys
import unittest
from datetime import datetime, timezone
from pathlib import Path


FIELD_PATTERN = re.compile(r"\b([a-z][a-z0-9_]*)=(\d+)")
ANSI_PATTERN = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
ERROR_PATTERNS = (
    re.compile(r"\bE \(\d+\) mol-[a-z0-9-]+:"),
    re.compile(r"assert failed", re.IGNORECASE),
    re.compile(r"Guru Meditation", re.IGNORECASE),
    re.compile(r"panic'ed", re.IGNORECASE),
)


def fields(line: str) -> dict[str, int]:
    return {name: int(value) for name, value in FIELD_PATTERN.findall(line)}


def validate_log(
    target: str, raw_log: str, return_code: int = 0
) -> tuple[list[str], dict[str, object]]:
    log = ANSI_PATTERN.sub("", raw_log)
    errors: list[str] = []
    audio_lines = [line for line in log.splitlines() if "audio frames=" in line]
    last_audio = fields(audio_lines[-1]) if audio_lines else {}
    frequency_match = re.search(
        r"Tiny core C4 passed: frequency=([0-9.]+) Hz peak=([0-9.]+)", log
    )
    expected_machine = f"-M {target}"

    if return_code != 0:
        errors.append(f"QEMU command exited with status {return_code}")
    if log.count("Reset reason=") != 1:
        errors.append("exactly one application boot was not observed")
    if expected_machine not in log:
        errors.append(f"Espressif QEMU machine {target} was not selected")
    if "ESP-IDF v6.1 2nd stage bootloader" not in log:
        errors.append("ESP-IDF 6.1 bootloader execution was not observed")
    if "Sequence FAT storage mounted with transactional recovery" not in log:
        errors.append("transactional sequence storage did not mount")
    if "Shared Mol Sequence passed: events=12" not in log:
        errors.append("shared Mol Sequence conformance did not pass")
    if frequency_match is None:
        errors.append("Tiny core C4 measurement was not reported")
        frequency = None
        peak = None
    else:
        frequency = float(frequency_match.group(1))
        peak = float(frequency_match.group(2))
        if not 260.5 <= frequency <= 262.5 or peak <= 0.01:
            errors.append("Tiny core C4 measurement is outside the firmware tolerance")
    if (
        "QEMU synthetic C4 command queued through the production input path" not in log
    ):
        errors.append("synthetic command did not enter the production input queue")
    if "QEMU virtual audio sink active: 32000 Hz" not in log:
        errors.append("the QEMU virtual audio sink did not start")
    if (
        "QEMU runtime excludes physical GPIO, Bluetooth, A2DP, USB, RF, and I2S claims"
        not in log
    ):
        errors.append("the physical-capability exclusion was not reported")
    if "Device control active:" not in log:
        errors.append("the production device-control task did not start")
    if len(audio_lines) < 3:
        errors.append("fewer than three firmware audio snapshots were observed")
    for name in (
        "render_fail",
        "write_fail",
        "partial",
        "wdt_fail",
        "input_drop",
        "input_reject",
        "nvs_io_fail",
        "seq_io_fail",
        "nonfinite",
    ):
        if last_audio.get(name, 0) != 0:
            errors.append(f"{name} is {last_audio[name]}, expected zero")
    if last_audio.get("frames", 0) < 32000:
        errors.append("the FreeRTOS audio task rendered fewer than 32000 frames")
    if last_audio.get("commands", 0) < 1:
        errors.append("the audio task did not drain any production input commands")
    if last_audio.get("nonzero", 0) < 1:
        errors.append("the live audio render was silent")
    if "QEMU firmware smoke passed:" not in log:
        errors.append("the firmware did not emit its terminal smoke-pass marker")
    if (
        "I2S active:" in log
        or "Bluetooth HID host active:" in log
        or "USB HID host active:" in log
    ):
        errors.append("a physical peripheral was unexpectedly started in the QEMU image")
    if any(pattern.search(log) for pattern in ERROR_PATTERNS):
        errors.append("a firmware assertion, panic, or project error was observed")

    return errors, {
        "boot_count": log.count("Reset reason="),
        "c4_frequency_hz": frequency,
        "c4_peak": peak,
        "audio_snapshots": len(audio_lines),
        "last_audio": last_audio or None,
    }


def file_evidence(build_directory: Path) -> dict[str, object]:
    evidence: dict[str, object] = {}
    for name in ("mol_keyboard_esp32.bin", "mol_keyboard_esp32.elf", "qemu_flash.bin"):
        path = build_directory / name
        if path.is_file():
            payload = path.read_bytes()
            evidence[name] = {
                "bytes": len(payload),
                "sha256": hashlib.sha256(payload).hexdigest(),
            }
    return evidence


def execute(command: list[str], cwd: Path, timeout_seconds: float) -> tuple[int, str]:
    creation_flags = subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0
    process = subprocess.Popen(
        command,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        creationflags=creation_flags,
        start_new_session=os.name != "nt",
    )
    try:
        output, _ = process.communicate(timeout=timeout_seconds)
        return process.returncode, output
    except subprocess.TimeoutExpired:
        if os.name == "nt":
            subprocess.run(
                ["taskkill", "/PID", str(process.pid), "/T", "/F"],
                check=False,
                capture_output=True,
            )
        else:
            os.killpg(process.pid, signal.SIGTERM)
        output, _ = process.communicate(timeout=10.0)
        return 124, output


def run(args: argparse.Namespace) -> int:
    project_directory = Path(args.project_directory).resolve()
    build_directory = Path(args.build_directory)
    if not build_directory.is_absolute():
        build_directory = project_directory / build_directory
    idf_path = os.environ.get("IDF_PATH")
    if not idf_path:
        raise RuntimeError("IDF_PATH is unset; activate the pinned ESP-IDF environment first")
    idf_py = Path(idf_path) / "tools" / "idf.py"
    command = [
        sys.executable,
        str(idf_py),
        "-B",
        str(build_directory),
        "qemu",
        "--qemu-extra-args=-no-reboot",
    ]
    started = datetime.now(timezone.utc).isoformat()
    return_code, raw_log = execute(
        command, project_directory, args.timeout_seconds
    )
    print(raw_log, end="" if raw_log.endswith("\n") else "\n")
    errors, firmware = validate_log(args.target, raw_log, return_code)
    sdkconfig = build_directory / "sdkconfig"
    if not sdkconfig.is_file() or "CONFIG_MOL_QEMU_RUNTIME=y" not in sdkconfig.read_text(
        encoding="utf-8", errors="replace"
    ):
        errors.append("the build does not have CONFIG_MOL_QEMU_RUNTIME=y")
    report = {
        "schema": 1,
        "verification_level": "emulated-firmware",
        "emulator": "Espressif QEMU",
        "target": args.target,
        "started_utc": started,
        "finished_utc": datetime.now(timezone.utc).isoformat(),
        "passed": not errors,
        "errors": errors,
        "firmware": firmware,
        "artifacts": file_evidence(build_directory),
        "excluded_claims": [
            "physical ESP32 or ESP32-S3 silicon execution",
            "physical GPIO, HID, USB, Bluetooth, RF, I2S, or acoustic behavior",
            "real-time deadline, watchdog, power, thermal, or endurance performance",
        ],
    }
    if args.log:
        log_path = Path(args.log)
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_path.write_text(raw_log, encoding="utf-8")
    if args.report:
        report_path = Path(args.report)
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0 if not errors else 1


def passing_log(target: str) -> str:
    return f"""Running qemu (fg): qemu-system-xtensa -M {target} -no-reboot
I (1) boot: ESP-IDF v6.1 2nd stage bootloader
I (2) mol-keyboard: Reset reason=1
I (3) mol-keyboard: Sequence FAT storage mounted with transactional recovery
I (4) mol-keyboard: Shared Mol Sequence passed: events=12 final=108000
I (5) mol-keyboard: Tiny core C4 passed: frequency=262.5000 Hz peak=0.058417
I (6) mol-keyboard: QEMU synthetic C4 command queued through the production input path
I (7) mol-keyboard: QEMU virtual audio sink active: 32000 Hz, block=128 frames; physical I2S is not exercised
I (8) mol-keyboard: QEMU runtime excludes physical GPIO, Bluetooth, A2DP, USB, RF, and I2S claims
I (9) mol-keyboard: Device control active: priority=3 core=0; audio never waits on storage
I (10) mol-keyboard: audio frames=32000 render_fail=0 write_fail=0 partial=0 wdt_fail=0 commands=12 input_drop=0 input_reject=0 nvs_io_fail=0 seq_io_fail=0 nonfinite=0 nonzero=100
I (11) mol-keyboard: audio frames=64000 render_fail=0 write_fail=0 partial=0 wdt_fail=0 commands=12 input_drop=0 input_reject=0 nvs_io_fail=0 seq_io_fail=0 nonfinite=0 nonzero=200
I (12) mol-keyboard: audio frames=96000 render_fail=0 write_fail=0 partial=0 wdt_fail=0 commands=12 input_drop=0 input_reject=0 nvs_io_fail=0 seq_io_fail=0 nonfinite=0 nonzero=300
I (13) mol-keyboard: QEMU firmware smoke passed: snapshots=3 frames=96000 commands=12 nonzero=300
"""


class ParserTests(unittest.TestCase):
    def test_both_target_logs_pass(self) -> None:
        for target in ("esp32", "esp32s3"):
            errors, summary = validate_log(target, passing_log(target))
            self.assertEqual([], errors)
            self.assertEqual(3, summary["audio_snapshots"])

    def test_missing_marker_and_nonfinite_audio_fail(self) -> None:
        log = passing_log("esp32").replace(
            "QEMU firmware smoke passed:", "QEMU firmware smoke incomplete:"
        )
        log = log.replace("nonfinite=0 nonzero=300", "nonfinite=1 nonzero=300")
        errors, _ = validate_log("esp32", log)
        self.assertTrue(any("terminal" in error for error in errors))
        self.assertTrue(any("nonfinite" in error for error in errors))

    def test_physical_peripheral_start_fails(self) -> None:
        errors, _ = validate_log(
            "esp32s3", passing_log("esp32s3") + "I2S active: 32000 Hz\n"
        )
        self.assertTrue(any("physical peripheral" in error for error in errors))


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--target", choices=("esp32", "esp32s3"))
    parser.add_argument(
        "--project-directory",
        default=str(Path(__file__).resolve().parents[2] / "platforms" / "esp32"),
    )
    parser.add_argument("--build-directory")
    parser.add_argument("--timeout-seconds", type=float, default=180.0)
    parser.add_argument("--report")
    parser.add_argument("--log")
    args = parser.parse_args()
    if not args.self_test and (not args.target or not args.build_directory):
        parser.error("--target and --build-directory are required")
    if args.timeout_seconds <= 0:
        parser.error("--timeout-seconds must be positive")
    return args


if __name__ == "__main__":
    arguments = parse_arguments()
    if arguments.self_test:
        unittest.main(argv=[sys.argv[0]])
    raise SystemExit(run(arguments))
