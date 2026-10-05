"""Asset-free binary/header, provenance and private-file package contracts."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("package_3ds_test", ROOT / "tools/package_3ds_test.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
COMMIT = "4a2cb0818f2b36aa221b88e69ee11b697203e4b3"


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.elf = self.root / "player.elf"
        self.dsx = self.root / "player.3dsx"
        self.smdh = self.root / "player.smdh"
        self.output = self.root / "test.zip"
        elf = bytearray(52)
        elf[:7] = b"\x7fELF\x01\x01\x01"
        struct.pack_into("<HH", elf, 16, 2, 40)
        struct.pack_into("<I", elf, 24, 0x100000)
        self.elf.write_bytes(elf)
        icon = b"SMDH\x00\x00" + bytes(14010)
        self.smdh.write_bytes(icon)
        header = struct.pack("<4sHH9I", b"3DSX", 44, 8, 0, 0, 4, 4, 8, 4, 80, 14016, 0)
        self.dsx.write_bytes(header + bytes(36) + icon)
        # Surrounding private files must never be swept into a package.
        (self.root / "Starfox-Assets.BIN").write_bytes(b"PRIVATE-ASSET-SENTINEL")
        (self.root / "private.sfc").write_bytes(b"PRIVATE-ROM-SENTINEL")

    def pack(self, commit=COMMIT):
        return MODULE.package(self.elf, self.dsx, self.smdh, commit, self.output)

    def test_valid_exact_members_and_provenance(self):
        info = self.pack()
        self.assertTrue(info["experimental"])
        self.assertFalse(info["hardware_accepted"])
        self.assertFalse(info["private_assets_included"])
        self.assertEqual(info["source_commit"], COMMIT)
        self.assertIn("New Nintendo 3DS", info["target"])
        self.assertEqual(info["hardware_policy"], {
            "new_3ds": "CPU/cache speedup and slider-controlled stereo",
            "new_2ds_xl": "CPU/cache speedup, mono",
            "original_3ds_xl_2ds": "standard CPU, mono; slider ignored",
            "unknown": "standard CPU, mono"})
        with zipfile.ZipFile(self.output) as archive:
            self.assertEqual(set(archive.namelist()), {MODULE.APP_DIR + "starfox-enhanced.3dsx",
                MODULE.APP_DIR + "starfox-enhanced.smdh", "README.txt", "BUILD-INFO.json", "SHA256SUMS.txt"})
            self.assertEqual(json.loads(archive.read("BUILD-INFO.json")), info)
            self.assertEqual(archive.read(MODULE.APP_DIR + "starfox-enhanced.3dsx"), self.dsx.read_bytes())
            self.assertIsNone(archive.testzip())
            self.assertIn(b"Esteban PDN", archive.read("README.txt"))
            self.assertIn(b"753952be5b170059d95068350d37759a966a3bd5", archive.read("README.txt"))
            for line in archive.read("SHA256SUMS.txt").decode().splitlines():
                digest, name = line.split("  ", 1)
                self.assertEqual(hashlib.sha256(archive.read(name)).hexdigest(), digest)
            for name in archive.namelist():
                self.assertNotIn(b"PRIVATE-", archive.read(name))

    def test_invalid_elf(self):
        original = self.elf.read_bytes()
        for at, value in [(0, 0), (4, 2), (5, 2), (6, 0), (16, 3), (18, 62), (24, 1)]:
            with self.subTest(at=at):
                bad = bytearray(original)
                bad[at] = value
                self.elf.write_bytes(bad)
                with self.assertRaises(ValueError):
                    self.pack()
                self.assertFalse(self.output.exists())
        self.elf.write_bytes(b"ELF")
        with self.assertRaises(ValueError):
            self.pack()

    def test_invalid_dsx(self):
        original = self.dsx.read_bytes()
        for at, value in [(0, 0), (4, 32), (6, 4), (8, 1), (12, 1), (16, 0), (24, 1), (32, 44), (36, 0), (40, 80)]:
            with self.subTest(at=at):
                bad = bytearray(original)
                bad[at] = value
                self.dsx.write_bytes(bad)
                with self.assertRaises(ValueError):
                    self.pack()
                self.assertFalse(self.output.exists())
        for bad in [original[:40], original[:-1], original + b"PRIVATE-TRAILER"]:
            self.dsx.write_bytes(bad)
            with self.assertRaises(ValueError):
                self.pack()

    def test_invalid_icon(self):
        original = self.smdh.read_bytes()
        for bad in [original[:-1], b"FAIL" + original[4:], original[:6] + b"x" + original[7:]]:
            self.smdh.write_bytes(bad)
            with self.assertRaises(ValueError):
                self.pack()
            self.assertFalse(self.output.exists())

    def test_invalid_commit_and_no_overwrite(self):
        for commit in ["main", "a" * 39, "a" * 41, "../private", "A" * 40]:
            with self.assertRaises(ValueError):
                self.pack(commit)
        self.pack()
        original = self.output.read_bytes()
        with self.assertRaises(ValueError):
            self.pack()
        self.assertEqual(self.output.read_bytes(), original)

    def test_reproducible_package(self):
        self.pack()
        first = self.output.read_bytes()
        self.output = self.root / "second.zip"
        self.pack()
        self.assertEqual(self.output.read_bytes(), first)


if __name__ == "__main__":
    unittest.main()
