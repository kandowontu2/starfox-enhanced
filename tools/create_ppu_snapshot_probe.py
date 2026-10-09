"""Build a private BG2-only reference probe from an explicit diagnostic snapshot.

The output contains the user's captured assets: keep it local, never package
or distribute it. This tests SNES PPU interpretation, not source-game timing.
"""
import argparse
import json
import struct
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('prefix', type=Path, help='Snapshot path without extension')
parser.add_argument('output', type=Path)
parser.add_argument('--host-origin', action='store_true',
                    help='Subtract one from VOFS to compare host row-zero sampling with SNES visible line one; not raw-register parity')
args = parser.parse_args()
def read(suffix, size):
    data = Path(str(args.prefix) + suffix).read_bytes()
    if len(data) != size:
        raise ValueError(f'{suffix}: expected {size} bytes, got {len(data)}')
    return data
state = json.loads(Path(str(args.prefix) + '.ppu.json').read_text())
if state['mode'] != 1:
    raise ValueError('Only Mode 1 snapshots supported; no guessed mode conversion')
vram, cgram = read('.vram', 65536), read('.cgram', 512)
horizontal = struct.unpack('<224h', read('.offsets', 448))
vertical = struct.unpack('<224h', read('.vertical', 448))
if args.host_origin:
    state['y'] -= 1
    vertical = tuple(value - 1 for value in vertical)
rom = bytearray(131072)
code = bytearray.fromhex('78 18 FB C2 30 A2 FF 1F 9A E2 20')
def reg(address, value):
    code.extend((0xA9, value & 255, 0x8D, address & 255, address >> 8))
def word(address, value):
    reg(address, value); reg(address, value >> 8)
def dma(bank, address, count, port, mode):
    reg(0x4300, mode); reg(0x4301, port)
    reg(0x4302, address); reg(0x4303, address >> 8); reg(0x4304, bank)
    reg(0x4305, count); reg(0x4306, count >> 8); reg(0x420B, 1)
reg(0x4200, 0); reg(0x420C, 0); reg(0x2100, 0x80)
reg(0x2115, 0x80); reg(0x2116, 0); reg(0x2117, 0)
dma(1, 0x8000, 32768, 0x18, 1)
dma(2, 0x8000, 32768, 0x18, 1)
reg(0x2121, 0); dma(3, 0x8000, 512, 0x22, 0)
reg(0x2105, 1 | (0x20 if state['bg2_tile16'] else 0))
reg(0x2106, 0)
reg(0x2108, ((state['bg2_map'] >> 8) & 0xFC) | state['bg2_size'])
reg(0x210B, (state['bg2_char'] >> 8) & 0xF0)
word(0x210F, state['x']); word(0x2110, state['y'])
reg(0x2123, 0); reg(0x2124, 0); reg(0x2125, 0)
reg(0x212C, 2); reg(0x212D, 0); reg(0x212E, 0); reg(0x212F, 0)
reg(0x2130, 0); reg(0x2131, 0); reg(0x2133, 0)
hdma = 0
for channel, port, offset, enabled, values in (
        (1, 0x0F, 0x18400, state['horizontal'], horizontal),
        (2, 0x10, 0x18800, state['vertical'], vertical)):
    if not enabled:
        continue
    table = b''.join(bytes([1]) + struct.pack('<H', value & 65535) for value in values) + b'\0'
    rom[offset:offset + len(table)] = table
    address = 0x8000 | (offset & 0x7FFF)
    base = 0x4300 + channel * 16
    reg(base, 2); reg(base + 1, port)
    reg(base + 2, address); reg(base + 3, address >> 8); reg(base + 4, 3)
    hdma |= 1 << channel
reg(0x420C, hdma); reg(0x2100, 15)
code.extend(bytes.fromhex('80 FE'))
assert len(code) < 0x7FC0
rom[:len(code)] = code
rom[0x8000:0x18000] = vram
rom[0x18000:0x18200] = cgram
rom[0x7FC0:0x7FD5] = b'BG2 SNAPSHOT PROBE   '
rom[0x7FD5:0x7FDC] = bytes([0x20, 0, 7, 0, 1, 0, 0])
rom[0x7FFC:0x7FFE] = bytes([0, 0x80])
rom[0x7FDC:0x7FE0] = bytes([255, 255, 0, 0])
checksum = sum(rom) & 65535
rom[0x7FDC:0x7FE0] = struct.pack('<HH', checksum ^ 65535, checksum)
assert len(rom) == 131072
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_bytes(rom)
print(f'Private snapshot probe: {args.output}; do not distribute captured assets')
