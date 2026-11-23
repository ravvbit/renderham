// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/importance/center_edge.hpp"

#include <cstddef>

#include "aic/registry.hpp"

namespace aic {

std::vector<float> CenterEdgeImportance::compute(const Image& image) const {
    std::vector<float> c = center_.compute(image);
    std::vector<float> e = edge_.compute(image);
    std::vector<float> out(c.size());
    for (size_t i = 0; i < c.size(); ++i) out[i] = c[i] * e[i];
    return out;
}

static AutoRegister<IImportanceMap, CenterEdgeImportance> _reg("center-edge");

}  // namespace aic
