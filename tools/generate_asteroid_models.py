#!/usr/bin/env python3
"""Convert the low-poly asteroid models into engine shape data.

The models in assets/models/asteroids replace the cartridge's whole-object
asteroid sprites. Each JSON file lists a palette measured from the sprite it
replaces; this tool maps those colours onto the Super FX palette slots the
sprite's texels use, so the replacement follows every level palette exactly
as the sprite does.

    python tools/generate_asteroid_models.py          # rewrite the header
    python tools/generate_asteroid_models.py --check  # fail if it is stale
"""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/models/asteroids"
DESTINATION = ROOT / "src/render/generated/asteroid_models_data.hpp"

# Model space is +X right, +Y up, +Z toward the viewer, with the sprite's
# square spanning [-1, 1]. Engine space is +Y down and +Z away from the
# camera. UNIT engine units span one model unit; the renderer scales the
# shape to each sprite's world size.
UNIT = 256

# Sprite texel slots in the Super FX palette, measured from the retail
# textures (the same texels appear in Original and EX).
GREY_RAMP = {0x182818: 9, 0x406048: 10, 0x709080: 11, 0xA8B8A8: 12, 0xD0E0D0: 13, 0xF0F8F8: 14}
ACCENTS = {0x881008: 1, 0xD04828: 2, 0xE8A840: 3, 0xF8D868: 4}

# name, source file, FNV-1a of the retail texels it replaces, lit palette
# entries (dark to light; the rest are unlit accents such as glowing eyes).
MODELS = [
    ("grey", "grey.json", 0x69637EFD, 6),
    ("orange", "orange.json", 0xA3B45AC9, 6),
    ("face", "face.json", 0x496C9F82, 6),
    ("crater", "crater.json", 0x21683FCC, 6),
]


def rgb(entry: list[int]) -> int:
    return (entry[0] << 16) | (entry[1] << 8) | entry[2]


def slot(colour: int) -> int:
    for table in (GREY_RAMP, ACCENTS):
        if colour in table:
            return table[colour]
    raise ValueError(f"colour {colour:06x} is not a known asteroid texel colour")


def cross(u, v):
    return (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])


def sub(u, v):
    return (u[0] - v[0], u[1] - v[1], u[2] - v[2])


def dot(u, v):
    return u[0] * v[0] + u[1] * v[1] + u[2] * v[2]


def unit_normal(a, b, c):
    n = cross(sub(b, a), sub(c, a))
    length = dot(n, n) ** 0.5
    return None if length == 0.0 else (n[0] / length, n[1] / length, n[2] / length)


# Face budgets per level of detail. The renderer picks a level from the
# rock's size on screen, as retail shapes do through their LOD pointers; the
# smallest is close to a real Super FX model's face count.
LOD_FACES = (None, 160, 60)


def simplify(points, faces, target):
    """Quadric edge collapse (Garland-Heckbert) down to `target` faces.

    Pure Python and plain floats so every platform produces identical output.
    Collapses that would flip a face or break the surface are skipped.
    """
    import heapq

    points = [tuple(point) for point in points]
    faces = [list(face) for face in faces]
    alive = [True] * len(faces)
    live = len(faces)
    removed = [False] * len(points)
    incident = [set() for _ in points]
    for index, face in enumerate(faces):
        for vertex in face:
            incident[vertex].add(index)

    def plane_quadric(face):
        a, b, c = (points[i] for i in face)
        n = unit_normal(a, b, c)
        if n is None:
            return [0.0] * 10
        d = -dot(n, a)
        x, y, z = n
        return [x * x, x * y, x * z, x * d, y * y, y * z, y * d, z * z, z * d, d * d]

    quadrics = [[0.0] * 10 for _ in points]
    for face in faces:
        q = plane_quadric(face)
        for vertex in face:
            quadrics[vertex] = [m + n for m, n in zip(quadrics[vertex], q)]

    def error(q, p):
        x, y, z = p
        return (q[0] * x * x + 2 * q[1] * x * y + 2 * q[2] * x * z + 2 * q[3] * x
                + q[4] * y * y + 2 * q[5] * y * z + 2 * q[6] * y
                + q[7] * z * z + 2 * q[8] * z + q[9])

    def neighbours(vertex):
        result = set()
        for face in incident[vertex]:
            result.update(faces[face])
        result.discard(vertex)
        return result

    def candidate(u, v):
        q = [m + n for m, n in zip(quadrics[u], quadrics[v])]
        middle = tuple((a + b) / 2.0 for a, b in zip(points[u], points[v]))
        options = sorted((error(q, position), index, position)
                         for index, position in enumerate((points[u], points[v], middle)))
        return options[0][0], options[0][2]

    version = [0] * len(points)
    heap = []

    def push(u, v):
        u, v = min(u, v), max(u, v)
        cost, _ = candidate(u, v)
        heapq.heappush(heap, (cost, u, v, version[u], version[v]))

    for face in faces:
        for i in range(3):
            if face[i] < face[(i + 1) % 3]:
                push(face[i], face[(i + 1) % 3])
            else:
                push(face[(i + 1) % 3], face[i])

    while live > target and heap:
        _, u, v, version_u, version_v = heapq.heappop(heap)
        if removed[u] or removed[v] or version[u] != version_u or version[v] != version_v:
            continue
        shared = incident[u] & incident[v]
        if not shared or len(neighbours(u) & neighbours(v)) != 2:
            continue  # Not an edge any more, or collapsing it would pinch the surface.
        _, position = candidate(u, v)
        flips = False
        for face in (incident[u] | incident[v]) - shared:
            corners = [points[i] for i in faces[face]]
            before = unit_normal(*corners)
            moved = [position if faces[face][k] in (u, v) else corners[k] for k in range(3)]
            after = unit_normal(*moved)
            if before is None or after is None or dot(before, after) < 0.2:
                flips = True
                break
        if flips:
            continue
        points[u] = position
        quadrics[u] = [m + n for m, n in zip(quadrics[u], quadrics[v])]
        for face in sorted(incident[v]):
            if face in shared:
                alive[face] = False
                live -= 1
                for vertex in faces[face]:
                    incident[vertex].discard(face)
            else:
                faces[face] = [u if vertex == v else vertex for vertex in faces[face]]
                incident[u].add(face)
        incident[v] = set()
        removed[v] = True
        version[u] += 1
        for vertex in sorted(neighbours(u)):
            push(u, vertex)

    remap, kept_points, kept_faces = {}, [], []
    for index, face in enumerate(faces):
        if not alive[index]:
            continue
        for vertex in face:
            if vertex not in remap:
                remap[vertex] = len(kept_points)
                kept_points.append(points[vertex])
        kept_faces.append([remap[vertex] for vertex in face])
    return kept_points, kept_faces


def recolour(source_points, source_faces, source_colours, points, faces):
    """Give each simplified face the colour covering most nearby source area."""
    def centre(pts, face):
        return tuple(sum(pts[i][k] for i in face) / 3.0 for k in range(3))

    centres = [centre(points, face) for face in faces]
    normals = [unit_normal(*(points[i] for i in face)) or (0.0, 0.0, 0.0) for face in faces]
    votes = [{} for _ in faces]
    for face, colour in zip(source_faces, source_colours):
        a, b, c = (source_points[i] for i in face)
        n = cross(sub(b, a), sub(c, a))
        area = dot(n, n) ** 0.5
        if area == 0.0:
            continue
        n = (n[0] / area, n[1] / area, n[2] / area)
        middle = centre(source_points, face)
        best = min(range(len(faces)), key=lambda i: (
            dot(sub(centres[i], middle), sub(centres[i], middle)) * (2.0 - dot(normals[i], n)), i))
        votes[best][colour] = votes[best].get(colour, 0.0) + area
    colours = []
    for index, vote in enumerate(votes):
        if vote:
            colours.append(max(sorted(vote), key=lambda colour: vote[colour]))
        else:
            nearest = min(range(len(source_faces)), key=lambda i: (
                dot(sub(centre(source_points, source_faces[i]), centres[index]),
                    sub(centre(source_points, source_faces[i]), centres[index])), i))
            colours.append(source_colours[nearest])
    return colours


def emit_mesh(name, points, faces, colours):
    vertices = [tuple(round(value) for value in point) for point in points]
    if len(vertices) > 256:
        raise ValueError(f"{name}: shapes address at most 256 vertices")
    kept_faces, normals, kept_colours = [], [], []
    for face, colour in zip(faces, colours):
        n = unit_normal(*(vertices[i] for i in face))
        if n is None:
            continue  # Rounding to engine units collapsed this sliver.
        kept_faces.append(tuple(face))
        # Retail face normals point inward: every MYSHIP/BASE/FACE triangle's
        # (b-a)x(c-a) opposes its stored normal. SHADESTAB2 lighting assumes it.
        normals.append(tuple(max(-127, min(127, round(-value * 127))) for value in n))
        kept_colours.append(colour)

    def rows(values, per_line):
        items = ["{" + ",".join(str(v) for v in value) + "}" for value in values]
        return ",\n".join("    " + ",".join(items[i:i + per_line]) for i in range(0, len(items), per_line))

    text = (
        f"inline constexpr std::array<std::array<std::int16_t,3>,{len(vertices)}> {name}_vertices{{{{\n"
        f"{rows(vertices, 8)}}}}};\n"
        f"inline constexpr std::array<std::array<std::uint8_t,3>,{len(kept_faces)}> {name}_faces{{{{\n"
        f"{rows(kept_faces, 10)}}}}};\n"
        f"inline constexpr std::array<std::array<std::int8_t,3>,{len(normals)}> {name}_normals{{{{\n"
        f"{rows(normals, 10)}}}}};\n"
        f"inline constexpr std::array<std::uint8_t,{len(kept_colours)}> {name}_colours{{\n"
        + ",\n".join("    " + ",".join(str(c) for c in kept_colours[i:i + 40]) for i in range(0, len(kept_colours), 40))
        + "};\n"
    )
    return text, f"Mesh{{{name}_vertices,{name}_faces,{name}_normals,{name}_colours}}"


def convert(name: str, data: dict, texture_hash: int, lit: int) -> str:
    palette = [rgb(entry) for entry in data["palette_rgb"]]
    ramp = [slot(colour) for colour in palette[:lit]]
    accents = [slot(colour) for colour in palette[lit:]]
    points = [(x * UNIT, -y * UNIT, -z * UNIT) for x, y, z in data["vertices"]]
    faces = []
    for (a, b, c), (nx, ny, nz) in zip(data["faces"], data["face_normals"]):
        normal = (nx, -ny, -nz)
        # The renderer shows a face when (b-a)x(c-a) points away from the
        # centre of view, so order every triangle by its outward normal.
        if dot(cross(sub(points[b], points[a]), sub(points[c], points[a])), normal) < 0:
            b, c = c, b
        faces.append((a, b, c))
    colours = data["face_palette_index"]
    if len(colours) != len(faces) or max(colours) >= len(palette):
        raise ValueError(f"{name}: face colours do not match the palette")
    text, meshes = "", []
    for level, budget in enumerate(LOD_FACES):
        if budget is None:
            level_points, level_faces, level_colours = points, faces, colours
        else:
            level_points, level_faces = simplify(points, faces, budget)
            level_colours = recolour(points, faces, colours, level_points, level_faces)
        mesh_text, mesh = emit_mesh(f"{name}_lod{level}", level_points, level_faces, level_colours)
        text += mesh_text
        meshes.append(mesh)
    return (
        text
        + f"inline constexpr ModelData {name}{{0x{texture_hash:08x}U,"
        f"{{{','.join(str(s) for s in ramp)}}},{len(ramp)},"
        f"{{{','.join(str(s) for s in accents) or '0'}}},{len(accents)},"
        f"{{{{{','.join(meshes)}}}}}}};\n"
    )


def generate() -> str:
    digest = hashlib.sha256()
    body = []
    for name, filename, texture_hash, lit in MODELS:
        data = json.loads((SOURCE / filename).read_text(encoding="utf-8"))
        digest.update(json.dumps(data, sort_keys=True).encode())
        body.append(convert(name, data, texture_hash, lit))
    return (
        "// Generated by tools/generate_asteroid_models.py; do not edit.\n"
        f"// Source SHA-256: {digest.hexdigest()}\n"
        "#pragma once\n"
        "#include <array>\n#include <cstdint>\n#include <span>\n\n"
        "namespace starfox::render::asteroid_data {\n"
        f"inline constexpr int unit={UNIT};\n"
        f"inline constexpr std::size_t lod_count={len(LOD_FACES)};\n"
        "struct Mesh {\n"
        "    std::span<const std::array<std::int16_t,3>> vertices;\n"
        "    std::span<const std::array<std::uint8_t,3>> faces;\n"
        "    std::span<const std::array<std::int8_t,3>> normals;\n"
        "    std::span<const std::uint8_t> colours;\n"
        "};\n"
        "struct ModelData {\n"
        "    std::uint32_t texture_hash;\n"
        "    std::array<std::uint8_t,6> ramp; std::size_t ramp_size;\n"
        "    std::array<std::uint8_t,4> accents; std::size_t accent_count;\n"
        "    std::array<Mesh,lod_count> lods; // Full detail first.\n"
        "};\n"
        + "\n".join(body)
        + f"inline constexpr std::array<const ModelData*,{len(MODELS)}> models{{"
        + ",".join(f"&{name}" for name, *_ in MODELS)
        + "};\n}\n"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    text = generate()
    if args.check:
        if not DESTINATION.exists() or DESTINATION.read_text(encoding="utf-8") != text:
            print(f"{DESTINATION.relative_to(ROOT)} is stale; run tools/generate_asteroid_models.py", file=sys.stderr)
            return 1
        return 0
    DESTINATION.parent.mkdir(parents=True, exist_ok=True)
    DESTINATION.write_text(text, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
