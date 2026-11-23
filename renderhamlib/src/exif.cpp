// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/exif.hpp"

#include <fstream>
#include <vector>

namespace aic {

namespace {

struct Reader {
    const uint8_t* p;
    size_t n;
    bool big;
    bool ok(size_t off, size_t len) const { return off <= n && len <= n - off; }
    uint16_t u16(size_t off) const {
        return big ? static_cast<uint16_t>((p[off] << 8) | p[off + 1]) : static_cast<uint16_t>((p[off + 1] << 8) | p[off]);
    }
    uint32_t u32(size_t off) const {
        return big ? (uint32_t(p[off]) << 24) | (uint32_t(p[off + 1]) << 16) | (uint32_t(p[off + 2]) << 8) | p[off + 3]
                   : (uint32_t(p[off + 3]) << 24) | (uint32_t(p[off + 2]) << 16) | (uint32_t(p[off + 1]) << 8) | p[off];
    }
};

// `tiff` points at the TIFF header inside the APP1 payload (length `n`).
int orientation_from_tiff(const uint8_t* tiff, size_t n) {
    if (n < 8) return 1;
    Reader r{tiff, n, false};
    if (tiff[0] == 'M' && tiff[1] == 'M') r.big = true;
    else if (!(tiff[0] == 'I' && tiff[1] == 'I')) return 1;
    if (r.u16(2) != 42) return 1;
    size_t ifd = r.u32(4);
    if (!r.ok(ifd, 2)) return 1;
    const unsigned count = r.u16(ifd);
    for (unsigned i = 0; i < count; ++i) {
        size_t e = ifd + 2 + size_t(i) * 12;
        if (!r.ok(e, 12)) return 1;
        if (r.u16(e) != 0x0112) continue;
        if (r.u16(e + 2) != 3 || r.u32(e + 4) < 1) return 1;  // must be one SHORT
        unsigned v = r.u16(e + 8);                            // a SHORT sits left-aligned in the value field
        return (v >= 1 && v <= 8) ? static_cast<int>(v) : 1;
    }
    return 1;
}

}  // namespace

int parse_exif_orientation(const uint8_t* data, size_t size) {
    if (!data || size < 4 || data[0] != 0xFF || data[1] != 0xD8) return 1;  // not a JPEG
    size_t pos = 2;
    while (pos + 4 <= size) {
        if (data[pos] != 0xFF) return 1;  // lost sync
        const uint8_t marker = data[pos + 1];
        if (marker == 0xFF) { ++pos; continue; }          // fill byte
        if (marker == 0xD8 || (marker >= 0xD0 && marker <= 0xD7) || marker == 0x01) { pos += 2; continue; }  // no length
        if (marker == 0xDA || marker == 0xD9) return 1;   // image data / end: no EXIF before it
        const size_t len = (size_t(data[pos + 2]) << 8) | data[pos + 3];
        if (len < 2 || pos + 2 + len > size) return 1;
        if (marker == 0xE1 && len >= 2 + 6 && data[pos + 4] == 'E' && data[pos + 5] == 'x' && data[pos + 6] == 'i' &&
            data[pos + 7] == 'f' && data[pos + 8] == 0 && data[pos + 9] == 0) {
            return orientation_from_tiff(data + pos + 10, len - 2 - 6);
        }
        pos += 2 + len;
    }
    return 1;
}

int read_exif_orientation(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return 1;
    // The EXIF block is one APP1 segment (max. 64 KB) at the very start.
    std::vector<uint8_t> buf(70000);
    f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    return parse_exif_orientation(buf.data(), static_cast<size_t>(f.gcount()));
}

}  // namespace aic
