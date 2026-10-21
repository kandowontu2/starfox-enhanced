"""Tiny portable ZIP fixtures; native Linux tests additionally exercise restart."""
from pathlib import Path
import hashlib
import importlib.util
import io
import json
import os
import stat
import struct
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("linux_updater", ROOT / "tools/package/apply-update-linux.py")
updater = importlib.util.module_from_spec(spec)
spec.loader.exec_module(updater)
SCRATCH = ROOT / "build/validation/active"
FIXTURE = Path(os.environ.get("STARFOX_UPDATE_TEST_FIXTURE", ROOT / "build/current/starfox_update_exit_fixture"))


def elf(machine=62):
    data = bytearray(64)
    data[:6] = b"\x7fELF\x02\x01"
    struct.pack_into("<H", data, 18, machine)
    return bytes(data)


class LinuxUpdateTests(unittest.TestCase):
    def setUp(self):
        SCRATCH.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(prefix="linux-update-", dir=SCRATCH)
        self.root = Path(self.temp.name)
        self.target = self.root / "installed game"
        self.target.mkdir()
        self.work = self.root / "sfe-update-abcd1234"
        self.work.mkdir(mode=0o700)
        (self.target / "starfox_pc").write_bytes(elf())
        self.private = ("pregame.cfg", "starfox-ex.srm", "input-bindings.cfg", "hud-layout.cfg", "touch-layout.cfg", "Starfox-Assets.BIN", "Starfox-MSU1.PAK", "user.sfc")
        for name in self.private:
            (self.target / name).write_bytes(b"private-user-data")

    def tearDown(self):
        self.temp.cleanup()

    def package(self, entries=None, nested=True):
        if entries is None:
            entries = [("starfox_pc", elf()), ("README.md", b"new notes"), ("licenses/test.txt", b"license")]
            entries += [(name, b"never overwrite") for name in self.private]
        with zipfile.ZipFile(self.work / "payload.zip", "w", zipfile.ZIP_DEFLATED) as archive:
            for name, data in entries:
                entry = zipfile.ZipInfo("placeholder")
                entry.filename = entry.orig_filename = (updater.PREFIX if nested else "") + name
                archive.writestr(entry, data)
        raw = (self.work / "payload.zip").read_bytes()
        self.digest, self.size = hashlib.sha256(raw).hexdigest(), len(raw)

    def prepare(self):
        updater.prepare(self.work, self.target, self.digest, self.size)

    def assert_private(self):
        for name in self.private:
            self.assertEqual((self.target / name).read_bytes(), b"private-user-data")

    def test_nested_release_layout_and_preservation(self):
        self.package()
        self.prepare()
        self.assertTrue((self.work / "stage/starfox_pc").is_file())
        self.assertTrue((self.work / "stage/licenses/test.txt").is_file())
        for name in self.private:
            self.assertFalse((self.work / "stage" / name).exists())
        self.assert_private()

    def test_future_flat_layout(self):
        self.package(nested=False)
        self.prepare()

    def test_hash_rejection_before_extraction(self):
        self.package()
        self.digest = "0" * 64
        with self.assertRaisesRegex(ValueError, "checksum"):
            self.prepare()
        self.assertFalse((self.work / "stage").exists())

    def test_wrong_cpu_rejected(self):
        self.package([("starfox_pc", elf(183))])
        with self.assertRaisesRegex(ValueError, "CPU architecture"):
            self.prepare()

    def test_unsafe_paths(self):
        for name in ("../outside", "/absolute", "licenses/../../outside", "licenses//x", "licenses/x:stream", "licenses\\x", "licenses/x\0bad", "licenses/x. "):
            with self.subTest(name=name):
                archive = io.BytesIO()
                with zipfile.ZipFile(archive, "w") as output:
                    output.writestr("starfox_pc", elf())
                    entry = zipfile.ZipInfo("placeholder")
                    entry.filename = entry.orig_filename = name
                    output.writestr(entry, b"bad")
                with zipfile.ZipFile(io.BytesIO(archive.getvalue())) as source:
                    with self.assertRaises(ValueError):
                        updater.inventory(source)

    def test_duplicate_alias_and_special_files(self):
        for names in (("starfox_pc", "STARFOX_PC"), ("starfox_pc", updater.PREFIX + "starfox_pc")):
            archive = io.BytesIO()
            with zipfile.ZipFile(archive, "w") as output:
                for name in names:
                    output.writestr(name, elf())
            with zipfile.ZipFile(io.BytesIO(archive.getvalue())) as source:
                with self.assertRaisesRegex(ValueError, "Duplicate"):
                    updater.inventory(source)
        for mode in (stat.S_IFLNK, stat.S_IFIFO, stat.S_IFSOCK):
            archive = io.BytesIO()
            with zipfile.ZipFile(archive, "w") as output:
                entry = zipfile.ZipInfo("starfox_pc")
                entry.external_attr = (mode | 0o777) << 16
                output.writestr(entry, elf())
            with zipfile.ZipFile(io.BytesIO(archive.getvalue())) as source:
                with self.assertRaisesRegex(ValueError, "special files"):
                    updater.inventory(source)

    def test_redirect_policy(self):
        redirect = updater.SafeRedirect()
        request = updater.urllib.request.Request(updater.REPOSITORY)
        for url in ("http://github.com/foo", "https://evil.example/foo", "https://github.com.evil.example/foo", "https://user:password@github.com/foo", "https://github.com:444/foo"):
            with self.assertRaisesRegex(ValueError, "Unsafe"):
                redirect.redirect_request(request, None, 302, "", {}, url)

    def test_download_policy_rejects_arbitrary_url(self):
        with self.assertRaisesRegex(ValueError, "Unexpected"):
            updater.download(self.work, "https://evil.example/game.zip", 100)

    @unittest.skipUnless(sys.platform.startswith("linux"), "native Linux filesystem policy")
    def test_source_and_package_managed_installs_are_manual(self):
        with self.assertRaisesRegex(ValueError, "Package-managed"):
            updater.eligible(Path("/usr/bin"))
        with self.assertRaisesRegex(ValueError, "Source checkout"):
            updater.eligible(self.target)
        with self.assertRaisesRegex(ValueError, "Package-managed"):
            updater.eligible(self.root / "steamapps/common/game")

    @unittest.skipUnless(sys.platform.startswith("linux"), "native Linux ownership and restart")
    def test_restart_waits_for_parent_preserves_data(self):
        self.assertTrue(FIXTURE.is_file(), "native restart fixture missing")
        binary = FIXTURE.read_bytes()
        (self.target / "starfox_pc").write_bytes(binary)
        (self.target / "starfox_pc").chmod(0o755)
        self.package([("starfox_pc", binary), ("README.md", b"updated")])
        self.prepare()
        parent = subprocess.Popen([str(self.target / "starfox_pc"), "--wait"])
        try:
            identity = updater.process_identity(parent.pid)
            self.assertIsNotNone(identity)
            # Fixtures live in the source checkout per the owner's scratch rule;
            # eligibility is separately tested, not bypassed in shipped CLI.
            with mock.patch.object(updater, "eligible"):
                import threading
                errors = []
                def run():
                    try:
                        updater.apply(self.work, self.target, self.digest, self.size, parent.pid, identity[0])
                    except Exception as error:
                        errors.append(error)
                thread = threading.Thread(target=run)
                thread.start()
                deadline = time.monotonic() + 5
                while not (self.work / "handoff-ready").exists() and time.monotonic() < deadline:
                    time.sleep(0.02)
                self.assertTrue((self.work / "handoff-ready").exists())
                self.assertFalse((self.target / "README.md").exists())
                parent.terminate()
                parent.wait(timeout=5)
                thread.join(timeout=10)
                self.assertFalse(thread.is_alive())
                self.assertEqual(errors, [])
        finally:
            if parent.poll() is None:
                parent.terminate()
            parent.wait(timeout=5)
        deadline = time.monotonic() + 5
        while not (self.target / "update-started.txt").exists() and time.monotonic() < deadline:
            time.sleep(0.02)
        self.assertTrue((self.target / "update-started.txt").exists())
        self.assertEqual((self.target / "README.md").read_bytes(), b"updated")
        self.assertTrue(os.access(self.target / "starfox_pc", os.X_OK))
        self.assertFalse(self.work.exists())
        self.assert_private()

    @unittest.skipUnless(sys.platform.startswith("linux"), "native Linux rollback")
    def test_launch_failure_rolls_back(self):
        self.package()
        self.prepare()
        with mock.patch.object(updater, "eligible"), mock.patch.object(updater, "process_identity", return_value=None):
            with self.assertRaisesRegex(RuntimeError, "rollback attempted for 3 files"):
                updater.apply(self.work, self.target, self.digest, self.size, 1234, "1", launch=mock.Mock(side_effect=OSError("fixture launch failed")))
        self.assertEqual((self.target / "starfox_pc").read_bytes(), elf())
        self.assertFalse((self.target / "README.md").exists())
        self.assert_private()


if __name__ == "__main__":
    unittest.main()
