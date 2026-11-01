#!/usr/bin/env python3
"""Stage pre-rendered opaque, indexed Vita launcher artwork."""
import argparse
from pathlib import Path
import struct
import zlib


def chunk(kind, payload):
    """PNG fixture helper also used by the package corruption tests."""
    return (struct.pack('>I', len(payload)) + kind + payload
            + struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff))

ASSETS = {
    "sce_sys/icon0.png": (128, 128),
    "sce_sys/livearea/contents/bg.png": (840, 500),
    "sce_sys/livearea/contents/startup.png": (280, 158),
}


def png(width, height):
    name = next(name for name, size in ASSETS.items() if size == (width, height))
    return (Path(__file__).resolve().parents[1] / "assets/icons/vita" / name).read_bytes()


def generate(output):
    for name, (width, height) in ASSETS.items():
        path = output / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(png(width, height))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    generate(parser.parse_args().output)
