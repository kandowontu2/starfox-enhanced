"""Verify the exact private DSP output edit, real-core parity and native objects.

This eliminates unused pure interpolation only, not clocks/voices or accuracy.
The whole state/PCM differential is streamed and discarded, without assets.
"""
import argparse
import json
import re
import shlex
import subprocess
import tempfile
from pathlib import Path
from check_3ds_spc_ram import host_check


def validate_output_source(source: Path) -> str:
    before = source.with_name('SPC_DSP_3ds_counters.cpp').read_text()
    anchor = '\t\tint output = interpolate( v );\n\t\t\n\t\t// Noise\n\t\tif ( m.t_non & v->vbit )\n\t\t\toutput = (int16_t) (m.noise * 2);'
    replacement = '\t\tint output = starfox::platform::nintendo_3ds::spc_voice_sample(\n\t\t\tv->env, (m.t_non & v->vbit) != 0, m.noise,\n\t\t\t[&]() { return interpolate( v ); });'
    expected = '#include "starfox/platform/nintendo_3ds/spc_output.hpp"\n' + before.replace(anchor, replacement)
    if before.count(anchor) != 1 or source.read_text() != expected:
        raise RuntimeError('Private DSP output changes exceed the pure sample-selection/include edit')
    return before


def inspect(build: Path, native: bool) -> None:
    entries = json.loads((build / 'compile_commands.json').read_text())
    active = [entry for entry in entries if Path(entry['file']).name == 'SPC_DSP_3ds_output.cpp']
    if len(active) != 1:
        raise RuntimeError('Expected exactly one real optimized DSP compilation')
    entry = active[0]
    source = Path(entry['file'])
    validate_output_source(source)
    args = entry.get('arguments') or shlex.split(entry['command'])
    with tempfile.TemporaryDirectory(prefix='starfox-spc-output-', dir=build.resolve()) as temporary:
        temporary = Path(temporary)
        if not native:
            cache = (build / 'CMakeCache.txt').read_text()
            compiler = re.search(r'^CMAKE_CXX_COMPILER:FILEPATH=(.+)$', cache, re.M)
            if not compiler:
                raise RuntimeError('Host compiler path is missing')
            host_check(Path(__file__).resolve().parent.parent, compiler[1], source,
                       source.with_name('SPC_DSP_3ds_counters.cpp'), source.parent, temporary, dsp=True)
            return
        required = {'-O2', '-march=armv6k', '-mtune=mpcore', '-mfloat-abi=hard', '-D__3DS__'}
        if not required.issubset(args) or any(flag in args for flag in ('-flto', '-Ofast', '-ffast-math')):
            raise RuntimeError('Review native DSP compiler flags')
        directory = Path(entry['directory'])
        old = temporary / 'baseline.o'
        baseline_args = args.copy()
        baseline_args[baseline_args.index(str(source))] = str(source.with_name('SPC_DSP_3ds_counters.cpp'))
        baseline_args[baseline_args.index('-o')+1] = str(old)
        subprocess.run(baseline_args, cwd=directory, check=True)
        objdump = Path(args[0]).with_name('arm-none-eabi-objdump')
        size_tool = Path(args[0]).with_name('arm-none-eabi-size')
        results = []
        for label, obj in (('baseline', old), ('candidate', directory / args[args.index('-o')+1])):
            text = subprocess.run([str(objdump), '-dr', str(obj)], check=True, capture_output=True, text=True).stdout
            sizes = subprocess.run([str(size_tool), str(obj)], check=True, capture_output=True, text=True).stdout.splitlines()
            results.append(dict(label=label, text_bytes=int(sizes[-1].split()[0]),
                                ssat=len(re.findall(r'\bssat\b', text)), umull=len(re.findall(r'\bumull\b', text))))
        if results[1]['text_bytes'] > results[0]['text_bytes'] * 1.5:
            raise RuntimeError('DSP output selection grew code excessively')
        probes = [item for item in entries if Path(item['file']).name == 'spc_saturation_probe.cpp'
                  and '-DSTARFOX_3DS_OUTPUT_CHECK=1' in item['command']]
        if len(probes) != 1:
            raise RuntimeError('Actual native output arithmetic probe compilation is missing')
        probe = probes[0]
        probe_args = probe.get('arguments') or shlex.split(probe['command'])
        probe_obj = Path(probe['directory']) / probe_args[probe_args.index('-o')+1]
        probe_asm = subprocess.run([str(objdump), '-dr', str(probe_obj)], check=True, capture_output=True, text=True).stdout
        if not re.search(r'R_ARM_CALL\s+\S*spc_output_probe_sample\S*', probe_asm):
            raise RuntimeError('Arithmetic probe lost the opaque actual selector call boundary')
        print(json.dumps(dict(status='PASS exact DSP selection source, actual ARM objects and opaque native probe calls',
                              results=results, scope='Compiler/source evidence, not native execution or whole-game/physical FPS'), indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('--native', action='store_true')
    args = parser.parse_args()
    inspect(args.build, args.native)
