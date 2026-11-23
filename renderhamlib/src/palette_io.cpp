// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/palette_io.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

#include "aic/ham6_encoder.hpp"
#include "aic/ilbm_writer.hpp"

namespace aic {
namespace {

uint32_t read_u32_be(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

void fail(std::string* error_out, const std::string& msg) {
    if (error_out) *error_out = msg;
}

}  // namespace

bool save_palette(const Palette& palette, const std::string& path, std::string* error_out) {
    // A trivial 1x1 HAM6Frame carrying only this palette, written
    // through the exact same writer real output uses - deliberately
    // not a bespoke "palette file" format, so the result is a genuine,
    // openable ILBM (a real Amiga viewer just sees a 1x1 picture).
    HAM6Frame frame;
    frame.width = 1;
    frame.height = 1;
    frame.palette = palette;
    frame.pixel_codes = {0};  // single pixel, select-mode -> palette index 0
    return write_ham6_ilbm(frame, path, IlbmWriteOptions{}, error_out);
}

bool load_palette(const std::string& path, Palette* out, std::string* error_out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        fail(error_out, "Could not open file: " + path);
        return false;
    }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    if (data.size() < 12 || std::memcmp(data.data(), "FORM", 4) != 0) {
        fail(error_out, "Not an IFF file (missing FORM header)");
        return false;
    }
    if (std::memcmp(data.data() + 8, "ILBM", 4) != 0) {
        fail(error_out, "Not an ILBM file");
        return false;
    }

    size_t pos = 12;
    while (pos + 8 <= data.size()) {
        const uint8_t* chunk_id = data.data() + pos;
        uint32_t chunk_size = read_u32_be(data.data() + pos + 4);
        size_t data_start = pos + 8;
        if (data_start + chunk_size > data.size()) {
            fail(error_out, "Truncated chunk in file");
            return false;
        }
        if (std::memcmp(chunk_id, "CMAP", 4) == 0) {
            if (chunk_size % 3 != 0) {
                fail(error_out, "Malformed CMAP chunk (size not a multiple of 3)");
                return false;
            }
            size_t num_colors = chunk_size / 3;
            if (num_colors != 16) {
                fail(error_out, "Expected a 16-color palette (HAM6's base palette), file has " +
                                     std::to_string(num_colors));
                return false;
            }
            Palette result;
            result.colors.reserve(16);
            for (size_t i = 0; i < 16; ++i) {
                const uint8_t* rgb = data.data() + data_start + i * 3;
                // Inverse of the nibble-replication (v*17) this project's
                // own writer uses - round() also handles genuine
                // real-Amiga-authored CMAP values gracefully, which
                // won't necessarily be exact multiples of 17.
                auto to4bit = [](uint8_t v8) {
                    int v4 = static_cast<int>(std::lround(v8 / 17.0));
                    return static_cast<uint8_t>(std::clamp(v4, 0, 15));
                };
                result.colors.push_back(AmigaColor{to4bit(rgb[0]), to4bit(rgb[1]), to4bit(rgb[2])});
            }
            *out = result;
            return true;
        }
        pos = data_start + chunk_size + (chunk_size % 2);  // chunks are word-aligned
    }

    fail(error_out, "No CMAP chunk found in file");
    return false;
}

}  // namespace aic
