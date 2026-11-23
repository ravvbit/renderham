// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
#include "aic/importance.hpp"

namespace aic {

// Weights pixels by local OKLab gradient magnitude (simple central
// differences). HAM6 fringing happens where a color jump needs more
// than one modify-step to cross; giving edge pixels more say in palette
// placement means more `select`-mode anchors land near those jumps,
// letting a single direct selection replace a multi-step chain there.
// A floor keeps flat regions from being ignored entirely.
class EdgeWeightedImportance : public IImportanceMap {
public:
    explicit EdgeWeightedImportance(float floor = 0.2f) : floor_(floor) {}

    std::vector<float> compute(const Image& image) const override;
    std::string name() const override { return "edge"; }

private:
    float floor_;
};

}  // namespace aic
