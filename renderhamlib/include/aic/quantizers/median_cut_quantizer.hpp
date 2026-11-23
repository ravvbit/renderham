// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
#include "aic/palette.hpp"

namespace aic {

// Classic median-cut quantization, run in OKLab space so the resulting
// 16 base colors are perceptually well spread rather than RGB-cube
// spread. This becomes the HAM6 "select mode" palette - i.e. the anchor
// points the per-pixel modify deltas branch out from.
class MedianCutQuantizer : public IPaletteQuantizer {
public:
    Palette quantize(const Image& image, int num_colors, const std::vector<float>& importance_weights,
                      const PerceptualWeights& perceptual_weights) const override;
    std::string name() const override { return "median-cut"; }
};

}  // namespace aic
