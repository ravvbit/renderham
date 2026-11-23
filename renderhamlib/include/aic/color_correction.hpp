// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/color_correction.hpp
//
// Optional brightness/contrast/gamma/saturation adjustment, applied to
// the resized canvas right before quantization/dithering - i.e. it
// only ever affects the HAM6-encoded output (and any preview decoded
// from it), never a caller's own unmodified copy of the source image.

#include "aic/image.hpp"

namespace aic {

struct ColorCorrectionOptions {
    // Additive, applied directly to the sRGB-encoded pixel values (the
    // same convention a typical photo tool's brightness slider uses -
    // not physically linear light). 0 = no change.
    float brightness = 0.0f;

    // Multiplicative around the 0.5 midpoint. 1.0 = no change; <1
    // flattens toward gray, >1 pushes toward the extremes.
    float contrast = 1.0f;

    // Power curve on the sRGB-encoded values: output = input^(1/gamma).
    // 1.0 = no change; >1 brightens midtones, <1 darkens them. Applied
    // to the encoded/display-referred values, matching how a "gamma"
    // slider behaves in common photo software - this is not a
    // linear-light radiometric gamma correction.
    float gamma = 1.0f;

    // Blends between grayscale (Rec. 709 luma) and the original color.
    // 1.0 = no change; 0 = grayscale; >1 boosts saturation.
    float saturation = 1.0f;

    bool is_identity() const {
        return brightness == 0.0f && contrast == 1.0f && gamma == 1.0f && saturation == 1.0f;
    }
};

// Applies brightness, then contrast, then gamma, then saturation, in
// place, each clamped to [0,1] before the next step runs. A no-op
// (returns immediately, no allocation) when options.is_identity().
void apply_color_correction(Image& image, const ColorCorrectionOptions& options);

}  // namespace aic
