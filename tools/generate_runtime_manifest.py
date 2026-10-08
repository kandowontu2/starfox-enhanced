#!/usr/bin/env python3
"""Generate only the public companion manifest, without embedding game assets."""
from __future__ import annotations

import argparse
from pathlib import Path
import struct
import zlib

RESOURCES = (
    (101, "assets/patches/ultrastarfox-v12.bps"),
    (102, "assets/symbols/ultrastarfox.txt"),
    (108, "assets/patches/starfox-ex-v12.bps"),
    (109, "assets/symbols/starfox-ex.txt"),
    (120, "assets/patches/retail-japan-v10-to-usa-v12.bps"),
    (121, "assets/patches/retail-japan-v11-to-usa-v12.bps"),
    (122, "assets/patches/retail-usa-v10-to-v12.bps"),
    (123, "assets/patches/retail-usa-v11-to-v12.bps"),
    (124, "assets/patches/retail-europe-v10-to-usa-v12.bps"),
    (125, "assets/patches/retail-europe-v11-to-usa-v12.bps"),
    (126, "assets/patches/retail-germany-v10-to-usa-v12.bps"),
)


def companion_manifest(root: Path) -> int:
    checksum = 0
    for identifier, relative in RESOURCES:
        payload = (root / relative).read_bytes()
        if not payload or payload.startswith(b"version https://git-lfs.github.com/spec/v1"):
            raise ValueError(f"Missing public resource payload: {relative}")
        if identifier in (102, 109):
            payload = payload.replace(b"\r\n", b"\n").replace(b"\r", b"\n")
        checksum = zlib.crc32(struct.pack("<I", len(payload)), checksum)
        checksum = zlib.crc32(payload, checksum)
    return checksum


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    manifest = companion_manifest(args.root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        "#pragma once\n#include <cstdint>\n"
        "namespace starfox::platform::nintendo_3ds {\n"
        f"inline constexpr std::uint32_t companion_manifest = 0x{manifest:08x}U;\n"
        "}\n", encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
