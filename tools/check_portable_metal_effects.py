"""SDK-compile the exact Metal sources embedded in the SDL effects renderer.

This is shader/build validation, not GPU image or physical-device acceptance.
--emit-only may run off Apple hardware; it never claims SDK compilation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from portable_shader_bindings import validate_metal_bindings


BINDINGS = {'Settings': 0, 'SceneSettings': 1, 'layerTags': 2, 'indexedPixels': 3,
            'surfaces': 4, 'shadowMask': 5, 'scalefxPixels': 6, 'setupPixels': 7}


def runtime_source(header):
    matches = re.findall(r'inline constexpr char metal\[\]=R"SFXMETAL\((.*?)\)SFXMETAL";', header, re.S)
    if len(matches) != 1:
        raise RuntimeError('Missing or duplicate embedded SDL Metal effects source')
    source = matches[0]
    validate_metal_bindings(source, BINDINGS)
    if len(re.findall(r'\bkernel\s+void\s+main0\(', source)) != 1:
        raise RuntimeError('Missing or duplicate actual SDL effects entry point')
    return source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', choices=('macosx', 'iphoneos'), default='iphoneos')
    parser.add_argument('--output-directory', type=Path)
    parser.add_argument('--emit-only', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    # Bind the generated payload to all the actual shared shader inputs first.
    subprocess.run([sys.executable, str(root / 'tools/generate_portable_effects.py'), '--check'], check=True)
    with tempfile.TemporaryDirectory(prefix='starfox-sdl-metal-') as temporary:
        output = args.output_directory or Path(temporary)
        output.mkdir(parents=True, exist_ok=True)
        records = []
        for variant in (0, 1):
            header = root / f'src/render/shaders/generated/effects_portable_{variant}.hpp'
            source = runtime_source(header.read_text(encoding='utf-8'))
            path = output / f'effects_{variant}.metal'
            path.write_text(source, encoding='utf-8', newline='\n')
            record = {'variant': variant, 'source_sha256': hashlib.sha256(source.encode()).hexdigest(),
                      'buffer_bindings': BINDINGS, 'sdk_compiled': False}
            if not args.emit_only:
                air = output / f'effects_{variant}.air'
                subprocess.run(['xcrun', '--sdk', args.sdk, 'metal', '-std=metal2.1',
                                '-fno-fast-math', '-c', str(path), '-o', str(air)], check=True)
                if not air.is_file() or air.stat().st_size == 0:
                    raise RuntimeError('Apple compiler did not produce an actual AIR program')
                record.update(sdk_compiled=True, air_sha256=hashlib.sha256(air.read_bytes()).hexdigest())
            records.append(record)
        report = {'scope': 'Exact embedded SDL effects source/SDK compilation; no GPU/device acceptance',
                  'sdk': args.sdk, 'programs': records}
        (output / 'portable-effects-compile.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
        print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
