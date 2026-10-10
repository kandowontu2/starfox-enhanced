"""Restricted syntax adapter for the canonical current/accepted liquid geometry.

No optical equation, footprint, displacement, material or normal is replaced.
The shared scalar water/lava helpers precede this code in the runtime library.
"""
from pathlib import Path
import argparse
import re


def liquid_optics_source(root: Path) -> str:
    source = (root / "src/render/shaders/liquid_optics.hlsli").read_text(encoding="utf-8")
    for name in ("water_caustics", "lava_surface"):
        directive = f'#include "../../../include/starfox/render/{name}.inc"'
        if source.count(directive) != 1:
            raise ValueError("Canonical liquid includes changed")
        source = source.replace(directive, "// Shared scalar helper is embedded earlier.")
    replacements = {
        "mul(hit,rotation)": "hit*rotation",
        "mul(direction,rotation)": "direction*rotation",
        "mul(rotation,float3(dx,-1,dz))": "rotation*float3(dx,-1,dz)",
        "(LavaSample)0": "LavaSample{0.0f,0.0f,0.0f,0.0f,0.0f,0.0f}",
    }
    for old, new in replacements.items():
        if source.count(old) != 1:
            raise ValueError("Canonical liquid matrix/initialization syntax changed")
        source = source.replace(old, new)
    source = re.sub(r"(?<![\w.])(?:\d+\.\d*|\.\d+)(?:[eE][+-]?\d+)?(?![\w.])",
                    lambda m: m[0] + "f", source)
    if re.search(r"\bmul\s*\(|#\s*include|\b(double|Texture\w*|matrix)\b", source):
        raise ValueError("Unsupported canonical liquid syntax")
    return "// Generated from liquid_optics.hlsli; syntax adaptation only.\n" + source


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    # Match the held generated input on every host, not Python's OS-dependent
    # default. Runtime assembly normalizes text identically on all platforms.
    args.output.write_text(liquid_optics_source(args.root), encoding="utf-8", newline="\r\n")
