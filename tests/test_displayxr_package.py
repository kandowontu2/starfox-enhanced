"""Pinned optional installer policy, plus offline packaged-file verification.

Pass --package PATH to exercise the real PowerShell size/hash/signature checker.
No installer is executed and no system runtime setting is changed.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = json.loads((ROOT / "tools/package/displayxr-runtime.json").read_text())
PACKAGE = None


class DisplayXrPackageTests(unittest.TestCase):
    def test_pinned_payloads(self):
        self.assertEqual(MANIFEST["schema"], 1)
        self.assertEqual(len(MANIFEST["installers"]), 2)
        self.assertEqual({x["file"] for x in MANIFEST["installers"]}, {
            "DisplayXRSetup-2.21.11.exe", "DisplayXRLeiaSRSetup-2.7.6.exe"})
        for entry in MANIFEST["installers"] + MANIFEST["notices"]:
            self.assertRegex(entry["sha256"], r"^[0-9A-F]{64}$")
            self.assertGreater(entry["bytes"], 0)
            self.assertIn(entry["project"], MANIFEST["projects"])
        for project in MANIFEST["projects"].values():
            self.assertRegex(project["revision"], r"^[0-9a-f]{40}$")
            self.assertTrue(project["repository"].startswith("DisplayXR/"))

    def test_all_upstream_notices_present(self):
        self.assertEqual(len(MANIFEST["notices"]), 14)
        for project in ("runtime", "leia-plugin"):
            paths = {x["source"] for x in MANIFEST["notices"] if x["project"] == project}
            self.assertTrue({"LICENSE", "NOTICE"}.issubset(paths))
        runtime = {x["source"] for x in MANIFEST["notices"] if x["project"] == "runtime"}
        self.assertEqual(len([x for x in runtime if x.startswith("LICENSES/")]), 10)

    def test_paths_are_relative_and_confined(self):
        for entry in MANIFEST["installers"] + MANIFEST["notices"]:
            path = Path(entry.get("source", entry.get("file")))
            self.assertFalse(path.is_absolute())
            self.assertNotIn("..", path.parts)
        for project in MANIFEST["projects"].values():
            self.assertTrue(project["license_directory"].startswith("licenses/"))

    def test_release_workflow_verifies_before_archive(self):
        workflow = (ROOT / ".github/workflows/portable-builds.yml").read_text()
        start = workflow.index("- name: Package runtime and builder")
        end = workflow.index("Compress-Archive", start)
        block = workflow[start:end]
        self.assertIn("./tools/package_displayxr.ps1 -OutputDirectory $runtime", block)
        self.assertIn("-OutputDirectory $runtime -VerifyOnly", block)
        self.assertIn("$runtime/licenses/displayxr/LICENSE.txt", block)
        self.assertIn("$runtime/licenses/displayxr/NOTICE.txt", block)

    def verify(self, package):
        shell = shutil.which("pwsh") or shutil.which("powershell")
        self.assertIsNotNone(shell)
        return subprocess.run([shell, "-NoProfile", "-File", str(ROOT / "tools/package_displayxr.ps1"),
                               "-OutputDirectory", str(package), "-VerifyOnly"],
                              capture_output=True, text=True, timeout=60)

    def test_real_package_valid(self):
        if PACKAGE is None:
            self.skipTest("--package not supplied")
        result = self.verify(PACKAGE)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("nothing executed or installed", result.stdout)

    def test_real_package_corruption_rejected(self):
        if PACKAGE is None:
            self.skipTest("--package not supplied")
        with tempfile.TemporaryDirectory(prefix="sfe-displayxr-package-") as directory:
            test_package = Path(directory) / "package"
            shutil.copytree(PACKAGE, test_package)
            manifest = json.loads((test_package / "optional-runtimes/displayxr/manifest.json").read_text())
            installer = test_package / "optional-runtimes/displayxr" / manifest["installers"][0]["file"]
            # Same size but incorrect hash: a byte-corrupt unsigned payload
            # cannot be accepted just because a filename/version looks right.
            with installer.open("r+b") as stream:
                old = stream.read(1)
                stream.seek(0)
                stream.write(bytes([old[0] ^ 1]))
            result = self.verify(test_package)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("size/hash mismatch", result.stdout + result.stderr)

    def test_real_package_missing_notice_rejected(self):
        if PACKAGE is None:
            self.skipTest("--package not supplied")
        with tempfile.TemporaryDirectory(prefix="sfe-displayxr-package-") as directory:
            test_package = Path(directory) / "package"
            shutil.copytree(PACKAGE, test_package)
            (test_package / "licenses/displayxr-leia-plugin/NOTICE").unlink()
            result = self.verify(test_package)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("size/hash mismatch", result.stdout + result.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", type=Path)
    arguments, rest = parser.parse_known_args()
    PACKAGE = arguments.package
    unittest.main(argv=[sys.argv[0], *rest])
