import importlib.util
from pathlib import Path
import plistlib
import struct
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location('package_ios', Path(__file__).resolve().parents[1] / 'tools/package_ios.py')
ios = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ios)


class IpaTests(unittest.TestCase):
    def app(self, root, platform=2):
        app = root / 'StarFoxEnhanced.app'
        app.mkdir()
        info = {'CFBundleExecutable': 'StarFoxEnhanced', 'CFBundleIdentifier': 'com.starfox.enhanced',
                'CFBundleSupportedPlatforms': ['iPhoneOS'], 'MinimumOSVersion': '15.0'}
        (app / 'Info.plist').write_bytes(plistlib.dumps(info))
        binary = struct.pack('<8I', 0xfeedfacf, 0x100000c, 0, 2, 1, 24, 0, 0)
        binary += struct.pack('<6I', 0x32, 24, platform, 0, 0, 0)
        (app / 'StarFoxEnhanced').write_bytes(binary)
        (app / 'StarFoxEnhanced').chmod(0o755)
        return app

    def test_direct_app_is_rooted_ipa(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = ios.package(root / 'test.ipa', app=self.app(root))
            self.assertEqual(result['app'], 'Payload/StarFoxEnhanced.app')
            self.assertEqual(ios.verify(root / 'test.ipa'), result)

    def test_old_wrapped_release_repaired_without_payload_change(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            app = self.app(root)
            with zipfile.ZipFile(root / 'old.zip', 'w') as source:
                for path in app.iterdir():
                    entry = zipfile.ZipInfo('release/Payload/StarFoxEnhanced.app/' + path.name)
                    entry.create_system = 3
                    entry.external_attr = 0o100755 << 16
                    source.writestr(entry, path.read_bytes())
                source.writestr('release/README.md', 'Not inside the IPA')
            with self.assertRaisesRegex(ValueError, 'root'):
                ios.verify(root / 'old.zip')
            ios.package(root / 'test.ipa', source=root / 'old.zip')
            with zipfile.ZipFile(root / 'test.ipa') as repaired:
                self.assertEqual(repaired.read('Payload/StarFoxEnhanced.app/StarFoxEnhanced'), (app / 'StarFoxEnhanced').read_bytes())
                self.assertTrue(all(name.startswith('Payload/') for name in repaired.namelist()))
                self.assertEqual(repaired.getinfo('Payload/StarFoxEnhanced.app/StarFoxEnhanced').external_attr >> 16 & 0o777, 0o755)

    def test_simulator_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaisesRegex(ValueError, 'physical iOS'):
                ios.package(root / 'bad.ipa', app=self.app(root, platform=7))
            self.assertFalse((root / 'bad.ipa').exists())

    def test_no_accidental_overwrite(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / 'existing.ipa'
            output.write_bytes(b'keep')
            with self.assertRaisesRegex(ValueError, 'overwrite'):
                ios.package(output, app=self.app(root))
            self.assertEqual(output.read_bytes(), b'keep')

    def test_path_escape_rejected(self):
        for name in ('../Payload/app', '/Payload/app', 'Payload/../../app', 'Payload\\app'):
            with self.assertRaises(ValueError):
                ios.safe_name(name)


if __name__ == '__main__':
    unittest.main()
