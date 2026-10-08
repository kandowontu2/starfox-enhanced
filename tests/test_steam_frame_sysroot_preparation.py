#!/usr/bin/env python3
"""Verify Sniper sysroot links normalize without escaping the extracted SDK."""

from __future__ import annotations

import importlib.util
import pathlib
import tempfile


SOURCE_ROOT = pathlib.Path(__file__).resolve().parents[1]
MODULE_PATH = SOURCE_ROOT / "tools/prepare_steam_frame_sysroot.py"
SPEC = importlib.util.spec_from_file_location("steam_frame_sysroot", MODULE_PATH)
assert SPEC and SPEC.loader
sysroot_module = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sysroot_module)


def test_chained_absolute_and_relative_links_normalize_to_same_file(
    temp: pathlib.Path,
) -> None:
    root = temp / "sysroot"
    abi_library = root / "lib/aarch64-linux-gnu"
    linker_library = root / "usr/lib/aarch64-linux-gnu"
    alternatives = root / "etc/alternatives"
    abi_library.mkdir(parents=True)
    linker_library.mkdir(parents=True)
    alternatives.mkdir(parents=True)

    final_library = abi_library / "libpthread-2.31.so"
    final_library.write_bytes(b"ELF fixture")
    soname = abi_library / "libpthread.so.0"
    soname.symlink_to("libpthread-2.31.so")
    alternative = alternatives / "libpthread.so"
    alternative.symlink_to("/lib/aarch64-linux-gnu/libpthread.so.0")
    linker_name = linker_library / "libpthread.so"
    linker_name.symlink_to("/etc/alternatives/libpthread.so")

    report = sysroot_module.normalize(root)

    assert linker_name.is_symlink()
    assert not pathlib.PurePosixPath(linker_name.readlink()).is_absolute()
    assert sysroot_module.resolve_in_sysroot(linker_name, root) == final_library
    assert len(report["entries"]) == 1
    assert report["entries"][0] == {
        "path": "usr/lib/aarch64-linux-gnu/libpthread.so",
        "original_target": "/etc/alternatives/libpthread.so",
        "relative_target": "../../../lib/aarch64-linux-gnu/libpthread-2.31.so",
        "resolved_target": "lib/aarch64-linux-gnu/libpthread-2.31.so",
    }
    assert report["transformation"] == (
        "absolute-sysroot-links-to-relative-direct-targets-v1"
    )

    # A retry after an interrupted rewrite finishes from the saved plan.
    linker_name.unlink()
    linker_name.symlink_to("/etc/alternatives/libpthread.so")
    recovered_report = sysroot_module.normalize(root)
    assert recovered_report == report
    assert sysroot_module.resolve_in_sysroot(linker_name, root) == final_library

    # A subsequent pass is stable and reuses the same transformation report.
    second_report = sysroot_module.normalize(root)
    assert second_report == report


def assert_rejected_without_partial_changes(
    root: pathlib.Path, bad_target: str, expected_message: str
) -> None:
    linker_directory = root / "usr/lib/aarch64-linux-gnu"
    linker_directory.mkdir(parents=True)
    valid = linker_directory / "valid.so"
    valid.symlink_to("/lib/aarch64-linux-gnu/valid.so.1")
    invalid = linker_directory / "invalid.so"
    invalid.symlink_to(bad_target)
    target = root / "lib/aarch64-linux-gnu/valid.so.1"
    target.parent.mkdir(parents=True)
    target.write_bytes(b"valid")

    try:
        sysroot_module.normalize(root)
    except RuntimeError as error:
        assert expected_message in str(error), str(error)
    else:
        raise AssertionError(f"accepted unsafe sysroot link: {bad_target}")

    assert valid.readlink() == pathlib.Path("/lib/aarch64-linux-gnu/valid.so.1")
    assert invalid.readlink() == pathlib.Path(bad_target)


def test_missing_target_fails_closed(temp: pathlib.Path) -> None:
    assert_rejected_without_partial_changes(
        temp / "missing", "/lib/aarch64-linux-gnu/not-present.so", "is missing"
    )


def test_escape_fails_closed(temp: pathlib.Path) -> None:
    assert_rejected_without_partial_changes(
        temp / "escape", "/../../outside.so", "escapes sysroot"
    )


def test_loop_fails_closed(temp: pathlib.Path) -> None:
    root = temp / "loop"
    linker_directory = root / "usr/lib/aarch64-linux-gnu"
    linker_directory.mkdir(parents=True)
    (linker_directory / "trigger.so").symlink_to(
        "/usr/lib/aarch64-linux-gnu/loop-a.so"
    )
    (linker_directory / "loop-a.so").symlink_to("loop-b.so")
    (linker_directory / "loop-b.so").symlink_to("loop-a.so")

    try:
        sysroot_module.normalize(root)
    except RuntimeError as error:
        assert "loop or excessive chain" in str(error), str(error)
    else:
        raise AssertionError("accepted a loop in an absolute sysroot link chain")

    assert (linker_directory / "trigger.so").readlink() == pathlib.Path(
        "/usr/lib/aarch64-linux-gnu/loop-a.so"
    )


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="starfox-sysroot-preparation-") as temp:
        root = pathlib.Path(temp)
        test_chained_absolute_and_relative_links_normalize_to_same_file(root)
        test_missing_target_fails_closed(root)
        test_escape_fails_closed(root)
        test_loop_fails_closed(root)
    print("Steam Frame sysroot normalization and fail-closed fixtures passed.")


if __name__ == "__main__":
    main()
