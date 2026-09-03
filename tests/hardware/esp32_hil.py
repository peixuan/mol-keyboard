#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run and validate a fail-closed ESP32 hardware-in-the-loop session."""

from __future__ import annotations

import argparse
import json
import math
import re
import subprocess
import sys
import time
import unittest
import wave
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import TextIO


FIELD_PATTERN = re.compile(r"\b([a-z][a-z0-9_]*)=(\d+)")
ANSI_PATTERN = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
PROJECT_ERROR_PATTERN = re.compile(r"\bE \(\d+\) mol-[a-z0-9-]+:")


def fields(line: str) -> dict[str, int]:
    return {name: int(value) for name, value in FIELD_PATTERN.findall(line)}


@dataclass
class HilState:
    target: str
    reset_count: int = 0
    sequence_passed: bool = False
    c4_frequency: float | None = None
    c4_peak: float | None = None
    engine_required: int | None = None
    engine_static: int | None = None
    sample_rate: int | None = None
    i2s_active: bool = False
    gpio_active: bool = False
    control_active: bool = False
    bluetooth_active: bool = False
    target_capability_passed: bool = False
    a2dp_capability_passed: bool = False
    usb_active: bool = False
    web_session_started: bool = False
    audio: list[dict[str, int]] = field(default_factory=list)
    control: list[dict[str, int]] = field(default_factory=list)
    a2dp: list[dict[str, int]] = field(default_factory=list)
    usb: list[dict[str, int]] = field(default_factory=list)
    project_errors: list[str] = field(default_factory=list)

    def observe(self, raw_line: str) -> None:
        line = ANSI_PATTERN.sub("", raw_line).strip()
        if "Reset reason=" in line:
            self.reset_count += 1
        if "Shared Mol Sequence passed:" in line:
            values = fields(line)
            self.sequence_passed = values.get("events") == 12 and values.get("final", 0) > 0
        if "Tiny engine memory:" in line:
            values = fields(line)
            self.engine_required = values.get("required")
            self.engine_static = values.get("static")
        if "Tiny core C4 passed:" in line:
            match = re.search(r"frequency=([0-9.]+) Hz peak=([0-9.]+)", line)
            if match:
                self.c4_frequency = float(match.group(1))
                self.c4_peak = float(match.group(2))
        if "I2S active:" in line:
            match = re.search(r"I2S active: (\d+) Hz", line)
            self.i2s_active = match is not None
            if match:
                self.sample_rate = int(match.group(1))
        if "GPIO matrix active:" in line:
            self.gpio_active = True
        if "Device control active:" in line:
            self.control_active = True
        if "Bluetooth HID host active:" in line:
            self.bluetooth_active = True
            if self.target == "esp32":
                self.target_capability_passed = "BLE + Classic" in line
            else:
                self.target_capability_passed = "Classic unsupported by this SoC" in line
        if self.target == "esp32" and "A2DP Source capability=available" in line:
            self.a2dp_capability_passed = True
        if self.target == "esp32s3" and "A2DP Source capability=unsupported" in line:
            self.a2dp_capability_passed = "Classic Bluetooth absent on this SoC" in line
        if "USB HID host active:" in line:
            self.usb_active = True
        if "Private configuration AP enabled" in line:
            self.web_session_started = True
        if "audio frames=" in line:
            self.audio.append(fields(line))
        elif "control_config=" in line:
            self.control.append(fields(line))
        elif "a2dp_found=" in line:
            self.a2dp.append(fields(line))
        elif "usb_ifaces=" in line:
            self.usb.append(fields(line))
        if PROJECT_ERROR_PATTERN.search(line):
            self.project_errors.append(line[-240:])


@dataclass(frozen=True)
class Requirements:
    duration_seconds: float
    require_gpio: bool = False
    require_bluetooth: bool = False
    require_usb: bool = False
    require_a2dp: bool = False
    require_web: bool = False
    require_clear_pairing: bool = False


def require_zero(snapshot: dict[str, int], names: tuple[str, ...], errors: list[str]) -> None:
    for name in names:
        if snapshot.get(name, 0) != 0:
            errors.append(f"{name} is {snapshot[name]}, expected zero")


def validate_state(state: HilState, requirements: Requirements) -> list[str]:
    errors: list[str] = []
    if state.reset_count != 1:
        errors.append(f"observed {state.reset_count} boots, expected exactly one")
    if not state.sequence_passed:
        errors.append("shared Mol Sequence startup conformance was not observed")
    if state.engine_required is None or state.engine_static is None:
        errors.append("engine memory budget was not reported")
    elif state.engine_required > state.engine_static:
        errors.append("engine requires more than the static arena")
    if state.c4_frequency is None or not 260.5 <= state.c4_frequency <= 262.5:
        errors.append("C4 startup frequency is missing or outside tolerance")
    if state.c4_peak is None or state.c4_peak <= 0.01:
        errors.append("C4 startup peak is missing or silent")
    if not state.i2s_active or state.sample_rate is None:
        errors.append("I2S activation was not observed")
    if not state.gpio_active:
        errors.append("GPIO matrix activation was not observed")
    if not state.control_active:
        errors.append("device control activation was not observed")
    if (
        not state.bluetooth_active
        or not state.target_capability_passed
        or not state.a2dp_capability_passed
    ):
        errors.append("target-specific Bluetooth capability report was not observed")
    if state.target == "esp32s3" and not state.usb_active:
        errors.append("ESP32-S3 USB HID host activation was not observed")
    minimum_snapshots = max(1, math.floor(requirements.duration_seconds / 10.0) - 2)
    if len(state.audio) < minimum_snapshots:
        errors.append(
            f"only {len(state.audio)} audio diagnostics were observed; expected {minimum_snapshots}"
        )
    if state.audio:
        last_audio = state.audio[-1]
        require_zero(
            last_audio,
            (
                "render_fail",
                "write_fail",
                "partial",
                "dma_q_ovf",
                "deadline_miss",
                "wdt_fail",
                "input_drop",
                "input_reject",
                "gpio_fail",
                "nvs_io_fail",
                "seq_corrupt",
                "seq_io_fail",
                "bt_invalid",
                "bt_fail",
            ),
            errors,
        )
        if requirements.require_gpio and last_audio.get("gpio_events", 0) == 0:
            errors.append("no GPIO key transition was observed")
        if requirements.require_bluetooth and last_audio.get("bt_report", 0) == 0:
            errors.append("no Bluetooth HID report was observed")
        if len(state.audio) >= 2:
            progress = (state.audio[-1].get("frames", 0) - state.audio[0].get("frames", 0)) % (
                1 << 32
            )
            expected = int(
                (state.sample_rate or 0)
                * max(0.0, requirements.duration_seconds - 20.0)
                * 0.90
            )
            if progress < expected:
                errors.append(f"audio advanced {progress} frames; expected at least {expected}")
    if state.control:
        require_zero(
            state.control[-1],
            ("control_reject", "control_q_reject", "control_io_fail"),
            errors,
        )
        if requirements.require_web and state.control[-1].get("control_config", 0) == 0:
            errors.append("physical Web configuration entry was not counted")
        if (
            requirements.require_clear_pairing
            and state.control[-1].get("control_unpair", 0) == 0
        ):
            errors.append("physical clear-pairing operation was not counted")
    if requirements.require_web and not state.web_session_started:
        errors.append("physically authorized Web configuration AP was not observed")
    if state.target == "esp32" and not state.a2dp:
        errors.append("ESP32 A2DP diagnostics were not observed")
    if requirements.require_a2dp:
        if state.target != "esp32":
            errors.append("A2DP cannot be required on ESP32-S3")
        elif not state.a2dp:
            errors.append("A2DP diagnostics were not observed")
        else:
            last_a2dp = state.a2dp[-1]
            if last_a2dp.get("a2dp_connect", 0) == 0:
                errors.append("no A2DP sink connection was observed")
            if last_a2dp.get("a2dp_callbacks", 0) == 0:
                errors.append("no A2DP PCM callback was observed")
            if last_a2dp.get("a2dp_pcm_bytes", 0) == 0:
                errors.append("no PCM was submitted to A2DP")
            require_zero(
                last_a2dp,
                (
                    "a2dp_conn_fail",
                    "a2dp_codec_reject",
                    "a2dp_ctrl_fail",
                    "a2dp_pcm_drop",
                    "a2dp_underrun",
                    "a2dp_auth_fail",
                ),
                errors,
            )
    if requirements.require_usb:
        if state.target != "esp32s3":
            errors.append("USB HID cannot be required on the original ESP32 target")
        elif not state.usb or state.usb[-1].get("usb_report", 0) == 0:
            errors.append("no ESP32-S3 USB HID report was observed")
    if state.usb:
        require_zero(
            state.usb[-1],
            ("usb_invalid", "usb_transfer_err", "usb_delivery_fail", "usb_driver_fail", "usb_queue_ovf"),
            errors,
        )
    errors.extend(f"firmware error: {line}" for line in state.project_errors)
    return errors


def analyze_capture(path: Path, expected_rate: int | None) -> dict[str, int | float]:
    with wave.open(str(path), "rb") as capture:
        channels = capture.getnchannels()
        sample_width = capture.getsampwidth()
        sample_rate = capture.getframerate()
        frame_count = capture.getnframes()
        payload = capture.readframes(frame_count)
    if channels != 2 or sample_width != 2:
        raise ValueError("I2S capture must be stereo PCM16 WAV")
    if expected_rate is not None and sample_rate != expected_rate:
        raise ValueError(f"capture rate {sample_rate} does not match firmware rate {expected_rate}")
    if frame_count < sample_rate * 5:
        raise ValueError("I2S capture must contain at least five seconds")
    samples = memoryview(payload).cast("h")
    peak = 0
    square_sum = 0.0
    clipped = 0
    for sample in samples:
        magnitude = abs(sample)
        peak = max(peak, magnitude)
        square_sum += float(sample) * float(sample)
        if magnitude >= 32767:
            clipped += 1
    rms = math.sqrt(square_sum / len(samples)) if samples else 0.0
    if peak < 100 or rms < 20.0:
        raise ValueError("I2S capture is silent or below the validation floor")
    if clipped != 0:
        raise ValueError(f"I2S capture contains {clipped} clipped samples")
    return {
        "channels": channels,
        "sample_rate": sample_rate,
        "frames": frame_count,
        "peak_pcm16": peak,
        "rms_pcm16": round(rms, 3),
        "clipped_samples": clipped,
    }


def state_summary(state: HilState) -> dict[str, object]:
    return {
        "reset_count": state.reset_count,
        "sequence_passed": state.sequence_passed,
        "engine_required": state.engine_required,
        "engine_static": state.engine_static,
        "c4_frequency": state.c4_frequency,
        "c4_peak": state.c4_peak,
        "sample_rate": state.sample_rate,
        "audio_snapshots": len(state.audio),
        "last_audio": state.audio[-1] if state.audio else None,
        "last_control": state.control[-1] if state.control else None,
        "last_a2dp": state.a2dp[-1] if state.a2dp else None,
        "last_usb": state.usb[-1] if state.usb else None,
    }


def replace_field(line: str, name: str, value: int) -> str:
    updated, count = re.subn(rf"\b{re.escape(name)}=\d+", f"{name}={value}", line)
    if count != 1:
        raise ValueError(f"diagnostic fixture does not contain exactly one {name} field")
    return updated


def simulated_state(target: str, duration_seconds: float, injected_fault: str | None) -> HilState:
    if duration_seconds < 20.0:
        raise ValueError("simulated HIL duration must be at least 20 seconds")
    lines = passing_log(target)
    audio_template = next(line for line in reversed(lines) if "audio frames=" in line)
    control_template = next(line for line in reversed(lines) if "control_config=" in line)
    a2dp_template = next((line for line in reversed(lines) if "a2dp_found=" in line), None)
    usb_template = next((line for line in reversed(lines) if "usb_ifaces=" in line), None)
    snapshot_count = math.floor(duration_seconds / 10.0)
    for snapshot in range(3, snapshot_count + 1):
        final_snapshot = snapshot == snapshot_count
        audio = replace_field(audio_template, "frames", snapshot * 10 * 32000)
        if final_snapshot:
            audio = replace_field(audio, "gpio_events", 4)
            audio = replace_field(audio, "bt_report", 4)
        lines.append(audio)
        control = control_template
        if final_snapshot:
            control = replace_field(control, "control_config", 1)
            control = replace_field(control, "control_unpair", 1)
        lines.append(control)
    lines.append("I (20) mol-keyboard: Private configuration AP enabled by physical hold")
    if a2dp_template is not None:
        a2dp = replace_field(a2dp_template, "a2dp_connect", 1)
        a2dp = replace_field(a2dp, "a2dp_callbacks", 400)
        a2dp = replace_field(a2dp, "a2dp_pcm_bytes", 1638400)
        lines.append(a2dp)
    if usb_template is not None:
        lines.append(replace_field(usb_template, "usb_report", 4))

    state = HilState(target)
    for line in lines:
        state.observe(line)
    if injected_fault == "reset":
        state.observe("I (999) mol-keyboard: Reset reason=7")
    elif injected_fault == "deadline-miss":
        state.audio[-1]["deadline_miss"] = 1
    elif injected_fault == "stalled-audio":
        state.audio[-1]["frames"] = state.audio[0]["frames"]
    elif injected_fault == "firmware-error":
        state.observe("E (999) mol-keyboard: injected model failure")
    return state


def simulation_requirements(target: str, duration_seconds: float) -> Requirements:
    return Requirements(
        duration_seconds=duration_seconds,
        require_gpio=True,
        require_bluetooth=True,
        require_usb=target == "esp32s3",
        require_a2dp=target == "esp32",
        require_web=True,
        require_clear_pairing=True,
    )


def run_simulation(args: argparse.Namespace) -> int:
    started = datetime.now(timezone.utc).isoformat()
    state = simulated_state(args.target, args.duration_seconds, args.inject_fault)
    errors = validate_state(state, simulation_requirements(args.target, args.duration_seconds))
    report = {
        "schema": 1,
        "verification_level": "simulated-hil",
        "target": args.target,
        "started_utc": started,
        "finished_utc": datetime.now(timezone.utc).isoformat(),
        "duration_seconds": args.duration_seconds,
        "virtual_clock": True,
        "injected_fault": args.inject_fault,
        "passed": not errors,
        "errors": errors,
        "firmware": state_summary(state),
        "excluded_claims": [
            "firmware execution on an ESP32 chip",
            "physical UART, GPIO, HID, I2S, USB, or Bluetooth behavior",
            "wall-clock endurance, watchdog, power, RF, or acoustic performance",
        ],
    }
    if args.report:
        Path(args.report).parent.mkdir(parents=True, exist_ok=True)
        Path(args.report).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0 if not errors else 1


def reset_target(target: str, port: str) -> None:
    subprocess.run(
        [
            sys.executable,
            "-m",
            "esptool",
            "--chip",
            target,
            "--port",
            port,
            "--after",
            "hard-reset",
            "read-mac",
        ],
        check=True,
    )


def monitor(args: argparse.Namespace, raw_log: TextIO | None) -> HilState:
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError as error:
        raise RuntimeError("pyserial is required; run inside the ESP-IDF environment") from error
    if not args.no_reset:
        reset_target(args.target, args.port)
    state = HilState(args.target)
    deadline = time.monotonic() + args.duration_seconds
    with serial.Serial(args.port, args.baud, timeout=1.0) as connection:
        connection.dtr = False
        connection.rts = False
        while time.monotonic() < deadline:
            payload = connection.readline()
            if not payload:
                continue
            line = payload.decode("utf-8", errors="replace")
            print(line, end="", flush=True)
            if raw_log is not None:
                raw_log.write(line)
                raw_log.flush()
            state.observe(line)
    return state


def run(args: argparse.Namespace) -> int:
    raw_log: TextIO | None = None
    started = datetime.now(timezone.utc).isoformat()
    try:
        if args.log:
            Path(args.log).parent.mkdir(parents=True, exist_ok=True)
            raw_log = Path(args.log).open("w", encoding="utf-8", newline="\n")
        state = monitor(args, raw_log)
        requirements = Requirements(
            args.duration_seconds,
            args.require_gpio,
            args.require_bluetooth,
            args.require_usb,
            args.require_a2dp,
            args.require_web,
            args.require_clear_pairing,
        )
        errors = validate_state(state, requirements)
        capture = None
        if args.i2s_capture:
            try:
                capture = analyze_capture(Path(args.i2s_capture), state.sample_rate)
            except (OSError, ValueError, wave.Error) as error:
                errors.append(f"I2S capture rejected: {error}")
        elif not args.serial_only:
            errors.append("a real I2S capture WAV is required unless --serial-only is selected")
        report = {
            "schema": 1,
            "target": args.target,
            "port": args.port,
            "started_utc": started,
            "finished_utc": datetime.now(timezone.utc).isoformat(),
            "duration_seconds": args.duration_seconds,
            "passed": not errors,
            "errors": errors,
            "firmware": state_summary(state),
            "i2s_capture": capture,
        }
        if args.report:
            Path(args.report).parent.mkdir(parents=True, exist_ok=True)
            Path(args.report).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(report, indent=2))
        return 0 if not errors else 1
    finally:
        if raw_log is not None:
            raw_log.close()


def passing_log(target: str) -> list[str]:
    target_lines = (
        [
            "I (3) mol-keyboard: Bluetooth HID host active: BLE + Classic",
            "I (3) mol-keyboard: A2DP Source capability=available mode=inactive codec=SBC sample_rate=32000 Hz",
            "I (10) mol-keyboard: a2dp_found=0 a2dp_attempt=0 a2dp_connect=0 a2dp_disconnect=0 a2dp_conn_fail=0 a2dp_codec_reject=0 a2dp_start=0 a2dp_ctrl_fail=0 a2dp_pcm_bytes=0 a2dp_pcm_drop=0 a2dp_callbacks=0 a2dp_underrun=0 a2dp_silence_bytes=0 a2dp_buffer=4096/0 avrc_connect=0 avrc_caps=0 avrc_events=0 a2dp_auth_fail=0 a2dp_sink_delay_100us=0 a2dp_stack_min=1000",
        ]
        if target == "esp32"
        else [
            "I (3) mol-keyboard: Bluetooth HID host active: BLE (Classic unsupported by this SoC)",
            "I (3) mol-keyboard: A2DP Source capability=unsupported (Classic Bluetooth absent on this SoC)",
            "I (3) mol-keyboard: USB HID host active: internal PHY D-=GPIO19 D+=GPIO20; external 5 V VBUS supply required",
            "I (10) mol-keyboard: usb_ifaces=0 usb_open=0 usb_reject=0 usb_disconnect=0 usb_report=0 usb_invalid=0 usb_transfer_err=0 usb_delivery_fail=0 usb_driver_fail=0 usb_queue_ovf=0 usb_host_stack_min=1000 usb_hid_stack_min=1000",
        ]
    )
    common = [
        "I (1) mol-keyboard: Reset reason=3",
        "I (1) mol-keyboard: Shared Mol Sequence passed: events=12 final=96000",
        "I (1) mol-keyboard: Tiny engine memory: required=37664 static=37888 bytes",
        "I (2) mol-keyboard: Tiny core C4 passed: frequency=261.2500 Hz peak=0.100000",
        "I (3) mol-keyboard: I2S active: 32000 Hz, BCLK=26 WS=25 DOUT=22, DMA=6 x 128, audio priority=20 core=1",
        "I (3) mol-keyboard: GPIO matrix active: 5x6, debounce=10 ms, ghost=suppress-ambiguous",
        "I (3) mol-keyboard: Device control active: priority=5 core=0; audio never waits on storage",
    ]
    audio = "I (10) mol-keyboard: audio frames={frames} render_fail=0 write_fail=0 partial=0 dma_q_ovf=0 deadline_miss=0 max_render_us=900 wdt_fail=0 commands=4 input_queued=4 input_drop=0 input_reject=0 input_high=2 gpio_scans=10000 gpio_events=2 gpio_ghost=0 gpio_fail=0 nvs_load=1 nvs_save=0 nvs_missing=1 nvs_corrupt=0 nvs_io_fail=0 seq_load=0 seq_save=0 seq_corrupt=0 seq_io_fail=0 bt_ble_scan=1 bt_classic_scan=0 bt_open=0 bt_connect=0 bt_disconnect=0 bt_report=0 bt_invalid=0 bt_fail=0 bt_stack_min=1000 audio_stack_min=1000 gpio_stack_min=1000 internal_heap_min=20000"
    control = "I (10) mol-keyboard: control_config=0 control_apply=0 control_reject=0 control_q_reject=0 control_save=0 control_io_fail=0 control_peer=0 control_unpair=0 control_factory=0 control_bond_remove=0 control_stack_min=1000"
    return common + target_lines + [audio.format(frames=320000), control, audio.format(frames=640000), control]


class ParserTests(unittest.TestCase):
    def test_valid_esp32_and_s3_logs(self) -> None:
        for target in ("esp32", "esp32s3"):
            state = HilState(target)
            for line in passing_log(target):
                state.observe(line)
            self.assertEqual([], validate_state(state, Requirements(20.0)))

    def test_counter_and_reset_failures_are_reported(self) -> None:
        state = HilState("esp32")
        for line in passing_log("esp32"):
            state.observe(line)
        state.observe("I (20) mol-keyboard: Reset reason=7")
        state.audio[-1]["deadline_miss"] = 1
        errors = validate_state(state, Requirements(20.0))
        self.assertTrue(any("boots" in error for error in errors))
        self.assertTrue(any("deadline_miss" in error for error in errors))

    def test_capability_mismatch_fails(self) -> None:
        state = HilState("esp32s3")
        for line in passing_log("esp32"):
            state.observe(line)
        self.assertTrue(any("capability" in error for error in validate_state(state, Requirements(20.0))))

    def test_complete_virtual_hil_sessions_pass(self) -> None:
        for target in ("esp32", "esp32s3"):
            state = simulated_state(target, 1800.0, None)
            self.assertEqual([], validate_state(state, simulation_requirements(target, 1800.0)))
            self.assertEqual(180, len(state.audio))

    def test_virtual_hil_fault_injection_fails_closed(self) -> None:
        expected = {
            "reset": "boots",
            "deadline-miss": "deadline_miss",
            "stalled-audio": "audio advanced",
            "firmware-error": "firmware error",
        }
        for fault, message in expected.items():
            with self.subTest(fault=fault):
                state = simulated_state("esp32", 1800.0, fault)
                errors = validate_state(state, simulation_requirements("esp32", 1800.0))
                self.assertTrue(any(message in error for error in errors), errors)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--simulate", action="store_true")
    parser.add_argument("--target", choices=("esp32", "esp32s3"))
    parser.add_argument("--port")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--duration-seconds", type=float, default=1800.0)
    parser.add_argument("--report")
    parser.add_argument("--log")
    parser.add_argument("--i2s-capture")
    parser.add_argument("--serial-only", action="store_true")
    parser.add_argument("--no-reset", action="store_true")
    parser.add_argument("--require-gpio", action="store_true")
    parser.add_argument("--require-bluetooth", action="store_true")
    parser.add_argument("--require-usb", action="store_true")
    parser.add_argument("--require-a2dp", action="store_true")
    parser.add_argument("--require-web", action="store_true")
    parser.add_argument("--require-clear-pairing", action="store_true")
    parser.add_argument(
        "--inject-fault",
        choices=("reset", "deadline-miss", "stalled-audio", "firmware-error"),
    )
    args = parser.parse_args()
    if args.self_test and args.simulate:
        parser.error("--self-test and --simulate are mutually exclusive")
    if args.simulate and not args.target:
        parser.error("--target is required for a simulated HIL run")
    if not args.self_test and not args.simulate and (not args.target or not args.port):
        parser.error("--target and --port are required for a live HIL run")
    if args.inject_fault and not args.simulate:
        parser.error("--inject-fault requires --simulate")
    if args.duration_seconds <= 0:
        parser.error("--duration-seconds must be positive")
    return args


if __name__ == "__main__":
    arguments = parse_arguments()
    if arguments.self_test:
        unittest.main(argv=[sys.argv[0]])
    if arguments.simulate:
        raise SystemExit(run_simulation(arguments))
    raise SystemExit(run(arguments))
