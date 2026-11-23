// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/importance/golden_ratio.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#include "aic/registry.hpp"

namespace aic {
namespace {
// 1/phi and 1/phi^2 - the golden section points (~0.618, ~0.382).
// These are the two fractions that satisfy minor/major = major/(major+minor),
// the defining self-similar property of the golden ratio.
constexpr float kGoldenMajor = 0.6180339887f;
constexpr float kGoldenMinor = 1.f - kGoldenMajor;  // ~0.381966
}  // namespace

std::vector<float> GoldenRatioImportance::compute(const Image& image) const {
    const int w = image.width(), h = image.height();
    std::vector<float> weights(static_cast<size_t>(w) * h);

    const float gx[2] = {(w - 1) * kGoldenMinor, (w - 1) * kGoldenMajor};
    const float gy[2] = {(h - 1) * kGoldenMinor, (h - 1) * kGoldenMajor};

    // Normalizes distance-to-nearest-point the same way
    // CenterWeightedImportance normalizes distance-to-center, just
    // scaled to the smaller golden segment (roughly one "cell" of the
    // golden grid) instead of the full center-to-corner reach, since
    // four points share the image here instead of one.
    const float scale_x = std::max(1.f, (w - 1) * kGoldenMinor);
    const float scale_y = std::max(1.f, (h - 1) * kGoldenMinor);
    const float max_dist = std::sqrt(scale_x * scale_x + scale_y * scale_y);

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float best_d2 = std::numeric_limits<float>::max();
            for (float px : gx) {
                for (float py : gy) {
                    float dx = (static_cast<float>(x) - px) / max_dist;
                    float dy = (static_cast<float>(y) - py) / max_dist;
                    best_d2 = std::min(best_d2, dx * dx + dy * dy);
                }
            }
            float gaussian = std::exp(-best_d2 / (2.f * sigma_ * sigma_));
            weights[static_cast<size_t>(y) * w + x] = floor_ + (1.f - floor_) * gaussian;
        }
    }
    return weights;
}

static AutoRegister<IImportanceMap, GoldenRatioImportance> _reg("golden-ratio");

}  // namespace aic
