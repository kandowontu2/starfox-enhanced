#!/usr/bin/env python3
"""Check ARM64 ELF identity and resolve every dynamic dependency in a sysroot."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys


NEEDED_PATTERN = re.compile(r"\(NEEDED\).*?Shared library: \[([^]]+)\]")
PATH_PATTERN = re.compile(r"\((?:RPATH|RUNPATH)\).*?Library (?:rpath|runpath): \[([^]]*)\]")
INTERPRETER_PATTERN = re.compile(r"Requesting program interpreter: ([^]]+)")


def readelf(*args: str) -> str:
    result = subprocess.run(
        ["readelf", *args], text=True, capture_output=True, check=True
    )
    return result.stdout


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def ensure_arm64(path: pathlib.Path, cache: dict[pathlib.Path, bool]) -> None:
    resolved = path.resolve(strict=True)
    if resolved not in cache:
        header = readelf("-h", str(resolved))
        cache[resolved] = bool(re.search(r"^\s*Machine:\s+AArch64\s*$", header, re.M))
    if not cache[resolved]:
        raise AssertionError(f"ELF dependency is not AArch64: {path}")


def library_index(sysroot: pathlib.Path) -> dict[str, list[pathlib.Path]]:
    names: dict[str, list[pathlib.Path]] = {}
    roots = []
    for relative in ("lib", "lib64", "usr/lib", "usr/lib64"):
        candidate = sysroot / relative
        if candidate.exists():
            resolved = candidate.resolve()
            if resolved.is_relative_to(sysroot) and resolved not in roots:
                roots.append(resolved)
    for root in roots:
        for directory, _, files in os.walk(root, followlinks=False):
            for name in files:
                path = pathlib.Path(directory) / name
                if path.is_file():
                    names.setdefault(name, []).append(path)
    return names


def find_library(
    soname: str,
    index: dict[str, list[pathlib.Path]],
    sysroot: pathlib.Path,
    machine_cache: dict[pathlib.Path, bool],
) -> pathlib.Path:
    for candidate in index.get(soname, []):
        resolved = candidate.resolve()
        if not resolved.is_relative_to(sysroot):
            continue
        try:
            ensure_arm64(resolved, machine_cache)
        except (AssertionError, subprocess.CalledProcessError):
            continue
        return resolved
    raise AssertionError(f"cannot resolve AArch64 dependency {soname} inside {sysroot}")


def dependency_closure(
    executable: pathlib.Path,
    sysroot: pathlib.Path,
    index: dict[str, list[pathlib.Path]],
    machine_cache: dict[pathlib.Path, bool],
) -> dict[str, str]:
    queue = [executable.resolve(strict=True)]
    seen: set[pathlib.Path] = set()
    closure: dict[str, str] = {}
    while queue:
        current = queue.pop()
        if current in seen:
            continue
        seen.add(current)
        ensure_arm64(current, machine_cache)
        dynamic = readelf("-d", str(current))
        for paths in PATH_PATTERN.findall(dynamic):
            if any(not component.startswith("$ORIGIN") for component in paths.split(":")):
                raise AssertionError(f"non-relocatable RPATH/RUNPATH in {current}: {paths}")
        for soname in NEEDED_PATTERN.findall(dynamic):
            resolved = find_library(soname, index, sysroot, machine_cache)
            closure[soname] = resolved.relative_to(sysroot).as_posix()
            queue.append(resolved)
        if current == executable.resolve(strict=True):
            interpreter = INTERPRETER_PATTERN.search(readelf("-l", str(current)))
            if interpreter:
                path = (sysroot / interpreter.group(1).lstrip("/")).resolve(strict=True)
                if not path.is_relative_to(sysroot):
                    raise AssertionError(f"ELF interpreter escapes sysroot: {interpreter.group(1)}")
                ensure_arm64(path, machine_cache)
                closure["<interpreter>"] = path.relative_to(sysroot).as_posix()
    return dict(sorted(closure.items()))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sysroot", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("executables", nargs="+", help="NAME=ELF_PATH entries")
    args = parser.parse_args()

    sysroot = args.sysroot.resolve(strict=True)
    index = library_index(sysroot)
    machine_cache: dict[pathlib.Path, bool] = {}
    results: dict[str, dict[str, object]] = {}
    for entry in args.executables:
        if "=" not in entry:
            raise SystemExit(f"expected NAME=ELF_PATH, got: {entry}")
        name, raw_path = entry.split("=", 1)
        executable = pathlib.Path(raw_path).resolve(strict=True)
        results[name] = {
            "sha256": sha256(executable),
            "machine": "AArch64",
            "dependencies": dependency_closure(executable, sysroot, index, machine_cache),
        }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps({"format_version": 1, "executables": results}, indent=2, sort_keys=True)
        + "\n",
        encoding="utf-8",
    )
    print(f"AArch64 ELF and sysroot dependency closure passed for {len(results)} executables.")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1) from error
