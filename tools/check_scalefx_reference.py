"""Execute original ScaleFX GLSL math with an OpenGL interface adapter.

Requires moderngl/glcontext and fixtures exported by starfox_scalefx_check.
The reference checkout and fixtures are explicit; no network or port shader
is used. Only Vulkan uniform declarations and stage markers are adapted.
"""
import argparse
from pathlib import Path
import re
import struct
import moderngl

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('reference', type=Path)
parser.add_argument('fixtures', type=Path)
args = parser.parse_args()
ctx = moderngl.create_standalone_context(require=450, backend='egl')
print('OpenGL reference:', ctx.info['GL_RENDERER'])
vertices = ctx.buffer(struct.pack('24f', -1,-1,0,1,0,0, 1,-1,0,1,1,0,
                                -1,1,0,1,0,1, 1,1,0,1,1,1))
identity = ctx.buffer(struct.pack('16f', 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1))
identity.bind_to_uniform_block(0)
parameters = ctx.buffer(reserve=32)
parameters.bind_to_uniform_block(3)
programs = []
for index in range(5):
    source = (args.reference / f'scalefx-pass{index}.slang').read_text()
    source = source.replace('layout(push_constant)', 'layout(std140, binding = 3)')
    source = source.replace('layout(set = 0, binding = 0, std140)', 'layout(binding = 0, std140)')
    source = re.sub(r'^#pragma parameter.*$', '', source, flags=re.MULTILINE)
    common, stages = source.split('#pragma stage vertex')
    vertex, fragment = stages.split('#pragma stage fragment')
    program = ctx.program(vertex_shader=common+vertex, fragment_shader=common+fragment)
    vao = ctx.vertex_array(program, [(vertices, '4f 2f', 'Position', 'TexCoord')])
    programs.append(vao)

def texture(size, data=None, dtype='f1'):
    result = ctx.texture(size, 4, data, dtype=dtype)
    result.filter = (moderngl.NEAREST, moderngl.NEAREST)
    result.repeat_x = result.repeat_y = False
    return result

cases = sorted(args.fixtures.glob('*-input.rgba'))
if not cases:
    raise SystemExit('No reference fixtures')
failures = 0
for fixture in cases:
    width, height = map(int, fixture.name.split('-')[0].split('x'))
    parameters.write(struct.pack('8f', width,height,1/width,1/height,.5,1,0,0))
    original = texture((width,height), fixture.read_bytes())
    outputs = []
    for index, vao in enumerate(programs):
        # Pass 1's first parameter is threshold=.5; pass 3's is corners=1.
        parameters.write(struct.pack('8f', width,height,1/width,1/height,
                                     .5 if index == 1 else 1,1,0,0))
        size = (width*3,height*3) if index == 4 else (width,height)
        output = texture(size, dtype='f4' if index<2 else 'f1')
        framebuffer = ctx.framebuffer(color_attachments=[output])
        framebuffer.use()
        ctx.viewport = (0,0,*size)
        (outputs[-1] if outputs else original).use(location=1)
        if index == 2:
            outputs[0].use(location=2)
        if index == 4:
            original.use(location=2)
        vao.render(moderngl.TRIANGLE_STRIP)
        if index < 4:
            port_path = fixture.with_name(fixture.name.replace('-input.rgba',f'-pass{index}.f32'))
            if port_path.exists():
                port = struct.unpack(f'{width*height*4}f', port_path.read_bytes())
                raw = output.read(alignment=1)
                values = struct.unpack(f'{width*height*4}f', raw) if index<2 else [v/255 for v in raw]
                tolerance = 1e-5 if index<2 else 1/255
                differing = sum(abs(a-b)>tolerance for a,b in zip(port,values))
                if differing:
                    print(f'  pass {index}: {differing} differing components')
        outputs.append(output)
        framebuffer.release()
    actual = outputs[-1].read(alignment=1)
    expected = fixture.with_name(fixture.name.replace('-input.rgba','-port.rgba')).read_bytes()
    if len(actual) != len(expected):
        raise RuntimeError('Fixture dimensions mismatch')
    different = sum(actual[i:i+4] != expected[i:i+4] for i in range(0,len(actual),4))
    print(f'{fixture.stem}: {different} differing pixels / {len(actual)//4}')
    failures += different != 0
    for output in outputs:
        output.release()
    original.release()
raise SystemExit(1 if failures else 0)
