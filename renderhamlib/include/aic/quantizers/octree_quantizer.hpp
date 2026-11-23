// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
#include "aic/palette.hpp"

namespace aic {

// Classic octree color quantization (Gervautz & Purgathofer, 1988).
// Structurally very different from median-cut/k-means: builds an 8-level
// tree from the raw 8-bit sRGB bytes (branching on one bit of R, G, B
// per level - this is inherently an RGB-space algorithm, unlike the
// rest of this codebase's OKLab-based distance work, since the octree's
// bucketing relies on the binary structure of 8-bit channel values) and
// merges the deepest, most-similar leaves until only num_colors remain.
// Tends to preserve locally-frequent exact/near-exact colors well (flat
// graphics, screenshots, dithered source material); the final leaf ->
// AmigaColor step still goes through the same OKLab nearest-search as
// every other quantizer, so only the clustering phase is RGB-native.
class OctreeQuantizer : public IPaletteQuantizer {
public:
    Palette quantize(const Image& image, int num_colors, const std::vector<float>& importance_weights,
                      const PerceptualWeights& perceptual_weights) const override;
    std::string name() const override { return "octree"; }
};

}  // namespace aic
