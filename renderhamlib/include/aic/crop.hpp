// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/crop.hpp
//
// Optional pre-scaling crop step: ImageMagick-style "WxH+X+Y" geometry.
// Each of the four components can independently be given as an
// absolute pixel count or as a percentage of the corresponding source
// dimension (trailing '%'), e.g. "320x200+10+5", "50%x50%+25%+25%", or
// mixed like "640x50%+0+10%". Always applied before resize_image (see
// pipeline.cpp): crop first, then fit/fill/stretch onto the target
// canvas.

#include <optional>
#include <string>

#include "aic/image.hpp"

namespace aic {

// One geometry component: either an absolute pixel value, or a
// percentage of the relevant source dimension.
struct CropValue {
    float value = 0.f;  // pixels, or a percent number (25 means 25%)
    bool is_percent = false;
};

struct CropSpec {
    CropValue width, height;  // box size
    CropValue x, y;           // box offset from the source's top-left corner
};

// Parses "WxH+X+Y". W/H must be positive; X/Y always carry an explicit
// sign, matching ImageMagick geometry syntax ("+10+5", "-10+0", ...) -
// here that sign simply shifts the box left/up/right/down from the
// top-left corner (NOT ImageMagick's "measured from the opposite edge"
// reading for negative offsets, which was judged more confusing than
// useful for this tool). Returns nullopt and fills error_out on
// malformed input.
std::optional<CropSpec> parse_crop_spec(const std::string& text, std::string* error_out = nullptr);

// A crop box in whole source pixels. May reach past any edge of the source.
struct CropBox {
    int x = 0, y = 0;
    int width = 0, height = 0;
};

// Resolves a CropSpec against a source of the given size - percentages become pixels,
// everything is rounded to whole pixels - exactly as crop_image() does. nullopt if the
// box has no area. (Exposed so that callers can know where a crop came from in the
// source, see ConversionOptions::anchor_dither_to_source.)
std::optional<CropBox> resolve_crop_box(int src_width, int src_height, const CropSpec& spec);

// Resolves percentages against src's actual dimensions. The box is NOT
// clamped to the source bounds - it may extend past any edge, or even
// fall entirely outside the source image (needed so an animation
// keyframe sequence can "wander into" or "wander out of" frame: a
// crop box starting or ending partially/fully outside the source).
// Any output pixel with no corresponding source pixel is filled with
// `fill_color` instead. Returns nullopt only if the resolved box has
// non-positive width/height.
std::optional<Image> crop_image(const Image& src, const CropSpec& spec, RGBf fill_color = {},
                                 std::string* error_out = nullptr);

}  // namespace aic
