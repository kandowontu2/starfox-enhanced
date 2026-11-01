"""Dependency-free platform icon checks; no game/desktop windows are opened."""
from pathlib import Path
import struct
import sys
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import check_vita_package
import generate_ios_assets
import generate_vita_assets

ASSETS = ROOT / 'assets/icons'


def png_header(path):
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    return struct.unpack_from('>IIBBBBB', data, 16)


class AppIconsTests(unittest.TestCase):
    def test_ios_opaque_sizes(self):
        for name, size in generate_ios_assets.ICONS.items():
            self.assertEqual(png_header(ASSETS / 'ios' / name), (size, size, 8, 2, 0, 0, 0))

    def test_vita_indexed_artwork(self):
        for name, size in generate_vita_assets.ASSETS.items():
            check_vita_package.check_png((ASSETS / 'vita' / name).read_bytes(), size)

    def test_windows_icon_directory_and_payloads(self):
        data = (ASSETS / 'starfox-enhanced.ico').read_bytes()
        reserved, kind, count = struct.unpack_from('<HHH', data)
        self.assertEqual((reserved, kind, count), (0, 1, 7))
        sizes = []
        for index in range(count):
            w, h, _, _, _, _, length, offset = struct.unpack_from('<BBBBHHII', data, 6 + index * 16)
            self.assertEqual(w, h)
            self.assertGreaterEqual(offset, 6 + count * 16)
            self.assertLessEqual(offset + length, len(data))
            self.assertEqual(data[offset:offset + 8], b'\x89PNG\r\n\x1a\n')
            self.assertEqual(struct.unpack_from('>II', data, offset + 16), (w or 256, h or 256))
            sizes.append(w or 256)
        self.assertEqual(sizes, [16, 24, 32, 48, 64, 128, 256])

    def test_mac_icns_chunks(self):
        data = (ASSETS / 'starfox-enhanced.icns').read_bytes()
        self.assertEqual(data[:4], b'icns')
        self.assertEqual(struct.unpack_from('>I', data, 4)[0], len(data))
        offset, kinds = 8, []
        while offset < len(data):
            kind, length = struct.unpack_from('>4sI', data, offset)
            self.assertGreater(length, 8)
            self.assertLessEqual(offset + length, len(data))
            self.assertEqual(data[offset + 8:offset + 16], b'\x89PNG\r\n\x1a\n')
            kinds.append(kind)
            offset += length
        self.assertEqual(offset, len(data))
        self.assertEqual(kinds, [b'icp4', b'icp5', b'icp6', b'ic07', b'ic08', b'ic09', b'ic10'])

    def test_shared_android_resource_wiring(self):
        for directory, prefix in (('android/app', '../../..'), ('quest', '../..')):
            base = ROOT / 'platform' / directory
            self.assertIn(f"'{prefix}/assets/icons/android'", (base / 'build.gradle').read_text())
            tree = ET.parse(base / 'src/main/AndroidManifest.xml')
            self.assertEqual(tree.find('application').get('{http://schemas.android.com/apk/res/android}icon'), '@mipmap/ic_launcher')
        for density, size in [('mdpi', 48), ('hdpi', 72), ('xhdpi', 96), ('xxhdpi', 144), ('xxxhdpi', 192)]:
            self.assertEqual(png_header(ASSETS / 'android' / f'mipmap-{density}/ic_launcher.png')[:4], (size, size, 8, 2))
        ET.parse(ASSETS / 'android/mipmap-anydpi-v26/ic_launcher.xml')
        self.assertEqual(png_header(ASSETS / 'android/drawable-nodpi/ic_launcher_foreground.png')[:4], (432, 432, 8, 6))

    def test_ps5_rgb_icon(self):
        self.assertEqual(png_header(ROOT / 'platform/ps5/sce_sys/icon0.png'), (512, 512, 8, 2, 0, 0, 0))


if __name__ == '__main__':
    unittest.main()
