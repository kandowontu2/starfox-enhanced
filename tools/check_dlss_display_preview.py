"""Check display-sized frozen DLSS previews (requires Pillow and NumPy).

Native glyph coverage must remain exact, allowing only one-code-value backend
rounding. --native-preview requires the frozen workspace to bypass the SDK;
--native-models additionally checks sharpness against native model pixels,
not just frame-to-frame stability. --held-models checks actual reconstruction
against a native reference and requires its settled output to stay fixed.
Use matched scenes, dimensions, effects and capture intervals.
"""
import argparse
import json
import re
from pathlib import Path

import numpy as np
from PIL import Image


def load(directory, frame):
    return np.asarray(Image.open(directory / f"lava.bmp.frame-{frame}.bmp").convert("RGB"))


def drawable_viewport(directory):
    log = (directory / "runtime.log").read_text()
    match = re.search(r"presentation-drawable-capture: (\d+)x(\d+) raster=\d+x\d+ viewport=([\d.eE+-]+),([\d.eE+-]+),([\d.eE+-]+),([\d.eE+-]+)", log)
    if not match:
        raise AssertionError(f"Missing actual drawable/viewport evidence: {directory}")
    w, h = map(int, match.group(1, 2))
    left, top, width, height = map(float, match.group(3, 4, 5, 6))
    if not (0 <= left < w and 0 <= top < h and width > 0 and height > 0
            and left+width <= w+.01 and top+height <= h+.01):
        raise AssertionError(f"Invalid drawable viewport: {directory}")
    return (w, h), (left, top, width, height)


def check(reference, directory, frame_numbers, menu_regions, viewport=None, native_models=False, held_models=False):
    h, w = reference.shape[:2]
    vx, vy, vw, vh = viewport or (0, 0, w, h)
    def region(left, top, right, bottom):
        return (slice(int(vy+top*vh), int(vy+bottom*vh)),
                slice(int(vx+left*vw), int(vx+right*vw)))
    menu = np.zeros((h, w), dtype=bool)
    for left, top, right, bottom in menu_regions:
        menu[region(left, top, right, bottom)] = True
    colors, counts = np.unique(reference[menu], axis=0, return_counts=True)
    # Check every authored ink, not only the most common (usually white).
    # Disabled labels are gray and headings/selected rows may be yellow;
    # checking white alone missed precisely the text the user can see blur.
    # The matched menu panel is dimmed below this brightness. Requiring a
    # repeated flat color avoids classifying isolated world highlights as ink.
    bright = (colors.max(axis=1) >= 144) & (counts >= 16)
    if not bright.any():
        raise AssertionError("Reference contains no setup-menu glyphs")
    glyph_colors = colors[bright]
    glyph_masks = [menu & (reference == color).all(axis=2) for color in glyph_colors]
    glyphs = np.logical_or.reduce(glyph_masks)
    if glyphs.sum() < 100:
        raise AssertionError("Insufficient native glyph coverage")
    shield = np.zeros((h, w), dtype=bool)
    shield[region(.02, .82, .11, .94)] = True
    shield &= reference.min(axis=2) >= 190
    if not shield.any():
        raise AssertionError("Reference contains no shield artwork")
    previous = previous_physical = None
    model_region = region(352/800, 80/224, 488/800, 146/224)
    max_model_reference_error = 0
    model_changed_pixels = 0
    whole, ship = [], []
    physical_ship, physical_ship_p99, physical_ship_max = [], [], []
    max_text_error = max_hud_error = 0
    for frame in frame_numbers:
        actual = load(directory, frame)
        if actual.shape != reference.shape:
            raise AssertionError(f"Output extent changed: {directory}, frame {frame}")
        actual_int = actual.astype(int)
        model_error=np.abs(actual_int[model_region]-reference[model_region])
        max_model_reference_error=max(max_model_reference_error,int(model_error.max()))
        model_changed_pixels=max(model_changed_pixels,int((model_error>1).any(axis=2).sum()))
        if native_models:
            max_model_reference_error = max(max_model_reference_error,
                int(np.abs(actual_int[model_region]-reference[model_region]).max()))
            if max_model_reference_error > 1:
                raise AssertionError(f"Native preview models differ/blurred: {directory}, frame {frame}")
        for color, mask in zip(glyph_colors, glyph_masks):
            actual_glyphs = menu & (np.abs(actual_int-color) <= 1).all(axis=2)
            if not np.array_equal(actual_glyphs, mask):
                raise AssertionError(f"Menu glyph coverage moved/softened: {directory}, frame {frame}, ink {color.tolist()}")
        max_text_error = max(max_text_error, int(np.abs(actual_int[glyphs]-reference[glyphs]).max()))
        max_hud_error = max(max_hud_error, int(np.abs(actual_int[shield]-reference[shield]).max()))
        if max_text_error > 1 or max_hud_error > 1:
            raise AssertionError(f"Native text/HUD colour altered: {directory}, frame {frame}")
        # Compare changes in common logical-pixel units, rather than calling
        # a bigger output a stability improvement solely because it has more
        # unchanged pixels. BOX integrates every physical output sample.
        content = actual[region(0, 0, 1, 1)]
        normalized = np.asarray(Image.fromarray(content).resize((800, 224), Image.Resampling.BOX), dtype=np.float32)
        if previous is not None:
            difference = np.abs(normalized-previous)
            whole.append(float(difference.mean()))
            ship.append(float(difference[80:146, 352:488].mean()))
            # Keep physical-pixel measurements too. Downsampling can average
            # alternating edge samples away and conceal the reported wobble.
            physical_difference = np.abs(actual_int-previous_physical)
            model = physical_difference[region(352/800, 80/224, 488/800, 146/224)]
            physical_ship.append(float(model.mean()))
            physical_ship_p99.append(float(np.percentile(model, 99)))
            physical_ship_max.append(int(model.max()))
        previous = normalized
        previous_physical = actual_int
    if held_models:
        if not model_changed_pixels:
            raise AssertionError(f"Reconstructed model is indistinguishable from native bypass: {directory}")
        if max(physical_ship_max) != 0:
            raise AssertionError(f"Held reconstructed model still moves: {directory}")
    return {"directory": str(directory), "extent": [w, h], "frames": len(frame_numbers),
            "glyph_pixels": int(glyphs.sum()), "glyph_colors": glyph_colors.tolist(),
            "text_max_rounding_error": max_text_error,
            "hud_max_rounding_error": max_hud_error,
            "native_model_max_reference_error": max_model_reference_error,
            "model_changed_pixels_from_native": model_changed_pixels,
            "whole_mean_change_0_255": float(np.mean(whole)),
            "ship_mean_change_0_255": float(np.mean(ship)),
            "ship_max_change_0_255": max(ship),
            "ship_physical_mean_change_0_255": float(np.mean(physical_ship)),
            "ship_physical_p99_change_0_255": max(physical_ship_p99),
            "ship_physical_max_change_0_255": max(physical_ship_max)}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("native", type=Path)
    parser.add_argument("captures", type=Path, nargs="+")
    parser.add_argument("--first", type=int, default=33)
    parser.add_argument("--last", type=int, default=61)
    parser.add_argument("--step", type=int, default=4)
    parser.add_argument("--include-native", action="store_true",
                        help="Also measure the matched DLSS-off sequence; exposure can change even with a frozen simulation")
    parser.add_argument("--drawable", action="store_true",
                        help="Require actual drawable captures and match their letterboxed viewports")
    parser.add_argument("--native-preview", action="store_true",
                        help="Require the frozen full-resolution preview instead of SDK evaluations")
    parser.add_argument("--native-models", action="store_true",
                        help="Require models to match the native reference; use a matched main menu with time-dependent effects OFF")
    parser.add_argument("--held-models", action="store_true",
                        help="Require different reconstructed model pixels, with zero settled variation; time-dependent effects must be OFF")
    parser.add_argument("--menu-region", type=float, nargs=4, action="append",
                        metavar=("LEFT", "TOP", "RIGHT", "BOTTOM"),
                        help="Unchanged menu labels in normalized coordinates (repeatable); exclude changed DLSS quality/scale values")
    args = parser.parse_args()
    if args.first < 1 or args.step < 1 or args.last < args.first+args.step:
        parser.error("Need at least two positive frames")
    menu_regions = args.menu_region or [(.38, .10, .65, .32)]
    for left, top, right, bottom in menu_regions:
        if not (0 <= left < right <= 1 and 0 <= top < bottom <= 1):
            parser.error("Menu regions must be nonempty and within 0..1")
    frames = range(args.first, args.last+1, args.step)
    reference = load(args.native, args.first)
    viewport = None
    if args.drawable:
        extent, viewport = drawable_viewport(args.native)
        if extent != (reference.shape[1], reference.shape[0]):
            raise AssertionError("Reference capture is not the logged drawable size")
    results = [check(reference, args.native, frames, menu_regions, viewport)] if args.include_native else []
    for directory in args.captures:
        log = (directory / "runtime.log").read_text()
        if args.native_preview:
            if log.count("dlss-preview: full-resolution native size=") < args.last or "dlss-gameplay: evaluated" in log:
                raise AssertionError(f"Frozen preview was not native: {directory}")
        else:
            evaluations=log.count("dlss-gameplay: evaluated frame=")
            reused=log.count("dlss-preview: reused reconstructed frame=")
            if evaluations < min(32,args.last) or evaluations+reused < args.last or "dlss-gameplay: failed" in log or "dlss-fallback:" in log:
                raise AssertionError(f"DLSS declined reconstruction: {directory}")
            if "frozen=1" not in log:
                raise AssertionError(f"Not a frozen preview: {directory}")
        if args.drawable and drawable_viewport(directory) != (extent, viewport):
            raise AssertionError(f"Native and DLSS drawable/viewport differ: {directory}")
        results.append(check(reference, directory, frames, menu_regions, viewport, args.native_models, args.held_models))
    print(json.dumps(results, indent=2))
