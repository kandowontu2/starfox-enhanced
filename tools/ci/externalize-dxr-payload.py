"""Embed an unchanged DXC byte array through COFF assembly, not the C++ parser.

This is a temporary native-build candidate, not a shader/compiler substitute.
The DXC command and its original header remain intact. The companion declaration
keeps the original symbol, byte type and compile-time array extent.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def extract(text, symbol):
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", symbol):
        raise ValueError("Invalid shader symbol")
    match = re.search(
        r"(?m)^const\s+unsigned\s+char\s+" + re.escape(symbol)
        + r"\s*\[\s*\]\s*=\s*\{([^}]*)\}\s*;\s*\Z", text)
    if match is None:
        raise ValueError("Expected exactly the final DXC unsigned-byte array")
    body = match.group(1)
    if not re.fullmatch(r"\s*(?:0x[0-9a-fA-F]{2}\s*,\s*)*0x[0-9a-fA-F]{2}\s*,?\s*", body):
        raise ValueError("Nonliteral or malformed shader byte initializer")
    data = bytes(int(token, 16) for token in re.findall(r"0x([0-9a-fA-F]{2})", body))
    if len(data) < 32 or data[:4] != b"DXBC":
        raise ValueError("Not a DXC/DXBC container")
    if struct.unpack_from("<I", data, 24)[0] != len(data):
        raise ValueError("DXBC container extent differs from the complete array")
    chunk_count = struct.unpack_from("<I", data, 28)[0]
    if chunk_count == 0 or 32 + 4 * chunk_count > len(data):
        raise ValueError("Incomplete DXBC chunk table")
    for index in range(chunk_count):
        at = struct.unpack_from("<I", data, 32 + 4 * index)[0]
        if at < 32 + 4 * chunk_count or at + 8 > len(data):
            raise ValueError("Invalid DXBC chunk offset")
        size = struct.unpack_from("<I", data, at + 4)[0]
        if at + 8 + size > len(data):
            raise ValueError("Incomplete DXBC chunk")
    return data


def emit(header, output, symbol):
    raw = header.read_bytes()
    data = extract(raw.decode("utf-8-sig"), symbol)
    output.mkdir(parents=True, exist_ok=True)
    binary = output / (symbol + ".bin")
    declaration = output / (symbol + ".hpp")
    assembly = output / (symbol + ".S")
    receipt = output / (symbol + ".json")
    path = binary.resolve().as_posix()
    if '"' in path or "\n" in path or "\r" in path:
        raise ValueError("Unsafe assembly path")
    binary.write_bytes(data)
    declaration.write_text(
        '#pragma once\n// Exact DXC byte payload; original extent retained.\n'
        + f'extern "C" {{ extern const unsigned char {symbol}[{len(data)}]; }}\n', encoding="utf-8")
    assembly.write_text(
        '.section .rdata,"dr"\n.balign 16\n'
        + f'.global {symbol}\n{symbol}:\n.incbin "{path}"\n', encoding="utf-8")
    if binary.read_bytes() != data:
        raise ValueError("Payload serialization changed shader bytes")
    receipt.write_text(json.dumps({
        "symbol": symbol, "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "original_header_sha256": hashlib.sha256(raw).hexdigest(),
        "shader_bytes_unchanged": True, "original_array_extent_retained": True,
        "native_gpu_accepted": False,
    }, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--header", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--symbol", required=True)
    args = parser.parse_args()
    emit(args.header, args.output, args.symbol)
