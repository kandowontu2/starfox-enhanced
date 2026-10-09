import unittest

from portable_shader_bindings import validate_metal_bindings


class MetalBindingsTests(unittest.TestCase):
    def test_explicit_slots(self):
        validate_metal_bindings('Settings [[buffer(0)]], commands [[buffer(5)]], masks [[buffer(6)]]',
                                {'Settings': 0, 'commands': 5, 'masks': 6})

    def test_duplicate_slot(self):
        with self.assertRaises(RuntimeError):
            validate_metal_bindings('commands [[buffer(5)]], masks [[buffer(5)]]')

    def test_duplicate_name(self):
        with self.assertRaises(RuntimeError):
            validate_metal_bindings('masks [[buffer(5)]], masks [[buffer(6)]]')

    def test_missing_resources(self):
        with self.assertRaises(RuntimeError):
            validate_metal_bindings('kernel void main0() {}')

    def test_unmapped_resource(self):
        with self.assertRaises(RuntimeError):
            validate_metal_bindings('commands [[buffer(5)]], masks [[buffer(6)]]', {'commands': 5})

    def test_incorrect_slot(self):
        with self.assertRaises(RuntimeError):
            validate_metal_bindings('commands [[buffer(4)]]', {'commands': 5})


if __name__ == '__main__':
    unittest.main()
