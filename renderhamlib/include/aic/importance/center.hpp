// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
#include "aic/importance.hpp"

namespace aic {

// Gaussian-ish falloff from the image center - the "subject is usually
// centered" heuristic. A floor keeps corners/edges from being weighted
// to (near-)zero, since a palette that ONLY serves the center would
// posterize the periphery badly.
class CenterWeightedImportance : public IImportanceMap {
public:
    explicit CenterWeightedImportance(float sigma = 0.5f, float floor = 0.15f) : sigma_(sigma), floor_(floor) {}

    std::vector<float> compute(const Image& image) const override;
    std::string name() const override { return "center"; }

private:
    float sigma_;
    float floor_;
};

}  // namespace aic
