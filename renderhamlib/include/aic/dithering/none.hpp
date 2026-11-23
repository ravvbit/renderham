// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
#include "aic/dither.hpp"

namespace aic {

// Baseline: no dithering at all. Useful for already-flat/clean source
// material (see the "RGB444 pre-rounding" tip) and as a quality baseline
// to compare other ditherers against.
class NoneDiffuser : public IErrorDiffuser {
public:
    void begin(int, int) override {}
    OKLab bias(int, int) const override { return OKLab{0.f, 0.f, 0.f}; }
    void feedback(int, int, const OKLab&) override {}
    std::string name() const override { return "none"; }
};

}  // namespace aic
