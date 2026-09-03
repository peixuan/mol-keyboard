#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Run the production macOS launchd smoke against a controlled launchd model."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import plistlib
import shutil
import signal
import subprocess
import sys
import tempfile
import time


SERVICE_LABEL_PREFIX = "cn.zhangpeixuan.molkeyboard.daemon.ci."


def _state_dir() -> Path:
    value = os.environ.get("MOL_LAUNCHD_SIM_STATE")
    if not value:
        raise RuntimeError("MOL_LAUNCHD_SIM_STATE is required")
    path = Path(value).resolve()
    path.mkdir(parents=True, exist_ok=True)
    return path


def _write_text_atomic(path: Path, value: str) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(value, encoding="utf-8")
    os.replace(temporary, path)


def _write_json_atomic(path: Path, value: object) -> None:
    _write_text_atomic(path, json.dumps(value, sort_keys=True))


def _load_job(path: Path) -> dict[str, object]:
    with path.open("rb") as source:
        value = plistlib.load(source)
    if not isinstance(value, dict):
        raise RuntimeError(f"plist root is not a dictionary: {path}")
    return value


def _pid_exists(pid: int) -> bool:
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def _terminate_group(pid: int) -> None:
    if not _pid_exists(pid):
        return
    try:
        os.killpg(pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    for _ in range(50):
        if not _pid_exists(pid):
            return
        time.sleep(0.02)
    try:
        os.killpg(pid, signal.SIGKILL)
    except ProcessLookupError:
        pass


def _supervise_launchd_job(state: Path) -> int:
    metadata = json.loads((state / "job.json").read_text(encoding="utf-8"))
    arguments = metadata["program_arguments"]
    stdout_path = Path(metadata["stdout_path"])
    stderr_path = Path(metadata["stderr_path"])
    stdout_path.parent.mkdir(parents=True, exist_ok=True)
    stderr_path.parent.mkdir(parents=True, exist_ok=True)
    with stdout_path.open("ab", buffering=0) as stdout, stderr_path.open(
        "ab", buffering=0
    ) as stderr:
        completed = subprocess.run(arguments, stdout=stdout, stderr=stderr, check=False)
    _write_text_atomic(state / "exit_code", f"{completed.returncode}\n")
    return 0


def _launchctl_bootstrap(arguments: list[str]) -> int:
    if len(arguments) != 2 or not arguments[0].startswith("gui/"):
        raise RuntimeError("launchctl bootstrap requires a GUI domain and plist")
    domain, plist_path_text = arguments
    plist_path = Path(plist_path_text).resolve()
    job = _load_job(plist_path)
    label = job.get("Label")
    program_arguments = job.get("ProgramArguments")
    expected_daemon = os.environ.get("MOL_LAUNCHD_SIM_DAEMON")
    if not isinstance(label, str) or not label.startswith(SERVICE_LABEL_PREFIX):
        raise RuntimeError("launchd simulation rejected an unexpected service label")
    if not isinstance(program_arguments, list) or not all(
        isinstance(value, str) for value in program_arguments
    ):
        raise RuntimeError("launchd simulation requires string ProgramArguments")
    if not program_arguments or not expected_daemon:
        raise RuntimeError("launchd simulation daemon contract is missing")
    if Path(program_arguments[0]).resolve() != Path(expected_daemon).resolve():
        raise RuntimeError("launchd simulation refused to execute an unexpected program")
    if len(program_arguments) != 6 or [
        program_arguments[1],
        program_arguments[2],
        program_arguments[4],
    ] != ["--null-backend", "--state-dir", "--endpoint"]:
        raise RuntimeError("launchd simulation received malformed daemon arguments")
    if job.get("RunAtLoad") is not True or not isinstance(job.get("KeepAlive"), dict):
        raise RuntimeError("launchd simulation requires the production lifecycle policy")
    stdout_path = job.get("StandardOutPath")
    stderr_path = job.get("StandardErrorPath")
    if not isinstance(stdout_path, str) or not isinstance(stderr_path, str):
        raise RuntimeError("launchd simulation requires explicit output paths")

    state = _state_dir()
    for stale in ("exit_code", "bootout.seen"):
        try:
            (state / stale).unlink()
        except FileNotFoundError:
            pass
    metadata: dict[str, object] = {
        "domain": domain,
        "label": label,
        "plist": str(plist_path),
        "program_arguments": program_arguments,
        "stdout_path": stdout_path,
        "stderr_path": stderr_path,
    }
    _write_json_atomic(state / "job.json", metadata)
    supervisor = subprocess.Popen(
        [sys.executable, str(Path(__file__).resolve()), "__launchd_child", str(state)],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        start_new_session=True,
    )
    metadata["supervisor_pid"] = supervisor.pid
    _write_json_atomic(state / "job.json", metadata)
    _write_text_atomic(state / "bootstrap.seen", "1\n")
    return 0


def _launchctl_print(arguments: list[str]) -> int:
    if len(arguments) != 1 or not arguments[0].startswith("gui/"):
        raise RuntimeError("launchctl print requires one GUI target")
    state = _state_dir()
    metadata_path = state / "job.json"
    if not metadata_path.exists():
        print(f"{arguments[0]} = {{ state = running }}")
        return 0
    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    service_target = f"{metadata['domain']}/{metadata['label']}"
    if arguments[0] == metadata["domain"]:
        print(f"{arguments[0]} = {{ state = running }}")
        return 0
    if arguments[0] != service_target:
        return 113
    exit_path = state / "exit_code"
    if exit_path.exists():
        exit_code = int(exit_path.read_text(encoding="utf-8").strip())
        print(f"{service_target} = {{")
        print("\tstate = exited")
        print(f"\tlast exit code = {exit_code}")
        print("}")
    else:
        print(f"{service_target} = {{")
        print("\tstate = running")
        print(f"\tpid = {metadata['supervisor_pid']}")
        print("}")
    return 0


def _launchctl_bootout(arguments: list[str]) -> int:
    if len(arguments) != 2 or not arguments[0].startswith("gui/"):
        raise RuntimeError("launchctl bootout requires a GUI domain and plist")
    state = _state_dir()
    _write_text_atomic(state / "bootout.seen", "1\n")
    metadata_path = state / "job.json"
    if metadata_path.exists() and not (state / "exit_code").exists():
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        _terminate_group(int(metadata["supervisor_pid"]))
    return 0


def _run_launchctl(arguments: list[str]) -> int:
    if not arguments:
        raise RuntimeError("launchctl command is required")
    command, *rest = arguments
    if command == "bootstrap":
        return _launchctl_bootstrap(rest)
    if command == "print":
        return _launchctl_print(rest)
    if command == "bootout":
        return _launchctl_bootout(rest)
    raise RuntimeError(f"unsupported launchctl simulation command: {command}")


def _run_plutil(arguments: list[str]) -> int:
    if len(arguments) < 2 or arguments[0] != "-lint":
        raise RuntimeError("the plutil simulation only supports -lint")
    for value in arguments[1:]:
        path = Path(value)
        _load_job(path)
        print(f"{path}: OK")
    return 0


def _install_command_models(directory: Path) -> None:
    source = Path(__file__).resolve()
    for name in ("launchctl", "plutil", "uname"):
        destination = directory / name
        shutil.copy2(source, destination)
        destination.chmod(0o755)


def _run_harness(arguments: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runner", required=True, type=Path)
    parser.add_argument("--daemon", required=True, type=Path)
    parser.add_argument("--controller", required=True, type=Path)
    options = parser.parse_args(arguments)
    runner = options.runner.resolve()
    daemon = options.daemon.resolve()
    controller = options.controller.resolve()
    bash = shutil.which("bash")
    if bash is None:
        raise RuntimeError("bash is required for the launchd simulation")
    for required in (runner, daemon, controller):
        if not required.exists():
            raise RuntimeError(f"required launchd simulation input is missing: {required}")

    with tempfile.TemporaryDirectory(prefix="mol-macos-launchd-sim-") as root_text:
        root = Path(root_text)
        command_dir = root / "bin"
        state = root / "state"
        command_dir.mkdir()
        state.mkdir()
        _install_command_models(command_dir)
        environment = os.environ.copy()
        environment["PATH"] = f"{command_dir}{os.pathsep}{environment.get('PATH', '')}"
        environment["MOL_LAUNCHD_SIM_STATE"] = str(state)
        environment["MOL_LAUNCHD_SIM_DAEMON"] = str(daemon)
        completed: subprocess.CompletedProcess[str] | None = None
        try:
            completed = subprocess.run(
                [bash, str(runner), str(daemon), str(controller)],
                env=environment,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=45,
                check=False,
            )
            sys.stdout.write(completed.stdout)
            sys.stderr.write(completed.stderr)
            if completed.returncode != 0:
                raise RuntimeError(
                    f"production launchd smoke exited with {completed.returncode}"
                )
            if "MOL_MACOS_LAUNCHD_SMOKE_PASS" not in completed.stdout:
                raise RuntimeError("production launchd smoke did not report success")
            for marker in ("bootstrap.seen", "bootout.seen", "exit_code"):
                if not (state / marker).exists():
                    raise RuntimeError(f"launchd simulation marker is missing: {marker}")
            if (state / "exit_code").read_text(encoding="utf-8").strip() != "0":
                raise RuntimeError("simulated LaunchAgent did not exit successfully")
            metadata = json.loads((state / "job.json").read_text(encoding="utf-8"))
            if _pid_exists(int(metadata["supervisor_pid"])):
                raise RuntimeError("simulated LaunchAgent supervisor is still running")
        finally:
            metadata_path = state / "job.json"
            if metadata_path.exists() and not (state / "exit_code").exists():
                metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
                _terminate_group(int(metadata["supervisor_pid"]))
    print("MOL_MACOS_LAUNCHD_SIMULATION_PASS")
    return 0


def main() -> int:
    if len(sys.argv) >= 2 and sys.argv[1] == "__launchd_child":
        return _supervise_launchd_job(Path(sys.argv[2]).resolve())
    command = Path(sys.argv[0]).name
    if command == "uname":
        print("Darwin")
        return 0
    if command == "plutil":
        return _run_plutil(sys.argv[1:])
    if command == "launchctl":
        return _run_launchctl(sys.argv[1:])
    return _run_harness(sys.argv[1:])


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as error:
        print(f"macOS launchd simulation failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
