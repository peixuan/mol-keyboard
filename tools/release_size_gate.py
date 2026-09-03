# SPDX-License-Identifier: Apache-2.0
"""Enforce the release binary-size budgets from CODEX_GOAL.md."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path


MIB = 1024 * 1024
BUDGETS = {
    "stripped_core": 1 * MIB,
    "daemon_and_cli": 5 * MIB,
    "compressed_wasm": int(1.5 * MIB),
    "web_core_resources": 2 * MIB,
}


def regular_file(path: Path, label: str) -> Path:
    resolved = path.resolve()
    if not resolved.is_file():
        raise ValueError(f"{label} is not a regular file: {resolved}")
    return resolved


def directory(path: Path, label: str) -> Path:
    resolved = path.resolve()
    if not resolved.is_dir():
        raise ValueError(f"{label} is not a directory: {resolved}")
    return resolved


def find_strip_tool(explicit: str | None) -> str:
    if explicit:
        candidate = Path(explicit)
        if candidate.is_file():
            return str(candidate.resolve())
        discovered = shutil.which(explicit)
        if discovered:
            return discovered
        raise ValueError(f"strip tool was not found: {explicit}")

    names = [
        "llvm-strip",
        *(f"llvm-strip-{version}" for version in range(22, 13, -1)),
        "strip",
    ]
    for name in names:
        discovered = shutil.which(name)
        if discovered:
            return discovered
    raise ValueError("no llvm-strip or strip executable was found")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def measure_web_resources(web_dir: Path) -> tuple[int, list[dict[str, object]]]:
    files: list[dict[str, object]] = []
    total = 0
    for path in sorted(web_dir.rglob("*")):
        if not path.is_file() or path.suffix == ".map":
            continue
        size = path.stat().st_size
        total += size
        files.append({"path": path.relative_to(web_dir).as_posix(), "bytes": size})
    if not files:
        raise ValueError(f"web resource directory contains no deployable files: {web_dir}")
    return total, files


def status(size: int, limit: int) -> str:
    return "pass" if size < limit else "fail"


def write_markdown(report: dict[str, object], destination: Path) -> None:
    measurements = report["measurements"]
    assert isinstance(measurements, dict)
    lines = [
        "# Release size gate",
        "",
        "All limits are exclusive and use binary mebibytes (1 MiB = 1,048,576 bytes).",
        "",
        "| Measurement | Actual bytes | Limit bytes | Result |",
        "| --- | ---: | ---: | --- |",
    ]
    for name in ("stripped_core", "daemon_and_cli", "compressed_wasm", "web_core_resources"):
        value = measurements[name]
        assert isinstance(value, dict)
        lines.append(
            f"| `{name}` | {value['bytes']} | < {value['limit_bytes']} | {value['status'].upper()} |"
        )
    lines.extend(
        [
            "",
            f"Strip tool: `{report['strip_tool']}`",
            "",
            "Source maps are developer diagnostics and are excluded from Web initial core resources.",
            "",
        ]
    )
    destination.write_text("\n".join(lines), encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-core", required=True, type=Path)
    parser.add_argument("--daemon", required=True, type=Path)
    parser.add_argument("--cli", required=True, type=Path)
    parser.add_argument("--wasm", required=True, type=Path)
    parser.add_argument("--web-dir", required=True, type=Path)
    parser.add_argument("--report-dir", required=True, type=Path)
    parser.add_argument("--strip-tool")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        native_core = regular_file(args.native_core, "native core")
        daemon = regular_file(args.daemon, "daemon")
        cli = regular_file(args.cli, "CLI")
        wasm = regular_file(args.wasm, "Wasm module")
        web_dir = directory(args.web_dir, "web resource directory")
        strip_tool = find_strip_tool(args.strip_tool)
    except ValueError as error:
        print(f"size gate configuration error: {error}", file=sys.stderr)
        return 2

    report_dir = args.report_dir.resolve()
    report_dir.mkdir(parents=True, exist_ok=True)
    stripped_core = report_dir / f"mol_core.stripped{native_core.suffix}"
    shutil.copy2(native_core, stripped_core)
    try:
        subprocess.run([strip_tool, "--strip-debug", str(stripped_core)], check=True)
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"size gate could not strip the native core: {error}", file=sys.stderr)
        return 2

    compressed_wasm = gzip.compress(wasm.read_bytes(), compresslevel=9, mtime=0)
    compressed_wasm_path = report_dir / f"{wasm.name}.gz"
    compressed_wasm_path.write_bytes(compressed_wasm)
    try:
        web_bytes, web_files = measure_web_resources(web_dir)
    except ValueError as error:
        print(f"size gate configuration error: {error}", file=sys.stderr)
        return 2

    sizes = {
        "stripped_core": stripped_core.stat().st_size,
        "daemon_and_cli": daemon.stat().st_size + cli.stat().st_size,
        "compressed_wasm": len(compressed_wasm),
        "web_core_resources": web_bytes,
    }
    measurements = {
        name: {"bytes": size, "limit_bytes": BUDGETS[name], "status": status(size, BUDGETS[name])}
        for name, size in sizes.items()
    }
    report: dict[str, object] = {
        "schema_version": 1,
        "strip_tool": strip_tool,
        "inputs": {
            "native_core": str(native_core),
            "daemon": str(daemon),
            "cli": str(cli),
            "wasm": str(wasm),
            "web_dir": str(web_dir),
        },
        "artifacts": {
            "stripped_core": {"path": str(stripped_core), "sha256": sha256(stripped_core)},
            "compressed_wasm": {
                "path": str(compressed_wasm_path),
                "sha256": sha256(compressed_wasm_path),
            },
        },
        "measurements": measurements,
        "web_files": web_files,
    }
    json_path = report_dir / "release-size-report.json"
    markdown_path = report_dir / "release-size-report.md"
    json_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    write_markdown(report, markdown_path)

    failed = [name for name, value in measurements.items() if value["status"] != "pass"]
    for name, value in measurements.items():
        print(f"{name}: {value['bytes']} bytes (limit < {value['limit_bytes']}) {value['status'].upper()}")
    print(f"reports: {json_path} and {markdown_path}")
    if failed:
        print(f"size gate failed: {', '.join(failed)}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
