from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest


FIXTURE = Path(sys.argv.pop(1)).resolve()
LAUNCHER = Path(__file__).resolve().parents[1] / "tools/package/LAUNCH-PCVR.bat"
BUILDER_LAUNCHER = LAUNCHER.with_name("BUILD-ASSETS.bat")


class PcvrLauncherTests(unittest.TestCase):
    def run_launcher(self, code, *, assets=True):
        # The fixture is a real native process returning the requested exit
        # code. No headset, game, ROM or crash is involved in these tests.
        with tempfile.TemporaryDirectory(prefix="sfe pcvr launcher ") as directory:
            root = Path(directory)
            shutil.copyfile(LAUNCHER, root / LAUNCHER.name)
            shutil.copyfile(FIXTURE, root / "starfox_pcvr.exe")
            if assets:
                (root / "Starfox-Assets.BIN").write_bytes(b"launcher fixture only")
            environment = os.environ.copy()
            environment["SFE_PCVR_TEST_EXIT_CODE"] = str(code)
            # cmd.exe does not interpret list2cmdline's backslash-escaped
            # quotes like a C runtime. Give it its own literal command syntax.
            comspec = os.environ.get("COMSPEC", "cmd.exe")
            result = subprocess.run(
                f'"{comspec}" /d /c call LAUNCH-PCVR.bat --fixture "argument with spaces"',
                cwd=root, env=environment, input="\n", text=True,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=15)
            log = root / "pcvr-log.txt"
            return result, log.read_text() if log.exists() else None

    def check_exit(self, code):
        result, log = self.run_launcher(code)
        self.assertEqual(result.returncode & 0xFFFFFFFF, code & 0xFFFFFFFF, result.stdout)
        self.assertIsNotNone(log)
        self.assertIn("PCVR launcher fixture invoked", log)
        self.assertIn("PCVR fixture stderr retained", log)
        self.assertIn("arg[1]=--fixture", log)
        self.assertIn("arg[2]=argument with spaces", log)
        self.assertIn(f"PCVR exit code: {code}", log)
        if code:
            self.assertIn(f"PCVR exited with error code {code}.", result.stdout)
            self.assertIn("PCVR launcher fixture invoked", result.stdout)
            self.assertIn("The full log is saved", result.stdout)
        else:
            self.assertNotIn("PCVR exited with error", result.stdout)
            self.assertNotIn("The full log is saved", result.stdout)

    def test_success(self):
        self.check_exit(0)

    def test_positive_failure(self):
        self.check_exit(7)

    def test_access_violation_exit(self):
        self.check_exit(-1073741819)

    def test_abort_exit(self):
        self.check_exit(-1073740791)

    def test_missing_assets(self):
        result, log = self.run_launcher(0, assets=False)
        self.assertEqual(result.returncode, 2, result.stdout)
        self.assertIsNone(log)
        self.assertIn("Starfox-Assets.BIN is missing.", result.stdout)
        self.assertNotIn("PCVR launcher fixture invoked", result.stdout)


class AssetBuilderLauncherTests(unittest.TestCase):
    def run_builder(self, code, *, builder=True, rom=True):
        with tempfile.TemporaryDirectory(prefix="sfe asset launcher ") as directory:
            root = Path(directory)
            shutil.copyfile(BUILDER_LAUNCHER, root / BUILDER_LAUNCHER.name)
            if builder:
                shutil.copyfile(FIXTURE, root / "starfox_asset_builder.exe")
            environment = os.environ.copy()
            environment["SFE_PCVR_TEST_EXIT_CODE"] = str(code)
            argument = ' "retail rom.sfc"' if rom else ""
            comspec = os.environ.get("COMSPEC", "cmd.exe")
            return subprocess.run(
                f'"{comspec}" /d /c call BUILD-ASSETS.bat{argument}',
                cwd=root, env=environment, input="\n", text=True,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=15)

    def check_exit(self, code):
        # Only process/error/argument handling: this fixture does not build a BIN.
        result = self.run_builder(code)
        self.assertEqual(result.returncode & 0xFFFFFFFF, code & 0xFFFFFFFF, result.stdout)
        self.assertIn("arg[1]=retail rom.sfc", result.stdout)
        self.assertIn("Starfox-Assets.BIN", result.stdout)
        self.assertIn("PCVR fixture stderr retained", result.stdout)
        if code:
            self.assertIn(f"Asset creation failed with exit code {code}.", result.stdout)
            self.assertNotIn("Assets are ready", result.stdout)
        else:
            self.assertIn("Assets are ready", result.stdout)
            self.assertNotIn("Asset creation failed", result.stdout)

    def test_success(self):
        self.check_exit(0)

    def test_positive_failure(self):
        self.check_exit(7)

    def test_negative_failure(self):
        self.check_exit(-1073741819)

    def test_missing_builder(self):
        result = self.run_builder(0, builder=False)
        self.assertEqual(result.returncode, 2, result.stdout)
        self.assertIn("The asset builder is missing", result.stdout)
        self.assertNotIn("fixture invoked", result.stdout)

    def test_missing_rom_argument(self):
        result = self.run_builder(0, rom=False)
        self.assertEqual(result.returncode, 2, result.stdout)
        self.assertIn("Drag your own supported", result.stdout)
        self.assertNotIn("fixture invoked", result.stdout)


if __name__ == "__main__":
    unittest.main()
