# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
import os
import numpy as np
from PIL import Image
import colorsys, random

random.seed(42)
W, H = 640, 512
img = np.zeros((H, W, 3), dtype=np.float64)

# Periphery: busy mosaic of many distinct, highly saturated random colors
# (large area AND high variety - a naive quantizer will happily spend
# most of the palette here, since it's both common and diverse).
tile = 24
for by in range(0, H, tile):
    for bx in range(0, W, tile):
        hue = random.random()
        sat = 0.7 + 0.3*random.random()
        val = 0.6 + 0.4*random.random()
        r,g,b = colorsys.hsv_to_rgb(hue, sat, val)
        img[by:by+tile, bx:bx+tile] = (r,g,b)

# Center: a smooth, LOW-variety skin-tone-like gradient oval (the actual
# "subject"). Small OKLab volume, easily merged into a single nearby
# periphery cluster under unweighted quantization - but banding here is
# exactly what a portrait shot can't afford.
cx, cy = W/2, H/2
rx, ry = W*0.16, H*0.20
yy, xx = np.mgrid[0:H, 0:W]
d = np.sqrt(((xx-cx)/rx)**2 + ((yy-cy)/ry)**2)
mask = d < 1.0
# light-to-shadow skin tone gradient across the oval
t = np.clip((xx - (cx-rx)) / (2*rx), 0, 1)
base_light  = np.array([0.94, 0.80, 0.69])
base_shadow = np.array([0.55, 0.38, 0.32])
for c in range(3):
    grad = base_light[c]*(1-t) + base_shadow[c]*t
    img[..., c] = np.where(mask, grad, img[..., c])

img = np.clip(img,0,1)
out = (img*255+0.5).astype(np.uint8)
Image.fromarray(out,'RGB').save(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'center_stress.png'))
print("saved", out.shape, "center pixels:", mask.sum())
