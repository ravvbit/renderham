// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/resize.hpp
//
// Gets an arbitrary source photo onto the exact target canvas the rest of
// the pipeline expects (320x256 for HAM6 lores by default).

#include "aic/image.hpp"

namespace aic {

enum class FitMode {
    Stretch,  // ignore aspect ratio, fill the canvas exactly
    Fit,      // scale to fit inside the canvas, pad with pad_color (letterbox)
    Fill,     // scale to cover the canvas, crop the overhang (like CSS "cover")
};

struct ResizeOptions {
    int target_width = 320;
    int target_height = 256;
    FitMode mode = FitMode::Fit;
    RGBf pad_color{0.f, 0.f, 0.f};

    // Amiga display pixels are not necessarily square (this depends on the
    // exact video mode, overscan setting and monitor/scandoubler). This
    // factor is applied as (source_aspect * pixel_aspect_ratio) before
    // fitting, so it can be calibrated against real hardware instead of
    // this library silently assuming a specific ratio. 1.0 = treat as
    // square (no correction).
    float pixel_aspect_ratio = 1.0f;
};

// Where resize_image() puts a source of the given size: the source is first scaled to
// scaled_width x scaled_height, then that scaled picture's top-left corner lands at
// (offset_x, offset_y) on the canvas - positive for Fit (the padding), negative for Fill
// (the overhang that is cropped away), 0 for Stretch.
struct ResizeGeometry {
    int scaled_width = 0, scaled_height = 0;
    int offset_x = 0, offset_y = 0;
};
ResizeGeometry resize_geometry(int src_width, int src_height, const ResizeOptions& options);

// Produces a new image of exactly target_width x target_height.
Image resize_image(const Image& src, const ResizeOptions& options);

}  // namespace aic
