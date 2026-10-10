"""Compile one additional native material coverage kernel; never execute a GPU."""
import argparse
import copy
import ctypes
import json
from pathlib import Path
import re
import subprocess
import sys

import compile_metal as observer

ROOT = Path(__file__).resolve().parents[1]
ENTRY = "starfox_native_coverage_probe"
OBSERVER_SHA = "cc770ea2068978eb0eeb381dc3bbf8983b838c2563f2605678272146f632fbb4"
DECODER_SHA = "69d61c1055f123f1b750eaa95bc3063994f2dbe754316430b41533c8cbe83a86"
FILES = (
    ".gitattributes", ".github/workflows/portable-builds.yml", "LICENSE", "README.md",
    "fixtures/native_indexed_cutout_fixture.hpp",
    "fixtures/native_reflection_fixture.hpp",
    "fixtures/native_rgba_shadow_fixture.hpp",
    "fixtures/native_shadow_cutout_fixture.hpp",
    "fixtures/reflected_liquid_oracle.hpp",
    "include/starfox/render/lava_surface.hpp",
    "include/starfox/render/lava_surface.inc",
    "include/starfox/render/metal_native_material_coverage.inc",
    "tests/coverage.cpp", "tests/coverage.metal",
    "tools/compile_metal.py", "tools/compile_coverage.py",
)


def qualify_component(manifest):
    rows = manifest.get("Sources", [])
    names = [row.get("File") for row in rows]
    if len(rows) != len(FILES) or len(set(names)) != len(FILES) or set(names) != set(FILES):
        raise RuntimeError("Complete unique allowlisted coverage component sources required")
    if manifest.get("Entry") != ENTRY or manifest.get("Scope") != "one-additional-coverage-kernel":
        raise RuntimeError("Separate component scope or exact entry changed")
    for row in rows:
        path = ROOT / row["File"]
        if row.get("Sha256") != observer.digest(path) or row.get("Bytes") != path.stat().st_size:
            raise RuntimeError("Coverage component input bytes changed")
        text = path.read_bytes().decode("utf-8", errors="strict")
        if "\0" in text:
            raise RuntimeError("Non-source binary in allowlisted component")
    if observer.digest(ROOT / "tools/compile_metal.py") != OBSERVER_SHA:
        raise RuntimeError("Original native observer/resource recipe changed")
    if observer.digest(ROOT / "include/starfox/render/metal_native_material_coverage.inc") != DECODER_SHA:
        raise RuntimeError("Previously CPU-checked decoder changed")
    code = (ROOT / "tests/coverage.metal").read_text(encoding="utf-8")
    if not re.search(r"kernel\s+void\s+" + ENTRY + r"\(", code) or "if(id>=query_count) return;" not in code:
        raise RuntimeError("Exact kernel entry or query-count guard missing")
    # All quoted include paths in this component must resolve within the held
    # source inventory. No system/dependency discovery is treated as a source pin.
    held = {ROOT / name for name in FILES}
    for name in FILES:
        if not name.endswith((".hpp", ".inc", ".cpp", ".metal")):
            continue
        path = ROOT / name
        for quoted in re.findall(r'^\s*#\s*include\s*"([^"\n]+)"', path.read_text(encoding="utf-8"), re.M):
            local = (path.parent / quoted).resolve()
            public = (ROOT / "include" / quoted).resolve()
            fixture = (ROOT / "fixtures" / quoted).resolve()
            if local not in held and public not in held and fixture not in held:
                raise RuntimeError("Quoted include outside complete held component")
    return rows


def controls(manifest):
    qualify_component(manifest)
    negatives = []
    missing = copy.deepcopy(manifest)
    missing["Sources"].pop()
    negatives.append(missing)
    duplicate = copy.deepcopy(manifest)
    duplicate["Sources"][-1] = copy.deepcopy(duplicate["Sources"][0])
    negatives.append(duplicate)
    extra = copy.deepcopy(manifest)
    extra["Sources"].append({"File": "private.bin", "Sha256": "0" * 64, "Bytes": 1})
    negatives.append(extra)
    for key, value in (("File", "../private.metal"), ("Sha256", "0" * 64), ("Bytes", 1)):
        changed = copy.deepcopy(manifest)
        changed["Sources"][0][key] = value
        negatives.append(changed)
    for key, value in (("Entry", "absent_kernel"), ("Scope", "complete25")):
        changed = copy.deepcopy(manifest)
        changed[key] = value
        negatives.append(changed)
    for item in negatives:
        try:
            qualify_component(item)
        except RuntimeError:
            continue
        raise RuntimeError("Incomplete/changed/escaping source or overclaimed scope accepted")
    owner = {"pid": 42, "birth": [1, 2], "path": "/actual/compiler"}
    if not observer.same_identity(owner, dict(owner)) or observer.same_identity(owner, None):
        raise RuntimeError("Original exact native owner positive/null control failed")
    for key, value in (("pid", 43), ("birth", [1, 3]), ("path", "/other/compiler")):
        if observer.same_identity(owner, {**owner, key: value}):
            raise RuntimeError("Original owner-incarnation mismatch accepted")
    allowed = {"/actual/compiler": "held", "/verified/compiler": "held2"}
    versioned = {**owner, "path": "/verified/compiler"}
    if not observer.authorized_exec(owner, versioned, allowed):
        raise RuntimeError("Pinned same-incarnation SDK exec refused")
    for changed in ({**versioned, "pid": 43}, {**versioned, "birth": [1, 3]},
                    {**versioned, "path": "/unverified/compiler"}):
        if observer.authorized_exec(owner, changed, allowed):
            raise RuntimeError("Unverified SDK exec or reused incarnation accepted")
    for data in (b"", b"MTLB" + bytes(88), b"MTLB" + bytes(1024)):
        try:
            observer.qualify_metallib(data, ENTRY)
        except RuntimeError:
            continue
        raise RuntimeError("Empty or missing-entry metallib accepted")
    return {"scope": "complete source and pure ownership/library controls only; no native compiler or GPU",
            "sources": len(FILES), "source_negative_controls": len(negatives),
            "manifest_sha256": observer.digest(ROOT / "inputs.json"),
            "observer_sha256": OBSERVER_SHA, "decoder_sha256": DECODER_SHA,
            "native_launched": False, "gpu_launched": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", choices=("macosx", "iphoneos"))
    parser.add_argument("--out", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    manifest = json.loads((ROOT / "inputs.json").read_text(encoding="utf-8-sig"))
    source_controls = controls(manifest)
    if args.self_test:
        print(json.dumps(source_controls, sort_keys=True))
        return
    if sys.platform != "darwin" or not args.sdk or not args.out:
        raise RuntimeError("Actual Darwin SDK and unique output directory required")
    library = ctypes.CDLL("/usr/lib/libproc.dylib")
    library.proc_pidinfo.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_int]
    library.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
    if ctypes.sizeof(observer.BsdInfo) != 136 or ctypes.sizeof(observer.TaskInfo) != 96:
        raise RuntimeError("Unexpected original native Darwin identity/task ABI")
    metal = Path(subprocess.check_output(["xcrun", "--sdk", args.sdk, "--find", "metal"], text=True).strip()).absolute()
    metallib = Path(subprocess.check_output(["xcrun", "--sdk", args.sdk, "--find", "metallib"], text=True).strip()).absolute()
    sdk_root = subprocess.check_output(["xcrun", "--sdk", args.sdk, "--show-sdk-path"], text=True).strip()
    sdk_environment = {"SDKROOT": sdk_root}
    args.out.mkdir(parents=True, exist_ok=False)
    (args.out / "source-controls.json").write_text(json.dumps(source_controls, indent=2), encoding="utf-8")
    # This is an additional one-kernel component, NOT an abbreviated run of the
    # original 25-program producer. Keep operate()/flags/floors/caps unchanged;
    # substitute only complete-source qualification for this new component.
    observer.qualify = qualify_component
    air, binary = args.out / "coverage.air", args.out / "coverage.metallib"
    flags = observer.FLAGS[args.sdk]
    target_flags = ["-target", observer.SDK_TARGETS[args.sdk], "-isysroot", sdk_root]
    receipt = {"scope": "one additional coverage kernel, actual SDK compilation/link only; not original complete25 acceptance, GPU execution, ray traversal, game adoption or performance",
               "sdk": args.sdk, "sdk_root": sdk_root, "flags": flags,
               "target": observer.SDK_TARGETS[args.sdk], "entry": ENTRY,
               "manifest_sha256": observer.digest(ROOT / "inputs.json"),
               "sources": manifest["Sources"], "operations": [], "success": False,
               "gpu_launched": False, "production_adopted": False}
    try:
        receipt["operations"].append(observer.operate("coverage-compile", metal,
            [*flags, *target_flags, "-I", str(ROOT / "include"), "-c", str(ROOT / "tests/coverage.metal"), "-o", str(air)],
            args.out, manifest, library, sdk_environment))
        receipt["operations"].append(observer.operate("coverage-link", metallib,
            [str(air), "-o", str(binary)], args.out, manifest, library, sdk_environment))
        observer.qualify_metallib(binary.read_bytes(), ENTRY)
        if re.search(r"ignoring (?:file|input)|warning:", (args.out / "coverage-link.stderr.log").read_text(), re.I):
            raise RuntimeError("Actual linker warning/ignored input is not a pass")
        qualify_component(manifest)
        receipt.update(success=True, air_sha256=observer.digest(air),
                       metallib_sha256=observer.digest(binary), metallib_bytes=binary.stat().st_size)
    except Exception as error:
        receipt["error"] = str(error)
        raise
    finally:
        (args.out / "coverage-receipt.json").write_text(json.dumps(receipt, indent=2), encoding="utf-8")
    print(f"Actual {args.sdk}: coverage kernel compiled and linked; no GPU execution or production adoption")


if __name__ == "__main__":
    main()
