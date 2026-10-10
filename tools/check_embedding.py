"""Embed the complete already-compiled SDK shader family and inspect native objects.

Source-only CI: no game assets, application publication or release. SDK bytecode,
original descriptor/workgroup recipes and strict compilation receipts must match.
Actual Metal pipeline loading, when a GPU exists, is a separate recorded result.
"""
import argparse
import ctypes
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SDK_RUN = 38054247676
SDK_COMMIT = '144fd04de703c87731240ba92c9d5fe917bbda99'
MANIFEST_SHA = '8b592f0e091df74062250313625b532e138863aaf1065eba254c6828cbd5cddc'
INPUTS_SHA = 'd6cce1f11865cb347ecce4fd103ce367c177cb5ea5d357cd25ac0ac0ddef23bd'
sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('held_native_sdk_observer', ROOT / 'tools/compile_metal.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)


def native_section(data, expected_cpu):
    magic, cpu, _, kind, commands, command_bytes, _, _ = struct.unpack_from('<IiiIIIII', data)
    if magic != 0xfeedfacf or cpu != expected_cpu or kind != 1 or not 1 <= commands <= 64:
        raise RuntimeError('Wrong native Mach-O object/header')
    if 32 + command_bytes > len(data):
        raise RuntimeError('Truncated native Mach-O command inventory')
    cursor, sections = 32, []
    for _ in range(commands):
        command, size = struct.unpack_from('<II', data, cursor)
        if size < 8 or cursor + size > 32 + command_bytes:
            raise RuntimeError('Invalid native Mach-O command extent')
        if command == 0x19:
            segment = struct.unpack_from('<II16sQQQQiiII', data, cursor)
            count = segment[-2]
            if size != 72 + 80 * count:
                raise RuntimeError('Incomplete native Mach-O section records')
            for index in range(count):
                section = struct.unpack_from('<16s16sQQIIIIIIII', data, cursor+72+80*index)
                if section[0].rstrip(b'\0') == b'__const' and section[1].rstrip(b'\0') == b'__TEXT':
                    length, offset, alignment, relocations = section[3], section[4], section[5], section[7]
                    if alignment != 4 or relocations or offset+length > len(data):
                        raise RuntimeError('Native payload alignment/relocation/extent changed')
                    sections.append(data[offset:offset+length])
        cursor += size
    if cursor != 32+command_bytes or len(sections) != 1:
        raise RuntimeError('One complete native read-only constant section required')
    return sections[0]


def generate(sdk_name, rows, recipes, libraries, out):
    source = ['.section __TEXT,__const']
    header = ['#pragma once', '#include <cstddef>', 'extern "C" {']
    table = []
    expected = bytearray()
    for row in rows:
        name, entry = row['Name'], row['Entry']
        path = libraries / f'metal-{sdk_name}-{name}' / f'{name}.metallib'
        literal = path.resolve().as_posix()
        if any(c in literal for c in ('"', '\n', '\r')):
            raise RuntimeError('Invalid compiler input path')
        symbol = 'sfe_embedding_' + name
        source.extend(('.balign 16,0', '.globl _' + symbol, '_' + symbol + ':', '.incbin "' + literal + '"'))
        header.append(f'extern const unsigned char {symbol}[{path.stat().st_size}];')
        u, r, w, samplers, threads = recipes[name]
        table.append('    {"'+name+'", "'+entry+'", '+symbol+', sizeof('+symbol+'), '+
                     ', '.join(map(str, (u, r, w, samplers, *threads)))+'},')
        expected.extend(bytes((-len(expected)) % 16))
        expected.extend(path.read_bytes())
    header.extend(('}', 'struct EmbeddingProgram { const char* name; const char* entry; const unsigned char* bytes; std::size_t size; unsigned uniforms, readonly_buffers, writable_buffers, samplers, x, y, z; };',
                   'inline constexpr EmbeddingProgram embedding_programs[] = {', *table, '};',
                   'static_assert(sizeof(embedding_programs)/sizeof(embedding_programs[0]) == 25);'))
    assembly = out / 'libraries.S'
    assembly.write_text('\n'.join(source)+'\n')
    (out / 'embedding_programs.hpp').write_text('\n'.join(header)+'\n')
    return assembly, bytes(expected)


def validate_results(sdk_name, rows, held, artifacts):
    names = {f'metal-{sdk_name}-{row["Name"]}' for row in rows}
    actual = {p.name for p in artifacts.iterdir() if p.is_dir()}
    if actual != names:
        raise RuntimeError('Exactly all25 selected SDK artifacts required')
    for row in rows:
        directory = artifacts / f'metal-{sdk_name}-{row["Name"]}'
        pin = held['Libraries'][sdk_name][row['Name']]
        receipt = json.loads((directory / 'receipt.json').read_text())
        if (sdk.digest(directory / 'receipt.json') != pin['ReceiptSha256'] or
                receipt['sdk'] != sdk_name or receipt['target'] != sdk.SDK_TARGETS[sdk_name] or
                receipt['flags'] != sdk.FLAGS[sdk_name] or receipt['manifest_sha256'] != MANIFEST_SHA or
                not receipt['success'] or receipt['failed'] or len(receipt['accepted']) != 1 or
                len(receipt['operations']) != 2 or receipt['full_source_inventory'] != 25):
            raise RuntimeError('Actual SDK receipt/profile/native operations changed')
        for suffix, operation in zip(('compile', 'link'), receipt['operations']):
            terminal = json.loads((directory / f'{row["Name"]}-{suffix}.terminal.json').read_text())
            if terminal != operation or terminal['exit_code'] or terminal['resource_stopped'] or terminal.get('monitor_failed'):
                raise RuntimeError('Failed/changed original SDK native terminal')
        binary = directory / f'{row["Name"]}.metallib'
        if sdk.digest(binary) != pin['LibrarySha256']:
            raise RuntimeError('Actual compiled kernel bytes changed')
        sdk.qualify_metallib(binary.read_bytes(), row['Entry'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', choices=('macosx', 'iphoneos'), required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    manifest = json.loads((ROOT / 'shaders.json').read_text())
    sdk.controls(manifest)
    if sdk.digest(ROOT / 'shaders.json') != MANIFEST_SHA:
        raise RuntimeError('Original complete25 source manifest changed')
    held = json.loads((ROOT / 'tools/embedding-inputs.json').read_text())
    if sdk.digest(ROOT / 'tools/embedding-inputs.json') != INPUTS_SHA:
        raise RuntimeError('Original all50 binary/receipt and full25 SDL recipe pins changed')
    if held['SourceRun'] != SDK_RUN or held['SourceCommit'] != SDK_COMMIT or set(held['Recipes']) != sdk.NAMES:
        raise RuntimeError('Original complete25 library/recipe producer changed')
    rows = sdk.qualify(manifest)
    if args.self_test:
        print('PASS original full25 source/recipe inventory; no native or GPU launch')
        return
    if sys.platform != 'darwin':
        raise RuntimeError('Actual Apple host required')
    validate_results(args.sdk, rows, held, args.artifacts)
    actual_run = json.loads(subprocess.check_output(['gh', 'api', f'repos/{held["Repository"]}/actions/runs/{SDK_RUN}'], text=True))
    if actual_run['head_sha'] != SDK_COMMIT or actual_run['status'] != 'completed' or actual_run['conclusion'] != 'success':
        raise RuntimeError('Actual original SDK producer is not terminal success')
    args.out.mkdir(parents=True, exist_ok=False)
    assembly, expected = generate(args.sdk, rows, held['Recipes'], args.artifacts, args.out)
    tool = Path(subprocess.check_output(['xcrun', '--sdk', args.sdk, '--find', 'clang'], text=True).strip()).resolve()
    sdk_root = subprocess.check_output(['xcrun', '--sdk', args.sdk, '--show-sdk-path'], text=True).strip()
    environment = {'SDKROOT': sdk_root}
    target, cpu = ('x86_64-apple-macosx11.0', 0x1000007) if args.sdk == 'macosx' else ('arm64-apple-ios15.0', 0x100000c)
    library = ctypes.CDLL('/usr/lib/libproc.dylib')
    library.proc_pidinfo.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_int]
    library.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
    if ctypes.sizeof(sdk.BsdInfo) != 136 or ctypes.sizeof(sdk.TaskInfo) != 96:
        raise RuntimeError('Unexpected native Darwin identity/task ABI')
    operations, failure, object_pass, runtime_pass = [], None, False, False
    try:
        obj = args.out / 'libraries.o'
        flags = ['-target', target, '-isysroot', sdk_root]
        operations.append(sdk.operate('embed-libraries', tool, [*flags, '-c', str(assembly), '-o', str(obj)],
                                      args.out, manifest, library, environment))
        data = native_section(obj.read_bytes(), cpu)
        if data[:len(expected)] != expected or len(data)-len(expected) not in range(16) or any(data[len(expected):]):
            raise RuntimeError('Native Mach-O payload bytes/order/padding differ')
        # Actual section-header refusal controls do not modify the native object.
        changed = bytearray(obj.read_bytes()); changed[:4] = bytes(4)
        try:
            native_section(changed, cpu)
        except RuntimeError:
            pass
        else:
            raise RuntimeError('Wrong native Mach-O header accepted')
        object_pass = True
        probe = args.out / ('probe' if args.sdk == 'macosx' else 'probe.o')
        probe_flags = [*flags, '-std=c++20', '-fobjc-arc', '-fblocks', '-Wall', '-Wextra', '-Werror', '-I'+str(args.out),
                       '-x', 'objective-c++', str(ROOT / 'tools/check_embedding.mm')]
        if args.sdk == 'macosx':
            probe_flags += ['-x', 'none', str(obj), '-lc++', '-framework', 'Foundation', '-framework', 'Metal', '-o', str(probe)]
        else:
            probe_flags += ['-c', '-o', str(probe)]
        operations.append(sdk.operate('compile-probe', tool, probe_flags, args.out, manifest, library, environment))
        if args.sdk == 'macosx':
            # Use the identical guarded native observer, pinning this reviewed
            # newly-built diagnostic executable rather than an Xcode frontend.
            original_pin = sdk.pin_sdk_executables
            sdk.pin_sdk_executables = lambda frontend: {str(probe.resolve()): sdk.digest(probe)} if frontend == probe else original_pin(frontend)
            operations.append(sdk.operate('load-all25-pipelines', probe, [], args.out, manifest, library, environment))
            text = (args.out / 'load-all25-pipelines.stdout.log').read_text()
            runtime_pass = (text.count('PIPELINE_BEGIN ') == 25 and text.count('PIPELINE_PASS ') == 25 and
                            'PIPELINE_SUMMARY attempted=25 libraries=25 kernels=25 passed=25 failed=0' in text and
                            'ALL25_NATIVE_METAL_PIPELINES_PASS' in text)
            if not runtime_pass:
                raise RuntimeError('Actual full25 Metal pipeline result missing')
    except RuntimeError as error:
        failure = str(error)
    finally:
        # A failed operate call still writes its actual native terminal. Retain it
        # in the summary as a failure, not as a successful or missing operation.
        runtime_terminal = args.out / 'load-all25-pipelines.terminal.json'
        if runtime_terminal.exists() and not any(op['name'] == 'load-all25-pipelines' for op in operations):
            operations.append(json.loads(runtime_terminal.read_text()))
        runtime_log = args.out / 'load-all25-pipelines.stdout.log'
        attempted = runtime_log.read_text().count('PIPELINE_BEGIN ') if runtime_log.exists() else 0
        report = {'Scope': 'Actual complete25 native Apple binary embedding and SDK probe compilation; Metal pipeline loading is separately reported, not dispatch/numerical/frame-cost/production acceptance',
                  'Sdk': args.sdk, 'SourceRun': SDK_RUN, 'SourceCommit': SDK_COMMIT, 'Programs': 25,
                  'NativeObjectAccepted': object_pass, 'Operations': operations, 'MetalPipelineLoadsAccepted': runtime_pass,
                  'MetalPipelineAttemptCount': attempted,
                  'Failure': failure, 'ProductionAdopted': False}
        (args.out / 'embedding-receipt.json').write_text(json.dumps(report, indent=2))
        print(json.dumps(report, indent=2))
    if failure:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
