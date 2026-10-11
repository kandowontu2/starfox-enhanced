#!/usr/bin/env python3
"""Inspect byte packing using an actual native_gpu compile command, not guessed flags.

The frozen baseline preserves the previous full/changed algorithms. This produces
assembly only; neither instruction counts nor a host benchmark prove console FPS.
"""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    if len(sys.argv) != 3:
        raise SystemExit("Usage: check_3ds_texture_pack.py NATIVE_BUILD OUTPUT")
    root = Path(__file__).resolve().parents[1]
    native = Path(sys.argv[1]).resolve()
    output = Path(sys.argv[2]).resolve()
    commands = json.loads((native / "compile_commands.json").read_text())
    units = [unit for unit in commands if Path(unit["file"]).resolve() == root / "platform/3ds/native_gpu.cpp"]
    if len(units) != 1:
        raise RuntimeError("Missing/duplicate actual native GPU compile command")
    unit = units[0]
    args = list(unit["arguments"]) if "arguments" in unit else shlex.split(unit["command"])
    if not Path(args[0]).name.startswith("arm-none-eabi-"):
        raise RuntimeError("Not the real ARM compiler")
    # The SDK supplies O2, then the already-accepted project graph appends O3.
    # GCC uses the final optimization flag. Preserve the entire actual command;
    # never strip/replace a prior flag or pretend the SDK default is effective.
    optimization = [arg for arg in args if arg.startswith("-O")]
    if not optimization or optimization[-1] != "-O3":
        raise RuntimeError("Native effective optimization differs from the accepted project O3 baseline")
    if any(arg.startswith("-flto") for arg in args) or not any(arg == "-mfloat-abi=hard" for arg in args):
        raise RuntimeError("Native IPO/ABI differs from the accepted baseline")
    output.mkdir(parents=True, exist_ok=True)
    probe = output / "texture-pack-probe.cpp"
    assembly = output / "texture-pack-probe.s"
    report = output / "texture-pack-compiler.json"
    disassembly = output / "texture-pack-native_gpu.asm"
    for path in (probe, assembly, report, disassembly):
        if path.exists():
            raise RuntimeError("Existing diagnostic output; no overwrite/retry")
    probe.write_text('''#include "starfox/platform/nintendo_3ds/pica_texture_update.hpp"
#include "3ds_texture_pack_baseline.inc"
using namespace starfox::platform::nintendo_3ds;
extern "C" __attribute__((noinline)) void texture_pack_candidate_rgba(std::uint8_t* out,const std::uint8_t* in) {
    detail::pica_store_abgr(out,in,4);
}
extern "C" __attribute__((noinline)) void texture_pack_baseline_rgba(std::uint8_t* out,const std::uint8_t* in) {
    out[0]=in[3];out[1]=in[2];out[2]=in[1];out[3]=in[0];
}
extern "C" __attribute__((noinline)) void texture_pack_candidate_full(PicaImage image,std::span<std::uint8_t> out) {
    pack_pica_texture(image,out);
}
extern "C" __attribute__((noinline)) void texture_pack_baseline_full(PicaImage image,std::span<std::uint8_t> out) {
    baseline_pack_pica_texture(image,out);
}
extern "C" __attribute__((noinline)) PicaTextureUploadRange texture_pack_candidate_changed(PicaTexturePacker& packer,
    PicaImage image,std::span<const std::uint8_t> previous,std::span<std::uint8_t> out) {
    return packer.pack(image,previous,out);
}
extern "C" __attribute__((noinline)) PicaTextureUploadRange texture_pack_baseline_changed(BaselinePicaTexturePacker& packer,
    PicaImage image,std::span<const std::uint8_t> previous,std::span<std::uint8_t> out) {
    return packer.pack(image,previous,out);
}
''')
    settings = [args[0]]
    object_path = None
    index = 1
    while index < len(args):
        arg = args[index]
        if arg in ("-o", "-MF", "-MT", "-MQ"):
            if index + 1 >= len(args):
                raise RuntimeError("Incomplete actual compiler output argument")
            if arg == "-o":
                object_path = (Path(unit["directory"]) / args[index + 1]).resolve()
            index += 2
        elif arg in ("-c", "-MD", "-MMD") or Path(arg).resolve() == Path(unit["file"]).resolve():
            index += 1
        else:
            settings.append(arg)
            index += 1
    if object_path is None or not object_path.is_file():
        raise RuntimeError("Actual compiled native GPU object missing")
    inspect_args = settings + ["-I" + str(root / "tests"), "-S", "-fverbose-asm", str(probe), "-o", str(assembly)]
    subprocess.run(inspect_args, cwd=unit["directory"], check=True)
    objdump = Path(args[0]).with_name("arm-none-eabi-objdump")
    with disassembly.open("w") as stream:
        subprocess.run([str(objdump), "-C", "-d", str(object_path)], stdout=stream, check=True)
    compiler_target = subprocess.run([args[0], *[a for a in settings[1:] if a.startswith(("-m", "-O"))],
                                      "-Q", "--help=target"], text=True, capture_output=True, check=True)
    (output / "texture-pack-target-options.txt").write_text(compiler_target.stdout + compiler_target.stderr)
    held = [root / "include/starfox/platform/nintendo_3ds/pica_frame.hpp",
            root / "include/starfox/platform/nintendo_3ds/pica_texture_update.hpp",
            root / "tests/3ds_texture_pack_baseline.inc", root / "tests/3ds_texture_pack_benchmark.cpp",
            root / "tests/nintendo_3ds_pica_tests.cpp", Path(__file__).resolve(),
            native / "compile_commands.json", object_path, probe, assembly, disassembly]
    report.write_text(json.dumps({"scope": "Exact ARM compiler assembly inspection only; not native runtime or FPS acceptance",
                                 "original_compile_unit": unit, "inspection_arguments": inspect_args,
                                 "optimization_flags_in_order": optimization, "effective_optimization": optimization[-1],
                                 "files_sha256": {str(p): digest(p) for p in held}}, indent=2) + "\n")
    print("PASS: actual native GPU object and same-settings byte/full/changed ARM packing assembly captured; no console FPS claim.")


if __name__ == "__main__":
    main()
