"""Asset-free Windows updater sandbox. No networking or real game install."""
from pathlib import Path
import hashlib
import ctypes
from ctypes import wintypes
import os
import struct
import subprocess
import sys
import tempfile
import time
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/package/apply-update.ps1"
FIXTURE = Path(os.environ.get("STARFOX_UPDATE_TEST_FIXTURE", ROOT / "build/current/starfox_update_exit_fixture.exe"))
SCRATCH = ROOT / "build/validation/active"
POWERSHELL = Path(os.environ.get("SystemRoot", "C:/Windows")) / "System32/WindowsPowerShell/v1.0/powershell.exe"


def pe(machine=0x8664):
    data = bytearray(512)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<H", data, 0x84, machine)
    return data


@unittest.skipUnless(sys.platform == "win32", "Windows updater helper")
class AppUpdateTests(unittest.TestCase):
    def setUp(self):
        self.prior_no_ui = os.environ.get('STARFOX_UPDATE_TEST_NO_UI')
        os.environ['STARFOX_UPDATE_TEST_NO_UI'] = '1'
        SCRATCH.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="app-update-", dir=SCRATCH)
        self.root = Path(self.temp.name)
        self.target = self.root / "installed game"
        self.target.mkdir()
        self.work = self.root / "sfe-update-abcd1234"
        self.work.mkdir()
        self.old = pe()
        self.old[-3:] = b"old"
        (self.target / "starfox_pc.exe").write_bytes(self.old)
        for name in ("pregame.cfg", "save.srm", "Starfox-Assets.BIN", "Starfox-MSU1.PAK", "touch-layout.cfg", "user.sfc"):
            (self.target / name).write_bytes(b"private-user-data")
        (self.target / 'dlss').mkdir()
        (self.target / 'dlss/starfox_dlss_native.dll').write_bytes(b'optional-addon-kept')

    def tearDown(self):
        self.temp.cleanup()
        if self.prior_no_ui is None:
            os.environ.pop('STARFOX_UPDATE_TEST_NO_UI', None)
        else:
            os.environ['STARFOX_UPDATE_TEST_NO_UI'] = self.prior_no_ui

    def package(self, entries=None):
        entries = entries or [("starfox_pc.exe", pe()), ("README.md", b"new notes"),
            ("licenses/", b""), ("licenses/test.txt", b"license"),
            ("pregame.cfg", b"DO NOT OVERWRITE"), ("save.srm", b"DO NOT OVERWRITE"),
            ("Starfox-Assets.BIN", b"DO NOT OVERWRITE"), ("user.sfc", b"DO NOT OVERWRITE")]
        path = self.work / "payload.zip"
        with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as archive:
            for name, data in entries:
                if isinstance(name, str):
                    # ZipInfo normally rewrites backslashes on Windows. Keep
                    # the raw hostile spelling to test the actual ZIP reader.
                    entry = zipfile.ZipInfo("placeholder")
                    entry.filename = entry.orig_filename = name
                    name = entry
                archive.writestr(name, data)
        raw = path.read_bytes()
        self.digest = hashlib.sha256(raw).hexdigest()
        self.size = len(raw)

    def args(self, mode="Prepare", digest=None):
        return [str(POWERSHELL), "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "RemoteSigned", "-File", str(SCRIPT),
            "-Mode", mode, "-Work", str(self.work), "-Target", str(self.target),
            "-Digest", digest or self.digest, "-Bytes", str(self.size)]

    def run_prepare(self, success=True, digest=None, reason=None):
        result = subprocess.run(self.args(digest=digest), capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        if reason:
            self.assertIn(reason, (self.work / "error.txt").read_text(encoding="utf-8-sig"))
        self.assertEqual((self.target / "starfox_pc.exe").read_bytes(), self.old)
        self.assert_private()

    def assert_private(self):
        self.assertEqual((self.target / 'dlss/starfox_dlss_native.dll').read_bytes(), b'optional-addon-kept')
        for name in ("pregame.cfg", "save.srm", "Starfox-Assets.BIN", "Starfox-MSU1.PAK", "touch-layout.cfg", "user.sfc"):
            self.assertEqual((self.target / name).read_bytes(), b"private-user-data", name)

    def test_stage_verified_files_not_user_data(self):
        self.package()
        self.run_prepare()
        self.assertTrue((self.work / "stage/starfox_pc.exe").is_file())
        self.assertTrue((self.work / "stage/licenses/test.txt").is_file())
        self.assertFalse((self.work / "stage/pregame.cfg").exists())
        self.assertFalse((self.work / "stage/Starfox-Assets.BIN").exists())

    def test_checksum_rejection(self):
        self.package()
        self.run_prepare(False, "0" * 64, "checksum or file size mismatch")
        self.assertFalse((self.work / "stage").exists())

    def test_new_package_without_builder_and_no_optional_installer(self):
        self.package([('starfox_pc.exe', pe()),
                      ('optional-runtimes/displayxr/README.txt', b'download instructions'),
                      ('optional-runtimes/displayxr/manifest.json', b'{}'),
                      ('optional-runtimes/displayxr/DisplayXRSetup-2.21.11.exe', b'inert fixture')])
        self.run_prepare()
        self.assertTrue((self.work / 'stage/optional-runtimes/displayxr/README.txt').is_file())
        self.assertFalse((self.work / 'stage/starfox_asset_builder.exe').exists())
        self.assertFalse((self.work / 'stage/optional-runtimes/displayxr/DisplayXRSetup-2.21.11.exe').exists())

    def test_wrong_architecture(self):
        self.package([("starfox_pc.exe", pe(0x14C))])
        self.run_prepare(False, reason="CPU architecture does not match")

    def test_traversal_absolute_ads_alias_devices(self):
        for name in ("../escape", "/absolute", "C:/absolute", "licenses/../../escape", "licenses/x:stream", "licenses/CON.txt", "licenses/x. ", "licenses//x", "licenses\\bad", "licenses/x?"):
            with self.subTest(name=name):
                self.package([("starfox_pc.exe", pe()), (name, b"evil")])
                self.run_prepare(False, reason="Unsafe or duplicate path")
                stage = self.work / "stage"
                # Failed path validation happens before any extraction.
                self.assertFalse((stage / "starfox_pc.exe").exists())
                if stage.exists():
                    stage.rmdir()

    def test_duplicate_case_alias(self):
        self.package([("starfox_pc.exe", pe()), ("STARFOX_PC.EXE", pe())])
        self.run_prepare(False, reason="Unsafe or duplicate path")

    def test_symlink_entry(self):
        entry = zipfile.ZipInfo("licenses/link")
        entry.create_system = 3
        entry.external_attr = 0o120777 << 16
        self.package([("starfox_pc.exe", pe()), (entry, b"../../outside")])
        self.run_prepare(False, reason="Unsafe or duplicate path")

    def test_missing_player(self):
        self.package([("other.exe", pe())])
        self.run_prepare(False, reason="game executable missing")

    @unittest.skipUnless(FIXTURE.is_file(), "build starfox_update_exit_fixture first")
    def test_apply_restart_preserves_data_and_cleans_own_files(self):
        self.old = FIXTURE.read_bytes()
        (self.target / "starfox_pc.exe").write_bytes(self.old)
        self.package([("starfox_pc.exe", self.old), ("README.md", b"updated")])
        self.run_prepare()
        parent = subprocess.Popen([str(self.target / "starfox_pc.exe"), "--wait"])
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.OpenProcess.restype = wintypes.HANDLE
        kernel.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        handle = kernel.OpenProcess(0x1000, False, parent.pid)
        times = [wintypes.FILETIME() for _ in range(4)]
        try:
            self.assertTrue(kernel.GetProcessTimes(handle, *[ctypes.byref(x) for x in times]))
            ticks = (times[0].dwHighDateTime << 32) + times[0].dwLowDateTime + 504911232000000000
        finally:
            kernel.CloseHandle(handle)
        helper = subprocess.Popen(self.args("Apply") + ["-ParentId", str(parent.pid), "-ParentStartTicks", str(ticks)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 10
            while not (self.work / "handoff-ready").exists() and time.monotonic() < deadline:
                time.sleep(0.05)
            self.assertTrue((self.work / "handoff-ready").exists(), "installer did not acknowledge handoff")
            self.assertIsNone(parent.poll())
            self.assertFalse((self.target / "README.md").exists(), "installation started before parent exited")
            # Only our tiny, asset-free waiting fixture is terminated.
            parent.terminate()
            parent.wait(timeout=5)
            out, err = helper.communicate(timeout=30)
            self.assertEqual(helper.returncode, 0, out + err)
        finally:
            if parent.poll() is None:
                parent.terminate()
            parent.wait(timeout=5)
            if helper.poll() is None:
                helper.terminate()
            helper.wait(timeout=5)
        deadline = time.monotonic() + 5
        while not (self.target / "update-started.txt").exists() and time.monotonic() < deadline:
            time.sleep(0.05)
        self.assertTrue((self.target / "update-started.txt").exists())
        self.assertEqual((self.target / "README.md").read_bytes(), b"updated")
        self.assertFalse(self.work.exists())
        self.assertFalse(any(self.target.glob("*-rollback")))
        self.assert_private()

    def test_failed_restart_rolls_back(self):
        self.package()
        self.run_prepare()
        # This intentionally non-runnable PE passes the architecture probe but
        # fails to launch. Wait for the error receipt after rollback, then stop
        # only this sandbox helper so its error dialog cannot block automation.
        process = subprocess.Popen(self.args("Apply") + ["-ParentId", "2147483000", "-ParentStartTicks", "1"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            deadline = time.monotonic() + 20
            while not (self.work / "error.txt").exists() and time.monotonic() < deadline:
                time.sleep(0.05)
            self.assertTrue((self.work / "error.txt").exists(), "rollback never completed")
            self.assertRegex((self.work / "error.txt").read_text(encoding="utf-8-sig"), r"Rollback restored [1-9][0-9]* file operations")
            self.assertEqual((self.target / "starfox_pc.exe").read_bytes(), self.old)
            self.assertFalse((self.target / "README.md").exists())
            self.assert_private()
        finally:
            if process.poll() is None:
                process.terminate()
            process.wait(timeout=5)


if __name__ == "__main__":
    unittest.main()
