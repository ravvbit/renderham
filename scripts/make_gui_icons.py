#!/usr/bin/env python3
"""Generates tools/renderhamgui/GuiIcons.hpp: 24x24 alpha masks for the icon buttons of the GUI
(zoom, reset, add/remove keyframe, loop, ping-pong, play, stop, cancel).

Same technique as scripts/make_transform_icons.py: masks instead of Unicode characters in the
button text (a font without the glyph shows empty buttons), tinted with the theme's text color
at run time, so they are also readable on dark themes. Drawn 8 times oversampled.

Usage:  python3 scripts/make_gui_icons.py        (needs Pillow)
"""
import math, os
from PIL import Image, ImageDraw

N, SS = 24, 8
S = N * SS


def canvas():
    return Image.new("L", (S, S), 0)


def finish(img):
    return list(img.resize((N, N), Image.LANCZOS).tobytes())


def P(x, y):
    """Fractions of the edge length -> pixels of the oversampled canvas."""
    return (x * S, y * S)


def line(d, a, b, w, round_caps=True):
    (x0, y0), (x1, y1) = P(*a), P(*b)
    d.line([(x0, y0), (x1, y1)], fill=255, width=int(w * S))
    if round_caps:
        r = w * S / 2
        for (x, y) in ((x0, y0), (x1, y1)):
            d.ellipse((x - r, y - r, x + r, y + r), fill=255)


def arrow_head(d, tip, direction, length=0.22, half_width=0.15):
    """Filled arrow head; direction = unit vector the tip points along."""
    tx, ty = P(*tip)
    dx, dy = direction
    nx, ny = -dy, dx
    L, W = length * S, half_width * S
    d.polygon([(tx, ty), (tx - dx * L + nx * W, ty - dy * L + ny * W), (tx - dx * L - nx * W, ty - dy * L - ny * W)], fill=255)


def magnifier(sign):
    im = canvas(); d = ImageDraw.Draw(im)
    cx, cy, r, w = 0.42, 0.42, 0.27, 0.085
    d.ellipse((P(cx - r, cy - r), P(cx + r, cy + r)), outline=255, width=int(w * S))
    line(d, (0.62, 0.62), (0.87, 0.87), 0.12)
    line(d, (cx - 0.12, cy), (cx + 0.12, cy), 0.07, round_caps=False)
    if sign > 0:
        line(d, (cx, cy - 0.12), (cx, cy + 0.12), 0.07, round_caps=False)
    return im


def zoom_actual():
    """'1:1'."""
    im = canvas(); d = ImageDraw.Draw(im)
    for cx in (0.22, 0.78):
        line(d, (cx, 0.26), (cx, 0.74), 0.085, round_caps=False)             # shaft
        d.polygon([P(cx - 0.13, 0.36), P(cx, 0.24), P(cx, 0.37)], fill=255)  # flag
        line(d, (cx - 0.11, 0.74), (cx + 0.11, 0.74), 0.07, round_caps=False)  # foot
    for y in (0.40, 0.62):
        r = 0.055
        d.ellipse((P(0.50 - r, y - r), P(0.50 + r, y + r)), fill=255)
    return im


def zoom_fit():
    """Four corner brackets around a picture."""
    im = canvas(); d = ImageDraw.Draw(im)
    m, L, w = 0.10, 0.24, 0.085
    for sx in (0, 1):
        for sy in (0, 1):
            x = m if sx == 0 else 1 - m
            y = m if sy == 0 else 1 - m
            ex = x + (L if sx == 0 else -L)
            ey = y + (L if sy == 0 else -L)
            line(d, (x, y), (ex, y), w, round_caps=False)
            line(d, (x, y), (x, ey), w, round_caps=False)
    d.rectangle((P(0.30, 0.32), P(0.70, 0.68)), fill=255)
    return im


def reset():
    """Half-circle arrow, counter-clockwise (like 'undo')."""
    im = canvas(); d = ImageDraw.Draw(im)
    cx, cy, r, w = 0.50, 0.56, 0.28, 0.095
    d.arc((P(cx - r, cy - r), P(cx + r, cy + r)), 180, 400, fill=255, width=int(w * S))
    arrow_head(d, (cx - r, cy + 0.12), (0, 1), length=0.24, half_width=0.17)
    return im


def keyframe(sign):
    im = canvas(); d = ImageDraw.Draw(im)
    cx, cy, h = 0.38, 0.40, 0.29
    d.polygon([P(cx, cy - h), P(cx + h, cy), P(cx, cy + h), P(cx - h, cy)], fill=255)
    bx, by, a, w = 0.75, 0.75, 0.18, 0.095
    line(d, (bx - a, by), (bx + a, by), w, round_caps=False)
    if sign > 0:
        line(d, (bx, by - a), (bx, by + a), w, round_caps=False)
    return im


def loop():
    """Repeat: two hooks with arrow heads."""
    im = canvas(); d = ImageDraw.Draw(im)
    w = 0.085
    line(d, (0.16, 0.64), (0.16, 0.36), w)           # left side up
    line(d, (0.16, 0.36), (0.66, 0.36), w)           # top edge to the right
    arrow_head(d, (0.92, 0.36), (1, 0), length=0.26, half_width=0.17)
    line(d, (0.84, 0.36), (0.84, 0.64), w)           # right side down
    line(d, (0.84, 0.64), (0.34, 0.64), w)           # bottom edge to the left
    arrow_head(d, (0.08, 0.64), (-1, 0), length=0.26, half_width=0.17)
    return im


def pingpong():
    im = canvas(); d = ImageDraw.Draw(im)
    line(d, (0.22, 0.50), (0.78, 0.50), 0.09, round_caps=False)
    arrow_head(d, (0.94, 0.50), (1, 0), length=0.28, half_width=0.2)
    arrow_head(d, (0.06, 0.50), (-1, 0), length=0.28, half_width=0.2)
    return im


def play():
    im = canvas(); d = ImageDraw.Draw(im)
    d.polygon([P(0.30, 0.16), P(0.30, 0.84), P(0.84, 0.50)], fill=255)
    return im


def stop():
    im = canvas(); d = ImageDraw.Draw(im)
    d.rectangle((P(0.22, 0.22), P(0.78, 0.78)), fill=255)
    return im


def cancel():
    im = canvas(); d = ImageDraw.Draw(im)
    line(d, (0.24, 0.24), (0.76, 0.76), 0.12)
    line(d, (0.76, 0.24), (0.24, 0.76), 0.12)
    return im


icons = {
    "kIconZoomIn": finish(magnifier(+1)),
    "kIconZoomOut": finish(magnifier(-1)),
    "kIconZoomActual": finish(zoom_actual()),
    "kIconZoomFit": finish(zoom_fit()),
    "kIconReset": finish(reset()),
    "kIconKeyframeAdd": finish(keyframe(+1)),
    "kIconKeyframeRemove": finish(keyframe(-1)),
    "kIconLoop": finish(loop()),
    "kIconPingPong": finish(pingpong()),
    "kIconPlay": finish(play()),
    "kIconStop": finish(stop()),
    "kIconCancel": finish(cancel()),
}

here = os.path.dirname(os.path.abspath(__file__))
out = os.path.join(here, "..", "tools", "renderhamgui", "GuiIcons.hpp")
with open(out, "w") as f:
    f.write("// SPDX-License-Identifier: MIT\n")
    f.write("// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.\n")
    f.write("// Generated by scripts/make_gui_icons.py - do not edit by hand.\n")
    f.write("// 24x24 alpha masks; tinted with the theme's text color at run time.\n#pragma once\n\n")
    f.write("constexpr int kGuiIconSize = %d;\n\n" % N)
    for name, data in icons.items():
        f.write("constexpr unsigned char %s[%d] = {\n" % (name, N * N))
        for r in range(N):
            f.write("    " + ", ".join("%3d" % v for v in data[r * N:(r + 1) * N]) + ",\n")
        f.write("};\n\n")

# Preview for looking at: enlarged view and original size
big = N * 5
prev = Image.new("L", (big * len(icons) + 8 * (len(icons) + 1), big + N + 24), 235)
for i, (name, data) in enumerate(icons.items()):
    m = Image.new("L", (N, N)); m.putdata([255 - v for v in data])
    prev.paste(m.resize((big, big), Image.NEAREST), (8 + i * (big + 8), 8))
    prev.paste(m, (8 + i * (big + 8), big + 14))
outdir = os.path.join(here, "..", "testdata", "out")
os.makedirs(outdir, exist_ok=True)
prev.save(os.path.join(outdir, "gui_icons_preview.png"))
print("written:", os.path.normpath(out), "and a preview")
