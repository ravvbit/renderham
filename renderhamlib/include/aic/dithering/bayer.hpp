// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
#include "aic/dither.hpp"

namespace aic {

// 8x8 ordered dithering. Produces a static, repeating threshold pattern
// rather than diffusing error to neighbors - on HAM6 this avoids the
// "fringing" that error-diffusion methods provoke on hard vertical edges
// (see BayerDiffuser::bias), at the cost of a visible dot pattern.
class BayerDiffuser : public IErrorDiffuser {
public:
    explicit BayerDiffuser(float strength = 1.0f) : strength_(strength) {}

    void begin(int width, int height) override;
    OKLab bias(int x, int y) const override;
    void feedback(int, int, const OKLab&) override {}  // no propagation
    std::string name() const override { return "bayer"; }
    void set_strength(float strength) override { strength_ = strength; }  // pattern amplitude
    void set_pattern_origin(int ox, int oy) override { ox_ = ox; oy_ = oy; }

private:
    float strength_;
    int ox_ = 0, oy_ = 0;
};

}  // namespace aic
