import unittest
from pathlib import Path

from check_portable_metal_effects import BINDINGS, metal_compile_command, runtime_source


class EmbeddedMetalSourceTests(unittest.TestCase):
    def source(self):
        resources = ', '.join(f'{name} [[buffer({slot})]]' for name, slot in BINDINGS.items())
        return f'kernel void main0({resources}) {{}}'

    def header(self, source):
        return f'inline constexpr char metal[]=R"SFXMETAL({source})SFXMETAL";'

    def test_exact_payload(self):
        source = self.source()
        self.assertEqual(runtime_source(self.header(source)), source)

    def test_missing_payload(self):
        with self.assertRaisesRegex(RuntimeError, 'embedded SDL'):
            runtime_source('inline constexpr unsigned char spirv[]={1,2,3};')

    def test_duplicate_payload(self):
        with self.assertRaisesRegex(RuntimeError, 'embedded SDL'):
            runtime_source(self.header(self.source())*2)

    def test_wrong_setup_buffer(self):
        with self.assertRaisesRegex(RuntimeError, 'Metal buffer binding'):
            runtime_source(self.header(self.source().replace('setupPixels [[buffer(7)]]', 'setupPixels [[buffer(8)]]')))

    def test_missing_setup_buffer(self):
        with self.assertRaisesRegex(RuntimeError, 'Metal buffer binding'):
            runtime_source(self.header(self.source().replace(', setupPixels [[buffer(7)]]', '')))

    def test_missing_entry(self):
        with self.assertRaisesRegex(RuntimeError, 'entry point'):
            runtime_source(self.header(self.source().replace('main0', 'not_the_runtime_entry')))

    def test_duplicate_entry(self):
        with self.assertRaisesRegex(RuntimeError, 'entry point'):
            runtime_source(self.header(self.source()+'\nkernel void main0() {}'))


class MetalCompileInvocationTests(unittest.TestCase):
    def test_iphoneos_keeps_metal21_and_precise_math(self):
        source, air = Path('shader folder/input.metal'), Path('output folder/input.air')
        self.assertEqual(metal_compile_command('iphoneos', source, air),
                         ['xcrun', '--sdk', 'iphoneos', 'metal', '-std=ios-metal2.1',
                          '-fno-fast-math', '-c', str(source), '-o', str(air)])

    def test_macosx_keeps_metal21_and_precise_math(self):
        source, air = Path('shader folder/input.metal'), Path('output folder/input.air')
        self.assertEqual(metal_compile_command('macosx', source, air),
                         ['xcrun', '--sdk', 'macosx', 'metal', '-std=macos-metal2.1',
                          '-fno-fast-math', '-c', str(source), '-o', str(air)])

    def test_unknown_sdk_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Unsupported Apple SDK'):
            metal_compile_command('unverified-sdk', Path('input.metal'), Path('input.air'))


if __name__ == '__main__':
    unittest.main()
