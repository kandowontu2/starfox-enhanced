"""Compile the Linux Vulkan ray-query shaders to checked-in SPIR-V headers."""

from pathlib import Path
import argparse
import struct
import subprocess
import tempfile
from vulkan_material_colour import material_colour_source, liquid_optics_source, liquid_motion_source


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--glslc", required=True)
    parser.add_argument("--check", action="store_true",
                        help="Compile and compare without rewriting checked-in headers")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    colour = root / "src/render/shaders/generated/vulkan_material_colour.glsl"
    generated_colour = material_colour_source(root)
    if args.check:
        if colour.read_text(encoding="utf-8") != generated_colour:
            raise RuntimeError(f"Stale calibrated material colour source: {colour}")
        print(f"Current generated calibrated material colour source: {colour}")
    else:
        colour.write_text(generated_colour, encoding="utf-8")
    optics = root / "src/render/shaders/generated/vulkan_liquid_optics.glsl"
    generated_optics = liquid_optics_source(root)
    if args.check:
        if optics.read_text(encoding="utf-8") != generated_optics:
            raise RuntimeError(f"Stale liquid optical source: {optics}")
        print(f"Current generated liquid optical source: {optics}")
    else:
        optics.write_text(generated_optics, encoding="utf-8")
    motion = root / "src/render/shaders/generated/vulkan_liquid_hit_motion.glsl"
    generated_motion = liquid_motion_source(root)
    if args.check:
        if motion.read_text(encoding="utf-8") != generated_motion:
            raise RuntimeError(f"Stale liquid motion source: {motion}")
        print(f"Current generated liquid motion source: {motion}")
    else:
        motion.write_text(generated_motion, encoding="utf-8")
    for name in ("shadow", "reflection", "reflection_history", "reflection_ground_history",
                 "reflection_liquid_history", "reflection_liquid_motion"):
        source_name = "reflection" if name.startswith("reflection_") and name != "reflection_liquid_motion" else name
        source = root / f"src/render/shaders/vulkan_{source_name}_rayquery.comp"
        output = root / f"src/render/shaders/generated/vulkan_{name}_rayquery.hpp"
        with tempfile.TemporaryDirectory(prefix="starfox-rayquery-") as temporary:
            binary = Path(temporary) / f"{name}.spv"
            defines = ["-DSTARFOX_VULKAN_REFLECTION_HISTORY=1"] if name.startswith("reflection_") else []
            if name == "reflection_ground_history":
                defines.append("-DSTARFOX_VULKAN_GROUND_HISTORY=1")
            if name == "reflection_liquid_history":
                defines.append("-DSTARFOX_VULKAN_LIQUID_HISTORY=1")
            subprocess.run([args.glslc, *defines, "-fshader-stage=compute", "--target-env=vulkan1.2",
                            "-o", str(binary), str(source)], check=True)
            data = binary.read_bytes()
        if len(data) % 4:
            raise ValueError("SPIR-V byte count is not word aligned")
        words = struct.unpack("<" + "I" * (len(data) // 4), data)
        lines = ["#pragma once", "#include <array>", "#include <cstdint>",
                 "namespace starfox::render::shadows {",
                 f"inline constexpr std::array<std::uint32_t,{len(words)}> vulkan_{name}_rayquery_spirv{{{{"]
        for pos in range(0, len(words), 8):
            lines.append("    " + ", ".join(f"0x{word:08x}U" for word in words[pos:pos + 8])
                         + ("," if pos + 8 < len(words) else ""))
        lines.extend(["}};", "}", ""])
        generated = "\n".join(lines)
        if args.check:
            if output.read_text(encoding="utf-8") != generated:
                raise RuntimeError(f"Stale ray-query header: {output}")
            print(f"Current compiled ray-query header: {output}")
        else:
            output.write_text(generated, encoding="utf-8")


if __name__ == "__main__":
    main()
