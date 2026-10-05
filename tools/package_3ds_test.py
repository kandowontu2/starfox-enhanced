#!/usr/bin/env python3
"""Package an explicit New 3DS stereo / original-model mono test, never private assets."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zipfile

APP_DIR = "3ds/starfox-enhanced/"
SMDH_BYTES = 14016


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def validate_native(elf: Path, three_dsx: Path, smdh: Path) -> dict:
    # Debug ELFs are large. Check their fixed header without reading one into
    # RAM; hashes stream too. The package contains only the actual 3DSX/SMDH.
    with elf.open("rb") as stream:
        header = stream.read(52)
    if (len(header) != 52 or header[:7] != b"\x7fELF\x01\x01\x01"
            or struct.unpack_from("<HH", header, 16) != (2, 40)
            or struct.unpack_from("<I", header, 24)[0] != 0x100000):
        raise ValueError("Expected an ELF32 little-endian ARM executable at 0x100000")
    dsx = three_dsx.read_bytes()
    if len(dsx) < 44:
        raise ValueError("Truncated 3DSX header")
    magic, size, reloc, version, flags, code, rodata, data, bss, icon_at, icon_size, romfs = struct.unpack_from("<4sHH9I", dsx)
    if magic != b"3DSX" or size != 44 or reloc != 8 or version != 0 or flags != 0 or code == 0 or data < bss:
        raise ValueError("Invalid extended native 3DSX header or segments")
    minimum_payload = size + 3 * reloc + code + rodata + data - bss
    if icon_size != SMDH_BYTES or icon_at < minimum_payload or icon_at + icon_size != len(dsx) or romfs != 0:
        raise ValueError("Invalid 3DSX payload/SMDH bounds, trailing data or unexpected RomFS")
    icon = smdh.read_bytes()
    if len(icon) != SMDH_BYTES or icon[:6] != b"SMDH\x00\x00" or dsx[icon_at:] != icon:
        raise ValueError("Embedded and standalone SMDH must be valid and identical")
    with elf.open("rb") as stream:
        elf_hash = hashlib.file_digest(stream, "sha256").hexdigest()
    return {"elf_sha256": elf_hash, "elf_bytes": elf.stat().st_size,
            "three_dsx_sha256": digest(dsx), "three_dsx_bytes": len(dsx),
            "smdh_sha256": digest(icon), "smdh_bytes": len(icon),
            "smdh_offset": icon_at, "romfs_embedded": False}


def package(elf: Path, three_dsx: Path, smdh: Path, source_commit: str, output: Path) -> dict:
    if not re.fullmatch(r"[0-9a-f]{40}", source_commit):
        raise ValueError("Source commit must be a full lowercase Git SHA-1")
    if output.suffix.lower() != ".zip" or output.exists():
        raise ValueError("Output must be a new .zip file; existing packages are never overwritten")
    info = validate_native(elf, three_dsx, smdh)
    info.update(source_commit=source_commit, experimental=True, hardware_accepted=False,
                target="New Nintendo 3DS / New 3DS XL stereo; original 3DS / XL and 2DS mono",
                hardware_policy={"new_3ds": "CPU/cache speedup and slider-controlled stereo",
                                 "new_2ds_xl": "CPU/cache speedup, mono",
                                 "original_3ds_xl_2ds": "standard CPU, mono; slider ignored",
                                 "unknown": "standard CPU, mono"},
                private_assets_included=False)
    instructions = Path(__file__).resolve().parents[1] / "platform/3ds/TESTING.md"
    attribution = instructions.with_name("UPSTREAM-STARWING.md")
    payload = {
        APP_DIR + "starfox-enhanced.3dsx": three_dsx.read_bytes(),
        APP_DIR + "starfox-enhanced.smdh": smdh.read_bytes(),
        "README.txt": instructions.read_bytes() + b"\n\n" + attribution.read_bytes(),
        "BUILD-INFO.json": (json.dumps(info, indent=2, sort_keys=True) + "\n").encode(),
    }
    payload["SHA256SUMS.txt"] = "".join(f"{digest(value)}  {name}\n" for name, value in sorted(payload.items())).encode()
    output.parent.mkdir(parents=True, exist_ok=True)
    # 'x' protects a prior package, including a file appearing after preflight.
    created = False
    try:
        with zipfile.ZipFile(output, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            created = True
            for name, value in sorted(payload.items()):
                entry = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
                entry.compress_type = zipfile.ZIP_DEFLATED
                entry.external_attr = 0o100644 << 16
                archive.writestr(entry, value)
        with zipfile.ZipFile(output) as archive:
            if archive.testzip() is not None or set(archive.namelist()) != set(payload):
                raise ValueError("3DS test package failed its exact-member or CRC check")
    except Exception:
        if created:
            output.unlink(missing_ok=True)  # Only this operation's incomplete ZIP.
        raise
    return info


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--3dsx", dest="three_dsx", type=Path, required=True)
    parser.add_argument("--smdh", type=Path, required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        info = package(args.elf, args.three_dsx, args.smdh, args.source_commit, args.output)
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        parser.exit(1, f"3DS package rejected: {error}\n")
    print(f"Validated experimental 3DS package: {args.output}")
    print(f"3DSX {info['three_dsx_bytes']} bytes SHA256 {info['three_dsx_sha256']}")
    print("New 3DS stereo / original-model mono hardware acceptance and full-flow validation remain required.")


if __name__ == "__main__":
    main()
