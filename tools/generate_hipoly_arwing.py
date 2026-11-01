#!/usr/bin/env python3
"""Assemble UltraStarFox's high-poly Arwing shapes for the HIGH POLY ARWINGS option.

UltraStarFox ships a build-time switch (CONFIG/GRAPHICS.INC hipolyarwing)
that replaces every player and teammate Arwing with MYDEMO, the ending
cutscene model scaled to the flight ship, plus broken-wing and wireframe
variants (SHAPES/SHAPES6.ASM). Retail builds leave it off, so the ROM never
contains these streams. This tool assembles them with the SHMACS.INC shape
macros into one position-independent bank image. The renderers map it into an
otherwise unused LoROM bank when the option is on.

UltraStarFox's broken-wing models keep nearly the whole wing, and colour
the two sides differently. The solid damage variants here are derived from
MYDEMO_4 the way retail derives MYSHIP_L/R/B from MYSHIP_4 instead: the tip
pulls in to a stub at retail's position, the tip fin goes, and the stub's
faces take retail's damage colour (6) with recomputed normals. No BSP split
plane uses a wingtip point, so MYDEMO_4's sort tree stays valid.

--verify assembles the retail MYSHIP_4/L/R/B streams the same way and
compares them with a ROM, which checks the macro expansion.
"""
import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "upstream-ultrastarfox" / "SF"
OUTPUT = ROOT / "include" / "starfox" / "assets" / "hipoly_arwing_data.hpp"
BANK = 0x40

# Retail shape header -> UltraStarFox replacement stream, by the slot the
# player tables use (GSTRATS.ASM def_playership: intact, R, L, both). The
# hipolyarwing tables list mydemo_l in retail's myship_r slot, and so on.
# Retail MYSHIP_R has lost its -x wingtip and MYSHIP_L its +x one.
REPLACEMENTS = [
    ("MYSHIP_4", "MYDEMO_4"), ("MYSHIP_R", "HPDAMAGE_R"),
    ("MYSHIP_L", "HPDAMAGE_L"), ("MYSHIP_B", "HPDAMAGE_B"),
    ("BMYSHIP_4", "MYDEMO_4"), ("BMYSHIP_R", "HPDAMAGE_R"),
    ("BMYSHIP_L", "HPDAMAGE_L"), ("BMYSHIP_B", "HPDAMAGE_B"),
    ("IMYSHIP_4", "MYDEMO_4"), ("FRIENDSHIP_4", "MYDEMO_4"),
    ("MY_UP", "MYDEMO_4"),
    ("MYZOOM_4", "MYDEMO_4"), ("MYZOOM_R", "HPDAMAGE_R"),
    ("MYZOOM_L", "HPDAMAGE_L"), ("MYZOOM_B", "HPDAMAGE_B"),
    ("MY_W", "WIREMYDEMO_4"), ("MY_R_W", "WIREMYDEMO_L"),
    ("MY_L_W", "WIREMYDEMO_R"), ("MY_B_W", "WIREMYDEMO_B"),
]


# MYDEMO_4 wingtip point and its two tip-fin points, per side (+x, -x).
WING_TIPS = {+1: (6, 8, 9), -1: (18, 20, 21)}
STUB = (25, 10, -20)  # Retail MYSHIP_R's broken tip, (-25,10,-20), mirrored.
DAMAGE_COLOUR = 6     # Retail broken-wing faces.


def strip(line):
    return line.split(";", 1)[0].rstrip()


def damaged(block, name, sides):
    """MYDEMO_4 with the wings on `sides` (+1, -1) broken to retail stubs."""
    def directive(line):
        text = strip(line)
        parts = text.split(None, 1)
        if not text[:1].isspace() or not parts:
            return "", []
        op = parts[0].lower()
        if op not in ("pb", "pointsb", "pointsxb") and not re.fullmatch(r"face\d+", op):
            return op, []
        return op, [int(v) for v in parts[1].split(",")]

    # Expand to point indices; a PointsXb row is a point and its x mirror.
    coordinates, rows, mirror = [], [], False
    for line in block:
        op, values = directive(line)
        if op in ("pointsb", "pointsxb"):
            mirror = op == "pointsxb"
        elif op == "pb":
            rows.append(len(coordinates))
            coordinates.append(tuple(values))
            if mirror:
                coordinates.append((-values[0], values[1], values[2]))
    moved, removed = set(), set()
    for side in sides:
        tip, *fin = WING_TIPS[side]
        for index in (tip, *fin):
            if rows.count(index) != 1:
                raise RuntimeError(f"MYDEMO_4 point {index} is not a plain points row")
            coordinates[index] = (STUB[0] * side, STUB[1], STUB[2])
        moved.add(tip)
        removed.update(fin)

    def normal(a, b, c):
        pa, pb, pc = (coordinates[i] for i in (a, b, c))
        u = [pb[i] - pa[i] for i in range(3)]
        v = [pc[i] - pa[i] for i in range(3)]
        n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
        length = sum(component * component for component in n) ** 0.5
        # Face arguments hold -x, -y, +z of the winding's cross product
        # (checked against MYDEMO_4's own normals).
        return [round(-n[0] * 127 / length), round(-n[1] * 127 / length), round(n[2] * 127 / length)]

    out, row = [], 0
    for line in block:
        op, values = directive(line)
        if op == "pb":
            x, y, z = coordinates[rows[row]]
            row += 1
            out.append(f"	pb	{x},{y},{z}")
            continue
        if re.fullmatch(r"face\d+", op):
            vertices = values[5:]
            if set(vertices) & removed:
                continue  # The tip fin broke off with the tip.
            if set(vertices) & moved:
                values[0] = DAMAGE_COLOUR
                values[2:5] = normal(*vertices[:3])
            out.append(f"	{op.capitalize()}	" + ",".join(str(v) for v in values))
            continue
        out.append(re.sub(r"MYDEMO_4", name, strip(line), flags=re.I))
    return out


def evaluate(expression, labels, here):
    expression = expression.strip()
    tokens = re.findall(r"\$[0-9A-Fa-f]+|\d+|[A-Za-z_.][A-Za-z0-9_.]*|\*|[-+()]", expression)
    if "".join(tokens) != re.sub(r"\s+", "", expression):
        raise ValueError(f"cannot evaluate {expression!r}")
    python = []
    for token in tokens:
        if token == "*":
            python.append(str(here))
        elif token.startswith("$"):
            python.append(str(int(token[1:], 16)))
        elif token[0].isdigit() or token in "-+()":
            python.append(token)
        else:
            python.append(str(labels.get(token.upper(), here)))
    return eval("".join(python), {"__builtins__": {}})


def assemble(lines, base, labels):
    """One pass: returns bytes and records labels. Run twice for forward refs."""
    out = bytearray()
    scope = ""
    pending = {}

    def here():
        return base + len(out)

    def emit(kind, values):
        for value in values:
            if kind == 1:
                out.append(value & 0xff)
            else:
                out.extend(((value & 0xffff) & 0xff, (value & 0xffff) >> 8))

    for raw in lines:
        line = strip(raw)
        if not line.strip():
            continue
        match = re.match(r"^([A-Za-z_.][A-Za-z0-9_.]*)?\s*(.*)$", line)
        label, rest = match.group(1), match.group(2).strip()
        if label and not line[0].isspace():
            name = label.upper()
            if name.startswith("."):
                name = scope + name
            else:
                scope = name
            labels[name] = here()
        else:
            rest = line.strip()
        if not rest:
            continue
        parts = rest.split(None, 1)
        op = parts[0].lower()
        args = [a.strip() for a in parts[1].split(",")] if len(parts) > 1 else []

        def value(index, at=None):
            text = args[index]
            if text.startswith("."):
                text = scope + text
            return evaluate(text, labels, here() if at is None else at)

        if op == "pointsb":
            emit(1, [4, value(0)])
        elif op == "pointsxb":
            emit(1, [56, value(0)])
        elif op == "pointsw":
            emit(1, [8, value(0)])
        elif op == "pointsxw":
            emit(1, [52, value(0)])
        elif op == "pb":
            emit(1, [value(0), value(1), value(2)])
        elif op == "pw":
            emit(2, [value(0), value(1), value(2)])
        elif op == "endpoints":
            emit(1, [12])
        elif op == "vizis":
            emit(1, [48, value(0)])
        elif op == "viz":
            emit(1, [value(0), value(1), value(2)])
        elif op == "bspinit":
            emit(1, [60])
        elif op == "bsp":
            emit(1, [40, value(0)])
            emit(2, [value(1) - here() - 1])
            emit(1, [value(2) - here()])
        elif op == "bspnull":
            emit(1, [40, value(0)])
            emit(2, [value(1) - here() - 1])
            emit(1, [0])
        elif op == "bspe":
            emit(1, [68])
            emit(2, [value(0) - here() - 1])
        elif op == "faces":
            emit(1, [20])
        elif op == "fendq":
            emit(1, [0xff])
        elif op == "fend":
            emit(1, [0xfe])
        elif op == "endshape":
            emit(1, [0])
        elif re.fullmatch(r"face(\d+)", op):
            count = int(op[4:])
            values = [value(i) for i in range(len(args))]
            if len(values) != 5 + count:
                raise ValueError(f"{raw.strip()}: expected {5 + count} arguments")
            emit(1, [count, values[1], values[0], values[2], values[3], -values[4], *values[5:]])
        elif op == "aface3":
            v = [value(i) for i in range(len(args))]
            for a, b in ((v[6], v[7]), (v[5], v[6]), (v[5], v[7])):
                emit(1, [2, -1, 44, 0, 0, 0, a, b])
        elif op == "aface4":
            v = [value(i) for i in range(len(args))]
            for a, b in ((v[5], v[6]), (v[6], v[7]), (v[7], v[8]), (v[8], v[5])):
                emit(1, [2, -1, 44, 0, 0, 0, a, b])
        else:
            raise ValueError(f"unsupported shape directive: {raw.strip()}")
    return bytes(out)


def assemble_missing(lines, labels):
    used = set()
    for raw in lines:
        for token in re.findall(r"(?<![.\w])[A-Za-z_][A-Za-z0-9_]*", strip(raw).split(None, 1)[-1]
                                if strip(raw)[:1].isspace() else ""):
            used.add(token.upper())
    keywords = {"POINTSB", "POINTSXB", "POINTSW", "POINTSXW", "PB", "PW", "ENDPOINTS", "VIZIS",
                "VIZ", "BSPINIT", "BSP", "BSPNULL", "BSPE", "FACES", "FENDQ", "FEND", "ENDSHAPE",
                "AFACE3", "AFACE4"}
    return sorted(name for name in used - keywords if not re.fullmatch(r"FACE\d+", name)
                  and name not in labels)


def shape_blocks(path, names):
    """Return each named shape's body: its _P label to its EndShape."""
    text = path.read_text(encoding="latin-1").splitlines()
    blocks = {}
    for name in names:
        start = next(i for i, line in enumerate(text)
                     if re.match(rf"^{re.escape(name)}_P\s*$", strip(line), re.I))
        end = next(i for i in range(start, len(text))
                   if re.match(r"^\s+EndShape\b", strip(text[i]), re.I))
        blocks[name] = text[start:end + 1]
    return blocks


def build(path, names, base):
    blocks = shape_blocks(path, names)
    return assemble_lines([line for name in names for line in blocks[name]], base)


def assemble_lines(lines, base):
    labels = {}
    assemble(lines, base, labels)  # Forward references resolve on the next pass.
    first = assemble(lines, base, labels)
    second = assemble(lines, base, labels)
    if first != second:
        raise RuntimeError("shape labels did not settle")
    missing = assemble_missing(lines, labels)
    if missing:
        raise RuntimeError(f"undefined shape labels: {missing}")
    return second, labels


def verify(rom_path, symbols_path):
    rom = rom_path.read_bytes()
    symbols = {}
    for line in symbols_path.read_text(encoding="latin-1").splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[1].startswith("$"):
            symbols.setdefault(parts[0].upper(), []).append(int(parts[1][1:], 16))
    names = ["MYSHIP_4", "MYSHIP_L", "MYSHIP_R", "MYSHIP_B"]
    for name in names:
        base = max(symbols[name + "_P"])
        data, labels = build(SOURCE / "SHAPES" / "PSHAPES.ASM", [name], base)
        offset = ((base >> 16) & 0x7f) * 0x8000 + (base & 0x7fff)
        actual = rom[offset:offset + len(data)]
        faces = labels[name + "_F"]
        if actual != data or faces != max(symbols[name + "_F"]):
            first = next((i for i, (a, b) in enumerate(zip(actual, data)) if a != b), None)
            print(f"{name}: MISMATCH at +{first} (faces ${faces:06x})")
            return 1
        print(f"{name}: {len(data)} bytes match the ROM")
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--verify", nargs=2, metavar=("ROM", "SYMBOLS"), type=pathlib.Path)
    arguments = parser.parse_args()
    if arguments.verify:
        return verify(*arguments.verify)

    names = ["MYDEMO_4", "WIREMYDEMO_4", "WIREMYDEMO_L", "WIREMYDEMO_R", "WIREMYDEMO_B"]
    blocks = shape_blocks(SOURCE / "SHAPES" / "SHAPES6.ASM", names)
    base = (BANK << 16) | 0x8000
    # Rewriting MYDEMO_4 with no broken wing must reproduce it exactly.
    if assemble_lines(damaged(blocks["MYDEMO_4"], "MYDEMO_4", ()), base)[0] \
            != assemble_lines(blocks["MYDEMO_4"], base)[0]:
        raise RuntimeError("MYDEMO_4 rewrite is not exact")
    blocks["HPDAMAGE_L"] = damaged(blocks["MYDEMO_4"], "HPDAMAGE_L", (+1,))
    blocks["HPDAMAGE_R"] = damaged(blocks["MYDEMO_4"], "HPDAMAGE_R", (-1,))
    blocks["HPDAMAGE_B"] = damaged(blocks["MYDEMO_4"], "HPDAMAGE_B", (+1, -1))
    order = ["MYDEMO_4", "HPDAMAGE_L", "HPDAMAGE_R", "HPDAMAGE_B", *names[1:]]
    data, labels = assemble_lines([line for name in order for line in blocks[name]], base)
    if len(data) > 0x8000:
        raise RuntimeError("high-poly Arwings exceed one LoROM bank")

    rows = []
    for retail, stream in REPLACEMENTS:
        points, faces = labels[stream + "_P"], labels[stream + "_F"]
        rows.append(f'    {{"{retail}", 0x{points & 0xffff:04x}U, 0x{faces & 0xffff:04x}U}},')
    body = []
    for i in range(0, len(data), 16):
        body.append("    " + ",".join(f"0x{b:02x}" for b in data[i:i + 16]) + ",")
    OUTPUT.write_text(
        "#pragma once\n"
        "// Generated by tools/generate_hipoly_arwing.py from UltraStarFox\n"
        "// SHAPES/SHAPES6.ASM (hipolyarwing); damage variants derived from\n"
        "// MYDEMO_4. Do not edit.\n\n"
        "#include <array>\n#include <cstdint>\n\n"
        "namespace starfox::assets::hipoly_arwing {\n\n"
        f"inline constexpr std::uint8_t bank = 0x{BANK:02x}U;\n\n"
        "struct Replacement {\n    const char* shape;\n"
        "    std::uint16_t points;\n    std::uint16_t faces;\n};\n\n"
        f"inline constexpr std::array<Replacement, {len(rows)}> replacements{{{{\n"
        + "\n".join(rows) + "\n}};\n\n"
        f"inline constexpr std::array<std::uint8_t, {len(data)}> streams{{{{\n"
        + "\n".join(body) + "\n}};\n\n"
        "} // namespace starfox::assets::hipoly_arwing\n",
        encoding="utf-8", newline="\n")
    print(f"wrote {OUTPUT.relative_to(ROOT)} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
