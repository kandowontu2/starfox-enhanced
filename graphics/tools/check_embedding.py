"""Complete five-stage graphics source/recipe and read-only bundle generation.

The original strict SDK observer is unchanged. This helper creates native source
inputs only: it neither creates a device nor qualifies a live game renderer.
"""
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
MANIFEST_SHA = '745331e87be67dc6e8fb670eec2d51aa91d8139ee7dc3fcb0c8a3467c4f2ef1b'
SDK_RUN = 38103454445
SDK_COMMIT = 'b9cf8e90b528347bcaede434b9248991afddf977'
# Set by the source preparation step after the complete recipe is constructed.
INPUTS_SHA = 'adb4a7fdfe13a8c94a0e56bba0c32958472cb97a1730bafc2c53e42d95961541'
spec = importlib.util.spec_from_file_location('graphics_sdk_observer', ROOT / 'tools/compile_metal.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)


def generate(sdk_name, rows, recipes, libraries, out):
    if len(rows) != 5 or {row['Name'] for row in rows} != sdk.NAMES:
        raise RuntimeError('Exactly five original graphics stages required')
    assembly = ['.section __TEXT,__const']
    header = ['#pragma once', '#include <iterator>', '#include "starfox/render/sdl_metal_composite_payload.hpp"', 'extern "C" {']
    table, expected = [], bytearray()
    for row in rows:
        name = row['Name']
        stage = 'SDL_GPU_SHADERSTAGE_VERTEX' if row['Stage'] == 'vert' else 'SDL_GPU_SHADERSTAGE_FRAGMENT'
        recipe = [row['Stage'], row['Samplers'], row['ReadonlyBuffers'], row['Uniforms']]
        if recipes[name] != recipe:
            raise RuntimeError('Original graphics factory recipe differs')
        path = (libraries / f'metal-{sdk_name}-{name}' / f'{name}.metallib').resolve(strict=True)
        literal = path.as_posix()
        if any(c in literal for c in ('"', '\n', '\r')):
            raise RuntimeError('Unsafe native assembly input path')
        data = path.read_bytes()
        sdk.qualify_metallib(data, row['Entry'])
        symbol = 'sfe_metal_composite_' + name
        assembly.extend(('.balign 16,0', '.globl _' + symbol, '_' + symbol + ':', '.incbin "' + literal + '"'))
        header.append(f'extern const unsigned char {symbol}[{len(data)}];')
        table.append('    {"' + name + '", "' + row['Entry'] + '", ' + symbol + ', sizeof(' + symbol + '), ' +
                     stage + ', ' + ', '.join(map(str, recipe[1:])) + '},')
        expected.extend(bytes((-len(expected)) % 16))
        expected.extend(data)
    header.extend(('}', 'inline const starfox::render::metal_composite::Program graphics_embedding_programs[] = {',
                   *table, '};', 'static_assert(std::size(graphics_embedding_programs) == 5);'))
    (out / 'libraries.S').write_text('\n'.join(assembly) + '\n', encoding='utf-8')
    (out / 'embedding_programs.hpp').write_text('\n'.join(header) + '\n', encoding='utf-8')
    return out / 'libraries.S', bytes(expected)
