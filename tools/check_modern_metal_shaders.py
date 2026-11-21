"""SDK-compile every Metal (MSL) payload the MODERN renderer loads on Apple.

MODERN's own shaders (tools/generate_modern_shaders.py) plus the upstream
portable compute-route shaders it shares with GPU FAST. This is shader
validation with Xcode's metal compiler, not a GPU or image test.
--emit-only writes the .metal files without compiling (works off Apple).
"""
import argparse
from pathlib import Path
import re
import subprocess
import sys
import tempfile

STANDARDS = {'macosx': 'macos-metal2.1', 'iphoneos': 'ios-metal2.1'}
MODERN = ('scene_sdl', 'layer_filter', 'ray_shadow')
PORTABLE = ('spans_portable', 'clip_continuous_portable', 'continuous_portable',
            'continuous_visibility_portable', 'bsp_portable')


def payloads(root):
    for name in MODERN:
        text = (root / f'src/modern/shaders/generated/{name}.hpp').read_text(encoding='utf-8')
        found = re.findall(r'msl_(\w+)_entry="(\w+)";\ninline constexpr char msl_\1\[\]=R"SFXMETAL\((.*?)\)SFXMETAL";',
                           text, re.S)
        if not found:
            raise RuntimeError(f'{name}: no Metal payloads; run tools/generate_modern_shaders.py')
        for stage, entry, source in found:
            yield f'{name}_{stage}', entry, source
    for name in PORTABLE:
        text = (root / f'src/render/shaders/generated/{name}.hpp').read_text(encoding='utf-8')
        found = re.findall(r'inline constexpr char metal\[\]=R"SFXMETAL\((.*?)\)SFXMETAL";', text, re.S)
        if len(found) != 1:
            raise RuntimeError(f'{name}: expected one Metal payload')
        yield name, 'main0', found[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', choices=tuple(STANDARDS), action='append',
                        help='Apple SDK to compile for (repeatable; default both)')
    parser.add_argument('--output-directory', type=Path)
    parser.add_argument('--emit-only', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    subprocess.run([sys.executable, str(root / 'tools/generate_modern_shaders.py'), '--check'], check=True)
    sdks = args.sdk or list(STANDARDS)
    failures = 0
    with tempfile.TemporaryDirectory(prefix='starfox-modern-metal-') as temporary:
        output = args.output_directory or Path(temporary)
        output.mkdir(parents=True, exist_ok=True)
        for name, entry, source in payloads(root):
            path = output / f'{name}.metal'
            path.write_text(source, encoding='utf-8', newline='\n')
            kinds = re.findall(r'^(?:vertex|fragment|kernel)\s+[\w:<>, ]+?\s+(\w+)\(', source, re.M)
            if kinds != [entry]:
                print(f'FAIL {name}: entry points {kinds}, loader uses {entry}')
                failures += 1
                continue
            if args.emit_only:
                print(f'emitted {path} (entry {entry}, {len(source)} bytes)')
                continue
            for sdk in sdks:
                air = output / f'{name}.{sdk}.air'
                command = ['xcrun', '--sdk', sdk, 'metal', f'-std={STANDARDS[sdk]}', '-c', str(path), '-o', str(air)]
                result = subprocess.run(command, capture_output=True, text=True)
                if result.returncode:
                    failures += 1
                    print(f'FAIL {name} ({sdk})\n{result.stderr}')
                else:
                    print(f'ok   {name} ({sdk}, entry {entry})')
    if failures:
        raise SystemExit(f'{failures} Metal shader check(s) failed')


if __name__ == '__main__':
    main()
