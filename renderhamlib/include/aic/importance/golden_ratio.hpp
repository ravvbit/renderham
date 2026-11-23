// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
#include "aic/importance.hpp"

namespace aic {

// Golden-ratio ("Phi grid") composition rule: instead of rule-of-thirds'
// even 1/3 divisions, each axis is split at 1/phi (~61.8%) and 1/phi^2
// (~38.2%) - the four intersections are the classic "put your subject
// here" points for a balanced composition, and where the golden spiral's
// tightest curve sits. Weights peak (Gaussian falloff, same shape as
// CenterWeightedImportance) at these four points instead of a single
// center point. A floor keeps the rest of the image from being
// posterized away, same reasoning as the other importance maps.
class GoldenRatioImportance : public IImportanceMap {
public:
    explicit GoldenRatioImportance(float sigma = 0.5f, float floor = 0.15f) : sigma_(sigma), floor_(floor) {}

    std::vector<float> compute(const Image& image) const override;
    std::string name() const override { return "golden-ratio"; }

private:
    float sigma_;
    float floor_;
};

}  // namespace aic
