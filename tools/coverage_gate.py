# SPDX-License-Identifier: Apache-2.0
"""Run CTest under LLVM coverage and enforce MoL core line thresholds."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
from typing import Iterable


CORE_PREFIXES = (
    "src/c_api/",
    "src/control/mol_wire.c",
    "src/dsp/",
    "src/effects/",
    "src/music/",
    "src/patch/",
    "src/sequence/",
    "src/transport/",
)
CRITICAL_GROUPS = {
    "queue-memory": ("src/c_api/mol_engine.c",),
    "music-state": ("src/music/", "src/transport/"),
    "patch": ("src/patch/mol_patch.c",),
    "sequence": ("src/sequence/mol_sequence.c",),
}


def run(command: list[str], *, environment: dict[str, str] | None = None) -> None:
    print("+", shlex.join(command), flush=True)
    subprocess.run(command, check=True, env=environment)


def inside(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def instrumented_test_objects(build_dir: Path) -> list[Path]:
    result = subprocess.run(
        ["ctest", "--test-dir", str(build_dir), "--show-only=json-v1"],
        check=True,
        capture_output=True,
        text=True,
    )
    description = json.loads(result.stdout)
    candidates: set[Path] = set()
    for test in description.get("tests", []):
        for token in test.get("command", []):
            value = token.split("=", 1)[-1] if "=" in token else token
            candidate = Path(value)
            if not candidate.is_absolute():
                continue
            candidate = candidate.resolve()
            if candidate.is_file() and inside(candidate, build_dir):
                candidates.add(candidate)
    objects = sorted(
        candidate
        for candidate in candidates
        if candidate.suffix.lower() == ".exe"
        or (os.name != "nt" and os.access(candidate, os.X_OK))
    )
    if not objects:
        raise RuntimeError("CTest did not expose any instrumented build executables")
    return objects


def aggregate(files: Iterable[dict[str, object]]) -> tuple[int, int]:
    count = 0
    covered = 0
    for record in files:
        lines = record["summary"]["lines"]  # type: ignore[index]
        count += int(lines["count"])  # type: ignore[index]
        covered += int(lines["covered"])  # type: ignore[index]
    return covered, count


def percentage(covered: int, count: int) -> float:
    return 100.0 * covered / count if count else 0.0


def matches(relative: str, selectors: tuple[str, ...]) -> bool:
    return any(relative == selector or relative.startswith(selector) for selector in selectors)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--source-dir", default=Path.cwd(), type=Path)
    parser.add_argument("--overall-threshold", default=90.0, type=float)
    parser.add_argument("--critical-threshold", default=95.0, type=float)
    parser.add_argument("--llvm-profdata", default="llvm-profdata")
    parser.add_argument("--llvm-cov", default="llvm-cov")
    args = parser.parse_args()

    build_dir = args.build_dir.resolve()
    source_dir = args.source_dir.resolve()
    if not build_dir.is_dir() or not inside(build_dir, source_dir):
        parser.error("--build-dir must be an existing directory inside --source-dir")
    for tool in (args.llvm_profdata, args.llvm_cov):
        if shutil.which(tool) is None:
            parser.error(f"required LLVM tool is not on PATH: {tool}")

    coverage_dir = build_dir / "coverage"
    coverage_dir.mkdir(exist_ok=True)
    for stale in coverage_dir.glob("*.profraw"):
        stale.unlink()
    for stale_name in ("merged.profdata", "coverage.json", "summary.json", "summary.md"):
        stale = coverage_dir / stale_name
        if stale.exists():
            stale.unlink()

    environment = os.environ.copy()
    environment["LLVM_PROFILE_FILE"] = str(coverage_dir / "%m-%p.profraw")
    run(
        ["ctest", "--test-dir", str(build_dir), "--output-on-failure"],
        environment=environment,
    )

    profiles = sorted(coverage_dir.glob("*.profraw"))
    if not profiles:
        raise RuntimeError("CTest produced no LLVM raw profiles")
    merged = coverage_dir / "merged.profdata"
    run(
        [
            args.llvm_profdata,
            "merge",
            "-sparse",
            *[str(profile) for profile in profiles],
            "-o",
            str(merged),
        ]
    )

    objects = instrumented_test_objects(build_dir)
    command = [
        args.llvm_cov,
        "export",
        f"-instr-profile={merged}",
        str(objects[0]),
    ]
    for candidate in objects[1:]:
        command.extend(("-object", str(candidate)))
    exported = subprocess.run(command, check=True, capture_output=True, text=True)
    raw_report = coverage_dir / "coverage.json"
    raw_report.write_text(exported.stdout, encoding="utf-8", newline="\n")
    report = json.loads(exported.stdout)

    records: list[dict[str, object]] = []
    for record in report["data"][0]["files"]:
        path = Path(record["filename"]).resolve()
        if not inside(path, source_dir):
            continue
        relative = path.relative_to(source_dir).as_posix()
        if matches(relative, CORE_PREFIXES) and "/generated/" not in relative:
            copied = dict(record)
            copied["relative"] = relative
            records.append(copied)
    if not records:
        raise RuntimeError("LLVM report contained no first-party mol_core source files")

    overall_covered, overall_count = aggregate(records)
    groups: dict[str, dict[str, object]] = {}
    failed = percentage(overall_covered, overall_count) + 1e-9 < args.overall_threshold
    for name, selectors in CRITICAL_GROUPS.items():
        selected = [record for record in records if matches(str(record["relative"]), selectors)]
        covered, count = aggregate(selected)
        if count == 0:
            raise RuntimeError(f"critical coverage group has no regions: {name}")
        measured = percentage(covered, count)
        groups[name] = {
            "covered": covered,
            "count": count,
            "percent": round(measured, 2),
            "threshold": args.critical_threshold,
        }
        failed = failed or measured + 1e-9 < args.critical_threshold

    summary = {
        "overall": {
            "covered": overall_covered,
            "count": overall_count,
            "percent": round(percentage(overall_covered, overall_count), 2),
            "threshold": args.overall_threshold,
        },
        "critical_groups": groups,
        "profiles": len(profiles),
        "objects": len(objects),
    }
    (coverage_dir / "summary.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8", newline="\n"
    )
    rows = [
        "# Core Coverage Gate",
        "",
        "| Scope | Covered / Count | Line coverage | Required |",
        "|---|---:|---:|---:|",
        (
            f"| mol_core overall | {overall_covered} / {overall_count} | "
            f"{summary['overall']['percent']:.2f}% | {args.overall_threshold:.2f}% |"
        ),
    ]
    for name, values in groups.items():
        rows.append(
            f"| {name} | {values['covered']} / {values['count']} | "
            f"{values['percent']:.2f}% | {args.critical_threshold:.2f}% |"
        )
    rows.extend(("", f"Result: **{'FAIL' if failed else 'PASS'}**", ""))
    (coverage_dir / "summary.md").write_text(
        "\n".join(rows), encoding="utf-8", newline="\n"
    )
    print("\n".join(rows), flush=True)
    return 1 if failed else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError, json.JSONDecodeError) as error:
        print(f"coverage gate failed: {error}", file=sys.stderr)
        sys.exit(2)
