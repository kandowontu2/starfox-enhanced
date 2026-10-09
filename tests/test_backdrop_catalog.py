"""Keep shared artwork identity aligned with desktop/portable packaging."""
from pathlib import Path
import re
import unittest
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


class BackdropCatalogTests(unittest.TestCase):
    def setUp(self):
        header = (ROOT / 'include/starfox/render/enhanced_backdrop_library.hpp').read_text()
        self.paths = re.findall(r'EnhancedBackdropAsset\{"([^"]+)"', header)
        self.expected = {200 + i: path for i, path in enumerate(self.paths)}

    def test_unique_complete_assets(self):
        self.assertEqual(len(self.paths), 38)
        self.assertEqual(len(set(self.paths)), len(self.paths))
        for path in self.paths:
            self.assertTrue((ROOT / path).is_file(), path)

    def test_windows_resources_match(self):
        source = (ROOT / 'src/app/starfox_assets.rc.in').read_text()
        pairs = re.findall(r'^(2\d\d) RCDATA "@CMAKE_CURRENT_SOURCE_DIR@/([^"]+)"', source, re.M)
        self.assertEqual(len(pairs), len(self.paths))
        self.assertEqual({int(key): path for key, path in pairs}, self.expected)

    def test_portable_resources_match(self):
        source = (ROOT / 'CMakeLists.txt').read_text()
        # Portable dispatch arguments and split translation units must come
        # from the same catalogue, not independent manually capped ID lists.
        self.assertRegex(source, r'starfox_enhanced_backdrop_resources\(\s*'
                         r'STARFOX_EMBED_BACKDROP_ARGS\s+STARFOX_EMBED_BACKDROP_FILES\)')
        self.assertIn('${STARFOX_EMBED_BACKDROP_ARGS}', source)
        self.assertIn('${STARFOX_EMBED_BACKDROP_FILES}', source)
        self.assertNotRegex(source, r'--resource "2\d\d=')
        self.assertRegex(source, r'list\(LENGTH STARFOX_EMBED_BACKDROP_FILES '
                         r'STARFOX_EMBED_BACKDROP_COUNT\)')
        self.assertIn('"199 + ${STARFOX_EMBED_BACKDROP_COUNT}"', source)
        self.assertIn('foreach(resource_id RANGE 200 ${STARFOX_EMBED_BACKDROP_LAST_ID})', source)

    def test_vr_cmake_resources_match(self):
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / 'catalogue.cmake'
            script.write_text(
                f'include("{ROOT.as_posix()}/cmake/EnhancedBackdropAssets.cmake")\n'
                'starfox_enhanced_backdrop_resources(arguments files)\n'
                'foreach(argument IN LISTS arguments)\n'
                'message(STATUS "${argument}")\n'
                'endforeach()\n', encoding='utf-8')
            result = subprocess.run(['cmake', '-P', str(script)], check=True,
                                    capture_output=True, text=True)
        pairs = re.findall(r'^-- (2\d\d)=(.+)$', result.stdout, re.M)
        self.assertEqual(len(pairs), len(self.paths))
        self.assertEqual({int(key): Path(path).relative_to(ROOT).as_posix()
                          for key, path in pairs}, self.expected)


if __name__ == '__main__':
    unittest.main()
