"""Inspect the actual application's four complete SDK libraries in read-only native sections.

This validates source/compile/link embedding only, not Metal loading, GPU output,
frame cost or a physical device. The existing app's deployment minimum is kept.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import plistlib
import struct
import zipfile

SOURCES = {
    'shadow': 'a88d290f7f0ac5521be1b3bea29f4b6afeb177095a404c8a328592a7cbe107ab',
    'reflection': '7f17f15a744ece3cfef89a4cb8734a1f091ca72d98730015793aa73711931b90',
    'empty_shadow': 'ec147e2968e1109d189a153f51b921625cb332441f5308e1ae9e396743207427',
    'empty_reflection': 'bbaf6771fa7fbc47908cd758baac4694065eb9ac7fefc0d2bbcd37cd53f91d3e',
}


def readonly_constants(stream, base, size, expected_cpu):
    stream.seek(base)
    magic, cpu, subtype, kind, commands, extent, flags, reserved = struct.unpack('<IiiIIIII', stream.read(32))
    if magic != 0xfeedfacf or cpu != expected_cpu or kind != 2 or not 0 < commands <= 4096 or extent > 1 << 20:
        raise RuntimeError('Wrong or unbounded actual native application')
    data = stream.read(extent)
    at, found, deployment = 0, [], []
    for _ in range(commands):
        command, length = struct.unpack_from('<II', data, at)
        if length < 8 or at + length > extent:
            raise RuntimeError('Malformed native load command')
        if command == 0x32:
            deployment.append(struct.unpack_from('<II', data, at + 8))
        if command == 0x19:
            if length < 72:
                raise RuntimeError('Truncated native segment')
            name = data[at+8:at+24].rstrip(b'\0')
            writable = struct.unpack_from('<I', data, at + 60)[0] & 2
            count = struct.unpack_from('<I', data, at + 64)[0]
            if 72 + count * 80 != length:
                raise RuntimeError('Malformed native section inventory')
            for i in range(count):
                start = at + 72 + i * 80
                section = data[start:start+16].rstrip(b'\0')
                if name == b'__TEXT' and section == b'__const':
                    if writable:
                        raise RuntimeError('Library section is writable')
                    address, amount, offset = struct.unpack_from('<QQI', data, start + 32)
                    if amount > 1 << 29 or offset + amount > size:
                        raise RuntimeError('Library section exceeds actual native slice')
                    stream.seek(base + offset)
                    constants = stream.read(amount)
                    if len(constants) != amount:
                        raise RuntimeError('Truncated actual read-only section')
                    found.append((address, constants))
        at += length
    if at != extent or len(found) != 1 or len(deployment) != 1:
        raise RuntimeError('Missing exact native section/deployment inventory')
    return found[0], deployment[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', choices=['macosx', 'iphoneos'], required=True)
    parser.add_argument('--generated-dir', type=Path, required=True)
    parser.add_argument('--package', type=Path, required=True)
    args = parser.parse_args()
    programs = []
    for name, source_sha in SOURCES.items():
        source = (args.generated_dir / f'{name}.metal').read_bytes()
        if hashlib.sha256(source).hexdigest() != source_sha:
            raise RuntimeError('Exact full runtime shader source changed: ' + name)
        payload = (args.generated_dir / f'{name}.metallib').read_bytes()
        entry = 'starfox_hardware_' + ('reflection' if name.endswith('reflection') else 'shadow')
        if len(payload) < 20 or payload[:4] != b'MTLB' or entry.encode() + b'\0' not in payload:
            raise RuntimeError('Actual compiled native library/entry is missing: ' + name)
        programs.append((name, source_sha, payload))
    with zipfile.ZipFile(args.package) as archive:
        prefix = 'Payload/StarFoxEnhanced.app/' if args.sdk == 'iphoneos' else 'StarFoxEnhanced.app/Contents/'
        info = plistlib.loads(archive.read(prefix + 'Info.plist'))
        if info['CFBundleExecutable'] != 'StarFoxEnhanced' or info['CFBundleIdentifier'] != 'com.starfox.enhanced':
            raise RuntimeError('Wrong actual application identity')
        if args.sdk == 'macosx' and info['LSMinimumSystemVersion'] != '11.0':
            raise RuntimeError('Existing macOS11 application minimum changed')
        if args.sdk == 'iphoneos' and info['MinimumOSVersion'] != '15.0':
            raise RuntimeError('Existing iOS15 application minimum changed')
        binary = archive.getinfo(prefix + ('MacOS/' if args.sdk == 'macosx' else '') + info['CFBundleExecutable'])
        with archive.open(binary) as stream:
            if args.sdk == 'iphoneos':
                slices = [(0x0100000c, 0, binary.file_size)]
            else:
                magic, count = struct.unpack('>II', stream.read(8))
                if magic != 0xcafebabe or count != 2:
                    raise RuntimeError('Actual universal macOS application required')
                slices = []
                for _ in range(count):
                    cpu, subtype, offset, size, alignment = struct.unpack('>iiIII', stream.read(20))
                    if offset + size > binary.file_size or alignment > 28 or offset % (1 << alignment):
                        raise RuntimeError('Invalid native universal slice')
                    slices.append((cpu, offset, size))
                if {row[0] for row in slices} != {0x01000007, 0x0100000c}:
                    raise RuntimeError('Both actual native architectures required')
            architectures = []
            for cpu, base, size in sorted(slices, key=lambda row: row[1]):
                (address, data), (platform, minimum) = readonly_constants(stream, base, size, cpu)
                if (platform, minimum) != ((2, 15 << 16) if args.sdk == 'iphoneos' else (1, 11 << 16)):
                    raise RuntimeError('Actual native binary deployment changed')
                for name, source_sha, payload in programs:
                    if data.count(payload) != 1 or (address + data.find(payload)) % 16:
                        raise RuntimeError('Full native RT library missing, duplicated or unaligned: ' + name)
                architectures.append('arm64' if cpu == 0x0100000c else 'x86_64')
        if archive.testzip() is not None:
            raise RuntimeError('Actual application archive CRC failure')
        for name in archive.namelist():
            path = PurePosixPath(name)
            if path.is_absolute() or '..' in path.parts or path.suffix.lower() in ('.sfc', '.smc', '.p12', '.keystore') or path.name.lower() == 'starfox-assets.bin':
                raise RuntimeError('Unsafe or private input in native test application')
    report = {'Scope': 'Actual application read-only native library/source embedding; no Metal/GPU/game/performance acceptance',
              'Sdk': args.sdk, 'Architectures': architectures, 'AllFourActualLibrariesEmbedded': True,
              'Programs': [{'Name': name, 'SourceSha256': source_sha, 'LibrarySha256': hashlib.sha256(data).hexdigest(),
                            'Bytes': len(data)} for name, source_sha, data in programs],
              'RuntimeGpuAccepted': False}
    output = args.generated_dir / 'native-library-embedding.json'
    if output.exists():
        raise RuntimeError('Existing embedding inspection; no overwrite')
    output.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
