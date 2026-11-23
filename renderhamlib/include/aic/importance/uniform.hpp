// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
#include <cstddef>

#include "aic/importance.hpp"

namespace aic {

// weight=1 everywhere. This is the neutral default: any weighted
// computation (weighted median-cut split, weighted refinement mean)
// degenerates exactly to its unweighted form with this map, so opting
// into importance weighting never changes behavior unless requested.
class UniformImportance : public IImportanceMap {
public:
    std::vector<float> compute(const Image& image) const override {
        return std::vector<float>(static_cast<size_t>(image.width()) * image.height(), 1.0f);
    }
    std::string name() const override { return "uniform"; }
};

}  // namespace aic
