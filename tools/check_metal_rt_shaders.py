"""Compile the exact runtime Metal RT sources on an Apple SDK host.

--emit-only produces inspectable sources elsewhere; it does not validate Metal.
"""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", choices=("macosx", "iphoneos"), default="iphoneos")
    parser.add_argument("--emit-only", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    implementation = (root / "src/render/metal_hardware_rt.mm").read_text(encoding="utf-8")
    template = (root / "src/render/shaders/metal_water_shared.hpp.in").read_text(encoding="utf-8")
    template = template.replace("@STARFOX_LAVA_SURFACE_SOURCE@",
                                (root / "include/starfox/render/lava_surface.inc").read_text(encoding="utf-8"))
    for token, name in (("CAUSTICS", "water_caustics"), ("TRANSMISSION", "water_transmission")):
        template = template.replace(f"@STARFOX_WATER_{token}_SOURCE@",
                                    (root / f"include/starfox/render/{name}.inc").read_text(encoding="utf-8"))
    if re.search(r"@STARFOX_[A-Z_]+@", template):
        raise RuntimeError("Unresolved shared Metal shader source token")
    shared = re.search(r'R"SF_WATER\((.*?)\)SF_WATER"', template, re.S)
    if not shared:
        raise RuntimeError("Missing shared Metal source template")
    native_template = (root / "src/render/shaders/metal_native_material_shared.hpp.in").read_text(encoding="utf-8")
    native_template = native_template.replace("@STARFOX_METAL_NATIVE_COVERAGE_SOURCE@",
        (root / "include/starfox/render/metal_native_material_coverage.inc").read_text(encoding="utf-8"))
    native_template = native_template.replace("@STARFOX_METAL_MATERIAL_COLOUR_SOURCE@",
        (root / "include/starfox/render/metal_material_colour.inc").read_text(encoding="utf-8"))
    native_template = native_template.replace("@STARFOX_METAL_NATIVE_COLOUR_SOURCE@",
        (root / "include/starfox/render/metal_native_material_colour.inc").read_text(encoding="utf-8"))
    if re.search(r"@STARFOX_[A-Z_]+@", native_template):
        raise RuntimeError("Unresolved native material Metal shader source token")
    native = re.search(r'R"SF_NATIVE\((.*?)\)SF_NATIVE"', native_template, re.S)
    if not native:
        raise RuntimeError("Missing native material Metal source template")
    indexed = re.search(r'constexpr char kIndexedShader\[\] = R"METAL\((.*?)\)METAL";', implementation, re.S)
    if not indexed:
        raise RuntimeError("Missing shared runtime Metal indexed candidate source")
    sources = {}
    for name in ("Shadow", "Reflection"):
        match = re.search(rf'constexpr char k{name}Shader\[\] = R"METAL\((.*?)\)METAL";', implementation, re.S)
        if not match:
            raise RuntimeError(f"Missing runtime {name} shader")
        sources[name] = (shared[1] if name == "Reflection" else "") + native[1] + indexed[1] + match[1]
    with tempfile.TemporaryDirectory(prefix="starfox-metal-check-") as temporary:
        destination = args.emit_only or Path(temporary)
        destination.mkdir(parents=True, exist_ok=True)
        for name, source in sources.items():
            path = destination / f"{name.lower()}.metal"
            path.write_text(source, encoding="utf-8")
            if args.emit_only:
                print(f"Emitted (NOT compiled): {path}")
            else:
                subprocess.run(["xcrun", "--sdk", args.sdk, "metal", "-std=metal3.0",
                                "-c", str(path), "-o", str(destination / f"{name.lower()}.air")], check=True)
                print(f"Compiled {name} runtime shader for {args.sdk}")


if __name__ == "__main__":
    main()
