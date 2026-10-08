"""Validate the Original LEVEL1_2 120-frame full-SBS cockpit fixture.

Compare a Screen-depth control with an otherwise identical finite-depth capture.
Checks the complete green reticle mask (not just its centroid) and three HUD
regions. Coordinates are logical 400x224 per eye; integer render scales work.
Requires Pillow. This does not claim dynamic aiming or general stage coverage.
"""
import argparse
from PIL import Image, ImageChops


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("control")
    parser.add_argument("depth")
    parser.add_argument("--left-offset", type=float, default=-2)
    args = parser.parse_args()
    with Image.open(args.control) as source:
        control = source.convert("RGB")
    with Image.open(args.depth) as source:
        depth = source.convert("RGB")
    if control.size != depth.size or control.width % 800:
        raise SystemExit("FAIL: expected matching integer-scaled 800x224 SBS captures")
    scale = control.width // 800
    if not scale or control.height != 224 * scale:
        raise SystemExit("FAIL: unexpected fixture aspect/scale")
    offset = args.left_offset * scale
    if not offset.is_integer() or abs(offset) > 12 * scale:
        raise SystemExit("FAIL: offset is non-integral or outside fixture crop")
    offset = int(offset)
    pixels_checked = 0
    for eye in range(2):
        origin = eye * 400 * scale
        def mask(image):
            pixels = image.load()
            return {(x, y) for y in range(110 * scale, 145 * scale)
                    for x in range(172 * scale, 228 * scale)
                    if pixels[origin + x, y][1] > 180
                    and pixels[origin + x, y][0] < 70
                    and pixels[origin + x, y][2] < 70}
        original, shifted = mask(control), mask(depth)
        if len(original) < 100 * scale * scale:
            raise SystemExit("FAIL: expected complete cockpit reticle is absent")
        dx = offset if eye == 0 else -offset
        if shifted != {(x + dx, y) for x, y in original}:
            raise SystemExit(f"FAIL: eye {eye} reticle differs from exact {dx}-pixel translation")
        # Stop at the bombs/boost ink, not the far-right world margin: an
        # asteroid can enter that margin between separately paced captures.
        for x, y, width, height in ((12, 12, 50, 18), (10, 178, 65, 26), (342, 178, 34, 26)):
            bounds = (origin + x * scale, y * scale,
                      origin + (x + width) * scale, (y + height) * scale)
            if ImageChops.difference(control.crop(bounds), depth.crop(bounds)).getbbox():
                raise SystemExit(f"FAIL: eye {eye} protected HUD region changed")
            pixels_checked += width * height * scale * scale
        print(f"Eye {eye}: {len(original)} reticle pixels translated exactly {dx} pixels")
    print(f"PASS: {pixels_checked} HUD pixels unchanged")


if __name__ == "__main__":
    main()
