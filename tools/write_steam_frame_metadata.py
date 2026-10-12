#!/usr/bin/env python3
"""Write deterministic source, toolchain, dependency, and package hashes."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import subprocess
import sys


SYSROOT_SHA256 = "8e162d235aeb1e6d283ab028e2c7b933061abc1d59830829c9e311cc73b3dd20"
SDL3_SHA256 = "30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb"
OPENXR_COMMIT = "f2448a8797c85814aa892efc1ab8707900fbcc78"
VULKAN_HEADERS_COMMIT = "e5323cdea4ed92dfe825397f6047b8604a40423c"
RETRO_CPU_COMMIT = "ea9049ab25084334f7cc1907b3a98bf1c2604a03"
SNES_SPC_COMMIT = "ec8ee2bbe30451614c1d02a83f7af1c97d497d45"
DR_LIBS_COMMIT = "b55a0d9a30b91ad8901f89ecf05f76a33186c185"
XBRZ_COMMIT = "93c54433fa0df37c689c919e8152fb0b9136584a"
SYSROOT_SNAPSHOT = "3.0.20260415.224995"
SYSROOT_LINK_TRANSFORMATION = "absolute-sysroot-links-to-relative-direct-targets-v1"
SYSROOT_LINK_COUNT = 38


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def command_version(command: list[str]) -> str:
    result = subprocess.run(command, text=True, capture_output=True, check=True)
    lines = (result.stdout or result.stderr).splitlines()
    if not lines:
        raise RuntimeError(f"version command returned no output: {command[0]}")
    return lines[0].strip()


def source_state(source_root: pathlib.Path) -> dict[str, object]:
    commit = subprocess.run(
        ["git", "-C", str(source_root), "rev-parse", "HEAD"],
        text=True,
        capture_output=True,
        check=True,
    ).stdout.strip()
    diff = subprocess.run(
        ["git", "-C", str(source_root), "diff", "--binary", "HEAD"],
        capture_output=True,
        check=True,
    ).stdout
    untracked_names = subprocess.run(
        ["git", "-C", str(source_root), "ls-files", "--others", "--exclude-standard", "-z"],
        capture_output=True,
        check=True,
    ).stdout.split(b"\0")
    untracked: dict[str, str] = {}
    for raw_name in filter(None, untracked_names):
        name = raw_name.decode("utf-8", errors="surrogateescape")
        path = source_root / name
        if path.is_file():
            untracked[name] = sha256(path)
    return {
        "commit": commit,
        "tracked_diff_sha256": hashlib.sha256(diff).hexdigest(),
        "untracked_files": untracked,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=pathlib.Path, required=True)
    parser.add_argument("--package-root", type=pathlib.Path, required=True)
    parser.add_argument("--sysroot-archive", type=pathlib.Path, required=True)
    parser.add_argument(
        "--sysroot-normalization-report", type=pathlib.Path, required=True
    )
    parser.add_argument(
        "--artifact-set",
        choices=("runtime-package", "hardware-diagnostics"),
        default="runtime-package",
    )
    parser.add_argument("--clang", default="clang")
    parser.add_argument("--clangxx", default="clang++")
    parser.add_argument("--lld", default="ld.lld")
    parser.add_argument("--cmake", default="cmake")
    parser.add_argument("--ninja", default="ninja")
    parser.add_argument("--python", default="python3")
    args = parser.parse_args()

    source_root = args.source_root.resolve()
    package_root = args.package_root.resolve()
    sysroot_archive = args.sysroot_archive.resolve()
    normalization_report_path = args.sysroot_normalization_report.resolve()
    if not package_root.is_dir():
        raise SystemExit(f"package directory does not exist: {package_root}")
    if sha256(sysroot_archive) != SYSROOT_SHA256:
        raise SystemExit("Sniper ARM64 sysroot archive checksum mismatch")
    normalization_report = json.loads(
        normalization_report_path.read_text(encoding="utf-8")
    )
    if not isinstance(normalization_report, dict):
        raise SystemExit("Sniper ARM64 sysroot link-normalization report is invalid")
    normalization_entries = normalization_report.get("entries")
    normalization_scope = normalization_report.get("scope")
    if (normalization_report.get("format_version") != 1
            or normalization_report.get("transformation") != SYSROOT_LINK_TRANSFORMATION
            or normalization_report.get("normalized_absolute_symlinks") != SYSROOT_LINK_COUNT
            or not isinstance(normalization_entries, list)
            or len(normalization_entries) != SYSROOT_LINK_COUNT
            or not isinstance(normalization_scope, list)
            or not normalization_scope
            or not all(isinstance(scope, str) for scope in normalization_scope)):
        raise SystemExit("Sniper ARM64 sysroot link-normalization report is invalid")

    artifacts = {
        path.relative_to(package_root).as_posix(): sha256(path)
        for path in sorted(package_root.rglob("*"))
        if path.is_file() and path.name != "BUILD-METADATA.json"
    }
    metadata = {
        "format_version": 1,
        "artifact_set": args.artifact_set,
        "source": source_state(source_root),
        "toolchain": {
            "clang": command_version([args.clang, "--version"]),
            "clangxx": command_version([args.clangxx, "--version"]),
            "lld": command_version([args.lld, "--version"]),
            "cmake": command_version([args.cmake, "--version"]),
            "ninja": command_version([args.ninja, "--version"]),
            "python": command_version([args.python, "--version"]),
        },
        "sysroot": {
            "name": "Steam Runtime Sniper ARM64",
            "snapshot": SYSROOT_SNAPSHOT,
            "archive_sha256": SYSROOT_SHA256,
            "link_normalization": {
                "transformation": SYSROOT_LINK_TRANSFORMATION,
                "absolute_symlinks": SYSROOT_LINK_COUNT,
                "scope": normalization_scope,
                "report_sha256": sha256(normalization_report_path),
            },
        },
        "dependencies": {
            "SDL3": {"version": "3.4.14", "archive_sha256": SDL3_SHA256},
            "OpenXR-SDK": {"commit": OPENXR_COMMIT},
            "Vulkan-Headers": {"commit": VULKAN_HEADERS_COMMIT},
            "retro_cpu": {"commit": RETRO_CPU_COMMIT},
            "snes_spc": {"commit": SNES_SPC_COMMIT},
            "dr_libs": {"commit": DR_LIBS_COMMIT},
            "xBRZ": {"commit": XBRZ_COMMIT},
        },
        "artifacts": artifacts,
    }
    output = package_root / "BUILD-METADATA.json"
    output.write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(output)


if __name__ == "__main__":
    try:
        main()
    except (OSError, subprocess.CalledProcessError, RuntimeError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1) from error
