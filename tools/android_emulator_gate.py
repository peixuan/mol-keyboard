#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Install and validate the packaged Android application on one emulator."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time
from typing import Any, Sequence


APPLICATION_ID = "cn.zhangpeixuan.molkeyboard"
TEST_APPLICATION_ID = f"{APPLICATION_ID}.test"
RUNNER = f"{TEST_APPLICATION_ID}/{APPLICATION_ID}.AndroidSmokeInstrumentation"
REQUIRED_INTEGER_RESULTS = (
    "audioApi",
    "backgroundCallbacks",
    "callbacks",
    "focusResumedCallbacks",
    "frames",
    "hardwareKeys",
    "lockedCallbacks",
    "sampleRate",
)
REQUIRED_TRUE_RESULTS = (
    "focusInterrupted",
    "hardwareRepeatSuppressed",
    "idleBackgroundStopped",
)


def parse_devices(output: str) -> list[dict[str, str]]:
    devices: list[dict[str, str]] = []
    for raw_line in output.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("List of devices attached"):
            continue
        columns = line.split(maxsplit=1)
        if len(columns) != 2:
            continue
        serial, remainder = columns
        fields = remainder.split()
        devices.append({"serial": serial, "state": fields[0] if fields else "unknown"})
    return devices


def select_device(output: str, requested_serial: str | None) -> str:
    devices = parse_devices(output)
    if requested_serial:
        matches = [device for device in devices if device["serial"] == requested_serial]
        if len(matches) != 1:
            raise ValueError(f"requested Android device is unavailable: {requested_serial}")
        if matches[0]["state"] != "device":
            raise ValueError(
                f"requested Android device is not ready: {requested_serial} "
                f"state={matches[0]['state']}"
            )
        return requested_serial
    ready = [device["serial"] for device in devices if device["state"] == "device"]
    if len(ready) != 1:
        raise ValueError(f"expected exactly one ready Android device, found {ready}")
    return ready[0]


def wait_for_device(adb: Path, requested_serial: str | None, timeout_seconds: int) -> str:
    deadline = time.monotonic() + timeout_seconds
    last_output = ""
    while time.monotonic() < deadline:
        last_output = run_command(
            [str(adb), "devices", "-l"], timeout_seconds=15
        ).stdout
        try:
            return select_device(last_output, requested_serial)
        except ValueError:
            time.sleep(1)
    raise ValueError(
        "Android device did not become uniquely ready before the timeout: "
        f"{last_output.strip()}"
    )


def parse_instrumentation(output: str) -> dict[str, Any]:
    values: dict[str, str] = {}
    result_code: int | None = None
    for raw_line in output.splitlines():
        line = raw_line.strip()
        if line.startswith("INSTRUMENTATION_RESULT: "):
            payload = line.removeprefix("INSTRUMENTATION_RESULT: ")
            key, separator, value = payload.partition("=")
            if not separator or not key or key in values:
                raise ValueError(f"invalid or duplicate instrumentation result: {line}")
            values[key] = value
        elif line.startswith("INSTRUMENTATION_CODE: "):
            if result_code is not None:
                raise ValueError("duplicate instrumentation result code")
            result_code = int(line.removeprefix("INSTRUMENTATION_CODE: "))

    if result_code != -1:
        failure = values.get("failure", "instrumentation did not report a failure detail")
        raise ValueError(f"Android instrumentation failed with code {result_code}: {failure}")
    if "failure" in values:
        raise ValueError(f"Android instrumentation reported failure: {values['failure']}")

    missing = set(REQUIRED_INTEGER_RESULTS + REQUIRED_TRUE_RESULTS) - values.keys()
    if missing:
        raise ValueError(f"Android instrumentation omitted results: {sorted(missing)}")
    integers = {name: int(values[name]) for name in REQUIRED_INTEGER_RESULTS}
    for name in REQUIRED_TRUE_RESULTS:
        if values[name] != "true":
            raise ValueError(f"Android instrumentation result must be true: {name}")
    if integers["audioApi"] != 2:
        raise ValueError(f"Android runtime did not use AAudio: {integers['audioApi']}")
    if not 8_000 <= integers["sampleRate"] <= 384_000:
        raise ValueError(f"Android runtime reported invalid sample rate: {integers['sampleRate']}")
    for name in ("callbacks", "frames", "focusResumedCallbacks", "backgroundCallbacks"):
        if integers[name] <= 0:
            raise ValueError(f"Android instrumentation result must be positive: {name}")
    if integers["lockedCallbacks"] <= integers["backgroundCallbacks"]:
        raise ValueError("Android callbacks did not advance while the screen was locked")
    if integers["hardwareKeys"] != 30:
        raise ValueError(
            f"Android hardware-key coverage is incomplete: {integers['hardwareKeys']}"
        )
    return {**integers, **{name: True for name in REQUIRED_TRUE_RESULTS}}


def run_command(
    command: Sequence[str], *, timeout_seconds: int, check: bool = True
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        list(command),
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout_seconds,
    )
    if check and result.returncode != 0:
        raise RuntimeError(
            f"command failed with exit code {result.returncode}: {' '.join(command)}\n"
            f"{result.stdout}"
        )
    return result


def adb_command(
    adb: Path,
    serial: str | None,
    arguments: Sequence[str],
    *,
    timeout_seconds: int,
    check: bool = True,
) -> subprocess.CompletedProcess[str]:
    command = [str(adb)]
    if serial:
        command += ["-s", serial]
    command += list(arguments)
    return run_command(command, timeout_seconds=timeout_seconds, check=check)


def artifact_evidence(path: Path) -> dict[str, Any]:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return {
        "path": str(path.resolve()),
        "bytes": path.stat().st_size,
        "sha256": digest.hexdigest(),
    }


def device_property(adb: Path, serial: str, name: str) -> str:
    return adb_command(
        adb,
        serial,
        ["shell", "getprop", name],
        timeout_seconds=15,
    ).stdout.strip()


def run_gate(arguments: argparse.Namespace) -> dict[str, Any]:
    adb = arguments.adb.resolve()
    debug_apk = arguments.debug_apk.resolve()
    test_apk = arguments.test_apk.resolve()
    for path, label in ((adb, "adb"), (debug_apk, "debug APK"), (test_apk, "test APK")):
        if not path.is_file():
            raise ValueError(f"{label} does not exist: {path}")

    serial = wait_for_device(adb, arguments.serial, min(arguments.timeout_seconds, 60))
    state = adb_command(adb, serial, ["get-state"], timeout_seconds=15).stdout.strip()
    if state != "device":
        raise ValueError(f"Android device is not ready: {serial} state={state}")

    sdk_text = device_property(adb, serial, "ro.build.version.sdk")
    sdk = int(sdk_text)
    if sdk < 33:
        raise ValueError(f"Android emulator API must be at least 33, found {sdk}")

    instrumentation_output = ""
    try:
        for package_name in (TEST_APPLICATION_ID, APPLICATION_ID):
            adb_command(
                adb,
                serial,
                ["uninstall", package_name],
                timeout_seconds=30,
                check=False,
            )
        for apk in (debug_apk, test_apk):
            installed = adb_command(
                adb,
                serial,
                ["install", str(apk)],
                timeout_seconds=arguments.timeout_seconds,
            ).stdout
            if "Success" not in installed:
                raise ValueError(f"Android APK installation did not report success: {installed}")
        adb_command(
            adb,
            serial,
            ["shell", "pm", "grant", APPLICATION_ID, "android.permission.POST_NOTIFICATIONS"],
            timeout_seconds=30,
        )
        adb_command(adb, serial, ["logcat", "-c"], timeout_seconds=30)
        instrumentation = adb_command(
            adb,
            serial,
            ["shell", "am", "instrument", "-w", RUNNER],
            timeout_seconds=arguments.timeout_seconds,
        )
        instrumentation_output = instrumentation.stdout
        runtime = parse_instrumentation(instrumentation_output)
        services = adb_command(
            adb,
            serial,
            ["shell", "dumpsys", "activity", "services", APPLICATION_ID],
            timeout_seconds=30,
        ).stdout
        if "isForeground=true" in services:
            raise ValueError("Android instrumentation left a foreground service running")
        report = {
            "schema_version": 1,
            "result": "pass",
            "device": {
                "serial": serial,
                "state": state,
                "model": device_property(adb, serial, "ro.product.model"),
                "release": device_property(adb, serial, "ro.build.version.release"),
                "sdk": sdk,
                "abi": device_property(adb, serial, "ro.product.cpu.abi"),
            },
            "artifacts": {
                "application": artifact_evidence(debug_apk),
                "instrumentation": artifact_evidence(test_apk),
            },
            "runtime": runtime,
        }
        return report
    finally:
        adb_command(
            adb,
            serial,
            ["shell", "am", "force-stop", APPLICATION_ID],
            timeout_seconds=15,
            check=False,
        )
        if not arguments.keep_installed:
            for package_name in (TEST_APPLICATION_ID, APPLICATION_ID):
                adb_command(
                    adb,
                    serial,
                    ["uninstall", package_name],
                    timeout_seconds=30,
                    check=False,
                )


def run_self_test() -> None:
    successful = """\
INSTRUMENTATION_RESULT: audioApi=2
INSTRUMENTATION_RESULT: backgroundCallbacks=101
INSTRUMENTATION_RESULT: callbacks=46
INSTRUMENTATION_RESULT: focusInterrupted=true
INSTRUMENTATION_RESULT: focusResumedCallbacks=3
INSTRUMENTATION_RESULT: frames=25856
INSTRUMENTATION_RESULT: hardwareKeys=30
INSTRUMENTATION_RESULT: hardwareRepeatSuppressed=true
INSTRUMENTATION_RESULT: idleBackgroundStopped=true
INSTRUMENTATION_RESULT: lockedCallbacks=203
INSTRUMENTATION_RESULT: sampleRate=48000
INSTRUMENTATION_CODE: -1
"""
    parsed = parse_instrumentation(successful)
    if parsed["hardwareKeys"] != 30 or parsed["lockedCallbacks"] != 203:
        raise AssertionError("successful Android instrumentation was parsed incorrectly")
    if select_device("List of devices attached\nemulator-5554\tdevice product:test\n", None) != "emulator-5554":
        raise AssertionError("ready Android emulator selection failed")

    rejected = (
        successful.replace("hardwareKeys=30", "hardwareKeys=29"),
        successful.replace("INSTRUMENTATION_CODE: -1", "INSTRUMENTATION_CODE: 0"),
        successful.replace("lockedCallbacks=203", "lockedCallbacks=100"),
        successful + "INSTRUMENTATION_RESULT: sampleRate=48000\n",
    )
    for sample in rejected:
        try:
            parse_instrumentation(sample)
        except (TypeError, ValueError):
            continue
        raise AssertionError("invalid Android instrumentation evidence was accepted")
    print("Android emulator gate self-test passed")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--adb", type=Path)
    parser.add_argument("--debug-apk", type=Path)
    parser.add_argument("--test-apk", type=Path)
    parser.add_argument("--report", type=Path, default=Path("build/android-emulator-gate.json"))
    parser.add_argument("--serial")
    parser.add_argument("--timeout-seconds", type=int, default=120)
    parser.add_argument("--keep-installed", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    arguments = parser.parse_args()
    if not arguments.self_test:
        missing = [name for name in ("adb", "debug_apk", "test_apk") if getattr(arguments, name) is None]
        if missing:
            parser.error(f"required arguments are missing: {', '.join(missing)}")
    if arguments.timeout_seconds <= 0:
        parser.error("--timeout-seconds must be positive")
    return arguments


def main() -> int:
    arguments = parse_arguments()
    if arguments.self_test:
        run_self_test()
        return 0
    try:
        report = run_gate(arguments)
        arguments.report.parent.mkdir(parents=True, exist_ok=True)
        arguments.report.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(report, indent=2))
        return 0
    except (OSError, RuntimeError, subprocess.SubprocessError, TypeError, ValueError) as error:
        print(f"Android emulator gate failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
