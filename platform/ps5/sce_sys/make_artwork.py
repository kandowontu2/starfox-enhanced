#!/usr/bin/env python3
"""Build the PS5 homebrew presentation artwork (sce_sys) for Star Fox Enhanced
from in-game captures.

Inputs (captures/, produced by capture_frames.sh from a Linux release built
from your own cartridge; native 16:9 render at 4x scale, 1600x896):

  corneria_start_f001694.png   Corneria stage start, the team in formation
  intro_carrier_f002076.png    attract intro, attack carrier over Corneria
  title_screen_f000108.png     title screen

Outputs (next to this script unless --output is given):

  pic0.png / pic0.dds   3840x2160 RGB PNG / BC7_UNORM DX10 DDS   selected-title background
  pic1.png / pic1.dds   3840x2160 RGB PNG / BC7_UNORM DX10 DDS   launch background
  icon0.png             512x512 8-bit RGB PNG                     launcher tile

Processing is limited to: cropping 1600x896 to exact 16:9 (1593x896),
wrapping empty starfield (pic1 only, to move the subject right of the PS5
Shell's left-hand title overlay), erasing the title screen's "PUSH START" and
copyright lines (icon only, nearest-pixel fill), and Lanczos resampling.  No
colour grading, glow, sharpening or synthetic elements are added.

The DDS files carry the exact 148-byte header of the hardware-validated
reference (PS5_Vulkan sce_sys/pic0.dds: DX10, DXGI_FORMAT_BC7_UNORM, one mip,
straight alpha) and a BC7 mode-6 payload from the encoder in this file.  The
script is deterministic: re-running it reproduces every output byte for byte.
Requires Python 3, numpy, scipy and Pillow.
"""

from __future__ import annotations

import argparse
import math
import pathlib

import numpy as np
from PIL import Image
from scipy import ndimage

HERE = pathlib.Path(__file__).resolve().parent
CAPTURES = HERE / "captures"
REFERENCE_DDS = HERE.parents[2] / "build/console-sdk/PS5_Vulkan/sce_sys/pic0.dds"

CAPTURE_SIZE = (1600, 896)            # 16:9 canvas at 4x internal scale
PIC_SIZE = (3840, 2160)
ICON_SIZE = 512

PIC0_SOURCE = "corneria_start_f001694.png"
PIC1_SOURCE = "intro_carrier_f002076.png"
ICON_SOURCE = "title_screen_f000108.png"

# pic1: the carrier and planet sit left of centre and touch the top edge.
# Wrap the frame (right/down) so they move right of the Shell overlay and get
# headroom; only empty starfield crosses the wrap seams (checked below).
PIC1_ROLL = (100, 170)                # (down, right) in capture pixels

# Title-screen text to erase, as capture-pixel boxes (x0, y0, x1, y1).
PUSH_START_BOX = (640, 370, 1180, 515)
COPYRIGHT_BOX = (580, 770, 1120, 840)
WING_EDGE_COLUMNS = (520, 760)        # where the wing edge is sampled
ICON_SAFE = 0.85                      # subject must fit the central 85 %


def load_capture(name: str) -> np.ndarray:
    path = CAPTURES / name
    img = Image.open(path)
    if img.size != CAPTURE_SIZE or img.mode != "RGB":
        raise SystemExit(f"{path}: expected {CAPTURE_SIZE} RGB, got {img.size} {img.mode}")
    return np.asarray(img)


def objects(rgb: np.ndarray, floor: int = 40, min_px: int = 64) -> np.ndarray:
    """Mask of lit regions larger than a star (stars are 4x4 blocks)."""
    lit = rgb.max(-1) > floor
    lab, n = ndimage.label(lit)
    if n == 0:
        return lit
    sizes = ndimage.sum(lit, lab, range(1, n + 1))
    return np.isin(lab, 1 + np.nonzero(sizes >= min_px)[0])


def roll_starfield(rgb: np.ndarray, down: int, right: int) -> np.ndarray:
    subject = objects(rgb)
    h, w = subject.shape
    if subject[h - down:].any() or subject[:, w - right:].any():
        raise SystemExit("pic1 roll would wrap part of the subject, not just starfield")
    return np.roll(rgb, (down, right), axis=(0, 1))


def widescreen(rgb: np.ndarray) -> np.ndarray:
    """Exact 16:9 centre crop, then Lanczos to 3840x2160."""
    h, w = rgb.shape[:2]
    cw = round(h * 16 / 9)
    x0 = (w - cw) // 2
    img = Image.fromarray(np.ascontiguousarray(rgb[:, x0:x0 + cw]), "RGB")
    return np.asarray(img.resize(PIC_SIZE, Image.LANCZOS))


def nearest_fill(rgb: np.ndarray, holes: np.ndarray, source: np.ndarray, out: np.ndarray):
    """Fill `holes` from the nearest pixel in `source` (no blur)."""
    _, (iy, ix) = ndimage.distance_transform_edt(~source, return_indices=True)
    out[holes] = rgb[iy[holes], ix[holes]]


def wing_edge(rgb: np.ndarray, mask: np.ndarray) -> tuple[float, float]:
    """Fit y = k*x + c to the Arwing wing's top edge where it runs under the
    "P" of PUSH START, from the unmasked columns on either side."""
    lit = rgb.max(-1) > 60
    xs, ys = [], []
    for x in range(*WING_EDGE_COLUMNS):
        for y in range(300, 560):
            if lit[y, x] and not mask[y, x] and lit[y:y + 6, x].all():
                if not lit[y - 1, x] and not mask[y - 2:y, x].any():
                    xs.append(x)
                    ys.append(y)
                break
    k, c = np.polyfit(np.array(xs, float), np.array(ys, float), 1)
    if len(xs) < 50 or np.abs(np.array(ys) - (k * np.array(xs) + c)).max() > 1.0:
        raise SystemExit("title capture changed: wing edge under PUSH START not found")
    return float(k), float(c)


def title_icon(rgb: np.ndarray) -> np.ndarray:
    a = rgb.astype(np.int16)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    h, w = a.shape[:2]
    yy, xx = np.mgrid[:h, :w]
    out = rgb.copy()

    # "PUSH START": green glyphs with a white upper half and a black outline;
    # dilating by 9 px swallows the outline.  Above the wing's top edge the
    # hole is backdrop (nearest backdrop pixel); below it the wing is a flat
    # polygon, so each hole pixel copies the wing pixel found by stepping back
    # along the edge direction (keeps the edge and its shading band intact).
    x0, y0, x1, y1 = PUSH_START_BOX
    box = np.zeros((h, w), bool)
    box[y0:y1, x0:x1] = True
    green = (g > 150) & (g - b > 80) & (r < 200)
    white = (a.min(-1) > 235) & ndimage.binary_dilation(green, iterations=12)
    mask = ndimage.binary_dilation((green | white) & box, iterations=9) & box
    k, c = wing_edge(rgb, mask)
    wing = yy >= k * xx + c - 0.5
    nearest_fill(rgb, mask & ~wing, ~mask & ~wing, out)
    for y, x in zip(*np.nonzero(mask & wing)):
        t = 1
        while mask[int(round(y - k * t)), x - t]:
            t += 1
        out[y, x] = rgb[int(round(y - k * t)), x - t]

    # "(c) 1993 Nintendo": white glyphs on the black backdrop.
    x0, y0, x1, y1 = COPYRIGHT_BOX
    box = np.zeros((h, w), bool)
    box[y0:y1, x0:x1] = True
    mask = ndimage.binary_dilation((a.min(-1) > 120) & box, iterations=5) & box
    nearest_fill(rgb, mask, ~mask, out)
    clean = out

    # Square crop centred on the remaining artwork (logo, team, Arwing,
    # twinkle), sized so it fills the central 85 %; pad with backdrop black.
    ys, xs = np.nonzero(objects(clean))
    cx, cy = (xs.min() + xs.max() + 1) / 2, (ys.min() + ys.max() + 1) / 2
    side = math.ceil(max(xs.max() + 1 - xs.min(), ys.max() + 1 - ys.min()) / ICON_SAFE)
    side += side % 2
    left, top = round(cx - side / 2), round(cy - side / 2)
    canvas = np.zeros((side, side, 3), np.uint8)
    h, w = clean.shape[:2]
    sx0, sy0 = max(left, 0), max(top, 0)
    sx1, sy1 = min(left + side, w), min(top + side, h)
    canvas[sy0 - top:sy1 - top, sx0 - left:sx1 - left] = clean[sy0:sy1, sx0:sx1]
    img = Image.fromarray(canvas, "RGB").resize((ICON_SIZE, ICON_SIZE), Image.LANCZOS)
    return np.asarray(img)


# --------------------------------------------------------------------------
# BC7 (mode 6) encoder + DDS writer
# --------------------------------------------------------------------------

# Exact header of the hardware-validated reference
# build/console-sdk/PS5_Vulkan/sce_sys/pic0.dds (3840x2160, DX10, BC7_UNORM=98,
# TEXTURE2D, arraySize 1, one mip, alpha mode 1 = straight).
DDS_HEADER = bytes.fromhex(
    "444453207c00000007100a0070080000000f000000907e00010000000100000000000000"
    "000000000000000000000000000000000000000000000000000000000000000000000000"
    "000000002000000004000000445831300000000000000000000000000000000000000000"
    "001000000000000000000000000000000000000062000000030000000000000001000000"
    "01000000"
)
assert len(DDS_HEADER) == 148
BC7_W4 = np.array([0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64], np.int32)


def bc7_encode(rgb: np.ndarray) -> bytes:
    """Encode an HxWx3 uint8 image (H, W multiples of 4) as opaque BC7 mode 6."""
    h, w = rgb.shape[:2]
    blocks = rgb.reshape(h // 4, 4, w // 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(-1, 16, 3)
    out = np.zeros((blocks.shape[0], 2), np.uint64)
    chunk = 16384
    for s in range(0, blocks.shape[0], chunk):
        out[s:s + chunk] = _bc7_mode6(blocks[s:s + chunk].astype(np.float32))
    return out.astype("<u8").tobytes()


def _quant_ep(v):
    # alpha is opaque -> p-bit fixed at 1 so A = (127 << 1) | 1 = 255
    return np.clip(np.rint((v - 1) / 2), 0, 127).astype(np.int32)


def _bc7_mode6(px: np.ndarray) -> np.ndarray:
    n = px.shape[0]
    mean = px.mean(axis=1, keepdims=True)
    d = px - mean
    cov = np.einsum("nki,nkj->nij", d, d)
    axis = np.ones((n, 3), np.float32)
    for _ in range(8):  # power iteration for principal axis
        axis = np.einsum("nij,nj->ni", cov, axis)
        axis /= np.maximum(np.linalg.norm(axis, axis=1, keepdims=True), 1e-6)
    proj = np.einsum("nki,ni->nk", d, axis)
    lo = mean[:, 0] + axis * proj.min(axis=1, keepdims=True)
    hi = mean[:, 0] + axis * proj.max(axis=1, keepdims=True)

    def fit(e0, e1):
        q0, q1 = _quant_ep(e0), _quant_ep(e1)
        v0 = (q0 * 2 + 1)[:, None, :]
        v1 = (q1 * 2 + 1)[:, None, :]
        pal = ((64 - BC7_W4)[None, :, None] * v0 + BC7_W4[None, :, None] * v1 + 32) >> 6
        err = ((px[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(-1)
        idx = err.argmin(-1)
        return q0, q1, idx, np.take_along_axis(err, idx[..., None], -1)[..., 0].sum(1)

    q0, q1, idx, err = fit(lo, hi)
    # one least-squares endpoint refinement pass
    wt = (BC7_W4[idx] / 64.0).astype(np.float32)
    a, b = 1 - wt, wt
    aa, bb, ab = (a * a).sum(1), (b * b).sum(1), (a * b).sum(1)
    ax_ = np.einsum("nk,nkc->nc", a, px)
    bx_ = np.einsum("nk,nkc->nc", b, px)
    det = aa * bb - ab * ab
    ok = np.abs(det) > 1e-6
    sd = np.where(ok, det, 1.0)[:, None]
    e0 = np.where(ok[:, None], (bb[:, None] * ax_ - ab[:, None] * bx_) / sd, lo)
    e1 = np.where(ok[:, None], (aa[:, None] * bx_ - ab[:, None] * ax_) / sd, hi)
    r0, r1, ridx, rerr = fit(np.clip(e0, 0, 255), np.clip(e1, 0, 255))
    better = rerr < err
    q0 = np.where(better[:, None], r0, q0)
    q1 = np.where(better[:, None], r1, q1)
    idx = np.where(better[:, None], ridx, idx)
    # anchor (pixel 0) index must have MSB 0: swap endpoints where needed
    swap = idx[:, 0] >= 8
    q0, q1 = np.where(swap[:, None], q1, q0), np.where(swap[:, None], q0, q1)
    idx = np.where(swap[:, None], 15 - idx, idx)
    q0 = q0.astype(np.uint64)
    q1 = q1.astype(np.uint64)
    idx = idx.astype(np.uint64)
    lo64 = np.full(n, 1 << 6, np.uint64)
    for c in range(3):
        lo64 |= q0[:, c] << np.uint64(7 + 14 * c)
        lo64 |= q1[:, c] << np.uint64(14 + 14 * c)
    lo64 |= np.uint64(127) << np.uint64(49)  # A0
    lo64 |= np.uint64(127) << np.uint64(56)  # A1
    lo64 |= np.uint64(1) << np.uint64(63)    # P0
    hi64 = np.ones(n, np.uint64)             # P1
    hi64 |= idx[:, 0] << np.uint64(1)
    for i in range(1, 16):
        hi64 |= idx[:, i] << np.uint64(4 + 4 * (i - 1))
    return np.stack([lo64, hi64], axis=1)


def write_dds(path: pathlib.Path, rgb: np.ndarray):
    h, w = rgb.shape[:2]
    if (w, h) != (3840, 2160):
        raise ValueError("DDS header is fixed to the 3840x2160 reference profile")
    path.write_bytes(DDS_HEADER + bc7_encode(rgb))


def check_dds(path: pathlib.Path, rgb: np.ndarray):
    data = path.read_bytes()
    assert data[:148] == DDS_HEADER and len(data) == 148 + 3840 * 2160
    dec = np.asarray(Image.open(path).convert("RGB"), np.float64)
    mse = ((dec - rgb.astype(np.float64)) ** 2).mean()
    psnr = 10 * math.log10(255 ** 2 / max(mse, 1e-9))
    print(f"  {path.name}: BC7 round-trip PSNR {psnr:.2f} dB")
    return psnr


def check_reference_header():
    if REFERENCE_DDS.is_file():
        ref = REFERENCE_DDS.read_bytes()
        assert ref[:148] == DDS_HEADER, "DDS header differs from the PS5_Vulkan reference"
        assert len(ref) == 148 + 3840 * 2160
        print(f"  header matches {REFERENCE_DDS.relative_to(HERE.parents[2])}")


# --------------------------------------------------------------------------

def save_png(path: pathlib.Path, rgb: np.ndarray):
    Image.fromarray(rgb, "RGB").save(path, optimize=True)
    print(f"wrote {path.name} {rgb.shape[1]}x{rgb.shape[0]}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--output", type=pathlib.Path, default=HERE)
    ap.add_argument("--only", choices=["icon0", "pic0", "pic1"], action="append")
    args = ap.parse_args()
    out = args.output
    out.mkdir(parents=True, exist_ok=True)
    want = set(args.only or ["icon0", "pic0", "pic1"])

    if "icon0" in want:
        save_png(out / "icon0.png", title_icon(load_capture(ICON_SOURCE)))
    pics = {
        "pic0": lambda: widescreen(load_capture(PIC0_SOURCE)),
        "pic1": lambda: widescreen(roll_starfield(load_capture(PIC1_SOURCE), *PIC1_ROLL)),
    }
    for name, build in pics.items():
        if name not in want:
            continue
        rgb = np.ascontiguousarray(build())
        save_png(out / f"{name}.png", rgb)
        write_dds(out / f"{name}.dds", rgb)
        check_dds(out / f"{name}.dds", rgb)
    if want & {"pic0", "pic1"}:
        check_reference_header()


if __name__ == "__main__":
    main()
