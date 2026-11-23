# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
import struct, sys
import numpy as np
from PIL import Image

import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from verify_ilbm import read_chunks, byterun1_decompress

def decode_ilbm_ham6(path, line_start="border"):
    # line_start="border": ILBM spec - a Modify as the first pixel of a line
    #   modifies the border color (palette register 0).
    # line_start="black": the convention of netpbm's ilbmtoppm.
    # Only differs for files whose lines may start with a Modify
    # (renderHAM --line-start-modify).
    with open(path, 'rb') as f:
        data = f.read()
    form_size = struct.unpack('>I', data[4:8])[0]
    chunks = read_chunks(data, 12, 12+form_size-4)
    info = {cid: body for cid, size, body in chunks}

    w,h,x,y,nplanes,masking,compression,pad1,transp,xa,ya,pw,ph = struct.unpack('>HHhhBBBBHBBhh', info['BMHD'])
    cmap = info['CMAP']
    palette = [(cmap[i], cmap[i+1], cmap[i+2]) for i in range(0, 48, 3)]

    body = info['BODY']
    planar = byterun1_decompress(body) if compression == 1 else body

    row_bytes = ((w+15)//16)*2
    # planar -> chunky (independent re-implementation, NOT reusing C++ logic)
    codes = np.zeros((h, w), dtype=np.uint8)
    pos = 0
    for yy in range(h):
        for p in range(nplanes):
            row = planar[pos:pos+row_bytes]
            pos += row_bytes
            for xx in range(w):
                byte = row[xx // 8]
                bit = (byte >> (7 - (xx % 8))) & 1
                if bit:
                    codes[yy, xx] |= (1 << p)

    # HAM6 decode (independent re-implementation of the spec, not the C++ code)
    out = np.zeros((h, w, 3), dtype=np.uint8)
    for yy in range(h):
        held = (0, 0, 0) if line_start == "black" else (palette[0][0] >> 4, palette[0][1] >> 4, palette[0][2] >> 4)
        for xx in range(w):
            code = int(codes[yy, xx])
            mode = (code >> 4) & 3
            val = code & 0xF
            if mode == 0:
                r4, g4, b4 = palette[val][0] >> 4, palette[val][1] >> 4, palette[val][2] >> 4
            elif mode == 1:  # modify blue
                r4, g4, b4 = held[0], held[1], val
            elif mode == 2:  # modify red
                r4, g4, b4 = val, held[1], held[2]
            else:  # modify green
                r4, g4, b4 = held[0], val, held[2]
            held = (r4, g4, b4)
            out[yy, xx] = (r4*17, g4*17, b4*17)  # 4-bit -> 8-bit nibble replication
    return out

if __name__ == "__main__":
    ilbm_path, ref_png_path, out_png_path = sys.argv[1], sys.argv[2], sys.argv[3]
    mode = "black" if "--line-start=black" in sys.argv[4:] else "border"
    decoded = decode_ilbm_ham6(ilbm_path, mode)
    Image.fromarray(decoded, 'RGB').save(out_png_path)

    ref = np.array(Image.open(ref_png_path).convert('RGB'))
    print("Independent Python HAM6 decode vs. C++ tool's --preview PNG:")
    print("  shapes:", decoded.shape, ref.shape)
    diff = np.abs(decoded.astype(int) - ref.astype(int))
    print("  identical pixels: %d / %d (%.4f%%)" % ((diff.sum(axis=2)==0).sum(), decoded.shape[0]*decoded.shape[1],
          100.0*(diff.sum(axis=2)==0).sum()/(decoded.shape[0]*decoded.shape[1])))
    print("  max per-channel diff:", diff.max())
    print("  mean per-channel diff:", diff.mean())
