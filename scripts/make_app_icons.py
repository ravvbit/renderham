#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
"""Generates the application icons for Windows and macOS from the SVG icon of the Linux packages:

    packaging/windows/renderham.ico    (16 ... 256 px, the 256 px entry PNG-compressed)
    packaging/macos/renderham.icns     (16 ... 1024 px, PNG entries as macOS 10.7 and newer read them)

The binaries are committed, so building renderHAM needs neither this script nor the tools it uses.
Run it again when packaging/linux/io.github.ravvbit.renderham.svg changes.

Usage:  python3 scripts/make_app_icons.py        (needs Pillow and rsvg-convert)
"""
import io
import os
import struct
import subprocess

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SVG = os.path.join(ROOT, "packaging", "linux", "io.github.ravvbit.renderham.svg")


def render(size):
    png = subprocess.run(["rsvg-convert", "-w", str(size), "-h", str(size), SVG], check=True,
                         stdout=subprocess.PIPE).stdout
    return Image.open(io.BytesIO(png)).convert("RGBA")


def write_ico(path):
    sizes = [16, 24, 32, 48, 64, 128, 256]
    images = [render(s) for s in sizes]
    images[-1].save(path, format="ICO", sizes=[(s, s) for s in sizes], append_images=images[:-1])


# ICNS entry types that hold a PNG: type -> pixel size.
ICNS_TYPES = [("icp4", 16), ("icp5", 32), ("ic11", 32), ("ic12", 64), ("ic07", 128), ("ic13", 256),
              ("ic08", 256), ("ic14", 512), ("ic09", 512), ("ic10", 1024)]


def write_icns(path):
    cache = {}
    entries = b""
    for type_, size in ICNS_TYPES:
        if size not in cache:
            buf = io.BytesIO()
            render(size).save(buf, format="PNG", optimize=True)
            cache[size] = buf.getvalue()
        data = cache[size]
        entries += type_.encode("ascii") + struct.pack(">I", 8 + len(data)) + data
    with open(path, "wb") as f:
        f.write(b"icns" + struct.pack(">I", 8 + len(entries)) + entries)


if __name__ == "__main__":
    ico = os.path.join(ROOT, "packaging", "windows", "renderham.ico")
    icns = os.path.join(ROOT, "packaging", "macos", "renderham.icns")
    write_ico(ico)
    write_icns(icns)
    for p in (ico, icns):
        print(p, os.path.getsize(p), "bytes")
