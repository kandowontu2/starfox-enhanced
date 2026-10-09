"""Real pinned-core RAM differential and actual native code-generation check.

Host streams are compared completely, then discarded. No assets, instruction
trace or giant state file is stored. Native assembly is not a FPS claim.
"""
import argparse
import hashlib
import json
import re
import shlex
import subprocess
import tempfile
from pathlib import Path


def inspect(build: Path, native: bool) -> None:
    root = Path(__file__).resolve().parent.parent
    entries = json.loads((build / 'compile_commands.json').read_text())
    entries = [item for item in entries if Path(item['file']).name == 'SNES_SPC_3ds_timers.cpp']
    if len(entries) != 1:
        raise RuntimeError('Expected exactly one real SPC CPU compilation')
    entry = entries[0]
    args = entry.get('arguments') or shlex.split(entry['command'])
    source = Path(entry['file'])
    generated = source.parent
    cache = (build / 'CMakeCache.txt').read_text()
    match = re.search(r'^(?:FETCHCONTENT_SOURCE_DIR_SNES_SPC:PATH|snes_spc_SOURCE_DIR:STATIC)=(.+)$', cache, re.M)
    dependency = Path(match[1]) if match else build / '_deps' / 'snes_spc-src'
    original = dependency / 'snes_spc' / 'SPC_CPU.h'
    before = original.read_text()
    after = (generated / 'SPC_CPU.h').read_text()
    prefix = '#include "starfox/platform/nintendo_3ds/spc_ram.hpp"\n'
    start = '#if SPC_MORE_ACCURACY || defined(STARFOX_3DS_SPC_RAM_DISABLE)'
    end = '\n#endif\n\n#if SPC_MORE_ACCURACY'
    if not after.startswith(prefix) or after.count(start) != 1:
        raise RuntimeError('Private RAM header extension is missing or duplicated')
    begin = after.index(start)
    finish = after.index(end, begin) + len('\n#endif')
    old_begin = before.index('#define CPU_READ( time, offset, addr )')
    old_finish = before.index('\n\n#if SPC_MORE_ACCURACY', old_begin)
    restored = (after[:begin] + before[old_begin:old_finish] + after[finish:])[len(prefix):]
    if restored != before:
        raise RuntimeError('Private RAM header changed more than its two dispatch macros')
    with tempfile.TemporaryDirectory(prefix='starfox-spc-ram-', dir=build.resolve()) as temporary:
        temporary = Path(temporary)
        baseline_dir = temporary / 'baseline'
        baseline_dir.mkdir()
        (baseline_dir / 'SPC_CPU.h').write_text(before)
        baseline_source = baseline_dir / source.name
        baseline_source.write_text(source.read_text())
        if native:
            native_check(args, entry, baseline_source, temporary)
        else:
            # CMake's Windows command string uses backslashes; POSIX shlex is
            # only appropriate for the Linux native check. Use its real cache
            # compiler path for the portable host subprocess argument lists.
            compiler = re.search(r'^CMAKE_CXX_COMPILER:FILEPATH=(.+)$', cache, re.M)
            if not compiler:
                raise RuntimeError('Host compiler path is missing')
            host_check(root, compiler[1], source, baseline_source, generated, temporary)


def host_check(root: Path, compiler: str, source: Path, baseline: Path,
               generated: Path, temporary: Path, dsp: bool = False) -> None:
    base = [compiler, '-std=c++20', '-O2', '-DNDEBUG', '-I' + str(root / 'include'),
            '-isystem', str(generated)]
    subprocess.run(base + ['-Wall', '-Wextra', '-Wpedantic', '-Werror', '-c',
                   str(root / 'tests/nintendo_3ds_spc_ram_machine.cpp'),
                   '-o', str(temporary / 'strict-machine.o')], check=True)
    sources = ['SNES_SPC_misc.cpp', 'SNES_SPC_state.cpp',
               'SNES_SPC_3ds_timers.cpp' if dsp else 'SPC_DSP_3ds_output.cpp']
    results = []
    for accuracy, hooks in ((0, False), (0, True), (1, True)):
        flags = ['-DSPC_MORE_ACCURACY=' + str(accuracy)]
        if hooks:
            flags += ['-include', str(root / 'tests/3ds_spc_ram_hook.hpp')]
        common = []
        for name in sources:
            obj = temporary / (name + '.o')
            subprocess.run(base + flags + ['-c', str(generated / name), '-o', str(obj)], check=True)
            common.append(str(obj))
        executables = []
        for label, implementation in (('baseline', baseline), ('candidate', source)):
            exe = temporary / (label + '.exe')
            subprocess.run(base + flags + [str(root / 'tests/nintendo_3ds_spc_ram_machine.cpp'),
                           str(implementation)] + common + ['-o', str(exe)], check=True)
            executables.append(exe)
        with subprocess.Popen([str(executables[0])], stdout=subprocess.PIPE, stderr=subprocess.PIPE) as old, \
             subprocess.Popen([str(executables[1])], stdout=subprocess.PIPE, stderr=subprocess.PIPE) as new:
            digest, count = hashlib.sha256(), 0
            try:
                while True:
                    a, b = old.stdout.read(1024 * 1024), new.stdout.read(1024 * 1024)
                    if a != b:
                        raise RuntimeError(f'Actual SPC state/PCM differs near byte {count}; accuracy={accuracy} hooks={hooks}')
                    if not a:
                        break
                    digest.update(a)
                    count += len(a)
                messages = [item.stderr.read().decode(errors='replace').strip() for item in (old, new)]
                if old.wait() or new.wait() or messages[0] != messages[1]:
                    raise RuntimeError('Actual SPC machine/callback check failed: ' + repr(messages))
                if 'cases=192 records=4800' not in messages[0] or not count:
                    raise RuntimeError('Incomplete machine differential')
                if hooks and not all(re.search(field + r'=[1-9][0-9]*', messages[0]) for field in ('opcodes', 'ports', 'dsp')):
                    raise RuntimeError('Official opcode/port/DSP callback coverage is missing')
                results.append(dict(accuracy=accuracy, hooks=hooks, bytes=count,
                                    sha256=digest.hexdigest(), machine=messages[0]))
            finally:
                for process in (old, new):
                    if process.poll() is None:
                        process.kill()
                        process.wait()
    print(json.dumps(dict(status='PASS exact actual SPC CPU/DSP state, PCM, time cuts, restore and callback order',
                         subject='voice sample selection' if dsp else 'RAM dispatch',
                         results=results, scope='Host correctness, not native performance; no trace stored'), indent=2))


def native_check(args: list, entry: dict, baseline: Path, temporary: Path) -> None:
    required = {'-O2', '-march=armv6k', '-mtune=mpcore', '-mfloat-abi=hard'}
    if not required.issubset(args) or any(flag in args for flag in ('-Ofast', '-ffast-math', '-flto')):
        raise RuntimeError('Review actual ARM SPC compiler settings')
    directory = Path(entry['directory'])
    source = str(entry['file'])
    active = directory / args[args.index('-o') + 1]
    old = temporary / 'baseline.o'
    baseline_args = args.copy()
    baseline_args[baseline_args.index(source)] = str(baseline)
    baseline_args[baseline_args.index('-o') + 1] = str(old)
    subprocess.run(baseline_args, cwd=directory, check=True)
    compiler = Path(args[0])
    objdump, size = compiler.with_name('arm-none-eabi-objdump'), compiler.with_name('arm-none-eabi-size')
    symbol = '_ZN8SNES_SPC10run_until_Ei'
    results = []
    for label, path in (('baseline', old), ('candidate', active)):
        assembly = subprocess.run([str(objdump), '-dr', '--disassemble=' + symbol, str(path)],
                                  check=True, capture_output=True, text=True).stdout
        if '<' + symbol + '>:' not in assembly:
            raise RuntimeError('Actual ARM SPC run loop is missing')
        sizes = subprocess.run([str(size), str(path)], check=True, capture_output=True, text=True).stdout.splitlines()
        results.append(dict(label=label, text_bytes=int(sizes[-1].split()[0]),
                            ram_slow_read_calls=len(re.findall(r'R_ARM_CALL\s+_ZN8SNES_SPC8cpu_readEii', assembly)),
                            ram_slow_write_calls=len(re.findall(r'R_ARM_CALL\s+_ZN8SNES_SPC9cpu_writeEiii', assembly))))
    # Do not demand fewer static fallback calls: exceptional paths are retained
    # intentionally. Bound code growth and retain both real I/O implementations.
    if results[1]['text_bytes'] > results[0]['text_bytes'] * 1.5:
        raise RuntimeError('Native SPC RAM fast path grew code excessively')
    print(json.dumps(dict(status='PASS exact source boundary and actual ARM CPU objects', results=results,
                         scope='Code-generation/size evidence, not runtime speed or physical FPS'), indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('--native', action='store_true')
    args = parser.parse_args()
    inspect(args.build, args.native)
