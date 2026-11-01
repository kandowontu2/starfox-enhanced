"""Offline, links-only optional runtime packaging; never download/run installers."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = json.loads((ROOT / 'tools/package/displayxr-runtime.json').read_text())
PACKAGE = None


class DisplayXrPackageTests(unittest.TestCase):
    def invoke(self, package, verify=False):
        shell = shutil.which('pwsh') or shutil.which('powershell')
        self.assertIsNotNone(shell)
        command = [shell, '-NoProfile', '-File', str(ROOT / 'tools/package_displayxr.ps1'),
                   '-OutputDirectory', str(package)]
        if verify:
            command.append('-VerifyOnly')
        return subprocess.run(command, capture_output=True, text=True, timeout=30)

    def make(self, path):
        path.mkdir()
        result = self.invoke(path)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return path

    def test_historical_reference_pins_remain_identifiable(self):
        self.assertEqual(MANIFEST['schema'], 1)
        self.assertEqual(len(MANIFEST['installers']), 2)
        self.assertEqual(len(MANIFEST['notices']), 14)
        for entry in MANIFEST['installers'] + MANIFEST['notices']:
            self.assertRegex(entry['sha256'], r'^[0-9A-F]{64}$')
            self.assertGreater(entry['bytes'], 0)
            self.assertIn(entry['project'], MANIFEST['projects'])
            path = Path(entry.get('source', entry.get('file')))
            self.assertFalse(path.is_absolute())
            self.assertNotIn('..', path.parts)

    def test_links_only_offline_package(self):
        with tempfile.TemporaryDirectory(prefix='sfe-displayxr-links-') as temporary:
            package = self.make(Path(temporary) / 'package')
            result = self.invoke(package, True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual({p.relative_to(package).as_posix() for p in package.rglob('*') if p.is_file()},
                             {'optional-runtimes/displayxr/README.txt',
                              'optional-runtimes/displayxr/manifest.json'})
            readme = (package / 'optional-runtimes/displayxr/README.txt').read_text()
            self.assertIn('https://displayxr.org/download', readme)
            self.assertIn('Do not\ndisable antivirus', readme)

    def test_stale_installers_rejected_without_deleting_them(self):
        for relative in ('DisplayXRSetup-2.21.11.exe',
                         'optional-runtimes/displayxr/DisplayXRLeiaSRSetup-2.7.6.exe'):
            with self.subTest(relative=relative), tempfile.TemporaryDirectory(prefix='sfe-displayxr-stale-') as temporary:
                package = self.make(Path(temporary) / 'package')
                installer = package / relative
                installer.write_text('Inert test-only marker, not a PE executable.')
                for verify in (False, True):
                    result = self.invoke(package, verify)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn('not permitted', result.stdout + result.stderr)
                    self.assertEqual(installer.read_text(), 'Inert test-only marker, not a PE executable.')

    def test_unknown_optional_payload_rejected(self):
        with tempfile.TemporaryDirectory(prefix='sfe-displayxr-unknown-') as temporary:
            package = self.make(Path(temporary) / 'package')
            (package / 'optional-runtimes/displayxr/unknown.dll').write_text('Inert fixture.')
            result = self.invoke(package, True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Unexpected optional runtime payload', result.stdout + result.stderr)

    def test_missing_or_modified_instructions_rejected(self):
        for missing in (False, True):
            with self.subTest(missing=missing), tempfile.TemporaryDirectory(prefix='sfe-displayxr-instructions-') as temporary:
                package = self.make(Path(temporary) / 'package')
                readme = package / 'optional-runtimes/displayxr/README.txt'
                if missing:
                    readme.unlink()
                else:
                    readme.write_text('Modified fixture.')
                result = self.invoke(package, True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('missing or modified', result.stdout + result.stderr)

    def test_release_gates_before_upload(self):
        workflow = (ROOT / '.github/workflows/portable-builds.yml').read_text()
        start = workflow.index('- name: Package runtime and builder')
        end = workflow.index('name: StarFoxEnhanced-0.0.8.8.3-windows-x64', start)
        block = workflow[start:end]
        self.assertIn('-OutputDirectory $runtime -VerifyOnly', block)
        self.assertIn('tests/test_displayxr_package.py --package $runtime', block)
        self.assertIn('$runtime/licenses/displayxr/LICENSE.txt', block)
        self.assertIn('$runtime/licenses/displayxr/NOTICE.txt', block)
        self.assertIn('tools/check_windows_defender.ps1 -UpdateSignatures', block)
        self.assertIn('windows-x64.zip', block)
        self.assertLess(block.index('Move-Item -LiteralPath "$runtime/starfox_asset_builder.exe"'),
                        block.index('Compress-Archive -Path "$runtime/*"'))
        script = (ROOT / 'tools/package_displayxr.ps1').read_text()
        self.assertNotIn('Invoke-WebRequest', script)
        self.assertNotIn('Start-Process', script)
        self.assertIn('needs: [updater-checks,', workflow)
        self.assertIn('gh release edit "${tag}" --draft=false --latest', workflow)

    def test_actual_package_if_supplied(self):
        if PACKAGE is None:
            self.skipTest('--package not supplied')
        result = self.invoke(PACKAGE, True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--package', type=Path)
    arguments, rest = parser.parse_known_args()
    PACKAGE = arguments.package
    unittest.main(argv=[sys.argv[0], *rest])
