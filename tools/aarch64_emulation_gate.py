#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise the complete Linux AArch64 product under QEMU user emulation."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import struct
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


EM_AARCH64 = 183


class GateError(RuntimeError):
    """Raised when emulated product evidence fails closed."""


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_aarch64_elf(path: Path) -> None:
    try:
        header = path.read_bytes()[:20]
    except OSError as error:
        raise GateError(f"cannot read target executable {path}: {error}") from error
    if len(header) < 20 or header[:4] != b"\x7fELF":
        raise GateError(f"target executable is not ELF: {path}")
    byte_order = "<" if header[5] == 1 else ">" if header[5] == 2 else ""
    if not byte_order or struct.unpack(f"{byte_order}H", header[18:20])[0] != EM_AARCH64:
        raise GateError(f"target executable is not AArch64 ELF: {path}")


def run_checked(command: list[str], timeout: float = 30.0) -> subprocess.CompletedProcess[str]:
    try:
        result = subprocess.run(
            command,
            check=False,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise GateError(f"command failed to execute: {command[0]}: {error}") from error
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).strip()
        raise GateError(
            f"command returned {result.returncode}: {' '.join(command)}"
            + (f"\n{detail[-2000:]}" if detail else "")
        )
    return result


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise GateError(message)


class EmulatedProduct:
    def __init__(self, qemu: Path, sysroot: Path, build_dir: Path, scratch: Path) -> None:
        self.qemu = qemu
        self.sysroot = sysroot
        self.build_dir = build_dir
        self.scratch = scratch
        self.daemon = build_dir / "apps/mol-keyboardd/mol-keyboardd"
        self.client = build_dir / "apps/molctl/molctl"
        self.renderer = build_dir / "apps/mol-render/mol-render"
        self.endpoint = scratch / "mol-keyboard.sock"
        self.state_dir = scratch / "state"
        self.process: subprocess.Popen[str] | None = None

    def command(self, executable: Path, *arguments: str) -> list[str]:
        return [
            str(self.qemu),
            "-L",
            str(self.sysroot),
            str(executable),
            *arguments,
        ]

    def validate_artifacts(self) -> dict[str, dict[str, Any]]:
        artifacts: dict[str, dict[str, Any]] = {}
        for name, path in (
            ("mol-keyboardd", self.daemon),
            ("molctl", self.client),
            ("mol-render", self.renderer),
        ):
            require_aarch64_elf(path)
            help_result = run_checked(self.command(path, "--help"))
            expect("Usage:" in help_result.stdout, f"{name} help output is missing usage")
            artifacts[name] = {
                "path": str(path),
                "bytes": path.stat().st_size,
                "sha256": sha256(path),
            }
        return artifacts

    def start_daemon(self) -> None:
        self.state_dir.mkdir(parents=True)
        command = self.command(
            self.daemon,
            "--null-backend",
            "--endpoint",
            str(self.endpoint),
            "--state-dir",
            str(self.state_dir),
        )
        try:
            self.process = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
        except OSError as error:
            raise GateError(f"cannot start emulated daemon: {error}") from error

    def stop_daemon(self) -> None:
        if self.process is None or self.process.poll() is not None:
            return
        self.process.terminate()
        try:
            self.process.wait(timeout=3.0)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait(timeout=3.0)

    def rpc(self, *arguments: str, timeout: float = 30.0) -> Any:
        command = self.command(
            self.client,
            "--json",
            "--endpoint",
            str(self.endpoint),
            "--state-dir",
            str(self.state_dir),
            *arguments,
        )
        response = run_checked(command, timeout=timeout)
        try:
            payload = json.loads(response.stdout)
            expect(payload.get("jsonrpc") == "2.0", "CLI returned an invalid JSON-RPC version")
            expect("error" not in payload, f"CLI returned a JSON-RPC error: {payload.get('error')}")
            return payload["result"]
        except (json.JSONDecodeError, KeyError, AttributeError, TypeError) as error:
            raise GateError(f"CLI returned invalid JSON: {response.stdout[-2000:]}") from error

    def wait_until_ready(self) -> dict[str, Any]:
        deadline = time.monotonic() + 10.0
        last_error = "daemon did not answer"
        while time.monotonic() < deadline:
            if self.process is not None and self.process.poll() is not None:
                output = self.process.stdout.read() if self.process.stdout is not None else ""
                raise GateError(
                    f"emulated daemon exited before readiness with {self.process.returncode}: {output}"
                )
            try:
                result = self.rpc("status", timeout=2.0)
                expect(isinstance(result, dict), "status result is not an object")
                return result
            except GateError as error:
                last_error = str(error)
                time.sleep(0.05)
        raise GateError(f"emulated daemon was not ready within 10 seconds: {last_error}")

    def exercise_service(self) -> dict[str, Any]:
        self.start_daemon()
        initial = self.wait_until_ready()
        expect(initial.get("sample_rate") == 48000, "daemon did not initialize at 48 kHz")
        expect(initial.get("channel_count") == 2, "daemon did not initialize stereo output")
        expect(initial.get("max_voices") == 32, "standard profile did not expose 32 voices")

        capabilities = self.rpc("capabilities")
        expect(capabilities.get("build_profile") == "standard", "unexpected build profile")
        expect(capabilities.get("persistent_storage") is True, "storage capability is absent")

        self.rpc("preset", "set", "violin")
        self.rpc("tempo", "137")
        self.rpc("record", "start")
        self.rpc("note", "on", "60", "--velocity", "0.8", "--gesture", "7001")
        time.sleep(0.08)
        self.rpc("note", "off", "--gesture", "7001")
        recording = self.scratch / "recorded.molseq"
        self.rpc("record", "stop", "--output", str(recording))
        expect(recording.is_file() and recording.stat().st_size > 0, "recording was not persisted")
        self.rpc("play", str(recording))
        self.rpc("rpc", "playback.stop", "{}")

        doctor = self.rpc("doctor")
        expect(doctor.get("ok") is True, "daemon doctor failed")
        checks = doctor.get("checks", [])
        expect(
            any(check.get("id") == "service-ipc" and check.get("status") == "pass" for check in checks),
            "doctor did not validate local IPC",
        )
        self_test = self.rpc("self-test")
        expect(
            self_test.get("ok") is True
            and self_test.get("patches") is True
            and self_test.get("runtime") is True,
            "daemon self-test failed",
        )
        benchmark = self.rpc("benchmark", timeout=60.0)
        expect(benchmark.get("frames") == 96000, "benchmark rendered an unexpected frame count")
        expect(benchmark.get("non_finite_samples") == 0, "benchmark produced non-finite samples")
        expect(float(benchmark.get("peak", 0.0)) > 0.0, "benchmark produced silent audio")

        self.rpc("all-notes-off")
        self.rpc("rpc", "engine.allSoundOff", "{}")
        final = self.rpc("status")
        expect(final.get("active_gestures") == 0, "gesture remained active after all-notes-off")
        expect(final.get("active_voices") == 0, "voice remained active after all-notes-off")
        self.rpc("shutdown")
        expect(self.process is not None, "daemon process was not started")
        try:
            return_code = self.process.wait(timeout=5.0)
        except subprocess.TimeoutExpired as error:
            raise GateError("daemon did not exit after system.shutdown") from error
        expect(return_code == 0, f"daemon exited with {return_code}")
        expect((self.state_dir / "config.json").is_file(), "daemon configuration was not persisted")
        return {
            "initial_state": {
                "sample_rate": initial["sample_rate"],
                "channel_count": initial["channel_count"],
                "max_voices": initial["max_voices"],
            },
            "capabilities": {
                "build_profile": capabilities["build_profile"],
                "persistent_storage": capabilities["persistent_storage"],
            },
            "doctor": {
                "ok": doctor["ok"],
                "pass": sum(check.get("status") == "pass" for check in checks),
                "warning": sum(check.get("status") == "warning" for check in checks),
            },
            "self_test": self_test,
            "benchmark": benchmark,
            "recording_bytes": recording.stat().st_size,
            "shutdown_exit_code": return_code,
        }

    def exercise_renderer(self, sequence: Path) -> dict[str, Any]:
        output = self.scratch / "render.wav"
        report_path = self.scratch / "render.json"
        run_checked(
            self.command(
                self.renderer,
                str(sequence),
                "--output",
                str(output),
                "--report",
                str(report_path),
            ),
            timeout=60.0,
        )
        try:
            report = json.loads(report_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise GateError(f"renderer report is invalid: {error}") from error
        expect(output.is_file() and output.stat().st_size > 44, "renderer did not produce a WAV")
        expect(output.read_bytes()[:4] == b"RIFF", "renderer output is not RIFF/WAVE")
        expect(report.get("sample_rate") == 48000, "renderer report has an unexpected sample rate")
        expect(report.get("channels") == 2, "renderer report has an unexpected channel count")
        expect(report.get("nan_inf_count") == 0, "renderer produced non-finite samples")
        expect(report.get("clipped_sample_count") == 0, "renderer clipped samples")
        expect(report.get("underrun_count") == 0, "offline renderer reported underruns")
        expect(float(report.get("peak", 0.0)) > 0.0, "renderer produced silent audio")
        expect(report.get("sha256") == sha256(output), "renderer WAV digest does not match report")
        return {
            "duration_seconds": report["duration_seconds"],
            "sample_rate": report["sample_rate"],
            "channels": report["channels"],
            "peak": report["peak"],
            "rms": report["rms"],
            "nan_inf_count": report["nan_inf_count"],
            "clipped_sample_count": report["clipped_sample_count"],
            "underrun_count": report["underrun_count"],
            "wav_bytes": output.stat().st_size,
            "wav_sha256": report["sha256"],
        }


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", required=True, type=Path, help="qemu-aarch64 executable")
    parser.add_argument("--sysroot", required=True, type=Path, help="AArch64 runtime library prefix")
    parser.add_argument("--build-dir", required=True, type=Path, help="configured AArch64 build")
    parser.add_argument("--artifact-commit", required=True, help="exact source commit for the artifacts")
    parser.add_argument("--report", required=True, type=Path, help="JSON report destination")
    parser.add_argument(
        "--sequence",
        type=Path,
        default=Path("examples/sequences/scale-study.molseq"),
        help="sequence used for the offline render",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_arguments()
    started = time.monotonic()
    report: dict[str, Any] = {
        "schema_version": 1,
        "gate": "linux-aarch64-qemu-user",
        "verification_level": "simulated-runtime",
        "artifact_commit": args.artifact_commit,
        "host_machine": os.uname().machine,
        "started_utc": datetime.now(timezone.utc).isoformat(),
        "passed": False,
        "excluded_claims": [
            "native ARM64 scheduling or performance",
            "physical audio output and latency",
            "physical keyboard or evdev input",
            "device suspend, route change, or device-loss recovery",
        ],
    }
    product: EmulatedProduct | None = None
    try:
        expect(
            re.fullmatch(r"[0-9a-f]{40}", args.artifact_commit) is not None,
            "--artifact-commit must be an exact lowercase 40-character Git object ID",
        )
        qemu = args.qemu.resolve(strict=True)
        sysroot = args.sysroot.resolve(strict=True)
        build_dir = args.build_dir.resolve(strict=True)
        sequence = args.sequence.resolve(strict=True)
        qemu_version = run_checked([str(qemu), "--version"]).stdout.splitlines()[0]
        expect("qemu-aarch64" in qemu_version, "emulator is not qemu-aarch64")
        with tempfile.TemporaryDirectory(prefix="mol-aarch64-emulation-") as temporary:
            product = EmulatedProduct(qemu, sysroot, build_dir, Path(temporary))
            report["emulator"] = qemu_version
            report["artifacts"] = product.validate_artifacts()
            report["service"] = product.exercise_service()
            report["renderer"] = product.exercise_renderer(sequence)
        report["passed"] = True
    except Exception as error:
        report["error"] = str(error)
    finally:
        if product is not None:
            product.stop_daemon()
        report["duration_seconds"] = round(time.monotonic() - started, 3)
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
