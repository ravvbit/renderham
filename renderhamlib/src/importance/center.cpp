// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/importance/center.hpp"

#include <cmath>
#include <cstddef>

#include "aic/registry.hpp"

namespace aic {

std::vector<float> CenterWeightedImportance::compute(const Image& image) const {
    const int w = image.width(), h = image.height();
    std::vector<float> weights(static_cast<size_t>(w) * h);

    const float cx = (w - 1) / 2.f, cy = (h - 1) / 2.f;
    const float max_dist = std::sqrt(cx * cx + cy * cy);  // center -> corner, so corners reach d≈1

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float dx = (x - cx) / max_dist, dy = (y - cy) / max_dist;
            float d2 = dx * dx + dy * dy;
            float gaussian = std::exp(-d2 / (2.f * sigma_ * sigma_));
            weights[static_cast<size_t>(y) * w + x] = floor_ + (1.f - floor_) * gaussian;
        }
    }
    return weights;
}

static AutoRegister<IImportanceMap, CenterWeightedImportance> _reg("center");

}  // namespace aic
