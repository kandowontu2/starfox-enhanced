#!/usr/bin/env python3
"""Stage pre-rendered UWP tile and splash PNGs without image dependencies."""

from __future__ import annotations

import argparse
import pathlib
import shutil


def write_png(path: pathlib.Path, width: int, height: int) -> None:
    shutil.copyfile(pathlib.Path(__file__).resolve().parents[1] / 'assets/icons/uwp' / path.name, path)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for name, width, height in (
        ("StoreLogo.png", 50, 50),
        ("Square44x44Logo.png", 44, 44),
        ("Square150x150Logo.png", 150, 150),
        ("Wide310x150Logo.png", 310, 150),
        ("SplashScreen.png", 620, 300),
    ):
        write_png(args.output / name, width, height)


if __name__ == "__main__":
    main()
