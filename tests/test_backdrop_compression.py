"""Compare actual runtime PNG decoding with every original BMP, pixel for pixel."""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def check(decoder):
    catalogue = (ROOT / 'include/starfox/render/enhanced_backdrop_library.hpp').read_text()
    paths = re.findall(r'EnhancedBackdropAsset\{"([^"]+)"', catalogue)
    if len(paths) != 38: raise ValueError('Unexpected runtime artwork count')
    raw = compressed = 0
    for name in paths:
        path = ROOT / name
        original = path.with_suffix('.bmp')
        subprocess.run([str(decoder), str(path), str(original)], check=True, capture_output=True, timeout=45)
        raw += original.stat().st_size
        compressed += path.stat().st_size
    if compressed >= raw: raise ValueError('Artwork did not shrink')
    print(f'38 backgrounds: {raw} -> {compressed} bytes; every runtime RGBA pixel and dimension identical.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--decoder', type=Path, required=True)
    check(parser.parse_args().decoder.resolve())
