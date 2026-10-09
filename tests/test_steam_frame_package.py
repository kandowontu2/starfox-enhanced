#!/usr/bin/env python3
"""Validate a staged Steam Frame package and its recorded payload hashes."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re


REQUIRED_FILES = {
    "starfox_steamframe",
    "LAUNCH-STEAM-FRAME.sh",
    "STEAM-FRAME-START-HERE.txt",
    "vrpreferences.json",
    "BUILD-METADATA.json",
    "THIRD_PARTY_NOTICES.md",
    "CREDITS.md",
    "LICENSE-XBRZ.txt",
    "licenses/fonts/README.md",
    "licenses/fonts/misaki.txt",
}
REQUIRED_DIAGNOSTICS = {
    "starfox_pc",
    "starfox_vr_runtime_check",
    "STEAM-FRAME-DIAGNOSTICS.txt",
    "ELF-DEPENDENCIES.json",
    "SYSROOT-LINK-NORMALIZATION.json",
    "BUILD-METADATA.json",
    "THIRD_PARTY_NOTICES.md",
    "CREDITS.md",
    "LICENSE-XBRZ.txt",
    "licenses/fonts/README.md",
    "licenses/fonts/misaki.txt",
}
ALLOWED_FILES = {
    "runtime-package": REQUIRED_FILES,
    "hardware-diagnostics": REQUIRED_DIAGNOSTICS,
}
ALLOWED_DIRECTORIES = {
    "runtime-package": {"licenses", "licenses/fonts"},
    "hardware-diagnostics": {"licenses", "licenses/fonts"},
}
FORBIDDEN_EXTENSIONS = {
    ".sfc", ".smc", ".srm", ".bin", ".rom", ".sav", ".state",
    ".pak", ".flac", ".pcm", ".wav", ".mp3", ".ogg", ".m4a",
}
FORBIDDEN_NAMES = {
    "starfox-assets.bin",
    "sf.sfc",
    "sfes.sfc",
    "starfox-msu1.pak",
}
EXPECTED_VR_PREFERENCES = {
    "steam_frame": {
        "preferResolution": 2160,
        "preferMinRefreshRate": 90,
        "preferHalfFramerate": False,
        "preferMotionSmoothingMode": "off",
    },
}


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate(package: pathlib.Path, artifact_set: str = "runtime-package") -> None:
    if not package.is_dir():
        raise AssertionError(f"artifact directory does not exist: {package}")
    entries = list(package.rglob("*"))
    symlinks = [path.relative_to(package).as_posix() for path in entries if path.is_symlink()]
    if symlinks:
        raise AssertionError(f"artifact set contains symlinks: {sorted(symlinks)}")
    files = {path.relative_to(package).as_posix(): path
             for path in entries if path.is_file()}
    directories = {path.relative_to(package).as_posix()
                   for path in entries if path.is_dir()}
    unexpected = set(files) - ALLOWED_FILES[artifact_set]
    unexpected_directories = directories - ALLOWED_DIRECTORIES[artifact_set]
    if unexpected or unexpected_directories:
        raise AssertionError(
            "artifact set contains unexpected files or directories: "
            f"files={sorted(unexpected)}, directories={sorted(unexpected_directories)}"
        )
    required = REQUIRED_FILES if artifact_set == "runtime-package" else REQUIRED_DIAGNOSTICS
    missing = required - set(files)
    if missing:
        raise AssertionError(f"artifact set is missing required files: {sorted(missing)}")
    executables = (
        {"starfox_steamframe"} if artifact_set == "runtime-package"
        else {"starfox_pc", "starfox_vr_runtime_check"}
    )
    for executable in executables:
        if not files[executable].stat().st_mode & 0o111:
            raise AssertionError(f"artifact is not executable: {executable}")
    if artifact_set == "runtime-package" and not files["LAUNCH-STEAM-FRAME.sh"].stat().st_mode & 0o111:
        raise AssertionError("Steam Frame launcher is not executable")
    if artifact_set == "runtime-package":
        try:
            preferences = json.loads(files["vrpreferences.json"].read_text(encoding="utf-8"))
        except (UnicodeError, json.JSONDecodeError) as error:
            raise AssertionError("Steam Frame VR preferences are not valid JSON") from error
        if preferences != EXPECTED_VR_PREFERENCES:
            raise AssertionError("Steam Frame VR preferences do not match the reviewed defaults")

    forbidden = []
    for name, path in files.items():
        lower_name = path.name.lower()
        lower_parts = {part.lower() for part in pathlib.PurePosixPath(name).parts}
        if (path.suffix.lower() in FORBIDDEN_EXTENSIONS
                or lower_name in FORBIDDEN_NAMES
                or "vulkan" in lower_name
                or {"vulkan", "icd.d"}.intersection(lower_parts)):
            forbidden.append(name)
        if re.match(r"^libvulkan\.so(?:\.|$)", lower_name):
            forbidden.append(name)
    if forbidden:
        raise AssertionError(f"package contains private/runtime files: {sorted(forbidden)}")

    metadata_path = files["BUILD-METADATA.json"]
    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    if metadata.get("format_version") != 1:
        raise AssertionError("unsupported Steam Frame artifact metadata version")
    if metadata.get("artifact_set") != artifact_set:
        raise AssertionError("metadata describes a different Steam Frame artifact set")
    expected = metadata.get("artifacts")
    actual = {name: sha256(path) for name, path in files.items()
              if name != "BUILD-METADATA.json"}
    if expected != actual:
        raise AssertionError("artifact payload does not match BUILD-METADATA.json checksums")
    sysroot = metadata.get("sysroot", {})
    if sysroot.get("archive_sha256") != (
        "8e162d235aeb1e6d283ab028e2c7b933061abc1d59830829c9e311cc73b3dd20"
    ):
        raise AssertionError("metadata does not identify the pinned Sniper ARM64 sysroot")
    normalization = sysroot.get("link_normalization", {})
    report_sha256 = normalization.get("report_sha256", "")
    if (normalization.get("transformation")
            != "absolute-sysroot-links-to-relative-direct-targets-v1"
            or normalization.get("absolute_symlinks") != 38
            or not isinstance(normalization.get("scope"), list)
            or not normalization.get("scope")
            or not all(isinstance(scope, str) for scope in normalization["scope"])
            or not isinstance(report_sha256, str)
            or not re.fullmatch(r"[0-9a-f]{64}", report_sha256)):
        raise AssertionError("metadata does not identify the normalized Sniper linker paths")

    if artifact_set == "hardware-diagnostics":
        normalization_report = json.loads(
            files["SYSROOT-LINK-NORMALIZATION.json"].read_text(encoding="utf-8")
        )
        if (normalization_report.get("transformation")
                != normalization.get("transformation")
                or normalization_report.get("normalized_absolute_symlinks")
                != normalization.get("absolute_symlinks")
                or normalization_report.get("scope") != normalization.get("scope")
                or len(normalization_report.get("entries", []))
                != normalization.get("absolute_symlinks")
                or sha256(files["SYSROOT-LINK-NORMALIZATION.json"])
                != normalization.get("report_sha256")):
            raise AssertionError("diagnostics report does not match the sysroot metadata")
        closure = json.loads(files["ELF-DEPENDENCIES.json"].read_text(encoding="utf-8"))
        for executable in executables:
            record = closure.get("executables", {}).get(executable)
            if (not record or record.get("machine") != "AArch64"
                    or record.get("sha256") != sha256(files[executable])
                    or not isinstance(record.get("dependencies"), dict)):
                raise AssertionError(f"invalid ELF dependency report for {executable}")
            if any("vulkan" in name.lower() for name in record["dependencies"]):
                raise AssertionError(f"diagnostic links the system Vulkan runtime: {executable}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("package", type=pathlib.Path)
    parser.add_argument(
        "--artifact-set",
        choices=("runtime-package", "hardware-diagnostics"),
        default="runtime-package",
    )
    args = parser.parse_args()
    validate(args.package.resolve(), args.artifact_set)
    print(f"Steam Frame {args.artifact_set} contents and checksums passed.")


if __name__ == "__main__":
    main()
