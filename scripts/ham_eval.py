#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
"""scripts/ham_eval.py - objective A/B comparison of renderHAM settings.

For every variant renderHAM is run with --preview and --canvas-png; the decoded
result is compared with exactly the canvas the encoder saw. All figures are
measured in OKLab:

  err_px   mean distance per pixel (the "mean OKLab error" the tool reports - used
           here as a cross-check of this evaluation; small differences are possible
           because the canvas is an 8-bit PNG, relative comparisons are unaffected)
  err_p99  99th percentile of the pixel error - catches outlier artifacts (fringing
           at edges shows up here, hardly in the mean)
  err_edge mean pixel error in the 10 % of pixels with the strongest edges of the
           canvas (gradient magnitude of the lightness) - this is where it is decided
           how well HAM follows color jumps; lookahead shows most clearly here
  err_blur mean distance AFTER a Gaussian blur (sigma = 1.5 px) of both pictures - the
           measure that rewards dithering: it judges how well the average over a small
           neighborhood is right, the way eye and CRT integrate, not every pixel alone
  ssim     SSIM of the OKLab lightness (Wang et al. 2004; Gaussian window sigma = 1.5,
           C1 = 0.01^2, C2 = 0.03^2, mean over the picture): 1 = identical, lower = structure
           (edges, texture, contrast) reproduced worse. Counts dither noise as foreign
           structure, so it is the counterpart of err_blur: err_blur rewards dithering,
           ssim punishes it. renderHAM does the same computation itself (the "SSIM=..."
           in its output), but on the float canvas instead of the 8-bit PNG.
  bytes    size of the IFF file (compressibility)
  sec      run time

Usage:
  scripts/ham_eval.py --bin build/tools/renderham/renderHAM \\
      --input a.png --input b.png [--common "--width 320 --height 256"] \\
      "FS 100%::--dither floyd-steinberg" "FS 50%::--dither-strength 50 ..."

Every variant argument has the form "label::additional options". Leading tokens of
the form NAME=value are set as environment variables.
"""
import argparse, os, re, shlex, subprocess, sys, tempfile, time
import numpy as np
from PIL import Image


def srgb_to_oklab(rgb8):
    c = rgb8.astype(np.float64) / 255.0
    lin = np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
    r, g, b = lin[..., 0], lin[..., 1], lin[..., 2]
    l = np.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b)
    m = np.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b)
    s = np.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b)
    return np.stack([0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
                     1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
                     0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s], axis=-1)


def gaussian_blur(img, sigma):
    radius = int(np.ceil(3 * sigma))
    k = np.exp(-0.5 * (np.arange(-radius, radius + 1) / sigma) ** 2)
    k /= k.sum()
    out = img
    for axis in (0, 1):
        pad = [(0, 0)] * out.ndim
        pad[axis] = (radius, radius)
        p = np.pad(out, pad, mode="reflect")
        acc = np.zeros_like(out)
        for i, w in enumerate(k):
            sl = [slice(None)] * out.ndim
            sl[axis] = slice(i, i + out.shape[axis])
            acc += w * p[tuple(sl)]
        out = acc
    return out


def ssim_lightness(ref, out, sigma=1.5):
    """SSIM of the OKLab lightness (channel 0), Gaussian window, reflected borders."""
    x, y = ref[..., 0], out[..., 0]
    mx, my = gaussian_blur(x, sigma), gaussian_blur(y, sigma)
    vx = gaussian_blur(x * x, sigma) - mx * mx
    vy = gaussian_blur(y * y, sigma) - my * my
    cxy = gaussian_blur(x * y, sigma) - mx * my
    c1, c2 = 0.01 ** 2, 0.03 ** 2
    return float((((2 * mx * my + c1) * (2 * cxy + c2)) / ((mx * mx + my * my + c1) * (vx + vy + c2))).mean())


def dist_map(a, b):
    return np.sqrt(((a - b) ** 2).sum(axis=-1))


def dist(a, b):
    return float(dist_map(a, b).mean())


def run_variant(binary, image, common, extra, tmp):
    iff, prev, canv = (os.path.join(tmp, n) for n in ("o.iff", "p.png", "c.png"))
    cmd = [binary, "-i", image, "-o", iff, "--preview", prev, "--canvas-png", canv]
    toks = shlex.split(extra)
    env = dict(os.environ)
    while toks and re.match(r"^[A-Za-z_][A-Za-z0-9_]*=", toks[0]):
        k, v = toks.pop(0).split("=", 1)
        env[k] = v
    cmd += shlex.split(common) + toks
    t0 = time.time()
    res = subprocess.run(cmd, capture_output=True, text=True, env=env)
    sec = time.time() - t0
    if res.returncode != 0:
        raise SystemExit(f"Error running {' '.join(cmd)}:\n{res.stderr}{res.stdout}")
    tool_err = float(re.search(r"mean OKLab error=([0-9.]+)", res.stdout).group(1))
    ref = srgb_to_oklab(np.array(Image.open(canv).convert("RGB")))
    out = srgb_to_oklab(np.array(Image.open(prev).convert("RGB")))
    dm = dist_map(ref, out)
    gy, gx = np.gradient(ref[..., 0])
    mag = np.hypot(gx, gy)
    edge = float(dm[mag >= np.percentile(mag, 90)].mean())
    return dict(tool=tool_err, px=float(dm.mean()), p99=float(np.percentile(dm, 99)), edge=edge,
                blur=dist(gaussian_blur(ref, 1.5), gaussian_blur(out, 1.5)),
                ssim=ssim_lightness(ref, out),
                bytes=os.path.getsize(iff), sec=sec)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", required=True)
    ap.add_argument("--input", action="append", required=True, help="may be given several times")
    ap.add_argument("--common", default="")
    ap.add_argument("variants", nargs="+")
    a = ap.parse_args()
    variants = [v.split("::", 1) if "::" in v else (v, v) for v in a.variants]
    totals = {lbl: [] for lbl, _ in variants}
    with tempfile.TemporaryDirectory() as tmp:
        for image in a.input:
            print(f"\n=== {os.path.basename(image)} ===")
            print(f"{'Variant':<30}{'tool':>9}{'err_px':>9}{'err_p99':>9}{'err_edge':>9}{'err_blur':>10}{'ssim':>8}{'bytes':>9}{'sec':>6}")
            for lbl, extra in variants:
                r = run_variant(a.bin, image, a.common, extra, tmp)
                totals[lbl].append(r)
                print(f"{lbl:<30}{r['tool']:>9.5f}{r['px']:>9.5f}{r['p99']:>9.5f}{r['edge']:>9.5f}{r['blur']:>10.5f}{r['ssim']:>8.4f}{r['bytes']:>9d}{r['sec']:>6.1f}")
    if len(a.input) > 1:
        print("\n=== Mean over all pictures ===")
        print(f"{'Variant':<30}{'err_px':>9}{'err_p99':>9}{'err_edge':>9}{'err_blur':>10}{'ssim':>8}{'bytes':>9}{'sec':>6}")
        for lbl, _ in variants:
            rs = totals[lbl]
            print(f"{lbl:<30}{np.mean([r['px'] for r in rs]):>9.5f}{np.mean([r['p99'] for r in rs]):>9.5f}"
                  f"{np.mean([r['edge'] for r in rs]):>9.5f}{np.mean([r['blur'] for r in rs]):>10.5f}"
                  f"{np.mean([r['ssim'] for r in rs]):>8.4f}"
                  f"{int(np.mean([r['bytes'] for r in rs])):>9d}{np.mean([r['sec'] for r in rs]):>6.1f}")


if __name__ == "__main__":
    sys.exit(main())
