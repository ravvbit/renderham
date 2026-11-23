// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/importance/edge.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "aic/registry.hpp"

namespace aic {

std::vector<float> EdgeWeightedImportance::compute(const Image& image) const {
    const int w = image.width(), h = image.height();
    std::vector<OKLab> lab(static_cast<size_t>(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) lab[static_cast<size_t>(y) * w + x] = srgb_to_oklab(image.at(x, y));

    std::vector<float> grad(static_cast<size_t>(w) * h, 0.f);
    float max_grad = 1e-6f;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int xm = std::max(x - 1, 0), xp = std::min(x + 1, w - 1);
            int ym = std::max(y - 1, 0), yp = std::min(y + 1, h - 1);
            OKLab gx = lab[static_cast<size_t>(y) * w + xp] - lab[static_cast<size_t>(y) * w + xm];
            OKLab gy = lab[static_cast<size_t>(yp) * w + x] - lab[static_cast<size_t>(ym) * w + x];
            float g = std::sqrt(gx.L * gx.L + gx.a * gx.a + gx.b * gx.b + gy.L * gy.L + gy.a * gy.a + gy.b * gy.b);
            grad[static_cast<size_t>(y) * w + x] = g;
            max_grad = std::max(max_grad, g);
        }
    }

    std::vector<float> weights(grad.size());
    for (size_t i = 0; i < grad.size(); ++i) weights[i] = floor_ + (1.f - floor_) * (grad[i] / max_grad);
    return weights;
}

static AutoRegister<IImportanceMap, EdgeWeightedImportance> _reg("edge");

}  // namespace aic
