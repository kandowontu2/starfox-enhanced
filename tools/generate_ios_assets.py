#!/usr/bin/env python3
"""Stage pre-rendered opaque iPhone/iPad icons without image dependencies."""
import argparse
from pathlib import Path
import shutil

ICONS = {
    'AppIcon60@2x.png': 120,
    'AppIcon60@3x.png': 180,
    'AppIcon76.png': 76,
    'AppIcon76@2x.png': 152,
    'AppIcon83.5@2x.png': 167,
}


def generate(output):
    output.mkdir(parents=True, exist_ok=True)
    for name, size in ICONS.items():
        shutil.copyfile(Path(__file__).resolve().parents[1] / 'assets/icons/ios' / name, output / name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    generate(parser.parse_args().output)
