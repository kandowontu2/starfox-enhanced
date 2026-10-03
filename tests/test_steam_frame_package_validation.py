#!/usr/bin/env python3
"""Exercise the public artifact allowlists and checksum validation."""

from __future__ import annotations

import hashlib
import json
import pathlib
import sys
import tempfile


SOURCE_ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SOURCE_ROOT / "tests"))
from test_steam_frame_package import (  # noqa: E402
    EXPECTED_VR_PREFERENCES,
    validate,
)


SYSROOT_SHA256 = "8e162d235aeb1e6d283ab028e2c7b933061abc1d59830829c9e311cc73b3dd20"
SYSROOT_LINK_TRANSFORMATION = "absolute-sysroot-links-to-relative-direct-targets-v1"


def normalization_report() -> dict[str, object]:
    entries = [
        {
            "path": f"usr/lib/aarch64-linux-gnu/libfixture-{index}.so",
            "original_target": f"/lib/aarch64-linux-gnu/libfixture-{index}.so",
            "relative_target": f"../../../lib/aarch64-linux-gnu/libfixture-{index}.so",
            "resolved_target": f"lib/aarch64-linux-gnu/libfixture-{index}.so",
        }
        for index in range(38)
    ]
    return {
        "format_version": 1,
        "transformation": SYSROOT_LINK_TRANSFORMATION,
        "scope": [
            "lib/aarch64-linux-gnu", "usr/lib/aarch64-linux-gnu", "lib64",
            "usr/lib64", "lib/gcc/aarch64-linux-gnu",
            "usr/lib/gcc/aarch64-linux-gnu",
            "usr/lib/gcc-cross/aarch64-linux-gnu",
        ],
        "normalized_absolute_symlinks": len(entries),
        "entries": entries,
    }


def normalization_report_bytes() -> bytes:
    return (json.dumps(normalization_report(), indent=2, sort_keys=True) + "\n").encode()


def sha256(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_metadata(root: pathlib.Path, artifact_set: str) -> None:
    payloads = {
        path.relative_to(root).as_posix(): sha256(path)
        for path in sorted(root.rglob("*"))
        if path.is_file() and path.name != "BUILD-METADATA.json"
    }
    metadata = {
        "format_version": 1,
        "artifact_set": artifact_set,
        "source": {"commit": "fixture", "tracked_diff_sha256": "fixture"},
        "toolchain": {"clang": "fixture"},
        "sysroot": {
            "archive_sha256": SYSROOT_SHA256,
            "link_normalization": {
                "transformation": SYSROOT_LINK_TRANSFORMATION,
                "absolute_symlinks": 38,
                "scope": normalization_report()["scope"],
                "report_sha256": hashlib.sha256(normalization_report_bytes()).hexdigest(),
            },
        },
        "dependencies": {"SDL3": {"version": "3.4.14"}},
        "artifacts": payloads,
    }
    (root / "BUILD-METADATA.json").write_text(json.dumps(metadata), encoding="utf-8")


def test_runtime_package(root: pathlib.Path) -> None:
    files = (
        "starfox_steamframe", "LAUNCH-STEAM-FRAME.sh", "STEAM-FRAME-START-HERE.txt",
        "vrpreferences.json",
        "THIRD_PARTY_NOTICES.md", "CREDITS.md", "LICENSE-XBRZ.txt",
        "licenses/fonts/README.md", "licenses/fonts/misaki.txt",
    )
    for relative in files:
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("fixture", encoding="utf-8")
    for executable in ("starfox_steamframe", "LAUNCH-STEAM-FRAME.sh"):
        (root / executable).chmod(0o755)
    (root / "vrpreferences.json").write_text(
        json.dumps(EXPECTED_VR_PREFERENCES), encoding="utf-8"
    )
    write_metadata(root, "runtime-package")
    validate(root, "runtime-package")

    (root / "vrpreferences.json").write_text(
        json.dumps({"steam_frame": {"preferMinRefreshRate": 120}}), encoding="utf-8"
    )
    write_metadata(root, "runtime-package")
    try:
        validate(root, "runtime-package")
    except AssertionError as error:
        if "reviewed defaults" not in str(error):
            raise
    else:
        raise AssertionError("runtime package accepted incorrect Steam Frame preferences")
    (root / "vrpreferences.json").write_text(
        json.dumps(EXPECTED_VR_PREFERENCES), encoding="utf-8"
    )

    (root / "vr-preferences.bin").write_bytes(b"private user data")
    write_metadata(root, "runtime-package")
    try:
        validate(root, "runtime-package")
    except AssertionError as error:
        if "vr-preferences.bin" not in str(error):
            raise
    else:
        raise AssertionError("runtime package accepted a user-data BIN")


def test_hardware_diagnostics(root: pathlib.Path) -> None:
    root.mkdir(parents=True, exist_ok=True)
    for relative in ("THIRD_PARTY_NOTICES.md", "CREDITS.md", "LICENSE-XBRZ.txt",
                     "licenses/fonts/README.md", "licenses/fonts/misaki.txt"):
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("fixture notice", encoding="utf-8")
    executable_names = ("starfox_pc", "starfox_vr_runtime_check")
    executables = {}
    for name in executable_names:
        path = root / name
        path.write_text(name, encoding="utf-8")
        path.chmod(0o755)
        executables[name] = {
            "sha256": sha256(path),
            "machine": "AArch64",
            "dependencies": {"libc.so.6": "usr/lib/aarch64-linux-gnu/libc.so.6"},
        }
    (root / "STEAM-FRAME-DIAGNOSTICS.txt").write_text("diagnostics", encoding="utf-8")
    (root / "SYSROOT-LINK-NORMALIZATION.json").write_bytes(normalization_report_bytes())
    (root / "ELF-DEPENDENCIES.json").write_text(
        json.dumps({"format_version": 1, "executables": executables}), encoding="utf-8"
    )
    write_metadata(root, "hardware-diagnostics")
    validate(root, "hardware-diagnostics")

    (root / "Starfox-Assets.BIN").write_bytes(b"generated game data")
    write_metadata(root, "hardware-diagnostics")
    try:
        validate(root, "hardware-diagnostics")
    except AssertionError as error:
        if "Starfox-Assets.BIN" not in str(error):
            raise
    else:
        raise AssertionError("diagnostics artifact accepted a generated asset bundle")


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="starfox-frame-package-test-") as temp:
        test_runtime_package(pathlib.Path(temp) / "runtime")
        test_hardware_diagnostics(pathlib.Path(temp) / "diagnostics")
    print("Steam Frame runtime and diagnostic package allowlists passed.")


if __name__ == "__main__":
    main()
