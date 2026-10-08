"""Inspect actual ARM timer code against the unchanged pinned CPU source.

This checks code generation, not emulator timing or physical-console FPS.
It neither links the baseline object nor changes the player's DSP/CPU settings.
"""
import argparse
import json
import re
import shlex
import subprocess
import tempfile
from pathlib import Path


def inspect(build: Path) -> None:
    entries = json.loads((build / 'compile_commands.json').read_text())
    entries = [entry for entry in entries if Path(entry['file']).name == 'SNES_SPC_3ds_timers.cpp']
    if len(entries) != 1:
        raise RuntimeError('Expected exactly one native optimized SPC CPU compilation')
    entry = entries[0]
    args = entry.get('arguments') or shlex.split(entry['command'])
    required = {'-O2', '-march=armv6k', '-mtune=mpcore', '-mfloat-abi=hard'}
    if not required.issubset(args) or any(arg in args for arg in ('-Ofast', '-ffast-math', '-flto')):
        raise RuntimeError('Review the native ARM timer compiler settings')
    directory = Path(entry['directory'])
    source = Path(entry['file'])
    objdump = Path(args[0]).with_name('arm-none-eabi-objdump')
    object_file = directory / args[args.index('-o') + 1]
    symbol = '_ZN8SNES_SPC10run_timer_EPNS_5TimerEi'

    def assembly(path: Path) -> str:
        result = subprocess.run([str(objdump), '-dr', '--disassemble=' + symbol, str(path)],
                                check=True, capture_output=True, text=True)
        if '<' + symbol + '>:' not in result.stdout:
            raise RuntimeError('Pinned timer symbol is missing from the ARM object')
        return result.stdout

    with tempfile.TemporaryDirectory(prefix='starfox-3ds-spc-timers-') as temporary:
        baseline = Path(temporary) / 'baseline.o'
        baseline_args = args.copy()
        baseline_args[baseline_args.index(str(source))] = str(source.with_name('SNES_SPC.cpp'))
        baseline_args[baseline_args.index('-o') + 1] = str(baseline)
        subprocess.run(baseline_args, cwd=directory, check=True)
        before, after = assembly(baseline), assembly(object_file)
    # O2 must lower the common runtime-selected divisors to exact constant
    # shifts. Generic divide remains present for restored nonstandard tempos.
    if not re.search(r'\basr\b[^\n]*#7\b', after) or not re.search(r'\basr\b[^\n]*#4\b', after):
        raise RuntimeError('Normal SPC timer prescalers still lack ARM shift paths')
    if '__aeabi_idiv' not in before or '__aeabi_idiv' not in after:
        raise RuntimeError('Review the pinned baseline/generic timer division fallback')
    print('Unmodified pinned SPC timer ARM code:\n' + before)
    print('Native optimized SPC timer ARM code:\n' + after)
    print('PASS actual ARM normal-prescaler shifts and original generic division fallback; not a console FPS claim')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    inspect(parser.parse_args().build)
