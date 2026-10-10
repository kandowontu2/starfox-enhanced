#!/usr/bin/env python3
"""Normalize absolute target-library links inside the pinned sysroot."""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import tempfile


LINK_ROOTS = (
    "lib/aarch64-linux-gnu",
    "usr/lib/aarch64-linux-gnu",
    "lib64",
    "usr/lib64",
    "lib/gcc/aarch64-linux-gnu",
    "usr/lib/gcc/aarch64-linux-gnu",
    "usr/lib/gcc-cross/aarch64-linux-gnu",
)
TRANSFORMATION = "absolute-sysroot-links-to-relative-direct-targets-v1"


def resolve_in_sysroot(path: pathlib.Path, root: pathlib.Path) -> pathlib.Path:
    """Resolve symlink chains with absolute links rooted in the SDK, not host."""
    try:
        pending = list(path.relative_to(root).parts)
    except ValueError as error:
        raise RuntimeError(f"path is outside sysroot: {path}") from error

    resolved: list[str] = []
    link_hops = 0
    while pending:
        part = pending.pop(0)
        if part in ("", "."):
            continue
        if part == "..":
            if not resolved:
                raise RuntimeError(f"symlink escapes sysroot: {path}")
            resolved.pop()
            continue

        resolved.append(part)
        candidate = root.joinpath(*resolved)
        if not candidate.is_symlink():
            continue

        link_hops += 1
        if link_hops > 40:
            raise RuntimeError(f"symlink loop or excessive chain in sysroot: {path}")
        target = os.readlink(candidate)
        resolved.pop()
        target_parts = pathlib.PurePosixPath(target).parts
        if target.startswith("/"):
            resolved.clear()
            target_parts = target_parts[1:]
        pending = list(target_parts) + pending

    result = root.joinpath(*resolved)
    if not result.exists():
        raise RuntimeError(f"symlink target is missing inside sysroot: {path} -> {result}")
    if not result.is_relative_to(root):
        raise RuntimeError(f"symlink target is outside sysroot: {path} -> {result}")
    return result


def scoped_symlinks(root: pathlib.Path) -> list[pathlib.Path]:
    links: list[pathlib.Path] = []
    for relative_root in LINK_ROOTS:
        directory = root / relative_root
        if directory.is_symlink():
            links.append(directory)
            continue
        if not directory.exists():
            continue
        if not directory.is_dir():
            continue
        for current, directories, files in os.walk(directory, followlinks=False):
            current_path = pathlib.Path(current)
            for name in (*directories, *files):
                candidate = current_path / name
                if candidate.is_symlink():
                    links.append(candidate)
    return sorted(set(links))


def write_json_atomic(path: pathlib.Path, document: dict[str, object]) -> None:
    with tempfile.NamedTemporaryFile(
        mode="w", encoding="utf-8", prefix=f".{path.name}.",
        dir=path.parent, delete=False
    ) as temporary:
        json.dump(document, temporary, indent=2, sort_keys=True)
        temporary.write("\n")
        temporary_path = pathlib.Path(temporary.name)
    os.replace(temporary_path, path)


def validate_normalization_plan(
    root: pathlib.Path, plan: dict[str, object]
) -> list[tuple[pathlib.Path, dict[str, str]]]:
    if (plan.get("format_version") != 1
            or plan.get("transformation") != TRANSFORMATION
            or plan.get("scope") != list(LINK_ROOTS)):
        raise RuntimeError("stored sysroot normalization report has an unsupported format")
    entries = plan.get("entries")
    if (not isinstance(entries, list)
            or plan.get("normalized_absolute_symlinks") != len(entries)):
        raise RuntimeError("stored sysroot normalization report is incomplete")

    validated: list[tuple[pathlib.Path, dict[str, str]]] = []
    expected_paths: set[pathlib.Path] = set()
    for raw_entry in entries:
        if not isinstance(raw_entry, dict):
            raise RuntimeError("stored sysroot normalization entry is invalid")
        required = {"path", "original_target", "relative_target", "resolved_target"}
        if set(raw_entry) != required or not all(
            isinstance(raw_entry[key], str) for key in required
        ):
            raise RuntimeError("stored sysroot normalization entry is incomplete")
        relative_path = pathlib.PurePosixPath(raw_entry["path"])
        resolved_relative = pathlib.PurePosixPath(raw_entry["resolved_target"])
        relative_text = relative_path.as_posix()
        if (relative_path.is_absolute() or ".." in relative_path.parts
                or resolved_relative.is_absolute() or ".." in resolved_relative.parts
                or not raw_entry["original_target"].startswith("/")
                or raw_entry["relative_target"].startswith("/")
                or not any(
                    relative_text == scope or relative_text.startswith(scope + "/")
                    for scope in LINK_ROOTS
                )):
            raise RuntimeError("stored sysroot normalization entry escapes its scope")
        link = root.joinpath(*relative_path.parts)
        if not link.is_symlink():
            raise RuntimeError(f"normalized sysroot link is missing: {link}")
        current_target = os.readlink(link)
        if current_target not in (raw_entry["original_target"], raw_entry["relative_target"]):
            raise RuntimeError(f"normalized sysroot link has changed unexpectedly: {link}")
        expected_target = root.joinpath(*resolved_relative.parts)
        if resolve_in_sysroot(link, root) != expected_target:
            raise RuntimeError(f"normalized sysroot link changed target: {link}")
        expected_paths.add(link)
        validated.append((link, raw_entry))

    if len(expected_paths) != len(entries):
        raise RuntimeError("stored sysroot normalization report contains duplicate paths")

    unreported_absolute = {
        link for link in scoped_symlinks(root)
        if os.readlink(link).startswith("/") and link not in expected_paths
    }
    if unreported_absolute:
        raise RuntimeError(
            "sysroot contains absolute linker-root links absent from its report: "
            f"{sorted(str(path.relative_to(root)) for path in unreported_absolute)}"
        )
    return validated


def normalize(root: pathlib.Path) -> dict[str, object]:
    root = root.resolve(strict=True)
    if not root.is_dir():
        raise RuntimeError(f"sysroot is not a directory: {root}")

    plan_path = root / ".starfox-sysroot-link-normalization.json"
    if plan_path.exists():
        try:
            plan = json.loads(plan_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            raise RuntimeError(f"cannot read stored sysroot normalization report: {error}") from error
        if not isinstance(plan, dict):
            raise RuntimeError("stored sysroot normalization report is invalid")
        updates = validate_normalization_plan(root, plan)
    else:
        entries: list[dict[str, str]] = []
        for link in scoped_symlinks(root):
            original_target = os.readlink(link)
            if not original_target.startswith("/"):
                continue
            resolved_target = resolve_in_sysroot(link, root)
            relative_target = os.path.relpath(resolved_target, link.parent)
            if relative_target.startswith("/"):
                raise RuntimeError(f"failed to create a relative sysroot link for {link}")
            entries.append(
                {
                    "path": link.relative_to(root).as_posix(),
                    "original_target": original_target,
                    "relative_target": relative_target,
                    "resolved_target": resolved_target.relative_to(root).as_posix(),
                }
            )
        plan = {
            "format_version": 1,
            "transformation": TRANSFORMATION,
            "scope": list(LINK_ROOTS),
            "normalized_absolute_symlinks": len(entries),
            "entries": entries,
        }
        # Persist the complete plan before touching links so a retry after an
        # interrupted extraction can finish and verify the same transformation.
        write_json_atomic(plan_path, plan)
        updates = validate_normalization_plan(root, plan)

    # Validate every target before changing any link, then replace each link
    # atomically. The pinned archive stays untouched; only its extracted build
    # sysroot receives these equivalent root-relative names.
    for link, entry in updates:
        if os.readlink(link) == entry["relative_target"]:
            continue
        with tempfile.NamedTemporaryFile(
            prefix=f".{link.name}.", dir=link.parent, delete=False
        ) as temporary:
            temporary_path = pathlib.Path(temporary.name)
        temporary_path.unlink()
        temporary_path.symlink_to(entry["relative_target"])
        os.replace(temporary_path, link)

    validate_normalization_plan(root, plan)
    return plan


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sysroot", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--expected-absolute-links", type=int)
    args = parser.parse_args()

    report = normalize(args.sysroot)
    actual_count = int(report["normalized_absolute_symlinks"])
    if (args.expected_absolute_links is not None
            and actual_count != args.expected_absolute_links):
        raise RuntimeError(
            "unexpected number of absolute links in the pinned SDK linker roots: "
            f"expected {args.expected_absolute_links}, found {actual_count}"
        )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(
        "Normalized "
        f"{actual_count} absolute links in the Sniper "
        "ARM64 linker roots."
    )


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError) as error:
        raise SystemExit(f"Steam Frame sysroot preparation failed: {error}") from error
