"""Actual native material producer ObjC++ and complete runtime shader SDK checks.

No GPU execution, full reflection-producer adoption or performance acceptance.
"""
import argparse
import ctypes
import json
from pathlib import Path
import re
import subprocess
import sys

import compile_metal as observer
from metal_liquid_optics import liquid_optics_source
from metal_material_colour import material_colour_source

ROOT = Path(__file__).resolve().parents[1]
OBSERVER_SHA = "cc770ea2068978eb0eeb381dc3bbf8983b838c2563f2605678272146f632fbb4"
DECODER_SHA = "69d61c1055f123f1b750eaa95bc3063994f2dbe754316430b41533c8cbe83a86"
SCOPE = "additional-native-metal-canonical-build-source-integration"
REQUIRED = {
    ".gitattributes", ".github/workflows/portable-builds.yml", "README.md", "LICENSE",
    "headers/SDL3/LICENSE.txt", "src/render/metal_hardware_rt.mm",
    "src/render/shaders/metal_water_shared.hpp.in", "src/render/shaders/metal_native_material_shared.hpp.in",
    "cmake/NativeMetalMaterialSource.cmake", "include/starfox/render/metal_native_material_coverage.inc",
    "include/starfox/render/metal_material_colour.inc", "include/starfox/render/metal_native_material_colour.inc",
    "include/starfox/render/lava_surface.inc", "include/starfox/render/water_caustics.inc",
    "include/starfox/render/water_transmission.inc", "tools/check_metal_rt_shaders.py",
    "tests/test_metal_shader_sources.py", "tools/compile_metal.py", "tools/check_producer.py",
    "tools/metal_liquid_optics.py", "src/render/shaders/liquid_optics.hlsli",
    "include/starfox/render/metal_liquid_optics.inc",
    "tools/metal_material_colour.py", "src/render/shaders/calibrated_colour.hlsli",
    "src/vr/shaders/scene_colour.hlsli",
}


def qualify(manifest):
    rows = manifest.get("Sources", [])
    names = [row["File"] for row in rows]
    if manifest.get("Scope") != SCOPE or len(names) != len(set(names)) or not REQUIRED <= set(names):
        raise RuntimeError("Incomplete/duplicate native producer source inventory or scope")
    for row in rows:
        name = row["File"]
        if name not in REQUIRED and not ((name.startswith("include/starfox/") or name.startswith("headers/SDL3/"))
                                        and name.endswith((".hpp", ".h", ".inc"))):
            raise RuntimeError("Unallowlisted/escaping source or non-source payload")
        if ".." in Path(name).parts or Path(name).is_absolute():
            raise RuntimeError("Escaping native producer source")
        path = ROOT / name
        if observer.digest(path) != row["Sha256"] or path.stat().st_size != row["Bytes"]:
            raise RuntimeError("Held native producer/component input changed")
        if "\0" in path.read_bytes().decode("utf-8", errors="strict"):
            raise RuntimeError("Binary asset in source-only component")
    if observer.digest(ROOT / "tools/compile_metal.py") != OBSERVER_SHA or observer.digest(
            ROOT / "include/starfox/render/metal_native_material_coverage.inc") != DECODER_SHA or observer.digest(
            ROOT / "include/starfox/render/metal_material_colour.inc") != "ec73381d05ad01b71780abb821d5f25b54080124410e8854a8c52931ea5d9347":
        raise RuntimeError("Original native observer or previously checked decoder changed")
    if (ROOT / "include/starfox/render/metal_liquid_optics.inc").read_text(encoding="utf-8") != liquid_optics_source(ROOT):
        raise RuntimeError("Canonical liquid geometry and generated Metal helper differ")
    if (ROOT / "include/starfox/render/metal_material_colour.inc").read_text(encoding="utf-8") != material_colour_source(ROOT):
        raise RuntimeError("Canonical colour equations and generated Metal helper differ")
    actual = {str(path.relative_to(ROOT)).replace('\\', '/') for path in ROOT.rglob('*') if path.is_file()
              and '__pycache__' not in path.parts and '.git' not in path.parts and path.name != 'CMakeLists.txt'}
    if actual != set(names) | {"inputs.json"}:
        raise RuntimeError("Unheld/extra component source file")
    return rows


def source_controls(manifest):
    qualify(manifest)
    mutants = []
    for kind in ("missing", "duplicate", "escaped", "hash", "bytes", "scope"):
        changed = json.loads(json.dumps(manifest))
        if kind == "missing":
            changed["Sources"] = [row for row in changed["Sources"] if row["File"] != "src/render/metal_hardware_rt.mm"]
        elif kind == "duplicate":
            changed["Sources"].append(dict(changed["Sources"][0]))
        elif kind == "escaped":
            changed["Sources"][0]["File"] = "../private.bin"
        elif kind == "hash":
            changed["Sources"][0]["Sha256"] = "0" * 64
        elif kind == "bytes":
            changed["Sources"][0]["Bytes"] = 1
        else:
            changed["Scope"] = "complete25-accepted"
        mutants.append(changed)
    for changed in mutants:
        try:
            qualify(changed)
        except RuntimeError:
            continue
        raise RuntimeError("Altered/narrowed producer source or overclaimed scope accepted")
    return {"scope": "source inventory/refusal and runtime-source assembly only; not native compilation or GPU",
            "source_count": len(manifest["Sources"]), "source_negative_controls": len(mutants),
            "manifest_sha256": observer.digest(ROOT / "inputs.json"), "gpu_launched": False, "native_launched": False}


def embedded_header(template, substitutions):
    text = template.read_text(encoding="utf-8")
    for key, path in substitutions.items():
        text = text.replace(key, path.read_text(encoding="utf-8"))
    if re.search(r"@STARFOX_[A-Z_]+@", text):
        raise RuntimeError("Unresolved producer runtime embedding token")
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", choices=("macosx", "iphoneos"))
    parser.add_argument("--out", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    manifest = json.loads((ROOT / "inputs.json").read_text(encoding="utf-8-sig"))
    controls = source_controls(manifest)
    subprocess.run([sys.executable, str(ROOT / "tests/test_metal_shader_sources.py")], check=True)
    if args.self_test:
        print(json.dumps(controls, sort_keys=True))
        return
    if sys.platform != "darwin" or not args.sdk or not args.out:
        raise RuntimeError("Actual Darwin SDK and unique output directory required")
    library = ctypes.CDLL("/usr/lib/libproc.dylib")
    library.proc_pidinfo.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_int]
    library.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
    if ctypes.sizeof(observer.BsdInfo) != 136 or ctypes.sizeof(observer.TaskInfo) != 96:
        raise RuntimeError("Original native Darwin identity/task ABI differs")
    tools = {name: Path(subprocess.check_output(["xcrun", "--sdk", args.sdk, "--find", name], text=True).strip()).absolute()
             for name in ("clang", "metal", "metallib")}
    sdk_root = subprocess.check_output(["xcrun", "--sdk", args.sdk, "--show-sdk-path"], text=True).strip()
    environment = {"SDKROOT": sdk_root}
    args.out.mkdir(parents=True, exist_ok=False)
    generated = args.out / "generated"
    generated.mkdir()
    water = embedded_header(ROOT / "src/render/shaders/metal_water_shared.hpp.in", {
        "@STARFOX_LAVA_SURFACE_SOURCE@": ROOT / "include/starfox/render/lava_surface.inc",
        "@STARFOX_WATER_CAUSTICS_SOURCE@": ROOT / "include/starfox/render/water_caustics.inc",
        "@STARFOX_WATER_TRANSMISSION_SOURCE@": ROOT / "include/starfox/render/water_transmission.inc",
        "@STARFOX_LIQUID_OPTICS_SOURCE@": ROOT / "include/starfox/render/metal_liquid_optics.inc"})
    native = embedded_header(ROOT / "src/render/shaders/metal_native_material_shared.hpp.in", {
        "@STARFOX_METAL_NATIVE_COVERAGE_SOURCE@": ROOT / "include/starfox/render/metal_native_material_coverage.inc",
        "@STARFOX_METAL_MATERIAL_COLOUR_SOURCE@": ROOT / "include/starfox/render/metal_material_colour.inc",
        "@STARFOX_METAL_NATIVE_COLOUR_SOURCE@": ROOT / "include/starfox/render/metal_native_material_colour.inc"})
    for name, text in (("metal_water_shared.hpp", water), ("metal_native_material_shared.hpp", native)):
        (generated / name).write_text(text, encoding="utf-8")
    shaders = args.out / "runtime-sources"
    subprocess.run([sys.executable, str(ROOT / "tools/check_metal_rt_shaders.py"), "--emit-only", str(shaders)], check=True)
    observer.qualify = qualify
    # Separate exact runtime-RT recipe: existing source checker uses Metal3.0.
    # Preserve its language level and add the same strict O3/FP flags as the
    # original shader SDK recipe. No original complete25 program is changed.
    shader_flags = ["-std=metal3.0", "-O3", "-Werror", "-fno-fast-math", "-ffp-contract=off"]
    cpp_target = "x86_64-apple-macosx11.0" if args.sdk == "macosx" else "arm64-apple-ios15.0"
    shader_target = observer.SDK_TARGETS[args.sdk]
    operations, accepted = [], []
    receipt = {"scope": "Actual entire native shadow producer translation unit and complete runtime shadow/reflection shader compilation/link only; not GPU, game integration, full native reflection ABI/history/colour parity or frame-cost acceptance",
               "sdk": args.sdk, "sdk_root": sdk_root, "manifest_sha256": observer.digest(ROOT / "inputs.json"),
               "sources": manifest["Sources"], "cpp_target": cpp_target, "shader_target": shader_target,
               "shader_flags": shader_flags, "operations": operations, "accepted_shaders": accepted,
               "generated_sha256": {p.name: observer.digest(p) for p in generated.iterdir()},
               "runtime_source_sha256": {p.name: observer.digest(p) for p in shaders.iterdir()},
               "gpu_launched": False, "production_adopted": False, "success": False}
    (args.out / "source-controls.json").write_text(json.dumps(controls, indent=2), encoding="utf-8")
    try:
        obj = args.out / "metal_hardware_rt.o"
        operations.append(observer.operate("producer-compile", tools["clang"],
            ["-x", "objective-c++", "-std=c++20", "-O3", "-DNDEBUG", "-Werror", "-fno-fast-math", "-ffp-contract=off",
             "-fobjc-arc", "-target", cpp_target, "-isysroot", sdk_root,
             "-I", str(ROOT / "include"), "-I", str(ROOT / "headers"), "-I", str(generated),
             "-c", str(ROOT / "src/render/metal_hardware_rt.mm"), "-o", str(obj)], args.out, manifest, library, environment))
        receipt["producer_object_sha256"] = observer.digest(obj)
        for name, entry in (("shadow", "starfox_hardware_shadow"), ("reflection", "starfox_hardware_reflection")):
            air, binary = args.out / f"{name}.air", args.out / f"{name}.metallib"
            operations.append(observer.operate(f"{name}-compile", tools["metal"],
                [*shader_flags, "-target", shader_target, "-isysroot", sdk_root,
                 "-c", str(shaders / f"{name}.metal"), "-o", str(air)], args.out, manifest, library, environment))
            operations.append(observer.operate(f"{name}-link", tools["metallib"], [str(air), "-o", str(binary)],
                args.out, manifest, library, environment))
            observer.qualify_metallib(binary.read_bytes(), entry)
            if re.search(r"ignoring (?:file|input)|warning:", (args.out / f"{name}-link.stderr.log").read_text(), re.I):
                raise RuntimeError("Actual linker warning/ignored runtime shader input")
            accepted.append({"name": name, "entry": entry, "air_sha256": observer.digest(air),
                             "metallib_sha256": observer.digest(binary), "bytes": binary.stat().st_size})
        qualify(manifest)
        receipt["success"] = len(operations) == 5 and len(accepted) == 2
    except Exception as error:
        receipt["error"] = str(error)
        raise
    finally:
        (args.out / "producer-receipt.json").write_text(json.dumps(receipt, indent=2), encoding="utf-8")
    print(f"Actual {args.sdk}: producer ObjC++ and both complete runtime shader libraries built; no GPU/game/performance acceptance")


if __name__ == "__main__":
    main()
