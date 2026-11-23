#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
"""scripts/export_sample_photos.py - writes freely licensed sample photos
(from scikit-image: public domain or CC0) as PNG, for scripts/ham_eval.py.
The pictures are deliberately NOT checked into the repository.

Usage: scripts/export_sample_photos.py [target directory, default /tmp/hamtest]
"""
import os, sys
from PIL import Image
from skimage import data

out = sys.argv[1] if len(sys.argv) > 1 else "/tmp/hamtest"
os.makedirs(out, exist_ok=True)
for name in ("astronaut", "chelsea", "coffee", "rocket"):
    Image.fromarray(getattr(data, name)()).convert("RGB").save(os.path.join(out, name + ".png"))
    print("written:", os.path.join(out, name + ".png"))
