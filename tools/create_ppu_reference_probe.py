"""Generate a minimal original LoROM program for reference-video calibration.

No game assets or third-party ROM bytes. The program sets CGRAM zero to red,
enables display brightness, and loops with interrupts disabled.
"""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("output", type=Path)
args = parser.parse_args()
rom = bytearray([0xEA] * 32768)
program = bytes.fromhex(
    "78 18 FB C2 30 A2 FF 1F 9A E2 20 "  # SEI; native mode; stack; 8-bit A
    "A9 80 8D 00 21 "                  # forced blank while setting CGRAM
    "9C 21 21 A9 1F 8D 22 21 9C 22 21 "  # CGRAM[0] = $001f
    "9C 2C 21 9C 2D 21 A9 0F 8D 00 21 "  # backdrop only, full brightness
    "80 FE"
)
rom[:len(program)] = program
rom[0x7FC0:0x7FD5] = b"PPU REFERENCE PROBE  "
rom[0x7FD5:0x7FDC] = bytes([0x20, 0, 5, 0, 1, 0, 0])
rom[0x7FE0:0x8000] = bytes(32)
rom[0x7FFC:0x7FFE] = bytes([0, 0x80])
rom[0x7FDC:0x7FE0] = bytes([0xFF, 0xFF, 0, 0])
checksum = sum(rom) & 0xFFFF
rom[0x7FDC:0x7FE0] = (checksum ^ 0xFFFF).to_bytes(2, "little") + checksum.to_bytes(2, "little")
assert len(rom) == 32768
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_bytes(rom)
print(f"Generated original red-backdrop probe: {args.output}")
