# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
"""Compares the renderHAM color-focus maps (uniform/center/edge/
center-edge/golden-ratio) on a given source image, reporting reconstruction error
separately for a center region vs. the periphery.

The global mean-OKLab error alone doesn't show whether importance
weighting worked: it's a deliberate trade-off (better in one region,
slightly worse elsewhere), not a global optimization, so it needs a
region-aware metric to evaluate honestly.

Usage:
    python3 analyze_importance.py [source_image [renderHAM_binary [width height]]]
"""
import sys, subprocess, tempfile, os
sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
from PIL import Image
from decode_ham6_independent import decode_ilbm_ham6


def srgb_to_oklab(rgb01):
    def s2l(c):
        return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
    lin = s2l(rgb01)
    r, g, b = lin[..., 0], lin[..., 1], lin[..., 2]
    l = 0.4122214708*r + 0.5363325363*g + 0.0514459929*b
    m = 0.2119034982*r + 0.6806995451*g + 0.1073969566*b
    s = 0.0883024619*r + 0.2817188376*g + 0.6299787005*b
    l_, m_, s_ = np.cbrt(l), np.cbrt(m), np.cbrt(s)
    L = 0.2104542553*l_ + 0.7936177850*m_ - 0.0040720468*s_
    A = 1.9779984951*l_ - 2.4285922050*m_ + 0.4505937099*s_
    B = 0.0259040371*l_ + 0.7827717662*m_ - 0.8086757660*s_
    return np.stack([L, A, B], axis=-1)


def region_error(iff_path, ref_rgb01, mask):
    decoded = decode_ilbm_ham6(iff_path).astype(np.float64) / 255.0
    err = np.sqrt(((srgb_to_oklab(decoded) - srgb_to_oklab(ref_rgb01)) ** 2).sum(axis=-1))
    return err[mask].mean()


def main():
    src_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "center_stress.png")
    cli = sys.argv[2] if len(sys.argv) > 2 else os.path.join(
        os.path.dirname(__file__), "..", "build", "tools", "renderham", "renderHAM")
    w, h = (int(sys.argv[3]), int(sys.argv[4])) if len(sys.argv) > 4 else (320, 256)

    ref = np.array(Image.open(src_path).convert("RGB").resize((w, h), Image.LANCZOS)).astype(np.float64) / 255.0

    yy, xx = np.mgrid[0:h, 0:w]
    cy, cx = h / 2, w / 2
    rx, ry = w * 0.16, h * 0.20
    center_mask = (((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2) < 1.0
    periph_mask = ~center_mask

    print(f"{'color focus':<14}{'error center':<18}{'error edge':<14}")
    with tempfile.TemporaryDirectory() as tmp:
        for imp in ["uniform", "center", "edge", "center-edge", "golden-ratio"]:
            out_path = os.path.join(tmp, f"{imp}.iff")
            subprocess.run([cli, "-i", src_path, "-o", out_path, "--color-focus", imp,
                             "--width", str(w), "--height", str(h)],
                            check=True, capture_output=True)
            c = region_error(out_path, ref, center_mask)
            p = region_error(out_path, ref, periph_mask)
            print(f"{imp:<14}{c:<18.5f}{p:<14.5f}")


if __name__ == "__main__":
    main()
