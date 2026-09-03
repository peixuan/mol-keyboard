# SPDX-License-Identifier: Apache-2.0
"""Run clang-tidy over first-party production translation units."""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import shutil
import subprocess
import sys


PRODUCTION_ROOTS = {"apps", "platforms", "src"}
EXCLUDED_PARTS = {"generated", "source_check", "tests", "third_party"}


def inside(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def source_files(database: Path, source_dir: Path) -> list[Path]:
    records = json.loads(database.read_text(encoding="utf-8"))
    files: set[Path] = set()
    for record in records:
        path = Path(record["file"])
        if not path.is_absolute():
            path = Path(record["directory"]) / path
        path = path.resolve()
        if not path.is_file() or not inside(path, source_dir):
            continue
        relative = path.relative_to(source_dir)
        if relative.parts[0] not in PRODUCTION_ROOTS:
            continue
        if any(part in EXCLUDED_PARTS for part in relative.parts):
            continue
        if path.name.startswith("test_"):
            continue
        files.add(path)
    if not files:
        raise RuntimeError("compile database contained no first-party production sources")
    return sorted(files, key=lambda item: item.as_posix().casefold())


def analyze(tool: str, build_dir: Path, source: Path) -> tuple[Path, subprocess.CompletedProcess[str]]:
    result = subprocess.run(
        [tool, "--quiet", f"-p={build_dir}", str(source)],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    return source, result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--source-dir", default=Path.cwd(), type=Path)
    parser.add_argument("--clang-tidy", default="clang-tidy")
    parser.add_argument("--jobs", default=4, type=int)
    args = parser.parse_args()

    source_dir = args.source_dir.resolve()
    build_dir = args.build_dir.resolve()
    database = build_dir / "compile_commands.json"
    if not build_dir.is_dir() or not inside(build_dir, source_dir):
        parser.error("--build-dir must be an existing directory inside --source-dir")
    if not database.is_file():
        parser.error("--build-dir does not contain compile_commands.json")
    if args.jobs < 1 or args.jobs > 64:
        parser.error("--jobs must be in the range 1..64")
    if shutil.which(args.clang_tidy) is None and not Path(args.clang_tidy).is_file():
        parser.error(f"clang-tidy executable not found: {args.clang_tidy}")

    files = source_files(database, source_dir)
    with ThreadPoolExecutor(max_workers=args.jobs) as executor:
        results = list(executor.map(lambda path: analyze(args.clang_tidy, build_dir, path), files))

    failures = 0
    for source, result in results:
        if result.returncode == 0:
            continue
        failures += 1
        print(f"\n--- {source.relative_to(source_dir).as_posix()} ---", file=sys.stderr)
        if result.stdout:
            print(result.stdout.rstrip(), file=sys.stderr)
        if result.stderr:
            print(result.stderr.rstrip(), file=sys.stderr)
    if failures:
        print(f"static analysis failed for {failures} of {len(files)} files", file=sys.stderr)
        return 1
    print(f"Static analysis passed for {len(files)} first-party production translation units.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError, json.JSONDecodeError) as error:
        print(f"static analysis failed: {error}", file=sys.stderr)
        sys.exit(2)
