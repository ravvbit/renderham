# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
import struct, sys

def read_chunks(data, start, end):
    chunks = []
    i = start
    while i < end:
        cid = data[i:i+4].decode('ascii')
        size = struct.unpack('>I', data[i+4:i+8])[0]
        body = data[i+8:i+8+size]
        chunks.append((cid, size, body))
        i += 8 + size + (size % 2)
    return chunks

def byterun1_decompress(data):
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        ctrl = struct.unpack('b', data[i:i+1])[0]
        i += 1
        if ctrl >= 0:
            count = ctrl + 1
            out += data[i:i+count]
            i += count
        elif ctrl != -128:
            count = 1 - ctrl
            out += bytes([data[i]]) * count
            i += 1
    return bytes(out)

def parse(path):
    with open(path, 'rb') as f:
        data = f.read()
    assert data[0:4] == b'FORM', f"not an IFF FORM file: {data[0:4]}"
    form_size = struct.unpack('>I', data[4:8])[0]
    assert data[8:12] == b'ILBM', f"not ILBM: {data[8:12]}"
    print(f"{path}: FORM size={form_size} (+8 header = {form_size+8}, actual file={len(data)})")
    assert form_size + 8 == len(data), "FORM size does not match actual file size!"

    chunks = read_chunks(data, 12, 12+form_size-4)
    info = {}
    for cid, size, body in chunks:
        print(f"  chunk {cid}: {size} bytes")
        info[cid] = body

    bmhd = info['BMHD']
    w,h,x,y,nplanes,masking,compression,pad1,transp,xa,ya,pw,ph = struct.unpack('>HHhhBBBBHBBhh', bmhd)
    print(f"  BMHD: {w}x{h} nPlanes={nplanes} masking={masking} compression={compression} xAspect={xa} yAspect={ya} page={pw}x{ph}")

    cmap = info['CMAP']
    colors = [tuple(cmap[i:i+3]) for i in range(0, len(cmap), 3)]
    print(f"  CMAP: {len(colors)} colors, first 4: {colors[:4]}")

    camg = struct.unpack('>I', info['CAMG'])[0]
    print(f"  CAMG: 0x{camg:04x}  (HAM bit set: {bool(camg & 0x800)})")

    body = info['BODY']
    if compression == 1:
        planar = byterun1_decompress(body)
        print(f"  BODY: compressed {len(body)} -> decompressed {len(planar)} bytes")
    else:
        planar = body
        print(f"  BODY: uncompressed {len(planar)} bytes")

    row_bytes = ((w+15)//16)*2
    expected = row_bytes * nplanes * h
    print(f"  expected planar size: row_bytes={row_bytes} * nplanes={nplanes} * h={h} = {expected}  match={expected==len(planar)}")
    return planar, colors, camg, (w,h,nplanes,compression)

if __name__ == "__main__":
    planar_c, colors_c, camg_c, meta_c = parse(sys.argv[1])
    print()
    planar_u, colors_u, camg_u, meta_u = parse(sys.argv[2])
    print()
    print("Round-trip check: decompressed BODY from compressed file == raw BODY from uncompressed file?",
          planar_c == planar_u)
    print("Palettes identical?", colors_c == colors_u)
