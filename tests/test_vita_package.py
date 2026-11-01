"""Asset-free Vita packaging fixtures; never imply a console boot test."""
from pathlib import Path
import io
import struct
import sys
import tempfile
import unittest
import warnings
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import check_vita_package as checker
import generate_vita_assets as generator


def native_self(load_bytes=256):
    data = bytearray(0x1100)
    # One real ARM load reservation and its plain SELF segment descriptor.
    struct.pack_into("<IIHHI14Q", data, 0, 0x454353, 3, 0xC0, 1, 0x600,
        0x1000, 0x1100, len(data), 0, 4, 0x80, 0xA0, 0xE0, 0, 0x100, 0, 0, 0, 0)
    data[0xA0:0xA7] = b"\x7fELF\x01\x01\x01"
    struct.pack_into("<H", data, 0xA0 + 18, 40)
    struct.pack_into("<HH", data, 0xA0 + 42, 32, 1)
    struct.pack_into("<8I", data, 0xE0, 1, 0x1000, 0x81000000, 0x81000000,
        256, load_bytes, 5, 0x1000)
    struct.pack_into("<4Q", data, 0x100, 0x1000, 256, 1, 2)
    return data


def sfo(**changes):
    fields = {"TITLE": "Star Fox Enhanced", "TITLE_ID": "SFXE00001", "CATEGORY": "gd", "APP_VER": "00.88"}
    fields.update(changes)
    keys, values, records = bytearray(), bytearray(), bytearray()
    for key, value in fields.items():
        raw = value.encode() + b"\0"
        records.extend(struct.pack("<HHIII", len(keys), 0x204, len(raw), len(raw), len(values)))
        keys.extend(key.encode() + b"\0"); values.extend(raw)
    key_offset = 20 + len(records)
    return b"\0PSF" + struct.pack("<4I", 0x101, key_offset, key_offset + len(keys), len(fields)) + records + keys + values


class VitaPackageTest(unittest.TestCase):
    def setUp(self):
        # Respect the owner's scratch budget on local and CI runs alike.
        scratch = ROOT / "build/validation/active"
        scratch.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=scratch, prefix="vita-package-")
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.entries = {name: generator.png(*size) for name, size in generator.ASSETS.items()}
        self.entries.update({"eboot.bin": native_self(), "sce_sys/param.sfo": sfo(),
            checker.TEMPLATE: (ROOT / "platform/vita" / checker.TEMPLATE).read_bytes()})

    def package(self, omit=None, extra=None, duplicate=False):
        path = self.directory / "fixture.vpk"
        with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as package:
            for name, data in self.entries.items():
                if name != omit: package.writestr(name, data)
            if extra:
                info = zipfile.ZipInfo("placeholder")
                info.filename = extra
                package.writestr(info, b"private")
            if duplicate:
                with warnings.catch_warnings():
                    warnings.simplefilter("ignore", UserWarning)
                    package.writestr("eboot.bin", self.entries["eboot.bin"])
        return path

    def test_complete_package(self):
        self.assertEqual(checker.check(self.package())["APP_VER"], "00.88")

    def test_required_files(self):
        for name in self.entries:
            with self.subTest(name=name), self.assertRaises(ValueError):
                checker.check(self.package(omit=name))

    def test_metadata(self):
        for change in ({"TITLE_ID": "WRONG0001"}, {"CATEGORY": "ac"}, {"APP_VER": "0.8"}, {"TITLE": "Other"}):
            self.entries["sce_sys/param.sfo"] = sfo(**change)
            with self.subTest(change=change), self.assertRaises(ValueError): checker.check(self.package())
        for payload in (b"bad", sfo()[:-1], b"\0PSF" + b"\xff" * 100):
            self.entries["sce_sys/param.sfo"] = payload
            with self.assertRaises(ValueError): checker.check(self.package())

    def test_private_and_unsafe_files(self):
        for name in ("game.sfc", "game.smc", "game.bin", "Starfox-Assets.BIN", "release.jks", "release.key", "../outside", "/absolute", "C:/file", "dir\\file"):
            with self.subTest(name=name), self.assertRaises(ValueError): checker.check(self.package(extra=name))
        with self.assertRaises(ValueError): checker.check(self.package(duplicate=True))

    def test_self_header(self):
        for payload in (b"", b"\x7fELF" + bytes(256), b"SCE\0", b"SCE\0" + bytes(256)):
            self.entries["eboot.bin"] = payload
            with self.assertRaises(ValueError): checker.check(self.package())

    def test_self_memory_budget(self):
        self.assertEqual(checker.check_self(io.BytesIO(native_self()), 0x1100), 256)
        # Reproduce the released executable's 177 MiB load reservation without
        # allocating/copying a large test binary onto the owner's drive.
        with self.assertRaisesRegex(ValueError, "exceeds 64 MiB"):
            checker.check_self(io.BytesIO(native_self(185745024)), 0x1100)
        for offset, value in ((0xA0 + 18, 62), (0xA0 + 42, 0), (0xA0 + 44, 65)):
            broken = native_self()
            struct.pack_into("<H", broken, offset, value)
            with self.assertRaises(ValueError): checker.check_self(io.BytesIO(broken), len(broken))
        broken = native_self()
        struct.pack_into("<Q", broken, 0x100, 0x1100)
        with self.assertRaises(ValueError): checker.check_self(io.BytesIO(broken), len(broken))

    def test_packaged_backdrops(self):
        with self.assertRaisesRegex(ValueError, "Missing packaged Vita backdrop"):
            checker.check(self.package(), require_backdrops=True)
        for name in checker.backdrop_paths(): self.entries[name] = b"BM" + bytes(52)
        checker.check(self.package(), require_backdrops=True)
        self.entries[checker.backdrop_paths()[-1]] = b"bad" + bytes(52)
        with self.assertRaisesRegex(ValueError, "Invalid packaged Vita backdrop"):
            checker.check(self.package(), require_backdrops=True)

    def test_png_validation(self):
        original = self.entries["sce_sys/icon0.png"]
        with self.assertRaises(ValueError):
            checker.check_png(original, (127, 128))
        for payload in (b"bad", original[:-3], original[:-20] + bytes(20)):
            with self.assertRaises(ValueError): checker.check_png(payload, (128, 128))
        transparent = original[:-12] + generator.chunk(b"tRNS", bytes([0, 255, 255])) + original[-12:]
        with self.assertRaises(ValueError): checker.check_png(transparent, (128, 128))

    def test_livearea_references(self):
        for payload in (b"<other/>", b"<livearea>", self.entries[checker.TEMPLATE].replace(b"bg.png", b"missing.png")):
            self.entries[checker.TEMPLATE] = payload
            with self.assertRaises((ValueError, checker.ET.ParseError)): checker.check(self.package())

    def test_deterministic_artwork(self):
        generator.generate(self.directory / "art")
        for name, size in generator.ASSETS.items():
            data = (self.directory / "art" / name).read_bytes()
            self.assertEqual(data, self.entries[name])
            checker.check_png(data, size)


if __name__ == "__main__": unittest.main()
