"""Inspect actual ARM signed-saturation code against the pinned DSP source.

This proves source transformation and code generation, not native correctness
or speed. The separate ARM probe and source/PCM replay supply those other gates.
No baseline object is linked into the player; no full disassembly is uploaded.
"""
import argparse
import json
import re
import shlex
import subprocess
import tempfile
from pathlib import Path


def inspect(build: Path) -> None:
    entries=json.loads((build/'compile_commands.json').read_text())
    entries=[entry for entry in entries if Path(entry['file']).name=='SPC_DSP_3ds_counters.cpp']
    if len(entries)!=1:
        raise RuntimeError('Expected one actual native optimized DSP compilation')
    entry=entries[0]
    args=entry.get('arguments') or shlex.split(entry['command'])
    required={'-O2','-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-D__3DS__'}
    if not required.issubset(args) or any(arg in args for arg in ('-Ofast','-ffast-math','-flto')):
        raise RuntimeError('Review native signed-saturation compiler settings')
    directory=Path(entry['directory'])
    source=Path(entry['file'])
    pinned=source.with_name('SPC_DSP.cpp')
    before_source=pinned.read_text()
    anchor='#define CLAMP16( io )\\\n{\\\n\tif ( (int16_t) io != io )\\\n\t\tio = (io >> 31) ^ 0x7FFF;\\\n}'
    replacement='#define CLAMP16( io ) { io = starfox::platform::nintendo_3ds::spc_saturate16(io); }'
    prefix='#include "starfox/platform/nintendo_3ds/spc_saturation.hpp"\n'
    counter_anchor='return ((unsigned) m.counter + counter_offsets [rate]) % counter_rates [rate];'
    counter_replacement='return starfox::platform::nintendo_3ds::spc_dsp_counter_remainder((unsigned) m.counter + counter_offsets [rate], counter_rates [rate], starfox::platform::nintendo_3ds::spc_dsp_counter_reciprocals[rate]);'
    expected='#include "starfox/platform/nintendo_3ds/spc_counters.hpp"\n'+prefix+before_source.replace(anchor,replacement).replace(counter_anchor,counter_replacement)
    if before_source.count(anchor)!=1 or before_source.count(counter_anchor)!=1 or source.read_text()!=expected:
        raise RuntimeError('Private DSP changes exceed the clamp/counter remainder transformations')
    objdump=Path(args[0]).with_name('arm-none-eabi-objdump')
    size_tool=Path(args[0]).with_name('arm-none-eabi-size')
    active=directory/args[args.index('-o')+1]

    def assembly(path: Path) -> dict:
        result=subprocess.run([str(objdump),'-d',str(path)],check=True,capture_output=True,text=True)
        instructions=re.findall(r'^\s*[0-9a-f]+:\s+[0-9a-f]{8}\s+([a-z0-9]+)\s*(.*)$',result.stdout,re.M)
        if not instructions:
            raise RuntimeError('Native DSP object has no decoded ARM instructions')
        ssat=[operands for instruction,operands in instructions if instruction=='ssat']
        sizes=subprocess.run([str(size_tool),str(path)],check=True,capture_output=True,text=True).stdout.splitlines()
        text_bytes=int(sizes[-1].split()[0])
        return {'instructions':len(instructions),'ssat_count':len(ssat),
                'ssat_operands':ssat,'text_bytes':text_bytes}

    with tempfile.TemporaryDirectory(prefix='starfox-3ds-spc-saturation-',dir=build.resolve()) as temporary:
        baseline=Path(temporary)/'baseline.o'
        baseline_args=args.copy()
        baseline_args[baseline_args.index(str(source))]=str(pinned)
        baseline_args[baseline_args.index('-o')+1]=str(baseline)
        subprocess.run(baseline_args,cwd=directory,check=True)
        before,after=assembly(baseline),assembly(active)
    if not after['ssat_count'] or any('#16' not in operands for operands in after['ssat_operands']):
        raise RuntimeError('Actual native DSP lacks the requested signed 16-bit saturation')
    print(json.dumps({'status':'PASS source and actual ARM code-generation gates',
        'baseline':before,'candidate':after,'production_dsp_diff':'Exact CLAMP16 and counter remainder + two helper includes only',
        'scope':'Compiler/object component evidence, not runtime correctness, whole-game cost or physical-console FPS'},indent=2))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build',type=Path)
    inspect(parser.parse_args().build)
