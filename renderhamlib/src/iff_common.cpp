// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "iff_common.hpp"

#include <cstddef>

namespace aic {
namespace iff {

int row_bytes_for(int width) { return ((width + 15) / 16) * 2; }

void put_u8(std::vector<uint8_t>& b, uint8_t v) { b.push_back(v); }
void put_u16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    b.push_back(static_cast<uint8_t>(v & 0xFF));
}
void put_i16(std::vector<uint8_t>& b, int16_t v) { put_u16(b, static_cast<uint16_t>(v)); }
void put_u32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    b.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    b.push_back(static_cast<uint8_t>(v & 0xFF));
}
void put_bytes(std::vector<uint8_t>& b, const std::vector<uint8_t>& d) { b.insert(b.end(), d.begin(), d.end()); }
void put_id(std::vector<uint8_t>& b, const char* id4) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(id4[i]));
}

std::vector<uint8_t> make_chunk(const char* id4, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> chunk;
    put_id(chunk, id4);
    put_u32(chunk, static_cast<uint32_t>(data.size()));
    put_bytes(chunk, data);
    if (data.size() % 2 != 0) chunk.push_back(0);  // IFF chunks are word-aligned
    return chunk;
}

std::vector<uint8_t> chunky_to_planar(const std::vector<uint8_t>& codes, int width, int height, int num_planes) {
    const int row_bytes = row_bytes_for(width);
    std::vector<uint8_t> out(static_cast<size_t>(row_bytes) * num_planes * height, 0);
    size_t out_pos = 0;
    for (int y = 0; y < height; ++y) {
        for (int p = 0; p < num_planes; ++p) {
            uint8_t* row = &out[out_pos];
            for (int x = 0; x < width; ++x) {
                uint8_t code = codes[static_cast<size_t>(y) * width + x];
                if ((code >> p) & 1) row[x / 8] |= static_cast<uint8_t>(0x80 >> (x % 8));
            }
            out_pos += row_bytes;
        }
    }
    return out;
}

std::vector<uint8_t> byterun1_compress(const std::vector<uint8_t>& src) {
    std::vector<uint8_t> out;
    size_t i = 0, n = src.size();
    while (i < n) {
        size_t run = 1;
        while (i + run < n && src[i + run] == src[i] && run < 128) run++;

        if (run >= 2) {
            int ctrl = 1 - static_cast<int>(run);  // -1 .. -127
            out.push_back(static_cast<uint8_t>(static_cast<int8_t>(ctrl)));
            out.push_back(src[i]);
            i += run;
        } else {
            size_t start = i, lit = 0;
            while (i < n && lit < 128) {
                size_t r = 1;
                while (i + r < n && src[i + r] == src[i] && r < 128) r++;
                if (r >= 2) break;  // let the outer loop turn this into a run chunk
                ++lit;
                ++i;
            }
            out.push_back(static_cast<uint8_t>(lit - 1));  // 0..127
            out.insert(out.end(), src.begin() + start, src.begin() + start + lit);
        }
    }
    return out;
}

std::vector<uint8_t> byterun1_compress_planar(const std::vector<uint8_t>& planar, int width, int height,
                                               int num_planes) {
    const int row_bytes = row_bytes_for(width);
    const int total_rows = num_planes * height;
    std::vector<uint8_t> out;
    out.reserve(planar.size());
    for (int r = 0; r < total_rows; ++r) {
        auto begin = planar.begin() + static_cast<std::ptrdiff_t>(r) * row_bytes;
        std::vector<uint8_t> row(begin, begin + row_bytes);
        std::vector<uint8_t> compressed_row = byterun1_compress(row);
        out.insert(out.end(), compressed_row.begin(), compressed_row.end());
    }
    return out;
}

}  // namespace iff
}  // namespace aic
