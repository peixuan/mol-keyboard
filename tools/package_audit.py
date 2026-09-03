# SPDX-License-Identifier: Apache-2.0
"""Audit and smoke-test a CPack portable distribution."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tarfile
import tempfile
import zipfile
from pathlib import Path, PurePosixPath


ARCHIVE_SUFFIXES = (".tar.gz", ".tgz", ".zip")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", required=True, type=Path)
    parser.add_argument("--report-dir", required=True, type=Path)
    parser.add_argument("--expected-version", required=True)
    return parser.parse_args()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def safe_member(name: str) -> bool:
    normalized = PurePosixPath(name.replace("\\", "/"))
    return not normalized.is_absolute() and ".." not in normalized.parts


def extract_archive(archive: Path, destination: Path) -> None:
    lower_name = archive.name.lower()
    if lower_name.endswith(".zip"):
        with zipfile.ZipFile(archive) as package:
            if any(not safe_member(member.filename) for member in package.infolist()):
                raise ValueError("ZIP contains an unsafe path")
            package.extractall(destination)
        return
    if lower_name.endswith((".tar.gz", ".tgz")):
        with tarfile.open(archive, "r:gz") as package:
            if any(not safe_member(member.name) for member in package.getmembers()):
                raise ValueError("tar archive contains an unsafe path")
            package.extractall(destination, filter="data")
        return
    raise ValueError(f"unsupported package archive: {archive}")


def package_root(extraction: Path) -> Path:
    entries = list(extraction.iterdir())
    if len(entries) != 1 or not entries[0].is_dir():
        raise ValueError("package must contain exactly one top-level directory")
    return entries[0]


def required_paths(root: Path) -> list[str]:
    executable_suffix = ".exe" if os.name == "nt" else ""
    fixed = [
        f"bin/mol-keyboardd{executable_suffix}",
        f"bin/molctl{executable_suffix}",
        f"bin/mol-play{executable_suffix}",
        f"bin/mol-patchc{executable_suffix}",
        f"bin/mol-seq{executable_suffix}",
        f"bin/mol-render{executable_suffix}",
        f"bin/mol-audio-analyze{executable_suffix}",
        "include/mol/engine.h",
        "include/mol/mol.h",
        "lib/cmake/mol_keyboard/mol_keyboardConfig.cmake",
        "lib/cmake/mol_keyboard/mol_keyboardConfigVersion.cmake",
        "lib/cmake/mol_keyboard/mol_keyboardTargets.cmake",
        "share/mol-keyboard/README.md",
        "share/mol-keyboard/CHANGELOG.md",
        "share/mol-keyboard/LICENSE",
        "share/mol-keyboard/NOTICE",
        "share/mol-keyboard/PRIVACY.md",
        "share/mol-keyboard/THIRD_PARTY_NOTICES.md",
        "share/mol-keyboard/sbom/mol-keyboard.spdx.json",
        "share/mol-keyboard/patches/grand-piano.molpatch",
        "share/mol-keyboard/patches/molpatch-v1.schema.json",
        "share/mol-keyboard/examples/sequences/scale-study.molseq",
        "share/mol-keyboard/web/index.html",
        "share/mol-keyboard/web/manifest.webmanifest",
        "share/mol-keyboard/web/generated/mol_audio_worklet_core.js",
        "share/mol-keyboard/web/generated/mol_audio_worklet_core.wasm",
        "share/mol-keyboard/service/mol-keyboardd.service",
    ]
    library_candidates = [root / "lib" / "libmol_core.a", root / "lib" / "mol_core.lib"]
    if not any(path.is_file() for path in library_candidates):
        fixed.append("lib/{libmol_core.a|mol_core.lib}")
    return [path for path in fixed if not (root / path).is_file()]


def run_smoke_test(executable: Path, arguments: list[str], expected: str) -> str:
    completed = subprocess.run(
        [str(executable), *arguments],
        check=False,
        capture_output=True,
        text=True,
        timeout=15,
    )
    output = completed.stdout + completed.stderr
    if completed.returncode != 0 or expected not in output:
        raise ValueError(
            f"smoke test failed for {executable.name}: exit={completed.returncode}, output={output!r}"
        )
    return output.strip()


def main() -> int:
    args = parse_args()
    archive = args.archive.resolve()
    if not archive.is_file() or not archive.name.lower().endswith(ARCHIVE_SUFFIXES):
        print(f"package audit configuration error: invalid archive: {archive}", file=sys.stderr)
        return 2

    try:
        checksum_path = archive.with_name(archive.name + ".sha256")
        if not checksum_path.is_file():
            raise ValueError(f"missing CPack checksum: {checksum_path}")
        archive_hash = sha256(archive)
        recorded_hash = checksum_path.read_text(encoding="utf-8").split()[0].lower()
        if recorded_hash != archive_hash:
            raise ValueError("CPack checksum does not match the archive")
        with tempfile.TemporaryDirectory(prefix="mol-package-audit-") as temporary:
            extraction = Path(temporary)
            extract_archive(archive, extraction)
            root = package_root(extraction)
            missing = required_paths(root)
            if missing:
                raise ValueError("missing required package paths: " + ", ".join(missing))
            targets_file = root / "lib" / "cmake" / "mol_keyboard" / "mol_keyboardTargets.cmake"
            if "add_library(mol::core STATIC IMPORTED)" not in targets_file.read_text(
                encoding="utf-8"
            ):
                raise ValueError("installed CMake package does not export mol::core")
            suffix = ".exe" if os.name == "nt" else ""
            daemon_output = run_smoke_test(
                root / "bin" / f"mol-keyboardd{suffix}",
                ["--version"],
                args.expected_version,
            )
            cli_output = run_smoke_test(
                root / "bin" / f"molctl{suffix}", ["--help"], "Commands:"
            )
            entries = sorted(
                path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_file()
            )
    except (OSError, subprocess.SubprocessError, tarfile.TarError, ValueError, zipfile.BadZipFile) as error:
        print(f"package audit failed: {error}", file=sys.stderr)
        return 1

    report = {
        "schema_version": 1,
        "archive": str(archive),
        "archive_bytes": archive.stat().st_size,
        "archive_sha256": archive_hash,
        "file_count": len(entries),
        "daemon_smoke_test": daemon_output,
        "cli_smoke_test": cli_output,
        "result": "pass",
    }
    report_dir = args.report_dir.resolve()
    report_dir.mkdir(parents=True, exist_ok=True)
    destination = report_dir / "package-audit.json"
    destination.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"package audit passed: {len(entries)} files, {archive.stat().st_size} bytes")
    print(f"archive SHA-256: {report['archive_sha256']}")
    print(f"report: {destination}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
