#!/usr/bin/env python3
"""Check Vita package metadata/artwork, not console boot compatibility."""
import argparse
from pathlib import Path, PurePosixPath
import re
import struct
import xml.etree.ElementTree as ET
import zipfile
import zlib

IMAGES = {
    "sce_sys/icon0.png": (128, 128),
    "sce_sys/livearea/contents/bg.png": (840, 500),
    "sce_sys/livearea/contents/startup.png": (280, 158),
}
TEMPLATE = "sce_sys/livearea/contents/template.xml"
MAX_LOAD_BYTES = 64 * 1024 * 1024  # project budget; heap/GXM are separate


def check_self(executable, file_size):
    """Inspect actual load reservations, not just the four-byte SELF magic."""
    header = executable.read(0x1000)
    if len(header) < 128 or header[:4] != b"SCE\0":
        raise ValueError("Missing native SELF header")
    fields = struct.unpack_from("<IIHHI14Q", header)
    version, kind, header_size, self_size = fields[1], fields[3], fields[5], fields[7]
    elf, phdrs, segments = fields[11], fields[12], fields[14]
    if version != 3 or kind != 1 or self_size != file_size or not 128 <= header_size <= file_size:
        raise ValueError("Invalid SELF header size/version")
    if elf > len(header) - 52 or header[elf:elf + 7] != b"\x7fELF\x01\x01\x01":
        raise ValueError("Missing little-endian ARM ELF header")
    if struct.unpack_from("<H", header, elf + 18)[0] != 40:
        raise ValueError("SELF is not an ARM executable")
    entry_size, count = struct.unpack_from("<HH", header, elf + 42)
    if entry_size != 32 or not 1 <= count <= 64 or max(phdrs, segments) > min(header_size, len(header)) - count * 32:
        raise ValueError("Invalid SELF program/segment table")
    reservations = []
    for index in range(count):
        kind, _, address, _, stored, memory, _, alignment = struct.unpack_from("<8I", header, phdrs + index * 32)
        offset, length, compression, encryption = struct.unpack_from("<4Q", header, segments + index * 32)
        if offset < header_size or offset > file_size or length > file_size - offset or compression not in (1, 2) or encryption != 2:
            raise ValueError("Invalid SELF payload segment")
        if compression == 1 and length != stored:
            raise ValueError("Incorrect uncompressed SELF segment size")
        if kind == 1:
            if stored > memory or address + memory > 2**32 or not alignment or alignment & (alignment - 1):
                raise ValueError("Invalid SELF load reservation")
            reservations.append((address, address + memory))
    reservations.sort()
    if not reservations or any(left[1] > right[0] for left, right in zip(reservations, reservations[1:])):
        raise ValueError("Missing/overlapping SELF load reservations")
    load_bytes = sum(end - start for start, end in reservations)
    if load_bytes > MAX_LOAD_BYTES:
        raise ValueError(f"SELF maps {load_bytes} bytes: exceeds 64 MiB executable budget; package artwork as files")
    return load_bytes


def backdrop_paths():
    catalogue = Path(__file__).resolve().parents[1] / "include/starfox/render/enhanced_backdrop_library.hpp"
    return re.findall(r'EnhancedBackdropAsset\{"(assets/enhanced-backdrops/[a-z0-9-]+\.(?:bmp|png))"', catalogue.read_text())


def check_png(data, size):
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError("Invalid PNG signature")
    offset, kinds, compressed, palette_size = 8, [], bytearray(), 0
    while offset < len(data):
        if len(data) - offset < 12:
            raise ValueError("Truncated PNG chunk")
        length, = struct.unpack_from(">I", data, offset)
        end = offset + 12 + length
        if end > len(data):
            raise ValueError("Truncated PNG payload")
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:end - 4]
        crc, = struct.unpack_from(">I", data, end - 4)
        if zlib.crc32(kind + payload) & 0xffffffff != crc:
            raise ValueError("PNG chunk checksum mismatch")
        if not kinds and kind != b"IHDR":
            raise ValueError("PNG must begin with IHDR")
        if kind == b"IHDR":
            if kinds or len(payload) != 13:
                raise ValueError("Invalid/duplicate IHDR")
            width, height, depth, colour, compression, filtering, interlace = struct.unpack(">IIBBBBB", payload)
            if (width, height) != size or (depth, colour, compression, filtering, interlace) != (8, 3, 0, 0, 0):
                raise ValueError("Vita artwork must be correctly sized opaque 8-bit indexed PNG")
        elif kind == b"PLTE":
            if b"PLTE" in kinds or b"IDAT" in kinds or not 3 <= len(payload) <= 768 or len(payload) % 3:
                raise ValueError("Invalid PNG palette")
            palette_size = len(payload) // 3
        elif kind == b"IDAT":
            if b"PLTE" not in kinds:
                raise ValueError("PNG image data precedes palette")
            compressed.extend(payload)
        elif kind == b"tRNS":
            raise ValueError("Vita artwork must not contain transparency")
        elif kind == b"IEND":
            if payload or end != len(data):
                raise ValueError("Invalid PNG end")
        elif not kind[0] & 32:
            raise ValueError("Unknown critical PNG chunk")
        kinds.append(kind)
        offset = end
    if not kinds or kinds[-1] != b"IEND" or b"IDAT" not in kinds:
        raise ValueError("Incomplete PNG")
    expected = (size[0] + 1) * size[1]
    inflater = zlib.decompressobj()
    pixels = inflater.decompress(compressed, expected + 1)
    if len(pixels) != expected or not inflater.eof or inflater.unused_data or inflater.unconsumed_tail:
        raise ValueError("Invalid PNG raster length/stream")
    if any(pixels[row * (size[0] + 1)] > 4 for row in range(size[1])):
        raise ValueError("Invalid PNG filter")
    previous = bytearray(size[0])
    for y in range(size[1]):
        offset = y * (size[0] + 1)
        filtering, row = pixels[offset], bytearray(pixels[offset + 1:offset + 1 + size[0]])
        for x in range(size[0]):
            left, above = row[x - 1] if x else 0, previous[x]
            corner = previous[x - 1] if x else 0
            if filtering == 1: prediction = left
            elif filtering == 2: prediction = above
            elif filtering == 3: prediction = (left + above) // 2
            elif filtering == 4:
                p = left + above - corner
                a, b, c = abs(p - left), abs(p - above), abs(p - corner)
                prediction = left if a <= b and a <= c else above if b <= c else corner
            else: prediction = 0
            row[x] = (row[x] + prediction) & 255
        if max(row) >= palette_size:
            raise ValueError("PNG contains an invalid palette index")
        previous = row


def read_sfo(data):
    if len(data) < 20 or data[:4] != b"\0PSF":
        raise ValueError("Invalid PARAM.SFO signature")
    version, keys, values, count = struct.unpack_from("<4I", data, 4)
    if version != 0x101 or not 20 + count * 16 <= keys < values <= len(data):
        raise ValueError("Invalid PARAM.SFO table bounds")
    result = {}
    for index in range(count):
        key, kind, length, capacity, value = struct.unpack_from("<HHIII", data, 20 + index * 16)
        start = keys + key
        stop = data.find(b"\0", start, values)
        if not keys <= start < stop < values or length > capacity or values + value + capacity > len(data):
            raise ValueError("Invalid PARAM.SFO entry bounds")
        name = data[start:stop].decode("ascii")
        if name in result:
            raise ValueError("Duplicate PARAM.SFO key")
        raw = data[values + value:values + value + length]
        if kind == 0x204:
            if not raw or raw[-1] != 0 or b"\0" in raw[:-1]:
                raise ValueError("Invalid PARAM.SFO string")
            result[name] = raw[:-1].decode("utf-8")
        elif kind == 0x404 and length == 4:
            result[name], = struct.unpack("<I", raw)
        else:
            result[name] = raw
    return result


def check(path, title_id="SFXE00001", require_backdrops=False):
    with zipfile.ZipFile(path) as package:
        entries = package.infolist()
        names = [item.filename for item in entries]
        if len(set(names)) != len(names):
            raise ValueError("Duplicate VPK entries")
        if sum(item.file_size for item in entries) > 512 * 1024 * 1024:
            raise ValueError("VPK exceeds validation size budget")
        for item in entries:
            name = item.filename
            parts = PurePosixPath(name).parts
            # ZipInfo normalizes backslashes on Windows and truncates NULs.
            # Inspect the original header name before trusting that alias.
            if not parts or item.orig_filename != name or "\0" in name or "\\" in name or ":" in name or name.startswith("/") or ".." in parts:
                raise ValueError("Unsafe VPK path")
            if PurePosixPath(name).suffix.lower() in {".sfc", ".smc", ".jks", ".keystore", ".p12", ".pfx", ".pem", ".key"} or (PurePosixPath(name).suffix.lower() == ".bin" and name != "eboot.bin"):
                raise ValueError("Private cartridge/credential file in VPK")
        required = {"eboot.bin", "sce_sys/param.sfo", TEMPLATE, *IMAGES}
        if required - set(names):
            raise ValueError("Missing VPK files: " + ", ".join(sorted(required - set(names))))
        with package.open("eboot.bin") as executable:
            check_self(executable, package.getinfo("eboot.bin").file_size)
        if require_backdrops:
            for name in backdrop_paths():
                if name not in names or package.getinfo(name).file_size < 54:
                    raise ValueError("Missing packaged Vita backdrop: " + name)
                with package.open(name) as image:
                    if not image.read(8).startswith((b"BM", b"\x89PNG\r\n\x1a\n")):
                        raise ValueError("Invalid packaged Vita backdrop: " + name)
        def small(name):
            if package.getinfo(name).file_size > 2 * 1024 * 1024:
                raise ValueError("Oversized VPK metadata/artwork")
            return package.read(name)
        metadata = read_sfo(small("sce_sys/param.sfo"))
        if metadata.get("TITLE_ID") != title_id or metadata.get("TITLE") != "Star Fox Enhanced" or metadata.get("CATEGORY") != "gd":
            raise ValueError("Incorrect Vita application metadata")
        if not re.fullmatch(r"[0-9]{2}\.[0-9]{2}", str(metadata.get("APP_VER", ""))):
            raise ValueError("Invalid Vita application version")
        for name, dimensions in IMAGES.items():
            check_png(small(name), dimensions)
        root = ET.fromstring(small(TEMPLATE))
        if root.tag != "livearea" or root.get("style") != "a1" or root.get("format-ver") != "01.00":
            raise ValueError("Invalid LiveArea template")
        if root.findtext("livearea-background/image") != "bg.png" or root.findtext("gate/startup-image") != "startup.png":
            raise ValueError("Missing/incorrect LiveArea image references")
        if package.testzip() is not None:
            raise ValueError("VPK entry checksum mismatch")
    return metadata


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("vpk", type=Path)
    parser.add_argument("--title-id", default="SFXE00001")
    parser.add_argument("--require-backdrops", action="store_true")
    args = parser.parse_args()
    try:
        result = check(args.vpk, args.title_id, args.require_backdrops)
    except (ValueError, OSError, KeyError, zipfile.BadZipFile, ET.ParseError, zlib.error) as error:
        parser.exit(1, f"Vita package rejected: {error}\n")
    print(f"Vita package metadata/artwork verified: {result['TITLE_ID']} {result['APP_VER']}; console boot not tested")
