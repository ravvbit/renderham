#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
"""Generates the small test pictures for tests/test_orientation_alpha.cpp.

  exif_orient6.jpg    stored 200x100, EXIF orientation 6: red on the left, blue on the right.
                      Displayed upright (90 degrees clockwise): 100x200, red on TOP, blue at the BOTTOM.
  exif_orient3.jpg    stored 120x60, orientation 3 (180 degrees): marked green at the top left,
                      upright the mark is at the bottom right.
  alpha_samples.png   RGBA 4x1: (255,255,255,128) | (255,0,0,0) | (0,0,255,255) | (0,255,0,64)

Usage:  python3 testdata/make_orientation_alpha_images.py   (needs Pillow)
"""
import os
from PIL import Image, ImageDraw

here = os.path.dirname(os.path.abspath(__file__))

im = Image.new("RGB", (200, 100), (30, 30, 30))
d = ImageDraw.Draw(im)
d.rectangle((0, 0, 99, 99), fill=(230, 20, 20))
d.rectangle((100, 0, 199, 99), fill=(20, 20, 230))
ex = Image.Exif(); ex[0x0112] = 6
im.save(os.path.join(here, "exif_orient6.jpg"), quality=95, subsampling=0, exif=ex)

im = Image.new("RGB", (120, 60), (200, 200, 200))
d = ImageDraw.Draw(im)
d.rectangle((0, 0, 39, 19), fill=(20, 200, 20))   # top left
ex = Image.Exif(); ex[0x0112] = 3
im.save(os.path.join(here, "exif_orient3.jpg"), quality=95, subsampling=0, exif=ex)

a = Image.new("RGBA", (4, 1))
for x, px in enumerate([(255, 255, 255, 128), (255, 0, 0, 0), (0, 0, 255, 255), (0, 255, 0, 64)]):
    a.putpixel((x, 0), px)
a.save(os.path.join(here, "alpha_samples.png"))
print("ok")
