"""Temporary build-only embedding of all 50 unchanged reflection-history arrays.

Run after the original shader commands. No shader is compiled, optimized,
replaced, or omitted here. Existing inline legacy arrays use distinct C++
namespaces, matching the source's existing external-payload branch.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


LEGACY = (
    ("calibrated_scene_portable", "calibrated_scene_shader", "reflection_history_external_scene_shader", "reflection_history"),
    ("calibrated_reflection_lobes", "reflection_lobes_shader", "reflection_history_external_lobes_shader", "lobes"),
    ("calibrated_reflection_paths", "reflection_paths_shader", "reflection_history_external_paths_shader", "paths"),
    ("calibrated_reflection_curved_paths", "reflection_curved_paths_shader", "reflection_history_external_curved_shader", "curved_paths"),
)


def validate_container(data):
    if data[:4] == b"DXBC":
        if len(data) < 36 or struct.unpack_from("<I", data, 24)[0] != len(data):
            raise ValueError("Incomplete DXBC extent")
        count = struct.unpack_from("<I", data, 28)[0]
        if not count or 32 + 4 * count > len(data):
            raise ValueError("Incomplete DXBC chunk table")
        for i in range(count):
            at = struct.unpack_from("<I", data, 32 + 4 * i)[0]
            if at < 32 + 4 * count or at + 8 > len(data):
                raise ValueError("Invalid DXBC chunk offset")
            if at + 8 + struct.unpack_from("<I", data, at + 4)[0] > len(data):
                raise ValueError("Incomplete DXBC chunk")
    elif data[:4] == b"\x03\x02\x23\x07":
        if len(data) < 20 or len(data) % 4:
            raise ValueError("Incomplete SPIR-V extent")
        words = struct.unpack("<" + "I" * (len(data) // 4), data)
        if not words[3] or words[4]:
            raise ValueError("Invalid SPIR-V header")
        at = 5
        while at < len(words):
            count = words[at] >> 16
            if not count or at + count > len(words):
                raise ValueError("Incomplete SPIR-V instruction")
            at += count
    else:
        raise ValueError("Expected complete DXBC or SPIR-V data")


def extract(text, symbol, namespace=None):
    if not re.fullmatch(r"[A-Za-z_][A-Za-z_0-9]*", symbol):
        raise ValueError("Invalid shader symbol")
    expression = (r"(?m)^(?:alignas\(4\)\s+)?(?:inline\s+constexpr\s+|const\s+)"
                  r"unsigned\s+char\s+" + re.escape(symbol)
                  + r"\s*\[\s*\]\s*=\s*\{([^}]*)\}\s*;")
    matches = list(re.finditer(expression, text))
    if len(matches) != 1:
        raise ValueError("Require exactly one original byte array")
    if namespace:
        declarations = list(re.finditer(r"(?m)^namespace starfox::render::"
                                       + re.escape(namespace) + r"\s*\{\s*$", text))
        if len(declarations) != 1:
            raise ValueError("Original payload namespace missing or duplicated")
        closing = re.search(r"(?m)^\}\s*$", text[declarations[0].end():])
        if not closing or not (declarations[0].end() <= matches[0].start()
                               and matches[0].end() <= declarations[0].end() + closing.start()):
            raise ValueError("Array escaped its original namespace")
    token = r"(?:0x[0-9a-fA-F]{2}|[0-9]{1,3})"
    body = matches[0].group(1)
    if not re.fullmatch(r"\s*" + token + r"(?:\s*,\s*" + token + r")*\s*,?\s*", body):
        raise ValueError("Nonliteral or incomplete byte initializer")
    values = [int(value, 16 if value.startswith("0x") else 10)
              for value in re.findall(token, body)]
    if any(value > 255 for value in values):
        raise ValueError("Byte outside the original unsigned-char range")
    data = bytes(values)
    validate_container(data)
    return data


def fixed_declaration(symbol, extent):
    return f"alignas(4) extern const unsigned char {symbol}[{extent}];\n"


def emit_data(raw, data, output, symbol, namespace=None, declaration_namespace=None):
    output.mkdir(parents=True, exist_ok=True)
    binary = output / (symbol + ".bin")
    path = binary.resolve().as_posix()
    if any(character in path for character in ('"', "\r", "\n")):
        raise ValueError("Unsafe assembly path")
    binary.write_bytes(data)
    (output / (symbol + ".S")).write_text(
        '.section .rdata,"dr"\n.balign 16\n'
        + f'.global {symbol}\n{symbol}:\n.incbin "{path}"\n', encoding="utf-8")
    if not namespace:
        (output / (symbol + ".hpp")).write_text(
            '#pragma once\nextern "C" {\n' + fixed_declaration(symbol, len(data)) + '}\n', encoding="utf-8")
    if binary.read_bytes() != data:
        raise ValueError("Serialization changed shader bytes")
    record = {
        "symbol": symbol, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
        "original_header_sha256": hashlib.sha256(raw).hexdigest(),
        "original_namespace": namespace, "declaration_namespace": declaration_namespace,
        "shader_bytes_unchanged": True, "original_array_extent_retained": True,
        "distinct_cpp_entity": bool(namespace and namespace != declaration_namespace),
        "gpu_accepted": False, "production_adopted": False,
    }
    (output / (symbol + ".json")).write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    return record


def emit_generated(header, output, symbol):
    raw = header.read_bytes()
    return emit_data(raw, extract(raw.decode("utf-8-sig"), symbol), output, symbol)


def emit_legacy(root, output):
    declarations = ["#pragma once\n"]
    records = []
    for filename, namespace, distinct_namespace, stem in LEGACY:
        raw = (root / "src/render/shaders/generated" / (filename + ".hpp")).read_bytes()
        text = raw.decode("utf-8-sig")
        declarations.append(f'namespace starfox::render::{distinct_namespace} {{\nextern "C" {{\n')
        for backend in ("dxil", "spirv"):
            symbol = stem + "_" + backend
            data = extract(text, symbol, namespace)
            records.append(emit_data(raw, data, output, symbol, namespace, distinct_namespace))
            declarations.append(fixed_declaration(symbol, len(data)))
        declarations.append("}\n}\n")
    if len(records) != 8 or len({record["symbol"] for record in records}) != 8:
        raise ValueError("Require all eight distinct legacy arrays")
    (output / "external_reflection_history_payloads_v2.hpp").write_text("".join(declarations), encoding="utf-8")
    return records


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--header", type=Path)
    mode.add_argument("--legacy-root", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--symbol")
    args = parser.parse_args()
    if args.header:
        if not args.symbol:
            parser.error("--header requires --symbol")
        emit_generated(args.header, args.output, args.symbol)
    else:
        if args.symbol:
            parser.error("--symbol does not apply to the complete legacy package")
        emit_legacy(args.legacy_root, args.output)
