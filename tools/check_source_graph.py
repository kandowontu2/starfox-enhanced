"""Configure and optionally build the actual offline Apple CMake component.

Actual SDK compiler identification occurs during CMake configuration. The
explicit build mode compiles every shader from source and links the component;
neither mode launches a GPU or proves game/runtime/performance acceptance.
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
    parser.add_argument('--build', action='store_true')
    args = parser.parse_args()
    manifest, rows, held = link.qualify_component()
    module = (ROOT / 'cmake/BuildMetalReflection.cmake').read_text()
    for required in ('STARFOX_REFLECTION_SOURCE_INDEX_AVAILABLE=1', 'STARFOX_REFLECTION_EMBEDDED_METAL=1',
                     'STARFOX_REFLECTION_METAL_EXACT_WORKGROUP=1',
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
    operation_names = [name]
    if args.build:
        operation_names.append('build-offline-complete25-component')
        if args.sdk == 'macosx':
            operation_names.append('run-offline-native-link-consumer')
            operation_names.append('run-offline-native-metadata-consumer')
    graph_verified, payload_verified, native_consumer, metadata_consumer = False, False, False, False
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
        for filename in ('gpu_calibrated_reflection_history.cpp', 'embedded_metal_reflection_programs.cpp',
                         'check_link.cpp', 'check_metal_pipeline_metadata.cpp'):
            matching = [c for c in compile_commands if Path(c['file']).name == filename]
            if len(matching) != 1 or any(flag not in matching[0]['command'] for flag in (
                    '-O3', '-fno-fast-math', '-ffp-contract=off', '-Wconversion', '-Werror', '-arch ' + arch)):
                raise RuntimeError('Actual native component compile recipe changed')
            if filename in ('gpu_calibrated_reflection_history.cpp', 'embedded_metal_reflection_programs.cpp'):
                if '-DSTARFOX_REFLECTION_METAL_EXACT_WORKGROUP=1' not in matching[0]['command']:
                    raise RuntimeError('Offline source-built component lost the exact-workgroup opt-in')
        patched_sdl = build / '_deps/sdl3-src/src/gpu/metal/SDL_gpu_metal.m'
        if sdk.digest(patched_sdl) != '3798a2b37450d933c2c30a443fe5da530a6f17234b11da4b599f4fd1d5c2d440':
            raise RuntimeError('Offline graph did not apply the original verified real SDL constructor patch')
        context = json.loads((build / 'generated-metal/sdk-context.json').read_text())
        if (context['sdk'], context['root']) != (args.sdk, sdk_root):
            raise RuntimeError('Actual graph configured a mismatched SDK context')
        shutil.copyfile(build / 'build.ninja', args.out / 'build.ninja')
        shutil.copyfile(build / 'compile_commands.json', args.out / 'compile_commands.json')
        shutil.copyfile(build / 'generated-metal/sdk-context.json', args.out / 'sdk-context.json')
        shutil.copyfile(patched_sdl, args.out / 'patched-SDL_gpu_metal.m')
        graph_verified = True
        if args.build:
            operations.append(sdk.operate(operation_names[1], cmake,
                ['--build', str(build), '--target', 'starfox_metal_factory_link_check',
                 'starfox_metal_pipeline_metadata_check',
                 '--parallel', '1', '--verbose'], args.out, manifest, library, {'SDKROOT': sdk_root}))
            # Fresh, unique CMake build: all25 SDK compiler operations must
            # actually complete. No artifact download/old bytecode substitution.
            spec = importlib.util.spec_from_file_location('source_build_adapter', ROOT / 'tools/build_metal_bundle.py')
            adapter = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(adapter)
            embedded, source_rows, source_held = adapter.load_source(ROOT)
            libraries = build / 'generated-metal/libraries'
            evidence = args.out / 'compiler-evidence'
            evidence.mkdir()
            actual_dirs = {p.name for p in libraries.iterdir() if p.is_dir()}
            if actual_dirs != {f'metal-{args.sdk}-{r["Name"]}' for r in source_rows}:
                raise RuntimeError('Actual source-built complete25 SDK inventory differs')
            for row in source_rows:
                directory = libraries / f'metal-{args.sdk}-{row["Name"]}'
                adapter.verify_library(embedded, row, args.sdk, directory, context)
                shutil.copytree(directory, evidence / directory.name)
            expected_directory = args.out / 'expected-embedding'
            expected_directory.mkdir()
            _, expected = embedded.generate(args.sdk, source_rows, source_held['Recipes'], libraries, expected_directory)
            for filename in ('libraries.S', 'embedding_programs.hpp'):
                if (expected_directory / filename).read_bytes() != (build / 'generated-metal/bundle' / filename).read_bytes():
                    raise RuntimeError('Actual source-built complete25 embedding changed')
            objects = list(build.glob('CMakeFiles/starfox_metal_reflection_history.dir/**/libraries.S.o'))
            if len(objects) != 1:
                raise RuntimeError('One actual complete25 native embedding object required')
            cpu = 0x1000007 if args.sdk == 'macosx' else 0x100000c
            actual = embedded.native_section(objects[0].read_bytes(), cpu)
            if actual[:len(expected)] != expected or len(actual)-len(expected) not in range(16) or any(actual[len(expected):]):
                raise RuntimeError('Actual complete25 read-only native object bytes differ')
            executable = (build / ('starfox_metal_factory_link_check.app/starfox_metal_factory_link_check'
                if args.sdk == 'iphoneos' else 'starfox_metal_factory_link_check')).resolve(strict=True)
            constants, address = link.native_executable_constants(executable.read_bytes(), cpu)
            if constants.count(expected) != 1 or (address + constants.find(expected)) % 16:
                raise RuntimeError('Actual final native executable lacks the complete read-only aligned bundle')
            archive = build / 'libstarfox_metal_reflection_history.a'
            if objects[0].read_bytes() not in archive.read_bytes():
                raise RuntimeError('Actual component archive lacks the original native embedding object')
            shutil.copyfile(objects[0], args.out / 'component-libraries.o')
            shutil.copyfile(executable, args.out / 'native-link-consumer')
            shutil.copyfile(archive, args.out / 'libstarfox_metal_reflection_history.a')
            metadata_executable = (build / ('starfox_metal_pipeline_metadata_check.app/starfox_metal_pipeline_metadata_check'
                if args.sdk == 'iphoneos' else 'starfox_metal_pipeline_metadata_check')).resolve(strict=True)
            shutil.copyfile(metadata_executable, args.out / 'native-metadata-consumer')
            for candidate in (executable, metadata_executable):
                data = candidate.read_bytes()
                if b'Metal returned nil without NSError\0' not in data or b'starfox.gpu.compute.exact_threadgroup.v1\0' not in data:
                    raise RuntimeError('Actual source-built executable lacks the patched SDL constructor')
            payload_verified = True
            if args.sdk == 'macosx':
                # Authorize only this verified newly linked executable before
                # launch; original SDK launcher restrictions remain unchanged.
                executable_digest = sdk.digest(executable)
                def pin_consumer_tool(tool):
                    if tool == executable:
                        if sdk.digest(tool) != executable_digest:
                            raise RuntimeError('Prelaunch native consumer bytes changed')
                        return {str(executable): executable_digest}
                    return pin_graph_tool(tool)
                sdk.pin_sdk_executables = pin_consumer_tool
                operations.append(sdk.operate(operation_names[2], executable, [], args.out,
                    manifest, library, {'SDKROOT': sdk_root}))
                output = (args.out / (operation_names[2] + '.stdout.log')).read_text()
                if output.count('PAYLOAD_PASS ') != 25 or 'ALL25_SDL_PAYLOAD_SELECTION_PASS refusals=625;' not in output or 'ACTUAL_FACTORY_OWNER_LINK_PASS;' not in output:
                    raise RuntimeError('Actual native idle-owner/full25/refusal consumer incomplete')
                native_consumer = True
                metadata_digest = sdk.digest(metadata_executable)
                def pin_metadata_tool(tool):
                    if tool == metadata_executable:
                        if sdk.digest(tool) != metadata_digest:
                            raise RuntimeError('Prelaunch native metadata consumer bytes changed')
                        return {str(metadata_executable): metadata_digest}
                    return pin_consumer_tool(tool)
                sdk.pin_sdk_executables = pin_metadata_tool
                operations.append(sdk.operate(operation_names[3], metadata_executable, [], args.out,
                    manifest, library, {'SDKROOT': sdk_root}))
                metadata_output = (args.out / (operation_names[3] + '.stdout.log')).read_text()
                if metadata_output.count('METAL_EXACT_WORKGROUP_METADATA_PASS; real SDL properties; no GPU or pipeline created') != 1:
                    raise RuntimeError('Source-built real SDL metadata lifecycle/ownership checks incomplete')
                metadata_consumer = True
    except Exception as error:
        failure = str(error)
    finally:
        for operation_name in operation_names:
            terminal = args.out / (operation_name + '.terminal.json')
            if terminal.exists() and not any(o['name'] == operation_name for o in operations):
                operations.append(json.loads(terminal.read_text()))
        receipt = {'Scope': 'Actual native source-built complete25 shader/component/SDL link when explicitly requested; no GPU, application integration/adoption, numerical/frame-cost/physical/goal acceptance',
                   'Sdk': args.sdk, 'Programs': 25, 'Operations': operations, 'Failure': failure,
                   'CmakeSha256': cmake_digest, 'BuildRequested': args.build,
                   'BuildLaunched': any(o['name'] == 'build-offline-complete25-component' and o.get('native_identity') for o in operations),
                   'GpuLaunched': False, 'ProductionAdopted': False,
                   'ConfigurationSucceeded': graph_verified,
                   'FinalReadonlyPayloadBytesAccepted': payload_verified,
                   'NativeConsumerAccepted': native_consumer,
                   'ExactWorkgroupMetadataAccepted': metadata_consumer,
                   'BuildSucceeded': args.build and failure is None and payload_verified and len(operations) == len(operation_names)}
        (args.out / 'source-graph-receipt.json').write_text(json.dumps(receipt, indent=2))
        print(json.dumps(receipt))
        sdk.pin_sdk_executables = original_pin
    if failure:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
