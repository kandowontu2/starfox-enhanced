"""Package/verify unsigned device IPAs; never rebuild or sign the application."""
import argparse
from copy import copy
import hashlib
import json
from pathlib import Path, PurePosixPath
import plistlib
import stat
import struct
import tempfile
import zipfile


def safe_name(name):
    parts = PurePosixPath(name).parts
    if not parts or name.startswith('/') or '\\' in name or '..' in parts:
        raise ValueError(f'Unsafe archive path: {name}')


def verify(path):
    with zipfile.ZipFile(path) as archive:
        entries = archive.infolist()
        names = [entry.filename for entry in entries]
        for name in names:
            safe_name(name)
            if not name.startswith('Payload/'):
                raise ValueError('IPA must contain Payload at its root, without an outer package folder')
        if len(set(names)) != len(names):
            raise ValueError('Duplicate IPA archive entries')
        apps = {PurePosixPath(name).parts[1] for name in names
                if len(PurePosixPath(name).parts) > 1 and PurePosixPath(name).parts[1].endswith('.app')}
        if len(apps) != 1:
            raise ValueError('IPA must contain exactly one application')
        app = 'Payload/' + apps.pop()
        info = plistlib.loads(archive.read(app + '/Info.plist'))
        executable = info.get('CFBundleExecutable', '')
        if not executable or PurePosixPath(executable).name != executable or '\\' in executable:
            raise ValueError('Invalid application executable name')
        if 'iPhoneOS' not in info.get('CFBundleSupportedPlatforms', []):
            raise ValueError('Application is not an iOS device bundle')
        if not info.get('CFBundleIdentifier'):
            raise ValueError('Application bundle identifier missing')
        binary = archive.read(app + '/' + executable)
        if len(binary) < 32 or struct.unpack_from('<II', binary) != (0xfeedfacf, 0x100000c):
            raise ValueError('Application executable is not a 64-bit arm64 Mach-O')
        _, _, _, filetype, count, size, _, _ = struct.unpack_from('<8I', binary)
        if filetype != 2 or size > len(binary) - 32:
            raise ValueError('Invalid Mach-O executable/load-command bounds')
        offset = 32
        ios = False
        for _ in range(count):
            if offset + 8 > 32 + size:
                raise ValueError('Truncated Mach-O load command')
            command, length = struct.unpack_from('<II', binary, offset)
            if length < 8 or offset + length > 32 + size:
                raise ValueError('Invalid Mach-O load command')
            if command == 0x32 and length >= 24:
                ios |= struct.unpack_from('<I', binary, offset + 8)[0] == 2
            if command == 0x25 and length >= 16:  # LC_VERSION_MIN_IPHONEOS
                ios = True
            offset += length
        if not ios or offset != 32 + size:
            raise ValueError('Mach-O is not built for physical iOS devices')
        damaged = archive.testzip()
        if damaged:
            raise ValueError(f'Damaged IPA entry: {damaged}')
        return {'app': app, 'identifier': info['CFBundleIdentifier'],
                'minimum_ios': info.get('MinimumOSVersion'), 'entries': len(entries),
                'executable_sha256': hashlib.sha256(binary).hexdigest(),
                'ipa_sha256': hashlib.sha256(Path(path).read_bytes()).hexdigest()}


def package(output, *, source=None, app=None):
    output = Path(output)
    if output.suffix.lower() != '.ipa':
        raise ValueError('Output must have the .ipa extension')
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        raise ValueError('Refusing to overwrite an existing IPA')
    with tempfile.NamedTemporaryFile(dir=output.parent, suffix='.ipa', delete=False) as pending_file:
        pending = Path(pending_file.name)
    try:
        with zipfile.ZipFile(pending, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as destination:
            if source:
                with zipfile.ZipFile(source) as archive:
                    matches = [entry.filename for entry in archive.infolist()
                               if entry.filename.endswith('/Payload/StarFoxEnhanced.app/Info.plist')
                               or entry.filename == 'Payload/StarFoxEnhanced.app/Info.plist']
                    if len(matches) != 1:
                        raise ValueError('Source archive must contain one StarFoxEnhanced device app')
                    prefix = matches[0][:-len('Payload/StarFoxEnhanced.app/Info.plist')]
                    for entry in archive.infolist():
                        safe_name(entry.filename)
                        if not entry.filename.startswith(prefix + 'Payload/'):
                            continue
                        target = copy(entry)
                        target.filename = entry.filename[len(prefix):]
                        destination.writestr(target, archive.read(entry))
            else:
                app = Path(app)
                if not app.is_dir() or app.suffix != '.app':
                    raise ValueError('Input must be an application directory')
                for path in sorted(app.rglob('*')):
                    name = 'Payload/' + app.name + '/' + path.relative_to(app).as_posix()
                    if path.is_symlink():
                        target = path.readlink().as_posix()
                        safe_name(target)
                        entry = zipfile.ZipInfo(name)
                        entry.create_system = 3
                        entry.external_attr = (stat.S_IFLNK | 0o777) << 16
                        destination.writestr(entry, target.encode())
                    else:
                        destination.write(path, name)
        result = verify(pending)
        pending.replace(output)
        return result
    finally:
        pending.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--source', type=Path, help='Repack an existing CI/release ZIP without changing the app')
    group.add_argument('--app', type=Path, help='Package a built device .app directory')
    group.add_argument('--verify', type=Path, help='Verify an existing IPA')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if not args.verify and not args.output:
        parser.error('--output required when packaging')
    try:
        result = verify(args.verify) if args.verify else package(args.output, source=args.source, app=args.app)
    except (ValueError, OSError, KeyError, zipfile.BadZipFile, plistlib.InvalidFileException) as error:
        parser.exit(1, f'Invalid iOS package: {error}\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
