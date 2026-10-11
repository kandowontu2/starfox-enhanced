"""Compile actual SDL payload selection against every unchanged embedded library.

No GPU/pipeline/dispatch is launched. This is independent of the live Metal
driver diagnostic; it neither repeats nor cancels that existing operation.
"""
import argparse
import ctypes
import importlib.util
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import tarfile
import urllib.request

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('held_embedding', ROOT / 'tools/check_embedding.py')
embedding = importlib.util.module_from_spec(spec)
spec.loader.exec_module(embedding)
sdk = embedding.sdk
SDL_URL = 'https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-3.4.14.tar.gz'
SDL_SHA = '30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb'
SDL_GPU_SHA = '209f37565e01ec0231b4f97a4ca24d157efa33a6454753fcdc255fdb43f54553'


def qualify_recipes(rows, held):
    source = (ROOT / 'include/starfox/render/sdl_metal_reflection_payload.hpp').read_text()
    actual = re.findall(r'\{"([a-z_]+)","([a-z_]+)",(\d+),(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\}', source)
    expected = []
    for row in rows:
        u, r, w, samplers, threads = held['Recipes'][row['Name']]
        expected.append((row['Name'], row['Entry'], *map(str, (u, r, w, samplers, *threads))))
    if actual != expected or len(actual) != 25:
        raise RuntimeError('Full original factory recipe/entry/workgroup table differs')


def fetch_sdl(out):
    archive = out / 'SDL3-3.4.14.tar.gz'
    with urllib.request.urlopen(SDL_URL, timeout=60) as response, archive.open('xb') as stream:
        while block := response.read(1 << 20):
            stream.write(block)
    if sdk.digest(archive) != SDL_SHA:
        raise RuntimeError('Pinned original SDL source archive differs')
    target = out / 'sdl-include'
    count = 0
    with tarfile.open(archive, 'r:gz') as source:
        for member in source.getmembers():
            name = PurePosixPath(member.name)
            if name.parts[:3] != ('SDL3-3.4.14', 'include', 'SDL3'):
                continue
            if name.is_absolute() or '..' in name.parts or member.issym() or member.islnk():
                raise RuntimeError('Escaping/symlink SDL header path')
            if not member.isfile():
                continue
            if name.suffix != '.h':
                raise RuntimeError('Unexpected nonheader SDL public include')
            destination = target.joinpath(*name.parts[2:])
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(source.extractfile(member).read())
            count += 1
    if count < 20 or sdk.digest(target / 'SDL3/SDL_gpu.h') != SDL_GPU_SHA:
        raise RuntimeError('Actual original SDL GPU interface missing/changed')
    return target, count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', choices=('macosx', 'iphoneos'), required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    manifest = json.loads((ROOT / 'shaders.json').read_text())
    sdk.controls(manifest)
    rows = sdk.qualify(manifest)
    held = json.loads((ROOT / 'tools/embedding-inputs.json').read_text())
    if sdk.digest(ROOT / 'shaders.json') != embedding.MANIFEST_SHA or sdk.digest(ROOT / 'tools/embedding-inputs.json') != embedding.INPUTS_SHA:
        raise RuntimeError('Original full source/binary/receipt/recipe pins changed')
    qualify_recipes(rows, held)
    if args.self_test:
        print('PASS all25 actual original SDL factory recipes and shader source pins; no native launch')
        return
    if sys.platform != 'darwin':
        raise RuntimeError('Actual Apple SDK host required')
    embedding.validate_results(args.sdk, rows, held, args.artifacts)
    run = json.loads(subprocess.check_output(['gh', 'api', f'repos/{held["Repository"]}/actions/runs/{embedding.SDK_RUN}'], text=True))
    if (run['head_sha'], run['status'], run['conclusion']) != (embedding.SDK_COMMIT, 'completed', 'success'):
        raise RuntimeError('Actual original SDK producer is not complete success')
    args.out.mkdir(parents=True, exist_ok=False)
    includes, header_count = fetch_sdl(args.out)
    assembly, expected = embedding.generate(args.sdk, rows, held['Recipes'], args.artifacts, args.out)
    tool = Path(subprocess.check_output(['xcrun', '--sdk', args.sdk, '--find', 'clang'], text=True).strip()).resolve()
    sdk_root = subprocess.check_output(['xcrun', '--sdk', args.sdk, '--show-sdk-path'], text=True).strip()
    target, cpu = ('x86_64-apple-macosx11.0', 0x1000007) if args.sdk == 'macosx' else ('arm64-apple-ios15.0', 0x100000c)
    library = ctypes.CDLL('/usr/lib/libproc.dylib')
    library.proc_pidinfo.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_int]
    library.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
    if ctypes.sizeof(sdk.BsdInfo) != 136 or ctypes.sizeof(sdk.TaskInfo) != 96:
        raise RuntimeError('Original native identity/task ABI changed')
    environment, operations = {'SDKROOT': sdk_root}, []
    failure, object_pass, native_pass = None, False, False
    try:
        obj = args.out / 'libraries.o'
        flags = ['-target', target, '-isysroot', sdk_root]
        operations.append(sdk.operate('embed-libraries', tool, [*flags, '-c', str(assembly), '-o', str(obj)], args.out, manifest, library, environment))
        actual = embedding.native_section(obj.read_bytes(), cpu)
        if actual[:len(expected)] != expected or len(actual)-len(expected) not in range(16) or any(actual[len(expected):]):
            raise RuntimeError('Actual complete25 embedded native bytes differ')
        object_pass = True
        probe = args.out / ('payload-probe' if args.sdk == 'macosx' else 'payload-probe.o')
        arguments = [*flags, '-std=c++20', '-O3', '-fno-fast-math', '-ffp-contract=off',
                     '-Wall', '-Wextra', '-Wconversion', '-Werror', '-I'+str(ROOT / 'include'),
                     '-I'+str(includes), '-I'+str(args.out), '-x', 'c++', str(ROOT / 'tools/check_payload.cpp')]
        arguments += ['-x', 'none', str(obj), '-lc++', '-o', str(probe)] if args.sdk == 'macosx' else ['-c', '-o', str(probe)]
        operations.append(sdk.operate('compile-payload-probe', tool, arguments, args.out, manifest, library, environment))
        if args.sdk == 'macosx':
            original_pin = sdk.pin_sdk_executables
            sdk.pin_sdk_executables = lambda frontend: {str(probe.resolve()): sdk.digest(probe)} if frontend == probe else original_pin(frontend)
            operations.append(sdk.operate('run-payload-probe', probe, [], args.out, manifest, library, environment))
            text = (args.out / 'run-payload-probe.stdout.log').read_text()
            native_pass = text.count('PAYLOAD_PASS ') == 25 and 'ALL25_SDL_PAYLOAD_SELECTION_PASS refusals=625;' in text
            if not native_pass:
                raise RuntimeError('Actual all25 native descriptor and refusal results missing')
    except RuntimeError as error:
        failure = str(error)
    finally:
        for name in ('embed-libraries', 'compile-payload-probe', 'run-payload-probe'):
            path = args.out / f'{name}.terminal.json'
            if path.exists() and not any(op['name'] == name for op in operations):
                operations.append(json.loads(path.read_text()))
        report = {'Scope': 'Actual entire original SDL payload-family selection and native SDK compilation; no Metal device/pipeline/dispatch/numerical/frame-cost/production acceptance',
                  'Sdk': args.sdk, 'Programs': 25, 'OriginalSourceRun': embedding.SDK_RUN,
                  'OriginalSourceCommit': embedding.SDK_COMMIT, 'SdlArchiveSha256': SDL_SHA,
                  'SdlGpuHeaderSha256': SDL_GPU_SHA, 'PublicHeaderCount': header_count,
                  'NativeObjectAccepted': object_pass, 'NativePayloadSelectionAccepted': native_pass,
                  'Operations': operations, 'Failure': failure, 'GpuLaunched': False, 'ProductionAdopted': False}
        (args.out / 'payload-receipt.json').write_text(json.dumps(report, indent=2))
        print(json.dumps(report, indent=2))
    if failure:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
