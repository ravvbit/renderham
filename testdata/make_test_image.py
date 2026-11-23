# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
import os
import numpy as np
from PIL import Image
import colorsys

W, H = 640, 400
img = np.zeros((H, W, 3), dtype=np.float64)

# Top 2/3: smooth HSV hue gradient (horizontal) x brightness gradient (vertical)
for y in range(0, int(H*0.66)):
    v = 0.35 + 0.65 * (1 - y / (H*0.66))
    for x in range(W):
        h = x / W
        r, g, b = colorsys.hsv_to_rgb(h, 0.85, v)
        img[y, x] = (r, g, b)

# Soft radial "sphere" shading, photo-like smooth gradient (stress test for banding)
cx, cy, rad = W*0.22, H*0.5+40, 90
yy, xx = np.mgrid[0:H, 0:W]
d = np.sqrt((xx-cx)**2 + (yy-cy)**2) / rad
mask = d < 1.0
shade = np.clip(1.0 - d, 0, 1)**1.5
sphere_color = np.array([0.85, 0.25, 0.15])
band = int(H*0.66)
region = slice(max(0,int(cy-rad-10)), min(H,int(cy+rad+10)))
for y in range(region.start, region.stop):
    for x in range(W):
        dd = np.sqrt((x-cx)**2 + (y-cy)**2)/rad
        if dd < 1.0 and y >= band-60:
            s = np.clip(1-dd,0,1)**1.5
            img[y,x] = sphere_color*0.3 + sphere_color*0.9*s

# Bottom strip: solid saturated color swatches (stress test palette selection)
swatches = [(1,0,0),(0,1,0),(0,0,1),(0,1,1),(1,0,1),(1,1,0),(0,0,0),(1,1,1),(0.5,0.5,0.5)]
sw_h0 = int(H*0.66)
sw_h1 = int(H*0.83)
n = len(swatches)
for i,c in enumerate(swatches):
    x0 = int(i*W/n); x1=int((i+1)*W/n)
    img[sw_h0:sw_h1, x0:x1] = c

# Bottom-right corner: fine checkerboard (worst case high-frequency detail)
cb_y0, cb_x0 = int(H*0.85), int(W*0.75)
for y in range(cb_y0, H):
    for x in range(cb_x0, W):
        v = 1.0 if ((x//3 + y//3) % 2 == 0) else 0.0
        img[y,x] = (v,v,v)

# Bottom-left: near-gray low-saturation smooth gradient (classic HAM fringing stress test)
gy0, gy1 = int(H*0.85), H
for y in range(gy0, gy1):
    for x in range(0, cb_x0):
        t = x / cb_x0
        g = 0.35 + 0.3*t
        img[y,x] = (g, g, g*1.03)

img = np.clip(img, 0, 1)
out = (img*255+0.5).astype(np.uint8)
Image.fromarray(out, 'RGB').save(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test_input.png'))
print("wrote", out.shape)
