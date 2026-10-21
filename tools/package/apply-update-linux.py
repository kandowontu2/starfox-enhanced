#!/usr/bin/env python3
"""Embedded portable Linux updater. Standard library only; never runs a shell."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import struct
import subprocess
import sys
import time
import urllib.parse
import urllib.request
import zipfile

MAX_EXPANDED = 8 * 1024**3
RESERVE = 256 * 1024**2
PREFIX = "dist/StarFoxEnhanced-linux-x64/"
REPOSITORY = "https://github.com/kandowontu2/starfox-enhanced"
MANAGED = ("/usr", "/opt", "/app", "/snap", "/nix", "/var/lib/flatpak")


def no_links(path):
    path = Path(path).absolute()
    for part in (path, *path.parents):
        if part.is_symlink():
            raise ValueError("Update paths must not contain symbolic links")


def safe_name(name):
    if not name or name.startswith("/") or re.search(r"[\\:\x00-\x1f<>\"|?*]", name):
        raise ValueError("Unsafe update ZIP path")
    parts = name.rstrip("/").split("/")
    if any(not part or part in (".", "..") or part.endswith((".", " ")) for part in parts):
        raise ValueError("Unsafe update ZIP path")
    return name.rstrip("/")


def child(root, name):
    safe_name(name)
    return root.joinpath(*PurePosixPath(name).parts)


def package_file(name):
    return name in ("starfox_pc", "starfox_asset_builder", "README.md", "CREDITS.md",
                    "THIRD_PARTY_NOTICES.md", "LICENSE-XBRZ.txt") or name.startswith("licenses/")


def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def machine(path):
    with path.open("rb") as stream:
        header = stream.read(64)
    if len(header) < 64 or header[:6] != b"\x7fELF\x02\x01":
        raise ValueError("Update is not a 64-bit little-endian ELF executable")
    value = struct.unpack_from("<H", header, 18)[0]
    if value != 62:
        raise ValueError("Update CPU architecture does not match Linux x64")
    return value


def eligible(target):
    target = Path(target).absolute()
    no_links(target)
    if target == Path(target.anchor) or any(target == Path(p) or Path(p) in target.parents for p in MANAGED):
        raise ValueError("Package-managed installations must use their normal installer")
    if any((part / ".git").exists() for part in (target, *target.parents)):
        raise ValueError("Source checkouts must be updated manually")
    if os.geteuid() == 0 or target.stat().st_uid != os.geteuid():
        raise ValueError("Updates require a user-owned portable installation")
    executable = target / "starfox_pc"
    no_links(executable)
    if not executable.is_file() or not os.access(target, os.W_OK) or executable.stat().st_uid != os.geteuid():
        raise ValueError("Portable installation is not writable")
    machine(executable)


def workspace(work, target):
    work, target = Path(work).absolute(), Path(target).absolute()
    no_links(work)
    no_links(target)
    if (not re.fullmatch(r"sfe-update-[0-9a-f]+", work.name) or work == target
            or work in target.parents or target in work.parents):
        raise ValueError("Unsafe update workspace")
    if os.name == "posix" and (work.stat().st_uid != os.geteuid() or stat.S_IMODE(work.stat().st_mode) & 0o077):
        raise ValueError("Update workspace must be private to the current user")


def inventory(archive):
    entries = archive.infolist()
    if len(entries) > 20000:
        raise ValueError("Update contains too many files")
    seen, output, expanded = set(), [], 0
    for entry in entries:
        raw = safe_name(entry.orig_filename)
        if entry.orig_filename != entry.filename:
            raise ValueError("Ambiguous update path")
        mode = entry.external_attr >> 16
        if stat.S_IFMT(mode) not in (0, stat.S_IFREG, stat.S_IFDIR):
            raise ValueError("Links and special files are not allowed in updates")
        if entry.flag_bits & 1:
            raise ValueError("Encrypted update entries are not supported")
        if entry.file_size > MAX_EXPANDED - expanded:
            raise ValueError("Update exceeds the extraction limit")
        expanded += entry.file_size
        # Support both existing nested release ZIPs and future flat ZIPs.
        name = raw[len(PREFIX):] if raw.startswith(PREFIX) else raw
        if name.casefold() in seen:
            raise ValueError("Duplicate update path")
        seen.add(name.casefold())
        if not entry.is_dir() and package_file(name):
            output.append((entry, name))
    if not any(name == "starfox_pc" for _, name in output):
        raise ValueError("Wrong update package: Linux player is missing")
    return output, expanded


def prepare(work, target, digest, size):
    workspace(work, target)
    archive_path = work / "payload.zip"
    if (not re.fullmatch(r"[0-9a-f]{64}", digest) or not 0 < size <= 4 * 1024**3
            or archive_path.stat().st_size != size or sha(archive_path) != digest):
        raise ValueError("Update checksum or size mismatch")
    stage = work / "stage"
    stage.mkdir(mode=0o700)  # Refuse stale staging directories.
    records = []
    with zipfile.ZipFile(archive_path) as archive:
        entries, expanded = inventory(archive)
        if shutil.disk_usage(work).free < expanded + RESERVE or shutil.disk_usage(target).free < 2 * expanded + RESERVE:
            raise ValueError("Not enough space for staging and rollback")
        for entry, name in entries:
            destination = child(stage, name)
            destination.parent.mkdir(parents=True, exist_ok=True)
            actual = 0
            with archive.open(entry) as source, destination.open("xb") as output:
                for block in iter(lambda: source.read(65536), b""):
                    actual += len(block)
                    if actual > entry.file_size:
                        raise ValueError("Expanded ZIP size mismatch")
                    output.write(block)
            if actual != entry.file_size:
                raise ValueError("Truncated ZIP entry")
            destination.chmod(0o755 if name in ("starfox_pc", "starfox_asset_builder") else 0o644)
            records.append({"name": name, "hash": sha(destination)})
    if machine(stage / "starfox_pc") != machine(target / "starfox_pc"):
        raise ValueError("Update CPU architecture does not match")
    with (work / "prepared.json").open("x", encoding="utf-8") as stream:
        json.dump(records, stream)


def process_identity(pid):
    try:
        # comm may contain spaces or ')'; fields after its final ')' begin at 3.
        tail = Path("/proc/{}/stat".format(pid)).read_text().rsplit(")", 1)[1].split()
        if tail[0] == "Z":
            return None
        return tail[19], Path(os.readlink("/proc/{}/exe".format(pid)))
    except FileNotFoundError:
        return None


def apply(work, target, digest, size, parent_id, parent_start, launch=None):
    workspace(work, target)
    eligible(target)
    if parent_id <= 0 or not parent_start.isdigit():
        raise ValueError("Missing parent process identity")
    expected = (parent_start, target / "starfox_pc")
    identity = process_identity(parent_id)
    if identity is not None and identity != expected:
        raise ValueError("Game process identity changed")
    (work / "handoff-ready").write_text("ready", encoding="ascii")
    deadline = time.monotonic() + 120
    while identity is not None:
        if identity != expected or time.monotonic() > deadline:
            raise ValueError("Game has not exited safely; nothing was installed")
        time.sleep(0.05)
        identity = process_identity(parent_id)
    if (work / "payload.zip").stat().st_size != size or sha(work / "payload.zip") != digest:
        raise ValueError("Downloaded update was altered")
    stage = work / "stage"
    no_links(stage)
    manifest_path = work / "prepared.json"
    if manifest_path.stat().st_size > 8 * 1024**2:
        raise ValueError("Invalid update inventory")
    records = json.loads(manifest_path.read_text(encoding="utf-8"))
    if not isinstance(records, list) or not 0 < len(records) <= 20000:
        raise ValueError("Invalid update inventory")
    names, expanded = set(), 0
    for record in records:
        name = safe_name(record["name"])
        if not package_file(name) or name.casefold() in names:
            raise ValueError("Invalid staged update file")
        names.add(name.casefold())
        source, destination = child(stage, name), child(target, name)
        no_links(source)
        no_links(destination)
        if not source.is_file() or sha(source) != record["hash"]:
            raise ValueError("Staged update was altered")
        expanded += source.stat().st_size
    if "starfox_pc" not in names or expanded > MAX_EXPANDED or shutil.disk_usage(target).free < 2 * expanded + RESERVE:
        raise ValueError("Incomplete update or insufficient rollback space")
    machine(stage / "starfox_pc")
    backup = target / (work.name + "-rollback")
    backup.mkdir(mode=0o700)
    changed = []
    try:
        for record in records:
            name = record["name"]
            incoming = child(backup / "incoming", name)
            incoming.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(child(stage, name), incoming)
            if sha(incoming) != record["hash"]:
                raise ValueError("Update copy failed verification")
            incoming.chmod(0o755 if name in ("starfox_pc", "starfox_asset_builder") else 0o644)
        for record in records:
            name = record["name"]
            destination, old = child(target, name), child(backup / "original", name)
            no_links(destination)
            destination.parent.mkdir(parents=True, exist_ok=True)
            old.parent.mkdir(parents=True, exist_ok=True)
            changed.append((name, destination.exists()))
            if destination.exists():
                os.replace(destination, old)
            os.replace(child(backup / "incoming", name), destination)
        (launch or subprocess.Popen)([str(target / "starfox_pc")], cwd=str(target), start_new_session=True)
    except Exception as error:
        failures = []
        for name, existed in reversed(changed):
            destination, old = child(target, name), child(backup / "original", name)
            try:
                if old.exists():
                    os.replace(old, destination)
                elif not existed and not child(backup / "incoming", name).exists():
                    destination.unlink()
            except OSError as failure:
                failures.append(str(failure))
        raise RuntimeError("{}; rollback attempted for {} files; backup: {}{}".format(
            error, len(changed), backup, "; RECOVERY NEEDED: " + str(failures) if failures else "")) from error
    # A cleanup failure after launch must not roll back a running new player.
    try:
        shutil.rmtree(backup)
        shutil.rmtree(work)
    except OSError:
        pass


class SafeRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, msg, headers, newurl):
        url = urllib.parse.urlsplit(newurl)
        if (url.scheme != "https" or url.port not in (None, 443) or url.username or url.password
                or url.hostname not in ("github.com", "release-assets.githubusercontent.com")):
            raise ValueError("Unsafe update redirect")
        return super().redirect_request(request, fp, code, msg, headers, newurl)


def download(work, url, size):
    if not re.fullmatch(re.escape(REPOSITORY) + r"/releases/download/v?[0-9]+(?:\.[0-9]+){2,3}/StarFoxEnhanced-[0-9]+(?:\.[0-9]+){2,3}-linux-x64\.zip", url):
        raise ValueError("Unexpected Linux update URL")
    if not 0 < size <= 4 * 1024**3 or shutil.disk_usage(work).free < 4 * size + RESERVE:
        raise ValueError("Invalid update size or not enough disk space")
    opener = urllib.request.build_opener(SafeRedirect())
    request = urllib.request.Request(url, headers={"User-Agent": "StarFoxEnhanced-Updater/1"})
    total, percent = 0, -1
    deadline = time.monotonic() + 15 * 60
    with opener.open(request, timeout=10) as response, (work / "payload.zip").open("xb") as output:
        if response.status != 200:
            raise ValueError("Update server returned an error")
        for block in iter(lambda: response.read(65536), b""):
            total += len(block)
            if total > size or time.monotonic() > deadline:
                raise ValueError("Download size limit or timeout exceeded")
            output.write(block)
            value = total * 100 // size
            if value != percent:
                percent = value
                (work / "progress").write_text(str(value), encoding="ascii")
    if total != size:
        raise ValueError("Incomplete update download")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("prepare", "apply"))
    parser.add_argument("work", type=Path)
    parser.add_argument("target", type=Path)
    parser.add_argument("digest")
    parser.add_argument("size", type=int)
    parser.add_argument("--url", default="")
    parser.add_argument("--parent-id", type=int, default=0)
    parser.add_argument("--parent-start", default="")
    args = parser.parse_args()
    try:
        workspace(args.work, args.target)
        eligible(args.target)
        if args.mode == "prepare":
            download(args.work, args.url, args.size)
            prepare(args.work, args.target, args.digest, args.size)
        else:
            os.setsid()  # Do not inherit a terminal/Steam process-group teardown.
            apply(args.work, args.target, args.digest, args.size, args.parent_id, args.parent_start)
    except Exception as error:
        try:
            (args.work / "error.txt").write_text(str(error), encoding="utf-8")
        except OSError:
            pass
        if args.mode == "apply":
            try:
                (args.work.parent / "last-install-error.txt").write_text(str(error)[:4096], encoding="utf-8")
                # If rollback completed and the old game has exited, reopen
                # the intact player. Its update screen reports this failure.
                if "RECOVERY NEEDED" not in str(error) and process_identity(args.parent_id) is None:
                    eligible(args.target)
                    subprocess.Popen([str(args.target / "starfox_pc")], cwd=str(args.target), start_new_session=True)
                for binary, options in (("/usr/bin/kdialog", ["--error"]), ("/usr/bin/zenity", ["--error", "--text"])):
                    if Path(binary).is_file():
                        subprocess.Popen([binary] + options + ["Star Fox Enhanced update failed. The original installation was kept.\n" + str(error)])
                        break
            except OSError:
                pass
            except ValueError:
                pass
        print("Update failed: {}".format(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
