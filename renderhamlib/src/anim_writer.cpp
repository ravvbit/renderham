// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/anim_writer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <optional>

#include "aic/limits.hpp"
#include "iff_common.hpp"

namespace aic {

int anim_jiffies_for_fps(int fps) {
    fps = std::clamp(fps, 1, kAnimJiffiesPerSecond);
    return std::max(1, (kAnimJiffiesPerSecond + fps / 2) / fps);
}

namespace {

constexpr int kNumPlanes = 6;  // HAM6

// The ANHD chunk is a fixed 40-byte structure (see the IFF ANIM spec
// by Gary Bonham/Sparta, 1998): operation(1) + mask(1) + w(2) + h(2) +
// x(2) + y(2) + abstime(4) + reltime(4) + interleave(1) + pad0(1) +
// bits(4) + pad[16](16) = 40. mask/w/h/x/y are XOR-mode-only fields,
// left zero here since method 5 doesn't use them. interleave=0 means
// "2 frames back" (the standard double-buffered scheme every real
// player supports) rather than the less-widely-supported interleave=1
// variant some brush tools use.
std::vector<uint8_t> make_anhd(uint8_t operation, uint32_t reltime_jiffies) {
    using namespace iff;
    std::vector<uint8_t> anhd;
    put_u8(anhd, operation);
    put_u8(anhd, 0);   // mask
    put_u16(anhd, 0);  // w
    put_u16(anhd, 0);  // h
    put_i16(anhd, 0);  // x
    put_i16(anhd, 0);  // y
    put_u32(anhd, 0);  // abstime
    put_u32(anhd, reltime_jiffies);
    put_u8(anhd, 0);  // interleave = 0 -> defaults to 2 frames back
    put_u8(anhd, 0);  // pad0
    put_u32(anhd, 0);  // bits - all zero, as the spec recommends for method 5
    for (int i = 0; i < 16; ++i) put_u8(anhd, 0);
    return anhd;
}

// ANIM-5 ("byte vertical delta", Jim Kent): for each of the 6
// bitplanes, each byte-column (a fixed x-byte-position, walked down
// through every row) is encoded independently as a run of ops:
//   - Skip  (0x01-0x7F): that many rows are unchanged from `ref` -
//     nothing stored, just advance.
//   - Uniq  (0x80-0xFF): low 7 bits = a literal byte count that
//     follows, one new byte per row.
//   - Same  (0x00, count, value): `count` rows all get set to the
//     single repeated `value` - an optional space-saving op this
//     encoder deliberately never emits (see below), but still
//     spec-legal for a decoder to encounter.
// A column's data is prefixed with an op-count byte, so a decoder
// knows when to stop - meaning any single column needs <= 255 ops
// total, which bounds how "noisy" (rapidly alternating changed/
// unchanged) a 6-plane HAM6 column can be before this encoder has to
// give up (see the loop below). This encoder only ever emits Skip and
// Uniq - detecting uniform-value runs for the more compact Same op is
// a possible future optimization, not a correctness requirement: a
// decoder that understands Same still decodes Uniq-only data
// correctly, since Same is purely an alternative encoding for a case
// Uniq already covers (just less compactly).
std::optional<std::vector<uint8_t>> encode_delta(const std::vector<uint8_t>& cur_planar,
                                                   const std::vector<uint8_t>& ref_planar, int width, int height,
                                                   std::string* error_out) {
    const int row_bytes = iff::row_bytes_for(width);
    std::vector<uint32_t> plane_offsets(16, 0);
    std::vector<uint8_t> plane_data;

    auto byte_at = [&](const std::vector<uint8_t>& buf, int plane, int row, int col) {
        return buf[(static_cast<size_t>(row) * kNumPlanes + static_cast<size_t>(plane)) * row_bytes +
                    static_cast<size_t>(col)];
    };

    for (int p = 0; p < kNumPlanes; ++p) {
        plane_offsets[static_cast<size_t>(p)] = static_cast<uint32_t>(64 + plane_data.size());
        for (int col = 0; col < row_bytes; ++col) {
            std::vector<uint8_t> col_ops;
            int num_ops = 0;
            int row = 0;
            while (row < height) {
                bool same_as_ref = byte_at(cur_planar, p, row, col) == byte_at(ref_planar, p, row, col);
                int run = 0;
                if (same_as_ref) {
                    while (row + run < height && run < 127 &&
                           byte_at(cur_planar, p, row + run, col) == byte_at(ref_planar, p, row + run, col)) {
                        ++run;
                    }
                    col_ops.push_back(static_cast<uint8_t>(run));
                } else {
                    while (row + run < height && run < 127 &&
                           byte_at(cur_planar, p, row + run, col) != byte_at(ref_planar, p, row + run, col)) {
                        ++run;
                    }
                    col_ops.push_back(static_cast<uint8_t>(0x80 | run));
                    for (int i = 0; i < run; ++i) col_ops.push_back(byte_at(cur_planar, p, row + i, col));
                }
                row += run;
                ++num_ops;
                if (num_ops > 255) {
                    if (error_out) {
                        *error_out =
                            "ANIM delta encoding failed: a bitplane column changed too many times between "
                            "consecutive frames to fit the format's 255-op-per-column limit (op5 expects mostly "
                            "unchanged/contiguous regions between frames, e.g. a slow pan - very noisy or "
                            "unrelated consecutive frames can exceed it)";
                    }
                    return std::nullopt;
                }
            }
            plane_data.push_back(static_cast<uint8_t>(num_ops));
            plane_data.insert(plane_data.end(), col_ops.begin(), col_ops.end());
        }
    }

    std::vector<uint8_t> dlta;
    for (int i = 0; i < 16; ++i) iff::put_u32(dlta, plane_offsets[static_cast<size_t>(i)]);
    iff::put_bytes(dlta, plane_data);
    return dlta;
}

}  // namespace

bool write_ham6_anim(const std::vector<HAM6Frame>& frames, const std::string& path, const AnimWriteOptions& opts,
                      std::string* error_out) {
    if (frames.size() < 2) {
        if (error_out) *error_out = "ANIM needs at least 2 frames";
        return false;
    }
    const HAM6Frame& f0 = frames[0];
    if (f0.width < 1 || f0.height < 1 || f0.width > limits::kMaxIffDimension ||
        f0.height > limits::kMaxIffDimension) {
        if (error_out) *error_out = "frame size cannot be stored in an IFF file";
        return false;
    }
    if (f0.palette.colors.size() != 16) {
        if (error_out) *error_out = "HAM6 requires an exact 16-color base palette";
        return false;
    }
    for (size_t i = 1; i < frames.size(); ++i) {
        if (frames[i].width != f0.width || frames[i].height != f0.height) {
            if (error_out) *error_out = "all ANIM frames must share the same dimensions";
            return false;
        }
        if (frames[i].palette.colors.size() != 16) {
            if (error_out) *error_out = "HAM6 requires an exact 16-color base palette";
            return false;
        }
        for (int c = 0; c < 16; ++c) {
            const AmigaColor& a = frames[i].palette.colors[static_cast<size_t>(c)];
            const AmigaColor& b = f0.palette.colors[static_cast<size_t>(c)];
            if (a.r4 != b.r4 || a.g4 != b.g4 || a.b4 != b.b4) {
                if (error_out) *error_out = "all ANIM frames must share the same palette";
                return false;
            }
        }
    }

    using namespace iff;

    std::vector<uint8_t> bmhd;
    put_u16(bmhd, static_cast<uint16_t>(f0.width));
    put_u16(bmhd, static_cast<uint16_t>(f0.height));
    put_i16(bmhd, 0);
    put_i16(bmhd, 0);
    put_u8(bmhd, kNumPlanes);
    put_u8(bmhd, 0);
    put_u8(bmhd, opts.compress ? 1 : 0);
    put_u8(bmhd, 0);
    put_u16(bmhd, 0);
    put_u8(bmhd, 10);
    put_u8(bmhd, 11);
    put_i16(bmhd, static_cast<int16_t>(f0.width));
    put_i16(bmhd, static_cast<int16_t>(f0.height));

    std::vector<uint8_t> cmap;
    cmap.reserve(48);
    for (const auto& c : f0.palette.colors) {
        auto rep = [](uint8_t v4) -> uint8_t { return static_cast<uint8_t>((v4 << 4) | v4); };
        cmap.push_back(rep(c.r4));
        cmap.push_back(rep(c.g4));
        cmap.push_back(rep(c.b4));
    }

    std::vector<uint8_t> camg;
    uint32_t camg_value = 0x00000800u;         // CAMG_HAM
    if (opts.lace) camg_value |= 0x00000004u;  // CAMG_LACE
    put_u32(camg, camg_value);

    // Every frame's planar bitplane bytes are needed up front, since
    // frame i's delta reference is 2 frames back (not just 1) - see
    // encode_delta's caller loop below.
    std::vector<std::vector<uint8_t>> planars;
    planars.reserve(frames.size());
    for (const auto& f : frames) planars.push_back(chunky_to_planar(f.pixel_codes, f.width, f.height, kNumPlanes));

    std::vector<uint8_t> body_data = opts.compress
                                          ? byterun1_compress_planar(planars[0], f0.width, f0.height, kNumPlanes)
                                          : planars[0];

    std::vector<uint8_t> frame0_content;
    put_id(frame0_content, "ILBM");
    put_bytes(frame0_content, make_chunk("BMHD", bmhd));
    put_bytes(frame0_content, make_chunk("CMAP", cmap));
    put_bytes(frame0_content, make_chunk("CAMG", camg));
    put_bytes(frame0_content, make_chunk("BODY", body_data));

    std::vector<uint8_t> anim_body;
    put_id(anim_body, "ANIM");
    put_bytes(anim_body, make_chunk("FORM", frame0_content));

    // Double-buffered scheme (matches ANHD interleave=0, the widely
    // supported default): frame i (i>=2 here, 0-indexed so i>=1 means
    // the 2nd frame overall) deltas against 2 frames back, except the
    // very first delta (i==1, the 2nd frame overall) which has no
    // choice but to reference frame 0 - see the spec quote this
    // mirrors: "frame 2 is stored as differences from frame 1, but all
    // other frames are stored as differences from two frames back."
    for (size_t i = 1; i < frames.size(); ++i) {
        size_t ref_idx = (i == 1) ? 0 : i - 2;
        auto delta = encode_delta(planars[i], planars[ref_idx], f0.width, f0.height, error_out);
        if (!delta) return false;

        std::vector<uint8_t> anhd = make_anhd(5, static_cast<uint32_t>(opts.reltime_jiffies));

        std::vector<uint8_t> frame_content;
        // Still nested as "FORM ILBM" per the spec's own acknowledged
        // inconsistency: these chunks aren't really interleaved
        // bitmaps (they hold ANHD+DLTA instead of BMHD+BODY), but
        // existing tooling universally expects the ILBM form-type
        // here anyway, so that's what gets written.
        put_id(frame_content, "ILBM");
        put_bytes(frame_content, make_chunk("ANHD", anhd));
        put_bytes(frame_content, make_chunk("DLTA", *delta));

        put_bytes(anim_body, make_chunk("FORM", frame_content));
    }

    std::vector<uint8_t> file;
    put_id(file, "FORM");
    put_u32(file, static_cast<uint32_t>(anim_body.size()));
    put_bytes(file, anim_body);

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        if (error_out) *error_out = "could not open output file: " + path;
        return false;
    }
    out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
    if (!out) {
        if (error_out) *error_out = "write failed for: " + path;
        return false;
    }
    return true;
}

}  // namespace aic
