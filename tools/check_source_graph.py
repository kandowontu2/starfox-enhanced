"""Configure the actual offline Apple CMake graph; do not launch GPU/build.

Actual SDK compiler identification occurs during CMake configuration. Full
shader compilation/linkage/runtime are separate gates, never inferred here.
"""
import argparse
import ctypes
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('held_link', ROOT / 'tools/check_link.py')
link = importlib.util.module_from_spec(spec)
spec.loader.exec_module(link)
sdk, payload = link.sdk, link.payload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', choices=('macosx', 'iphoneos'), required=True)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    manifest, rows, held = link.qualify_component()
    module = (ROOT / 'cmake/BuildMetalReflection.cmake').read_text()
    for required in ('STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE=1', 'STARFOX_REFLECTION_EMBEDDED_METAL=1',
                     '-fno-fast-math', '-ffp-contract=off', '-Wconversion', '-Werror',
                     'DEPENDS ${inputs} ${previous}', 'sdk-context.json', 'SDL3::SDL3'):
        if required not in module:
            raise RuntimeError('Missing original complete recipe/serialized build input: ' + required)
    if args.self_test:
        print('PASS complete original source/factory/recipe inputs; offline graph prepared, not native configuration')
        return
    if sys.platform != 'darwin':
        raise RuntimeError('Actual Apple SDK host required')
    args.out.mkdir(parents=True, exist_ok=False)
    payload.fetch_sdl(args.out)
    cmake = Path(shutil.which('cmake')).resolve(strict=True)
    ninja = Path(shutil.which('ninja')).resolve(strict=True)
    clang = Path(subprocess.check_output(['xcrun', '--sdk', args.sdk, '--find', 'clang'], text=True).strip()).absolute()
    clangxx = Path(subprocess.check_output(['xcrun', '--sdk', args.sdk, '--find', 'clang++'], text=True).strip()).absolute()
    sdk_root = subprocess.check_output(['xcrun', '--sdk', args.sdk, '--show-sdk-path'], text=True).strip()
    arch, minimum = ('x86_64', '11.0') if args.sdk == 'macosx' else ('arm64', '15.0')
    library = ctypes.CDLL('/usr/lib/libproc.dylib')
    library.proc_pidinfo.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_int]
    library.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
    if ctypes.sizeof(sdk.BsdInfo) != 136 or ctypes.sizeof(sdk.TaskInfo) != 96:
        raise RuntimeError('Original native observer ABI differs')
    original_pin = sdk.pin_sdk_executables
    cmake_digest = sdk.digest(cmake)
    def pin_graph_tool(tool):
        if tool == cmake:
            if sdk.digest(tool) != cmake_digest:
                raise RuntimeError('Prelaunch native CMake bytes changed')
            return {str(cmake): cmake_digest}
        return original_pin(tool)
    sdk.pin_sdk_executables = pin_graph_tool
    name, failure, operations = 'configure-offline-complete25', None, []
    build = args.out / 'build'
    try:
        configure = ['-S', str(ROOT / 'build-integration'), '-B', str(build), '-G', 'Ninja',
                     '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
                     '-DCMAKE_C_COMPILER='+str(clang), '-DCMAKE_CXX_COMPILER='+str(clangxx),
                     '-DCMAKE_OBJC_COMPILER='+str(clang), '-DCMAKE_OBJCXX_COMPILER='+str(clangxx),
                     '-DCMAKE_ASM_COMPILER='+str(clang), '-DCMAKE_MAKE_PROGRAM='+str(ninja),
                     '-DCMAKE_OSX_SYSROOT='+sdk_root, '-DCMAKE_OSX_ARCHITECTURES='+arch,
                     '-DCMAKE_OSX_DEPLOYMENT_TARGET='+minimum,
                     '-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO',
                     '-DSTARFOX_BUILD_SDK='+args.sdk,
                     '-DSTARFOX_SDL_ARCHIVE='+str(args.out / 'SDL3-3.4.14.tar.gz')]
        if args.sdk == 'iphoneos':
            configure.append('-DCMAKE_SYSTEM_NAME=iOS')
        operations.append(sdk.operate(name, cmake, configure, args.out, manifest, library, {'SDKROOT': sdk_root}))
        graph = (build / 'build.ninja').read_text()
        commands = re.findall(r'(?m)^  COMMAND = (.*--shader .*compile)\s*$', graph)
        if len(commands) != 25:
            raise RuntimeError('Actual native graph lacks all25 compiler commands')
        actual_names = re.findall(r'--shader ([a-z_]+) ', '\n'.join(commands))
        if len(actual_names) != 25 or set(actual_names) != sdk.NAMES:
            raise RuntimeError('Wrong/duplicated native graph shader scope')
        previous = None
        for row in rows:
            prefix = f'generated-metal/libraries/metal-{args.sdk}-{row["Name"]}/'
            candidates = [line for line in graph.splitlines() if line.startswith('build ' + prefix) and ': CUSTOM_COMMAND ' in line]
            if len(candidates) != 1 or (previous and previous not in candidates[0].split(': CUSTOM_COMMAND ', 1)[1]):
                raise RuntimeError('Actual native graph loses full serialized dependencies')
            previous = f'generated-metal/libraries/metal-{args.sdk}-{row["Name"]}/{row["Name"]}.metallib'
        compile_commands = json.loads((build / 'compile_commands.json').read_text())
        for filename in ('gpu_calibrated_reflection_history.cpp', 'embedded_metal_reflection_programs.cpp', 'check_link.cpp'):
            matching = [c for c in compile_commands if Path(c['file']).name == filename]
            if len(matching) != 1 or any(flag not in matching[0]['command'] for flag in (
                    '-O3', '-fno-fast-math', '-ffp-contract=off', '-Wconversion', '-Werror', '-arch ' + arch)):
                raise RuntimeError('Actual native component compile recipe changed')
        context = json.loads((build / 'generated-metal/sdk-context.json').read_text())
        if (context['sdk'], context['root']) != (args.sdk, sdk_root):
            raise RuntimeError('Actual graph configured a mismatched SDK context')
        shutil.copyfile(build / 'build.ninja', args.out / 'build.ninja')
        shutil.copyfile(build / 'compile_commands.json', args.out / 'compile_commands.json')
        shutil.copyfile(build / 'generated-metal/sdk-context.json', args.out / 'sdk-context.json')
    except Exception as error:
        failure = str(error)
    finally:
        terminal = args.out / (name + '.terminal.json')
        if not operations and terminal.exists():
            operations.append(json.loads(terminal.read_text()))
        receipt = {'Scope': 'Actual native CMake configure and complete25 offline build graph only; no full shader/component build, GPU, game or performance acceptance',
                   'Sdk': args.sdk, 'Programs': 25, 'Operations': operations, 'Failure': failure,
                   'CmakeSha256': cmake_digest, 'BuildLaunched': False, 'GpuLaunched': False,
                   'ConfigurationSucceeded': failure is None and len(operations) == 1}
        (args.out / 'source-graph-receipt.json').write_text(json.dumps(receipt, indent=2))
        print(json.dumps(receipt))
        sdk.pin_sdk_executables = original_pin
    if failure:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
