// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
#include "aic/palette.hpp"

namespace aic {

// Classic K-Means (Lloyd's algorithm) clustering in OKLab space, as a
// structurally different alternative seed strategy to MedianCutQuantizer.
// Where median-cut recursively splits boxes by extent, this iteratively
// reassigns every pixel to its nearest of K centers and re-averages -
// sometimes finds a better clustering for color distributions median-cut
// handles awkwardly, sometimes not; having both lets the user compare.
//
// Deterministic (no RNG): centers are seeded via greedy farthest-point
// selection (importance-weighted) rather than random restarts, so the
// same image always produces the same palette.
class KMeansQuantizer : public IPaletteQuantizer {
public:
    Palette quantize(const Image& image, int num_colors, const std::vector<float>& importance_weights,
                      const PerceptualWeights& perceptual_weights) const override;
    std::string name() const override { return "kmeans"; }
};

}  // namespace aic
