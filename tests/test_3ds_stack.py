"""Independent ELF fixtures for the native stack gate (no SDK/assets needed)."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("stack_gate", Path(__file__).parents[1] / "tools/check_3ds_stack.py")
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


def fixture(value=256 * 1024, binding=1, size=4, name=b"__stacksize__", machine=40):
    data = bytearray(0x1A0)
    ident = b"\x7fELF\1\1\1" + bytes(9)
    struct.pack_into("<16sHHIIIIIHHHHHH", data, 0, ident, 2, machine, 1, 0x100000,
                     0, 0x100, 0, 52, 0, 0, 40, 4, 0)
    struct.pack_into("<I", data, 0x40, value)
    names = b"\0" + name + b"\0"
    data[0x50:0x50 + len(names)] = names
    struct.pack_into("<IIIBBH", data, 0x80, 1, 0x200000, size, (binding << 4) | 1, 0, 1)
    for index, fields in enumerate(((0, 1, 3, 0x200000, 0x40, 4, 0, 0, 4, 0),
                                    (0, 3, 0, 0, 0x50, len(names), 0, 0, 1, 0),
                                    (0, 2, 0, 0, 0x70, 32, 2, 1, 4, 16)), 1):
        struct.pack_into("<10I", data, 0x100 + index * 40, *fields)
    return bytes(data)


class StackGateTests(unittest.TestCase):
    def test_strong_native_reservation(self):
        self.assertEqual(gate.stack_reservation(fixture()), 262144)

    def test_default_weak_missing_wrong_arch_or_size(self):
        for data in (fixture(value=32768), fixture(binding=2), fixture(size=0),
                     fixture(name=b"absent"), fixture(machine=62), fixture()[:0x101]):
            with self.subTest(data=data[:25]):
                with self.assertRaises(ValueError):
                    gate.stack_reservation(data)

    def test_frame_records_and_missing_or_exhausted_stack(self):
        with tempfile.TemporaryDirectory(prefix="sfe-3ds-stack-") as temporary:
            root = Path(temporary)
            with self.assertRaises(ValueError):
                gate.compiler_frames(root)
            path = root / "spc.su"
            path.write_text("spc.cpp:176:5:load_driver\t66080\tstatic\nmain.cpp:10:1:main\t4096\tstatic\n", encoding="utf-8")
            frames = gate.compiler_frames(root)
            self.assertEqual([frame["bytes"] for frame in frames], [66080, 4096])
            path.write_text("bad record\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                gate.compiler_frames(root)
            path.write_text("main\t262144\tstatic\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                gate.compiler_frames(root)


if __name__ == "__main__":
    unittest.main()
