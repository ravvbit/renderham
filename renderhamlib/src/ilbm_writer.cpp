// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/ilbm_writer.hpp"
#include "aic/limits.hpp"

#include <cstdint>
#include <fstream>

#include "file_path.hpp"
#include "iff_common.hpp"

namespace aic {
namespace {

constexpr int kNumPlanes = 6;  // HAM6

}  // namespace

bool write_ham6_ilbm(const HAM6Frame& frame, const std::string& path, const IlbmWriteOptions& opts,
                      std::string* error_out) {
    if (frame.width < 1 || frame.height < 1 || frame.width > limits::kMaxIffDimension ||
        frame.height > limits::kMaxIffDimension) {
        if (error_out)
            *error_out = "image size " + std::to_string(frame.width) + "x" + std::to_string(frame.height) +
                         " cannot be stored (IFF ILBM allows 1.." + std::to_string(limits::kMaxIffDimension) + ")";
        return false;
    }
    if (frame.palette.colors.size() != 16) {
        if (error_out) *error_out = "HAM6 requires an exact 16-color base palette";
        return false;
    }

    using namespace iff;

    std::vector<uint8_t> bmhd;
    put_u16(bmhd, static_cast<uint16_t>(frame.width));
    put_u16(bmhd, static_cast<uint16_t>(frame.height));
    put_i16(bmhd, 0);  // x origin
    put_i16(bmhd, 0);  // y origin
    put_u8(bmhd, kNumPlanes);
    put_u8(bmhd, 0);                      // masking: mNone
    put_u8(bmhd, opts.compress ? 1 : 0);  // compression: cmpByteRun1
    put_u8(bmhd, 0);                      // pad1
    put_u16(bmhd, 0);                     // transparentColor
    // xAspect/yAspect are a cosmetic hint for foreign viewers only - real
    // Amiga hardware doesn't read them - so an approximate lores value is
    // fine here; adjust if a specific target viewer cares.
    put_u8(bmhd, 10);
    put_u8(bmhd, 11);
    put_i16(bmhd, static_cast<int16_t>(frame.width));   // pageWidth
    put_i16(bmhd, static_cast<int16_t>(frame.height));  // pageHeight

    std::vector<uint8_t> cmap;
    cmap.reserve(48);
    for (const auto& c : frame.palette.colors) {
        auto rep = [](uint8_t v4) -> uint8_t { return static_cast<uint8_t>((v4 << 4) | v4); };
        cmap.push_back(rep(c.r4));
        cmap.push_back(rep(c.g4));
        cmap.push_back(rep(c.b4));
    }

    std::vector<uint8_t> camg;
    uint32_t camg_value = 0x00000800u;         // CAMG_HAM
    if (opts.lace) camg_value |= 0x00000004u;  // CAMG_LACE
    put_u32(camg, camg_value);

    std::vector<uint8_t> planar = chunky_to_planar(frame.pixel_codes, frame.width, frame.height, kNumPlanes);
    std::vector<uint8_t> body_data =
        opts.compress ? byterun1_compress_planar(planar, frame.width, frame.height, kNumPlanes) : planar;

    std::vector<uint8_t> form_data;
    put_id(form_data, "ILBM");
    put_bytes(form_data, make_chunk("BMHD", bmhd));
    put_bytes(form_data, make_chunk("CMAP", cmap));
    put_bytes(form_data, make_chunk("CAMG", camg));
    put_bytes(form_data, make_chunk("BODY", body_data));

    std::vector<uint8_t> file;
    put_id(file, "FORM");
    put_u32(file, static_cast<uint32_t>(form_data.size()));
    put_bytes(file, form_data);

    std::ofstream out(detail::utf8_path(path), std::ios::binary);
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
