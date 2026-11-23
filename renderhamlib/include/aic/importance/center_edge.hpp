// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
#include "aic/importance.hpp"
#include "aic/importance/center.hpp"
#include "aic/importance/edge.hpp"

namespace aic {

// Multiplicative blend of CenterWeightedImportance and
// EdgeWeightedImportance: subject-centric AND fringing-aware at once.
class CenterEdgeImportance : public IImportanceMap {
public:
    std::vector<float> compute(const Image& image) const override;
    std::string name() const override { return "center-edge"; }

private:
    CenterWeightedImportance center_;
    EdgeWeightedImportance edge_;
};

}  // namespace aic
