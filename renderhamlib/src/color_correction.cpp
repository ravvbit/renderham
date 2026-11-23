// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/color_correction.hpp"

#include <algorithm>
#include <cmath>

namespace aic {
namespace {

inline float apply_brightness(float c, float brightness) { return std::clamp(c + brightness, 0.f, 1.f); }

inline float apply_contrast(float c, float contrast) { return std::clamp((c - 0.5f) * contrast + 0.5f, 0.f, 1.f); }

inline float apply_gamma(float c, float gamma) {
    if (gamma <= 0.f) return c;  // guard against a bad/zero slider value
    return std::clamp(std::pow(std::clamp(c, 0.f, 1.f), 1.f / gamma), 0.f, 1.f);
}

}  // namespace

void apply_color_correction(Image& image, const ColorCorrectionOptions& options) {
    if (options.is_identity()) return;

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            RGBf& px = image.at(x, y);

            px.r = apply_brightness(px.r, options.brightness);
            px.g = apply_brightness(px.g, options.brightness);
            px.b = apply_brightness(px.b, options.brightness);

            px.r = apply_contrast(px.r, options.contrast);
            px.g = apply_contrast(px.g, options.contrast);
            px.b = apply_contrast(px.b, options.contrast);

            px.r = apply_gamma(px.r, options.gamma);
            px.g = apply_gamma(px.g, options.gamma);
            px.b = apply_gamma(px.b, options.gamma);

            if (options.saturation != 1.0f) {
                // Rec. 709 luma weights - a fast, standard grayscale mix,
                // good enough for a UI saturation slider. Deliberately
                // not OKLab lightness: this needs to be cheap since it
                // runs per-pixel per-frame, and exact perceptual
                // uniformity isn't the point of a "saturation" knob.
                float gray = 0.2126f * px.r + 0.7152f * px.g + 0.0722f * px.b;
                px.r = std::clamp(gray + (px.r - gray) * options.saturation, 0.f, 1.f);
                px.g = std::clamp(gray + (px.g - gray) * options.saturation, 0.f, 1.f);
                px.b = std::clamp(gray + (px.b - gray) * options.saturation, 0.f, 1.f);
            }
        }
    }
}

}  // namespace aic
