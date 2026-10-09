"""Check an actual packed SBS presentation, not just its pre-presentation eyes.

Use a gameplay capture with visible 3D geometry. --hud-y is optional and marks
the start of a HUD-only region in image pixels (e.g. 330 in the 448-high fixture).
Requires Pillow. Works for both Half and Full SBS.
"""
import argparse
from PIL import Image, ImageChops


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image")
    parser.add_argument("--hud-y", type=int)
    args = parser.parse_args()
    with Image.open(args.image) as source:
        image = source.convert("RGB")
    width, height = image.size
    if width % 2:
        raise SystemExit("SBS output must have equal-width eyes")
    left = image.crop((0, 0, width // 2, height))
    right = image.crop((width // 2, 0, width, height))
    difference = ImageChops.difference(left, right)
    if difference.getbbox() is None:
        raise SystemExit("FAIL: packed SBS eyes are identical; depth was lost")
    if args.hud_y is not None:
        if not 0 <= args.hud_y < height:
            raise SystemExit("HUD boundary is outside the capture")
        if difference.crop((0, args.hud_y, width // 2, height)).getbbox():
            raise SystemExit("FAIL: screen-depth HUD differs between eyes")
    print(f"PASS: distinct {width // 2}x{height} eye images"
          + (", identical HUD" if args.hud_y is not None else ""))


if __name__ == "__main__":
    main()
