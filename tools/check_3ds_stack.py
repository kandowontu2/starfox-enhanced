"""Validate the real ARM app's main-stack reservation and compiler frames.

This rejects a lost/weak libctru override or a single frame larger than the
reservation. It is not a whole call-chain or physical peak-memory measurement.
"""
from pathlib import Path
import argparse
import json
import struct

STACK_BYTES = 256 * 1024


def stack_reservation(data: bytes) -> int:
    if len(data) < 52 or data[:7] != b"\x7fELF\1\1\1":
        raise ValueError("Expected little-endian ELF32")
    if struct.unpack_from("<HH", data, 16) != (2, 40):
        raise ValueError("Expected an ARM executable")
    shoff = struct.unpack_from("<I", data, 32)[0]
    shsize, shnum = struct.unpack_from("<HH", data, 46)
    if shsize != 40 or not shnum or shoff + shnum * shsize > len(data):
        raise ValueError("Invalid ELF sections")
    sections = [struct.unpack_from("<10I", data, shoff + i * shsize) for i in range(shnum)]
    matches = []
    for section in sections:
        if section[1] != 2:  # SHT_SYMTAB
            continue
        if section[9] != 16 or section[5] % 16 or section[6] >= shnum:
            raise ValueError("Invalid ELF symbols")
        strings_section = sections[section[6]]
        strings = data[strings_section[4]:strings_section[4] + strings_section[5]]
        if len(strings) != strings_section[5] or section[4] + section[5] > len(data):
            raise ValueError("Truncated ELF symbols")
        for offset in range(section[4], section[4] + section[5], 16):
            name_at, address, size, info, other, index = struct.unpack_from("<IIIBBH", data, offset)
            if name_at >= len(strings):
                raise ValueError("Invalid ELF symbol name")
            if strings[name_at:].split(b"\0", 1)[0] != b"__stacksize__":
                continue
            if info >> 4 != 1 or index == 0 or index >= shnum or size != 4:
                raise ValueError("Main-stack override must be a defined strong 32-bit symbol")
            owner = sections[index]
            if owner[1] == 8 or address < owner[3] or address + 4 > owner[3] + owner[5]:
                raise ValueError("Invalid initialized main-stack storage")
            at = owner[4] + address - owner[3]
            if at + 4 > len(data):
                raise ValueError("Truncated main-stack value")
            matches.append(struct.unpack_from("<I", data, at)[0])
    if len(matches) != 1 or matches[0] != STACK_BYTES:
        raise ValueError(f"Expected one {STACK_BYTES}-byte main-stack override, got {matches}")
    return matches[0]


def compiler_frames(root: Path):
    frames = []
    for path in sorted(root.rglob("*.su")):
        for line in path.read_text(encoding="utf-8").splitlines():
            fields = line.rsplit("\t", 2)
            if len(fields) != 3 or not fields[1].isdigit():
                raise ValueError(f"Invalid compiler stack record in {path}")
            name, amount, kind = fields
            frames.append({"function": name, "bytes": int(amount), "kind": kind})
    if not frames:
        raise ValueError("Missing ARM compiler stack-usage records")
    if max(frame["bytes"] for frame in frames) >= STACK_BYTES:
        raise ValueError("An individual frame exhausts the reserved main stack")
    return sorted(frames, key=lambda frame: frame["bytes"], reverse=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    args = parser.parse_args()
    reservations = {}
    for target in ("frontend_check", "gpu_check", "game_core_check", "test_player"):
        path = args.build / f"starfox_3ds_{target}.elf"
        reservations[path.name] = stack_reservation(path.read_bytes())
    # Older packages legitimately predate this additional arithmetic probe;
    # when present, it must retain the same actual linked stack reservation.
    saturation = args.build / "starfox_3ds_spc_saturation_check.elf"
    if saturation.exists():
        reservations[saturation.name] = stack_reservation(saturation.read_bytes())
    counters = args.build / "starfox_3ds_spc_counters_check.elf"
    if counters.exists():
        reservations[counters.name] = stack_reservation(counters.read_bytes())
    frames = compiler_frames(args.build)
    print(json.dumps({"status": "passed", "main_stack_bytes": reservations,
                      "compiler_frame_count": len(frames), "largest_frames": frames[:20],
                      "scope": "Linked reservation and individual frames, not whole call-chain/peak RAM"}, indent=2))


if __name__ == "__main__":
    main()
