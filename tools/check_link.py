"""Build/link the entire real factory component against pinned native SDL.

No GPU device, shader pipeline, game asset or application release is launched.
The existing driver diagnostic is independent and is never restarted here.
"""
import argparse
import ctypes
import importlib.util
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('held_factory_wiring', ROOT / 'tools/check_wiring.py')
wiring = importlib.util.module_from_spec(spec)
spec.loader.exec_module(wiring)
payload, embedding, sdk = wiring.payload, wiring.embedding, wiring.sdk


def native_executable_constants(data, expected_cpu):
    magic, cpu, _, kind, count, extent, _, _ = struct.unpack_from('<IiiIIIII', data)
    if magic != 0xfeedfacf or cpu != expected_cpu or kind != 2 or not 1 <= count <= 128:
        raise RuntimeError('Wrong final executable Mach-O type/CPU')
    cursor, selected = 32, []
    for _ in range(count):
        command, size = struct.unpack_from('<II', data, cursor)
        if size < 8 or cursor+size > 32+extent or 32+extent > len(data):
            raise RuntimeError('Truncated final native command')
        if command == 0x19:
            segment = struct.unpack_from('<II16sQQQQiiII', data, cursor)
            if size != 72+80*segment[-2]:
                raise RuntimeError('Incomplete final segment section inventory')
            for n in range(segment[-2]):
                section = struct.unpack_from('<16s16sQQIIIIIIII', data, cursor+72+80*n)
                if section[0].rstrip(b'\0') == b'__const' and section[1].rstrip(b'\0') == b'__TEXT':
                    length, offset = section[3:5]
                    if segment[8] & 2 or section[7] or offset+length > len(data):
                        raise RuntimeError('Final SDK payload is writable, relocated or truncated')
                    selected.append((data[offset:offset+length], section[2]))
        cursor += size
    if cursor != 32+extent or len(selected) != 1:
        raise RuntimeError('One actual read-only executable constant section required')
    return selected[0]


def qualify_component():
    wiring.qualify_factories()
    wiring.qualify_sdk_casts()
    manifest = json.loads((ROOT / 'shaders.json').read_text())
    sdk.controls(manifest)
    rows = sdk.qualify(manifest)
    held = json.loads((ROOT / 'tools/embedding-inputs.json').read_text())
    if sdk.digest(ROOT / 'shaders.json') != embedding.MANIFEST_SHA or sdk.digest(ROOT / 'tools/embedding-inputs.json') != embedding.INPUTS_SHA:
        raise RuntimeError('Full original SDK source/recipe pins changed')
    payload.qualify_recipes(rows, held)
    module = (ROOT / 'cmake/EmbeddedMetalReflection.cmake').read_text()
    for required in ('gpu_calibrated_reflection_history.cpp', 'embedded_metal_reflection_programs.cpp',
                     'libraries.S', 'STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE=1',
                     'STARFOX_REFLECTION_EMBEDDED_METAL=1', '-fno-fast-math', '-ffp-contract=off',
                     '-Wconversion', '-Werror', 'SDL3::SDL3'):
        if required not in module:
            raise RuntimeError('Actual native component input or strict recipe missing: '+required)
    consumer = (ROOT / 'tools/check_link.cpp').read_text()
    if 'const auto programs=programs_for_sdk();' not in consumer or 'GpuCalibratedReflectionHistory owner;' not in consumer:
        raise RuntimeError('Native link consumer bypasses real component/provider')
    # Native resource-lifetime control must not create a device/window or call a
    # different driver probe while the separate existing driver owner is live.
    if re.search(r'SDL_(Create|Init)|MTLCreate|newComputePipeline|newCommandQueue', consumer):
        raise RuntimeError('Link-only consumer unexpectedly launches SDL/Metal resources')
    return manifest, rows, held


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', choices=('macosx', 'iphoneos'), required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    manifest, rows, held = qualify_component()
    if args.self_test:
        print('PASS original full16 factories/25 variants/strict recipes; real component/provider consumer; no native/GPU launch')
        return
    if sys.platform != 'darwin':
        raise RuntimeError('Actual Apple SDK host required')
    embedding.validate_results(args.sdk, rows, held, args.artifacts)
    producer = json.loads(subprocess.check_output(['gh', 'api', f'repos/{held["Repository"]}/actions/runs/{embedding.SDK_RUN}'], text=True))
    if (producer['head_sha'], producer['status'], producer['conclusion']) != (embedding.SDK_COMMIT, 'completed', 'success'):
        raise RuntimeError('Original complete SDK producer is not actual successful terminal')
    args.out.mkdir(parents=True, exist_ok=False)
    includes, header_count = payload.fetch_sdl(args.out)
    del includes  # The CMake component builds real SDL from that exact archive.
    _, expected = embedding.generate(args.sdk, rows, held['Recipes'], args.artifacts, args.out)
    sdk_root = subprocess.check_output(['xcrun', '--sdk', args.sdk, '--show-sdk-path'], text=True).strip()
    clang = Path(subprocess.check_output(['xcrun', '--sdk', args.sdk, '--find', 'clang'], text=True).strip()).resolve()
    clangxx = Path(subprocess.check_output(['xcrun', '--sdk', args.sdk, '--find', 'clang++'], text=True).strip()).absolute()
    cmake = Path(shutil.which('cmake')).resolve(strict=True)
    ninja = Path(shutil.which('ninja')).resolve(strict=True)
    cpu, arch, minimum = (0x1000007, 'x86_64', '11.0') if args.sdk == 'macosx' else (0x100000c, 'arm64', '15.0')
    library = ctypes.CDLL('/usr/lib/libproc.dylib')
    library.proc_pidinfo.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_int]
    library.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
    if ctypes.sizeof(sdk.BsdInfo) != 136 or ctypes.sizeof(sdk.TaskInfo) != 96:
        raise RuntimeError('Original native observer ABI differs')
    # A CMake root and the later compiled native consumer are NOT Xcode Metal
    # frontends. Pin these exact executables before launch, while reusing the
    # unchanged original identity/tree/resource observer. No path discovered
    # after a PID transition is authorized, and SDK tools retain their original
    # stricter toolchain launcher policy.
    original_pin = sdk.pin_sdk_executables
    explicit_tools = {str(cmake): sdk.digest(cmake)}
    def pin_component_tool(tool):
        key = str(tool)
        if key not in explicit_tools:
            return original_pin(tool)
        if sdk.digest(tool) != explicit_tools[key]:
            raise RuntimeError('Prelaunch-pinned native build/consumer tool changed')
        return {key: explicit_tools[key]}
    sdk.pin_sdk_executables = pin_component_tool
    environment = {'SDKROOT': sdk_root}
    operations, failure, byte_pass, native_pass = [], None, False, False
    build = args.out / 'build'
    names = ['configure-native-component', 'build-native-component']
    if args.sdk == 'macosx':
        names.append('run-native-link-consumer')
    try:
        configure = ['-S', str(ROOT), '-B', str(build), '-G', 'Ninja',
                     '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
                     '-DCMAKE_C_COMPILER='+str(clang), '-DCMAKE_CXX_COMPILER='+str(clangxx),
                     '-DCMAKE_OBJC_COMPILER='+str(clang), '-DCMAKE_OBJCXX_COMPILER='+str(clangxx),
                     '-DCMAKE_ASM_COMPILER='+str(clang), '-DCMAKE_MAKE_PROGRAM='+str(ninja),
                     '-DCMAKE_OSX_SYSROOT='+sdk_root, '-DCMAKE_OSX_ARCHITECTURES='+arch,
                     '-DCMAKE_OSX_DEPLOYMENT_TARGET='+minimum,
                     '-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO',
                     '-DSTARFOX_EMBEDDING_DIRECTORY='+str(args.out),
                     '-DSTARFOX_SDL_ARCHIVE='+str(args.out / 'SDL3-3.4.14.tar.gz')]
        if args.sdk == 'iphoneos':
            configure.append('-DCMAKE_SYSTEM_NAME=iOS')
        operations.append(sdk.operate(names[0], cmake, configure, args.out, manifest, library, environment))
        operations.append(sdk.operate(names[1], cmake, ['--build', str(build), '--target',
            'starfox_metal_factory_link_check', '--parallel', '1', '--verbose'], args.out, manifest, library, environment))
        commands = json.loads((build / 'compile_commands.json').read_text())
        for file in ('gpu_calibrated_reflection_history.cpp', 'embedded_metal_reflection_programs.cpp', 'check_link.cpp'):
            selected = [c for c in commands if Path(c['file']).name == file]
            if len(selected) != 1 or any(flag not in selected[0]['command'] for flag in
                    ('-O3', '-fno-fast-math', '-ffp-contract=off', '-Wall', '-Wextra', '-Wconversion', '-Werror', '-arch '+arch)):
                raise RuntimeError('Actual native CMake strict compile recipe differs for '+file)
            if file != 'check_link.cpp' and any(flag not in selected[0]['command'] for flag in
                    ('-DSTARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE=1', '-DSTARFOX_REFLECTION_EMBEDDED_METAL=1')):
                raise RuntimeError('Actual native factory graph macro missing')
        shutil.copyfile(build / 'compile_commands.json', args.out / 'compile_commands.json')
        assembly_objects = list(build.glob('CMakeFiles/starfox_metal_reflection_history.dir/**/libraries.S.o'))
        if len(assembly_objects) != 1:
            raise RuntimeError('Exactly one real CMake complete25 embedding object required')
        obj = args.out / 'component-libraries.o'
        shutil.copyfile(assembly_objects[0], obj)
        actual = embedding.native_section(obj.read_bytes(), cpu)
        if actual[:len(expected)] != expected or len(actual)-len(expected) not in range(16) or any(actual[len(expected):]):
            raise RuntimeError('Actual CMake native read-only all25 SDK payload bytes differ')
        executable = (build / ('starfox_metal_factory_link_check.app/starfox_metal_factory_link_check'
            if args.sdk == 'iphoneos' else 'starfox_metal_factory_link_check')).resolve(strict=True)
        final, address = native_executable_constants(executable.read_bytes(), cpu)
        if final.count(expected) != 1 or (address+final.find(expected)) % 16:
            raise RuntimeError('Final native executable lacks the one original complete read-only SDK bundle')
        byte_pass = True
        shutil.copyfile(executable, args.out / 'native-link-consumer')
        shutil.copyfile(build / 'libstarfox_metal_reflection_history.a', args.out / 'libstarfox_metal_reflection_history.a')
        if args.sdk == 'macosx':
            explicit_tools[str(executable)] = sdk.digest(executable)
            operations.append(sdk.operate(names[2], executable, [], args.out, manifest, library, environment))
            output = (args.out / (names[2]+'.stdout.log')).read_text()
            if output.count('PAYLOAD_PASS ') != 25 or 'ALL25_SDL_PAYLOAD_SELECTION_PASS refusals=625;' not in output or 'ACTUAL_FACTORY_OWNER_LINK_PASS;' not in output:
                raise RuntimeError('Actual native component owner/full25/refusal consumer incomplete')
            native_pass = True
    except Exception as error:
        failure = str(error)
    finally:
        for name in names:
            terminal = args.out / (name+'.terminal.json')
            if terminal.exists() and not any(op['name'] == name for op in operations):
                operations.append(json.loads(terminal.read_text()))
        report = {'Scope': 'Actual native CMake static component and real SDL link; no GPU, application integration/adoption, numerical/frame-cost/physical/release acceptance',
                  'Sdk': args.sdk, 'Architecture': arch, 'Minimum': minimum,
                  'Programs': 25, 'FactorySites': 16, 'SdlArchiveSha256': payload.SDL_SHA,
                  'OriginalSourceRun': embedding.SDK_RUN, 'OriginalSourceCommit': embedding.SDK_COMMIT,
                  'SdlGpuHeaderCount': header_count, 'CmakeSha256': explicit_tools[str(cmake)],
                  'NinjaSha256': sdk.digest(ninja), 'FinalReadonlyPayloadBytesAccepted': byte_pass,
                  'NativeConsumerAccepted': native_pass, 'Operations': operations, 'Failure': failure,
                  'GpuLaunched': False, 'ProductionAdopted': False,
                  'BuildSucceeded': failure is None and len(operations) == len(names)}
        (args.out / 'link-receipt.json').write_text(json.dumps(report, indent=2))
        print(json.dumps(report, indent=2))
        sdk.pin_sdk_executables = original_pin
    if failure:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
