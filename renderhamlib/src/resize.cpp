// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/resize.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

namespace aic {
namespace {

// Resamples src to exactly (w,h). Resampling is done in *linear* light
// (not gamma-encoded sRGB) so that e.g. downscaling a bright/dark edge
// doesn't produce the classic "muddy" gamma-space resize artifacts.
Image resample(const Image& src, int w, int h) {
    std::vector<float> in(static_cast<size_t>(src.width()) * src.height() * 3);
    for (int y = 0; y < src.height(); ++y) {
        for (int x = 0; x < src.width(); ++x) {
            RGBf lin = srgb_to_linear(src.at(x, y));
            size_t idx = (static_cast<size_t>(y) * src.width() + x) * 3;
            in[idx + 0] = lin.r;
            in[idx + 1] = lin.g;
            in[idx + 2] = lin.b;
        }
    }

    std::vector<float> out(static_cast<size_t>(w) * h * 3);
    stbir_resize_float_linear(
        in.data(), src.width(), src.height(), 0,
        out.data(), w, h, 0,
        STBIR_RGB);

    Image result(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            size_t idx = (static_cast<size_t>(y) * w + x) * 3;
            RGBf lin{out[idx + 0], out[idx + 1], out[idx + 2]};
            result.at(x, y) = linear_to_srgb(lin);
        }
    }
    return result;
}

}  // namespace

ResizeGeometry resize_geometry(int src_width, int src_height, const ResizeOptions& opt) {
    ResizeGeometry g;
    if (opt.mode == FitMode::Stretch) {
        g.scaled_width = opt.target_width;
        g.scaled_height = opt.target_height;
        return g;
    }

    // Fit/Fill logic operates in a "physical" (square-pixel-equivalent)
    // space so that non-square Amiga display pixels can be compensated
    // for via pixel_aspect_ratio without duplicating the crop/pad code.
    const double par = (opt.pixel_aspect_ratio > 0.f) ? opt.pixel_aspect_ratio : 1.0;
    const double phys_target_w = opt.target_width * par;
    const double phys_target_h = opt.target_height;

    const double sx = phys_target_w / src_width;
    const double sy = phys_target_h / src_height;
    const double scale = (opt.mode == FitMode::Fit) ? std::min(sx, sy) : std::max(sx, sy);

    g.scaled_width = std::max(1, static_cast<int>(std::lround(src_width * scale / par)));
    g.scaled_height = std::max(1, static_cast<int>(std::lround(src_height * scale)));
    if (opt.mode == FitMode::Fit) {
        g.offset_x = (opt.target_width - g.scaled_width) / 2;
        g.offset_y = (opt.target_height - g.scaled_height) / 2;
    } else {  // Fill: center-crop the overhang away
        g.offset_x = -((g.scaled_width - opt.target_width) / 2);
        g.offset_y = -((g.scaled_height - opt.target_height) / 2);
    }
    return g;
}

Image resize_image(const Image& src, const ResizeOptions& opt) {
    if (src.empty() || opt.target_width <= 0 || opt.target_height <= 0) {
        return Image(std::max(opt.target_width, 1), std::max(opt.target_height, 1));
    }

    if (opt.mode == FitMode::Stretch) {
        return resample(src, opt.target_width, opt.target_height);
    }

    const ResizeGeometry g = resize_geometry(src.width(), src.height(), opt);
    const int scaled_w = g.scaled_width, scaled_h = g.scaled_height;

    Image scaled = resample(src, scaled_w, scaled_h);

    Image out(opt.target_width, opt.target_height);
    for (int y = 0; y < out.height(); ++y)
        for (int x = 0; x < out.width(); ++x) out.at(x, y) = opt.pad_color;

    if (opt.mode == FitMode::Fit) {
        const int off_x = g.offset_x;
        const int off_y = g.offset_y;
        for (int y = 0; y < scaled_h; ++y) {
            const int ty = y + off_y;
            if (ty < 0 || ty >= out.height()) continue;
            for (int x = 0; x < scaled_w; ++x) {
                const int tx = x + off_x;
                if (tx < 0 || tx >= out.width()) continue;
                out.at(tx, ty) = scaled.at(x, y);
            }
        }
    } else {  // Fill: center-crop the overhang away
        const int off_x = -g.offset_x;
        const int off_y = -g.offset_y;
        for (int y = 0; y < opt.target_height; ++y) {
            const int sy_ = std::clamp(y + off_y, 0, scaled_h - 1);
            for (int x = 0; x < opt.target_width; ++x) {
                const int sx_ = std::clamp(x + off_x, 0, scaled_w - 1);
                out.at(x, y) = scaled.at(sx_, sy_);
            }
        }
    }
    return out;
}

}  // namespace aic
