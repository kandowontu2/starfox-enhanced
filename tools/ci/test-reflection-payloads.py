import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("reflection_payloads", Path(__file__).with_name("externalize-reflection-payloads.py"))
payload = importlib.util.module_from_spec(spec)
spec.loader.exec_module(payload)


def dxbc():
    data = bytearray(b"DXBC" + bytes(16) + struct.pack("<III", 1, 300, 1))
    data += struct.pack("<I", 36) + b"TEST" + struct.pack("<I", 256) + bytes(range(256))
    return bytes(data)


def spirv():
    # Authored header and one OpNop, not a runnable shader.
    return struct.pack("<6I", 0x07230203, 0x00010000, 0, 1, 0, 0x00010000)


def header(data, symbol="sample", namespace=None):
    body = ",".join(str(x) if namespace else f"0x{x:02x}" for x in data)
    declaration = ("inline constexpr " if namespace else "alignas(4) const ")
    declaration += "unsigned char " + symbol + "[]={\n" + body + ",\n};\n"
    return ("namespace starfox::render::" + namespace + " {\n" + declaration + "}\n") if namespace else "#pragma once\n" + declaration


class ReflectionPayloadTests(unittest.TestCase):
    def test_complete_formats_and_all_byte_values(self):
        for data in (dxbc(), spirv()):
            self.assertEqual(payload.extract(header(data), "sample"), data)
            self.assertEqual(payload.extract(header(data, namespace="old"), "sample", "old"), data)

    def test_duplicate_and_missing_array(self):
        for text in (header(dxbc()) * 2, header(dxbc(), "other")):
            with self.assertRaises(ValueError):
                payload.extract(text, "sample")

    def test_expression_range_and_symbol_rejected(self):
        for text in (header(dxbc()).replace("0x7f", "128-1"), header(dxbc()).replace("0x7f", "256")):
            with self.assertRaises(ValueError):
                payload.extract(text, "sample")
        with self.assertRaises(ValueError):
            payload.extract(header(dxbc()), "sample;unsafe")

    def test_complete_container_required(self):
        for data in (dxbc()[:-1], spirv()[:-1], struct.pack("<6I", 0x07230203, 0x10000, 0, 1, 0, 0), b"not a shader"):
            with self.assertRaises(ValueError):
                payload.extract(header(data), "sample")

    def test_namespace_cannot_be_silently_changed(self):
        with self.assertRaises(ValueError):
            payload.extract(header(dxbc(), namespace="old"), "sample", "different")
        with self.assertRaises(ValueError):
            payload.extract("namespace starfox::render::old {\n}\n" + header(dxbc()), "sample", "old")

    def test_serialization_retains_extent_and_bytes(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as folder:
            root = Path(folder)
            source = root / "sample.hpp"
            source.write_text(header(dxbc()), encoding="utf-8")
            original = source.read_bytes()
            record = payload.emit_generated(source, root / "out", "sample")
            self.assertEqual(source.read_bytes(), original)
            self.assertEqual((root / "out/sample.bin").read_bytes(), dxbc())
            self.assertIn("sample[300]", (root / "out/sample.hpp").read_text())
            self.assertTrue(record["shader_bytes_unchanged"])
            self.assertFalse(record["production_adopted"])

    def test_every_legacy_array_uses_distinct_cpp_entity(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as folder:
            root = Path(folder)
            directory = root / "src/render/shaders/generated"
            directory.mkdir(parents=True)
            for filename, namespace, _, stem in payload.LEGACY:
                text = "namespace starfox::render::" + namespace + " {\n"
                for backend, data in (("dxil", dxbc()), ("spirv", spirv())):
                    text += "inline constexpr unsigned char " + stem + "_" + backend + "[]={\n" + ",".join(str(x) for x in data) + ",\n};\n"
                (directory / (filename + ".hpp")).write_text(text + "}\n", encoding="utf-8")
            records = payload.emit_legacy(root, root / "out")
            self.assertEqual(len(records), 8)
            declaration = (root / "out/external_reflection_history_payloads_v2.hpp").read_text()
            for record in records:
                self.assertTrue(record["distinct_cpp_entity"])
                self.assertIn(record["declaration_namespace"], declaration)
                expected = dxbc() if record["symbol"].endswith("_dxil") else spirv()
                self.assertEqual((root / "out" / (record["symbol"] + ".bin")).read_bytes(), expected)
                saved = json.loads((root / "out" / (record["symbol"] + ".json")).read_text())
                self.assertEqual(saved, record)


if __name__ == "__main__":
    unittest.main()
