"""Run the complete actual SDL/Metal constructor sweep on its own CI host.

The prior native build, exact read-only payloads, executable incarnation and
original resource monitor must qualify. No game data, draw/dispatch result,
frame-rate or physical-device acceptance follows from pipeline construction.
"""
import argparse
import ctypes
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('complete_native_link', ROOT / 'tools/check_link.py')
link = importlib.util.module_from_spec(spec)
spec.loader.exec_module(link)
sdk = link.sdk


def qualify(compiled):
    receipt = json.loads((compiled / 'link-receipt.json').read_text())
    if (receipt['Sdk'], receipt['Architecture'], receipt['Programs'], receipt['FactorySites'],
        receipt['BuildSucceeded'], receipt['FinalReadonlyPayloadBytesAccepted'],
        receipt['NativeConsumerAccepted'], receipt['SdlPipelineProbeCompiled'],
        receipt['SdlPipelineProbeExecuted'], receipt['Failure']) != (
            'macosx', 'x86_64', 30, 21, True, True, True, True, False, None):
        raise RuntimeError('Actual full native compile/link/consumer required before driver admission')
    for op in receipt['Operations']:
        actual = json.loads((compiled / (op['name'] + '.terminal.json')).read_text())
        if actual != op or actual['exit_code'] != 0 or actual.get('resource_stopped') or actual.get('monitor_failed'):
            raise RuntimeError('Original native compile/link/consumer did not actually retire successfully')
    probe = (compiled / 'native-sdl-pipeline-probe').resolve(strict=True)
    data = probe.read_bytes()
    for marker in (b'ALL30_ACTUAL_SDL_METAL_PROGRAMS_PASS;', b'ACTUAL_COMPOSITE_OWNER_INITIALIZATION_PASS',
                   b'ACTUAL_HISTORY_OWNER_INITIALIZATION_PASS'):
        if marker not in data:
            raise RuntimeError('Complete actual driver consumer is missing')
    constants, address = link.native_executable_constants(data, 0x1000007)
    for object_name in ('component-libraries.o', 'graphics-component-libraries.o'):
        expected = link.embedding.native_section((compiled / object_name).read_bytes(), 0x1000007)
        # Assembly may have zero padding at its tail; the actual ELF/Mach-O
        # object is compared as a whole, not a partial entry token.
        if constants.count(expected) != 1 or (address + constants.find(expected)) % 16:
            raise RuntimeError('Driver consumer lacks its full actual read-only native SDK bundle')
    # check_link.py intentionally makes a non-executable artifact copy. Run
    # the actual CMake-linked executable, not that copy with invented modes.
    native = (compiled / 'build/starfox_metal_sdl_pipeline_check').resolve(strict=True)
    if sdk.digest(native) != sdk.digest(probe):
        raise RuntimeError('Actual CMake-linked driver executable differs from validated artifact bytes')
    return native


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiled', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        # The actual prior artifact contains all25 history pipelines, but not
        # this new five-stage driver sweep. It must not be reused as full30
        # runtime evidence merely because the link consumer included graphics.
        try:
            qualify(args.compiled)
        except RuntimeError as error:
            if str(error) != 'Complete actual driver consumer is missing': raise
        else: raise RuntimeError('Old history-only GPU consumer admitted as complete30')
        print('PASS: actual previous history-only driver consumer rejected as full30; no native or GPU launch')
        return
    if sys.platform != 'darwin': raise RuntimeError('Actual Apple SDK host required')
    probe = qualify(args.compiled)
    args.out.mkdir(parents=True, exist_ok=False)
    # A dedicated CI job owns this probe; unrelated local 3DS/native owners are
    # on a different host. Do not overlap another copy of this GPU consumer.
    processes = subprocess.check_output(['/bin/ps', '-axo', 'pid=,command='], text=True)
    if any(str(probe) in line for line in processes.splitlines()):
        raise RuntimeError('An actual native driver consumer is already live on this CI host')
    manifest = json.loads((ROOT / 'shaders.json').read_text())
    sdk.controls(manifest)
    held = sdk.digest(probe)
    original_pin = sdk.pin_sdk_executables
    def pin_native(tool):
        if Path(tool) != probe: return original_pin(tool)
        if sdk.digest(probe) != held: raise RuntimeError('Native driver executable changed before launch')
        return {str(probe): held}
    sdk.pin_sdk_executables = pin_native
    library = ctypes.CDLL('/usr/lib/libproc.dylib')
    library.proc_pidinfo.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_int]
    library.proc_pidpath.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32]
    if ctypes.sizeof(sdk.BsdInfo) != 136 or ctypes.sizeof(sdk.TaskInfo) != 96:
        raise RuntimeError('Original actual native observer ABI changed')
    environment = {'SDKROOT': subprocess.check_output(['xcrun', '--sdk', 'macosx', '--show-sdk-path'], text=True).strip()}
    status, failure = 'unproven', None
    name = 'load-all30-actual-sdl-metal-programs'
    try:
        sdk.operate(name, probe, ['--run-gpu'], args.out, manifest, library, environment)
        text = (args.out / (name + '.stdout.log')).read_text()
        if (sum(line.startswith('SDL_PIPELINE_PASS ') for line in text.splitlines()) != 25 or
            text.count('SDL_GRAPHICS_SHADER_PASS ') != 5 or text.count('SDL_GRAPHICS_PIPELINE_PASS ') != 4 or
            'ALL30_ACTUAL_SDL_METAL_PROGRAMS_PASS;' not in text):
            raise RuntimeError('Actual full30 Metal constructor result incomplete')
        status = 'passed'
    except Exception as error:
        failure = str(error)
        terminal_path = args.out / (name + '.terminal.json')
        if terminal_path.exists():
            terminal = json.loads(terminal_path.read_text())
            log = (args.out / (name + '.stderr.log')).read_text()
            if terminal['exit_code'] == 77 and not terminal.get('resource_stopped') and not terminal.get('monitor_failed') and (
                'SDL_VIDEO_UNAVAILABLE:' in log or 'SDL_METAL_DEVICE_UNAVAILABLE:' in log):
                status = 'unavailable'
            else: status = 'failed'
        elif isinstance(error, sdk.Deferred): status = 'deferred-before-launch'
        else: status = 'admission-failed'
    finally:
        sdk.pin_sdk_executables = original_pin
        terminal_path = args.out / (name + '.terminal.json')
        terminal = json.loads(terminal_path.read_text()) if terminal_path.exists() else None
        report = {'Scope': 'Actual complete30 SDL/Metal constructor investigation only; no draw/dispatch, numerical, game/frame-cost or physical acceptance',
                  'Status': status, 'All30MetalConstructorsAccepted': status == 'passed',
                  'GpuAttempted': terminal is not None, 'ActualTerminal': terminal,
                  'ExecutableSha256': held, 'Failure': failure, 'ApplicationAdopted': False}
        (args.out / 'runtime-receipt.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        print(json.dumps({'Status': status, 'All30MetalConstructorsAccepted': status == 'passed', 'Failure': failure}))
    if status != 'passed': raise SystemExit(77 if status == 'unavailable' else 1)


if __name__ == '__main__': main()
