"""Compile the actual complete reflection factories for both Apple targets.

No GPU launch, application adoption, device-support or frame-cost acceptance.
The separate existing Metal driver diagnostic is not restarted by this job.
"""
import argparse
import ctypes
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('held_payload', ROOT / 'tools/check_payload.py')
payload = importlib.util.module_from_spec(spec)
spec.loader.exec_module(payload)
embedding, sdk = payload.embedding, payload.sdk


def qualify_factories():
    edits = json.loads((ROOT / 'tools/factory-edits.json').read_text())
    original = ROOT / 'tools/original_reflection_factory.cpp.txt'
    if sdk.digest(original).upper() != edits['OriginalSha256']:
        raise RuntimeError('Original actual reflection factory bytes changed')
    source = (ROOT / 'src/render/gpu_calibrated_reflection_history.cpp').read_text()
    if len(re.findall(r'=create_reflection_compute_pipeline\(', source)) != 16:
        raise RuntimeError('Original full16 factory-site graph incomplete')
    restored = source
    for change in reversed(edits['Changes']):
        if change['newText'] not in restored:
            raise RuntimeError('Payload-only inverse source change missing')
        restored = restored.replace(change['newText'], change['oldText'])
    if restored.rstrip() != original.read_text().rstrip():
        raise RuntimeError('Original math/dispatch/resource/qualification code changed')
    return edits


def qualify_sdk_casts():
    manifest = json.loads((ROOT / 'tools/sdk-cast-edits.json').read_text())
    if {row['Path'] for row in manifest['Headers']} != {
            'include/starfox/render/shadow_scene.hpp', 'include/starfox/render/raster_commands.hpp',
            'include/starfox/render/framebuffer.hpp'}:
        raise RuntimeError('SDK conversion change file set differs')
    for row in manifest['Headers']:
        original = ROOT / 'tools/held_headers' / (Path(row['Path']).name + '.txt')
        if sdk.digest(original).upper() != row['OriginalSha256']:
            raise RuntimeError('Original header conversion bytes changed')
        restored = (ROOT / row['Path']).read_text()
        for change in reversed(row['Changes']):
            if restored.count(change['newText']) != 1:
                raise RuntimeError('Exact same-type explicit conversion missing/duplicated')
            restored = restored.replace(change['newText'], change['oldText'])
        if restored.rstrip() != original.read_text().rstrip():
            raise RuntimeError('SDK header changes exceed original target-type conversions')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', choices=('macosx', 'iphoneos'), required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    edits = qualify_factories()
    casts = qualify_sdk_casts()
    manifest = json.loads((ROOT / 'shaders.json').read_text())
    sdk.controls(manifest)
    rows = sdk.qualify(manifest)
    held = json.loads((ROOT / 'tools/embedding-inputs.json').read_text())
    if sdk.digest(ROOT / 'shaders.json') != embedding.MANIFEST_SHA or sdk.digest(ROOT / 'tools/embedding-inputs.json') != embedding.INPUTS_SHA:
        raise RuntimeError('Original full shader/resource/binary receipt manifest changed')
    payload.qualify_recipes(rows, held)
    if args.self_test:
        print('PASS exact inverse factory and3 explicit same-type header conversions; all16 sites/full25 original shader, bindings and workgroups; no native launch')
        return
    if sys.platform != 'darwin':
        raise RuntimeError('Actual Apple SDK host required')
    embedding.validate_results(args.sdk, rows, held, args.artifacts)
    run = json.loads(subprocess.check_output(['gh', 'api', f'repos/{held["Repository"]}/actions/runs/{embedding.SDK_RUN}'], text=True))
    if (run['head_sha'], run['status'], run['conclusion']) != (embedding.SDK_COMMIT, 'completed', 'success'):
        raise RuntimeError('Original SDK producer has not actually completed successfully')
    args.out.mkdir(parents=True, exist_ok=False)
    includes, header_count = payload.fetch_sdl(args.out)
    assembly, expected = embedding.generate(args.sdk, rows, held['Recipes'], args.artifacts, args.out)
    tool = Path(subprocess.check_output(['xcrun', '--sdk', args.sdk, '--find', 'clang'], text=True).strip()).resolve()
    sdk_root = subprocess.check_output(['xcrun', '--sdk', args.sdk, '--show-sdk-path'], text=True).strip()
    target, cpu = ('x86_64-apple-macosx11.0', 0x1000007) if args.sdk == 'macosx' else ('arm64-apple-ios15.0', 0x100000c)
    library = ctypes.CDLL('/usr/lib/libproc.dylib')
    library.proc_pidinfo.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_int]
    library.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
    if ctypes.sizeof(sdk.BsdInfo) != 136 or ctypes.sizeof(sdk.TaskInfo) != 96:
        raise RuntimeError('Original native identity/task ABI changed')
    operations, failure, object_pass = [], None, False
    try:
        obj = args.out / 'libraries.o'
        flags = ['-target', target, '-isysroot', sdk_root]
        operations.append(sdk.operate('embed-libraries', tool, [*flags, '-c', str(assembly), '-o', str(obj)], args.out, manifest, library, {'SDKROOT': sdk_root}))
        actual = embedding.native_section(obj.read_bytes(), cpu)
        if actual[:len(expected)] != expected or len(actual)-len(expected) not in range(16) or any(actual[len(expected):]):
            raise RuntimeError('Actual complete25 read-only native library bytes differ')
        object_pass = True
        common = [*flags, '-std=c++20', '-O3', '-fno-fast-math', '-ffp-contract=off',
                  '-Wall', '-Wextra', '-Wconversion', '-Werror', '-I'+str(ROOT / 'include'),
                  '-I'+str(includes), '-I'+str(args.out), '-DSTARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE=1',
                  '-DSTARFOX_REFLECTION_EMBEDDED_METAL=1', '-x', 'c++', '-c']
        for name, file in (('compile-actual-factories', 'gpu_calibrated_reflection_history.cpp'),
                           ('compile-program-provider', 'embedded_metal_reflection_programs.cpp')):
            arguments = [*common, str(ROOT / 'src/render' / file), '-o', str(args.out / (name+'.o'))]
            operations.append(sdk.operate(name, tool, arguments, args.out, manifest, library, {'SDKROOT': sdk_root}))
    except RuntimeError as error:
        failure = str(error)
    finally:
        for name in ('embed-libraries', 'compile-actual-factories', 'compile-program-provider'):
            path = args.out / f'{name}.terminal.json'
            if path.exists() and not any(op['name'] == name for op in operations):
                operations.append(json.loads(path.read_text()))
        report = {'Scope': 'Actual original full16 reflection factory sites with25 SDK programs, payload-only compile integration; no GPU, link/application/device/numerical/frame-cost acceptance',
                  'Sdk': args.sdk, 'Programs': 25, 'FactorySites': 16, 'OriginalSourceRun': embedding.SDK_RUN,
                  'OriginalSourceCommit': embedding.SDK_COMMIT, 'OriginalFactorySha256': edits['OriginalSha256'],
                  'CandidateFactorySha256': sdk.digest(ROOT / 'src/render/gpu_calibrated_reflection_history.cpp'),
                  'SdlArchiveSha256': payload.SDL_SHA, 'SdlGpuHeaderSha256': payload.SDL_GPU_SHA,
                  'PublicHeaderCount': header_count, 'NativeObjectAccepted': object_pass,
                  'ExplicitSameTypeHeaderFiles': len(casts['Headers']),
                  'ActualFactoryCompileAccepted': failure is None and len(operations) == 3,
                  'Operations': operations, 'Failure': failure, 'GpuLaunched': False, 'ProductionAdopted': False}
        (args.out / 'wiring-receipt.json').write_text(json.dumps(report, indent=2))
        print(json.dumps(report, indent=2))
    if failure:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
