import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("payload", Path(__file__).with_name("externalize-dxr-payload.py"))
payload = importlib.util.module_from_spec(spec)
spec.loader.exec_module(payload)


def container():
    # Authored miniature DXBC container; not a runnable shader or game data.
    data = bytearray(b"DXBC" + bytes(16) + struct.pack("<III", 1, 48, 1))
    data += struct.pack("<I", 36) + b"TEST" + struct.pack("<I", 4) + bytes([0, 127, 128, 255])
    return bytes(data)


def header(data, symbol="sample_shader"):
    return "#if 0\nignored compiler listing\n#endif\nconst unsigned char " + symbol + "[] = {\n" + ",".join(f"0x{x:02x}" for x in data) + ",\n};\n"


class PayloadTests(unittest.TestCase):
    def test_all_bytes_and_extent(self):
        self.assertEqual(payload.extract(header(container()), "sample_shader"), container())

    def test_wrong_symbol(self):
        with self.assertRaises(ValueError):
            payload.extract(header(container()), "different_shader")

    def test_nonliteral(self):
        with self.assertRaises(ValueError):
            payload.extract(header(container()).replace("0x7f", "128-1"), "sample_shader")

    def test_truncated_extent(self):
        with self.assertRaises(ValueError):
            payload.extract(header(container()[:-1]), "sample_shader")

    def test_bad_chunk(self):
        data = bytearray(container())
        struct.pack_into("<I", data, 32, 4)
        with self.assertRaises(ValueError):
            payload.extract(header(data), "sample_shader")

    def test_serialization_and_fixed_declaration(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as folder:
            root = Path(folder)
            source = root / "input.hpp"
            source.write_text(header(container()), encoding="utf-8")
            payload.emit(source, root / "out", "sample_shader")
            self.assertEqual((root / "out/sample_shader.bin").read_bytes(), container())
            self.assertIn("sample_shader[48]", (root / "out/sample_shader.hpp").read_text())
            self.assertIn('.section .rdata,"dr"', (root / "out/sample_shader.S").read_text())
            self.assertTrue(source.read_text().startswith("#if 0"))


if __name__ == "__main__":
    unittest.main()
