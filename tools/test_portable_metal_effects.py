import unittest

from check_portable_metal_effects import BINDINGS, runtime_source


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


if __name__ == '__main__':
    unittest.main()
