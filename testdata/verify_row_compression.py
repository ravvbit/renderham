# SPDX-License-Identifier: MIT
# Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
import struct, sys

def byterun1_decompress_stream(data, pos, out, row_bytes):
    """Row-aware decompression: decode exactly row_bytes output bytes
    starting at data[pos], mimicking a strict per-row decoder. Returns
    the new position in `data`."""
    produced = 0
    while produced < row_bytes:
        ctrl = struct.unpack('b', data[pos:pos+1])[0]
        pos += 1
        if ctrl >= 0:
            count = ctrl + 1
            if produced + count > row_bytes:
                raise ValueError(f"literal run overshoots row: produced={produced} count={count} row_bytes={row_bytes}")
            out += data[pos:pos+count]
            pos += count
            produced += count
        elif ctrl != -128:
            count = 1 - ctrl
            if produced + count > row_bytes:
                raise ValueError(f"replicate run overshoots row: produced={produced} count={count} row_bytes={row_bytes}")
            out += bytes([data[pos]]) * count
            pos += 1
            produced += count
        # ctrl == -128: no-op, produced unchanged
    return pos

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

def load(path):
    with open(path, 'rb') as f:
        data = f.read()
    form_size = struct.unpack('>I', data[4:8])[0]
    chunks = read_chunks(data, 12, 12+form_size-4)
    info = {cid: body for cid, size, body in chunks}
    w,h,x,y,nplanes,masking,compression,pad1,transp,xa,ya,pw,ph = struct.unpack('>HHhhBBBBHBBhh', info['BMHD'])
    return info, w, h, nplanes, compression

if __name__ == "__main__":
    compressed_path, uncompressed_path = sys.argv[1], sys.argv[2]
    info_c, w, h, nplanes, compression = load(compressed_path)
    info_u, w2, h2, nplanes2, compression2 = load(uncompressed_path)
    assert (w,h,nplanes) == (w2,h2,nplanes2)
    assert compression == 1 and compression2 == 0

    row_bytes = ((w+15)//16)*2
    total_rows = nplanes * h

    body = info_c['BODY']
    out = bytearray()
    pos = 0
    bad_rows = []
    for r in range(total_rows):
        try:
            pos = byterun1_decompress_stream(body, pos, out, row_bytes)
        except Exception as e:
            bad_rows.append((r, str(e)))
            break

    raw = info_u['BODY']
    print(f"row_bytes={row_bytes}  total_rows={total_rows}  expected_bytes={row_bytes*total_rows}")
    print(f"row-aware decompressed: {len(out)} bytes, consumed {pos}/{len(body)} compressed bytes")
    print(f"raw (uncompressed) BODY: {len(raw)} bytes")
    print("bad rows:", bad_rows if bad_rows else "none")
    print("bit-exact match vs. uncompressed reference:", bytes(out) == raw)
    if bytes(out) != raw and len(out) == len(raw):
        diffs = [i for i in range(len(raw)) if out[i] != raw[i]]
        print(f"  {len(diffs)} differing bytes, first few at offsets: {diffs[:10]}")
        for off in diffs[:5]:
            row_idx = off // row_bytes
            print(f"    offset {off} (row {row_idx}, plane {row_idx % nplanes}, y {row_idx // nplanes}): got {out[off]:#04x} expected {raw[off]:#04x}")
