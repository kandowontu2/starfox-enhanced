"""Inspect the actual ARM DSP counter path against the prior saturation-only DSP.

These are source/compiler gates, not runtime correctness or physical FPS.
Host/native arithmetic and the complete source/SPC/PCM replay are separate.
"""
import argparse
import json
import re
import shlex
import subprocess
import tempfile
from pathlib import Path
from check_3ds_spc_output import validate_output_source


def inspect(build: Path) -> None:
    entries = json.loads((build / 'compile_commands.json').read_text())
    entries = [entry for entry in entries if Path(entry['file']).name == 'SPC_DSP_3ds_output.cpp']
    if len(entries) != 1:
        raise RuntimeError('Expected one actual native DSP counter compilation')
    entry = entries[0]
    args = entry.get('arguments') or shlex.split(entry['command'])
    required = {'-O2', '-march=armv6k', '-mtune=mpcore', '-mfloat-abi=hard', '-D__3DS__'}
    if not required.issubset(args) or any(arg in args for arg in ('-Ofast', '-ffast-math', '-flto')):
        raise RuntimeError('Review native counter compiler settings')
    source = Path(entry['file'])
    baseline_source = source.with_name('SPC_DSP_3ds_saturation.cpp')
    anchor = 'return ((unsigned) m.counter + counter_offsets [rate]) % counter_rates [rate];'
    replacement = 'return starfox::platform::nintendo_3ds::spc_dsp_counter_remainder((unsigned) m.counter + counter_offsets [rate], counter_rates [rate], starfox::platform::nintendo_3ds::spc_dsp_counter_reciprocals[rate]);'
    before = baseline_source.read_text()
    if before.count(anchor) != 1 or validate_output_source(source) != '#include "starfox/platform/nintendo_3ds/spc_counters.hpp"\n' + before.replace(anchor, replacement):
        raise RuntimeError('Native DSP counter changes exceed the single remainder/include transformation')
    # Confirm that reciprocal generation still matches the actual pinned table,
    # including its special never-firing first rate, before trusting codegen.
    rate_table = re.search(r'counter_rates\s*\[32\]\s*=\s*\{(.*?)\}', before, re.S)
    if not rate_table:
        raise RuntimeError('Missing pinned DSP rate table')
    contents = re.sub(r'//[^\n]*', '', rate_table[1]).replace('simple_counter_range + 1', '30721')
    rates = [int(value.strip()) for value in contents.split(',') if value.strip()]
    if rates != [30721, 2048, 1536, 1280, 1024, 768, 640, 512, 384, 320, 256, 192, 160, 128, 96, 80,
                 64, 48, 40, 32, 24, 20, 16, 12, 10, 8, 6, 5, 4, 3, 2, 1]:
        raise RuntimeError('Pinned DSP counter rates changed; review reciprocals')
    directory = Path(entry['directory'])
    objdump = Path(args[0]).with_name('arm-none-eabi-objdump')
    size_tool = Path(args[0]).with_name('arm-none-eabi-size')
    active = directory / args[args.index('-o') + 1]

    def assembly(path: Path) -> dict:
        text = subprocess.run([str(objdump), '-dr', str(path)], check=True, capture_output=True, text=True).stdout
        instructions = re.findall(r'^\s*[0-9a-f]+:\s+[0-9a-f]{8}\s+([a-z0-9]+)\s*(.*)$', text, re.M)
        if not instructions:
            raise RuntimeError('Native DSP object has no decoded ARM instructions')
        sizes = subprocess.run([str(size_tool), str(path)], check=True, capture_output=True, text=True).stdout.splitlines()
        return {'instructions': len(instructions), 'umull_count': sum(op.startswith('umull') for op, _ in instructions),
                'software_unsigned_divide_relocations': len(re.findall(r'R_ARM_CALL\s+__aeabi_uidiv\w*', text)),
                'text_bytes': int(sizes[-1].split()[0])}

    with tempfile.TemporaryDirectory(prefix='starfox-3ds-spc-counters-', dir=build.resolve()) as temporary:
        baseline = Path(temporary) / 'baseline.o'
        baseline_args = args.copy()
        baseline_args[baseline_args.index(str(source))] = str(baseline_source)
        baseline_args[baseline_args.index('-o') + 1] = str(baseline)
        subprocess.run(baseline_args, cwd=directory, check=True)
        old, new = assembly(baseline), assembly(active)
    if not old['software_unsigned_divide_relocations'] or new['software_unsigned_divide_relocations'] or not new['umull_count']:
        raise RuntimeError('Actual ARM DSP counter must replace software modulo with unsigned high products')
    print(json.dumps({'status': 'PASS source and actual ARM counter code-generation gates', 'baseline': old, 'candidate': new,
                      'scope': 'Compiler/object evidence only; no runtime correctness, whole-game cost or physical FPS claim'}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    inspect(parser.parse_args().build)
