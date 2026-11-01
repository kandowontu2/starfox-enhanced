"""Guard the optional DLSS download and release inventory without an SDK."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DlssAddonTests(unittest.TestCase):
    def test_runtime_is_packaged_not_embedded(self):
        workflow = (ROOT / '.github/workflows/portable-builds.yml').read_text()
        windows = workflow.split('  windows-x64:', 1)[1].split('  windows-x86:', 1)[0]
        self.assertIn('"-DSTARFOX_DLSS_RUNTIME_DIRECTORY:PATH="', windows)
        self.assertNotIn('-DSTARFOX_DLSS_RUNTIME_DIRECTORY=$staging', windows)
        self.assertIn('tools/package_dlss.ps1', windows)
        self.assertIn('DLSS-ADDON.txt', windows)
        self.assertIn('starfox-dlss-addon', windows)
        self.assertIn('Streamline SDK archive checksum mismatch', windows)
        name = re.search(r"DestinationPath '(StarFoxEnhanced-[0-9.]+-dlss-windows-x64.zip)'", windows)[1]
        self.assertIn(name, windows.split('-Files @(', 1)[1])
        self.assertIn(name, windows.split('- name: Add to GitHub release', 1)[1])
        self.assertIn(name, workflow.split('assets=(', 1)[1])
        self.assertIn('path: ' + name, windows)

    def test_external_runtime_stays_optional(self):
        host = (ROOT / 'src/app/dlss_host.hpp').read_text()
        self.assertIn('executable_directory/"dlss"', host)
        self.assertIn('DLSS runtime not installed', host)
        package = (ROOT / 'tools/package_dlss.ps1').read_text()
        for name in ('Streamline-LICENSE.txt', 'Streamline-THIRD-PARTY.md', 'nvngx_dlss.license.txt', 'runtime-manifest.json'):
            self.assertIn(name, package)
        self.assertIn('verify_dlss_package.ps1', package)


if __name__ == '__main__':
    unittest.main()
