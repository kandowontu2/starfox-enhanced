"""Independent binary fixtures for the hardware-oriented native shader gate."""
import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location("shader_gate", Path(__file__).parents[1] / "tools/check_3ds_shader.py")
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


def fixture(masks=(15,), destinations=None, opcode=0x13, declaration_mask=15):
    destinations = destinations or [0] * len(masks)
    data = bytearray(256)
    struct.pack_into("<4sII", data, 0, b"DVLB", 1, 160)
    struct.pack_into("<4sIIIII", data, 12, b"DVLP", 0, 28, len(masks) + 1, 68, len(masks))
    for index, (mask, destination) in enumerate(zip(masks, destinations)):
        shift = 24 if opcode >= 0x30 else 21
        struct.pack_into("<I", data, 40 + index * 4, (opcode << 26) | (destination << shift) | index)
        struct.pack_into("<II", data, 80 + index * 8, mask, 0)
    struct.pack_into("<I", data, 40 + len(masks) * 4, 0x22 << 26)
    struct.pack_into("<4sIII", data, 160, b"DVLE", 0, 0, len(masks) + 1)
    struct.pack_into("<II", data, 200, 64, 1)
    struct.pack_into("<HHB", data, 224, 0, 0, declaration_mask)
    return bytes(data)


class ShaderGateTests(unittest.TestCase):
    def test_complete_and_disjoint_writes(self):
        for masks in ((15,), (8, 4, 2, 1), (12, 3)):
            self.assertTrue(gate.validate_shader(fixture(masks))["complete_single_writes"])

    def test_missing_lanes_and_repeated_writes(self):
        for masks in ((12,), (8,), (15, 8), (8, 4, 2, 2), (0,)):
            with self.subTest(masks=masks), self.assertRaises(ValueError):
                gate.validate_shader(fixture(masks))

    def test_mad_destination_encoding(self):
        for opcode in (0x30, 0x38):
            self.assertTrue(gate.validate_shader(fixture(opcode=opcode))["complete_single_writes"])
            with self.assertRaises(ValueError):
                gate.validate_shader(fixture(opcode=opcode, destinations=[1]))

    def test_invalid_headers_bounds_descriptors_and_flow(self):
        for offset, value in ((4, 2), (8, 254), (24, 513), (32, 129),
                              (172, 0), (204, 30), (40, 127 | (0x13 << 26)),
                              (40, 0x24 << 26), (44, 0x13 << 26)):
            data = bytearray(fixture())
            struct.pack_into("<I", data, offset, value)
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                gate.validate_shader(bytes(data))
        for length in (0, 4, 12, 24, 159, 225):
            with self.subTest(length=length), self.assertRaises(ValueError):
                gate.validate_shader(fixture()[:length])


if __name__ == "__main__":
    unittest.main()
