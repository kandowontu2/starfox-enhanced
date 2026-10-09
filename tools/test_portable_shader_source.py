import hashlib
import pathlib
import tempfile
import unittest
from portable_shader_source import source_digest


class ShaderSourceTests(unittest.TestCase):
    def test_legacy_and_line_endings(self):
        with tempfile.TemporaryDirectory() as directory:
            source = pathlib.Path(directory) / 'main.hlsl'
            source.write_bytes(b'abc\r\n')
            self.assertEqual(source_digest(source), hashlib.sha256(b'abc\n').hexdigest())

    def test_nested_include_invalidates(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source = root / 'main.hlsl'
            source.write_text('#include "helper.hlsli"\n')
            (root / 'helper.hlsli').write_text('#include "nested.hlsli"\n')
            nested = root / 'nested.hlsli'
            nested.write_text('first')
            before = source_digest(source)
            nested.write_text('second')
            self.assertNotEqual(before, source_digest(source))

    def test_missing_and_cycle_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            source = pathlib.Path(directory) / 'main.hlsl'
            source.write_text('#include "missing.hlsli"\n')
            with self.assertRaises(FileNotFoundError):
                source_digest(source)
            source.write_text('#include "main.hlsl"\n')
            with self.assertRaises(RuntimeError):
                source_digest(source)


if __name__ == '__main__':
    unittest.main()
