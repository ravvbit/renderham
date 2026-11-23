// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
#include "aic/dither.hpp"

namespace aic {

// Ordered dithering using "interleaved gradient noise" (Jimenez, 2014)
// as the threshold source instead of a fixed Bayer matrix. IGN is a
// cheap analytic formula with much better (blue-noise-like) high-
// frequency statistical properties than a Bayer matrix - no visible
// repeating grid, no precomputed table needed. Same rationale as
// BayerDiffuser (no error propagation, so no HAM6 fringing from
// forced multi-channel jumps), just a less mechanical-looking pattern.
class BlueNoiseDiffuser : public IErrorDiffuser {
public:
    explicit BlueNoiseDiffuser(float strength = 1.0f) : strength_(strength) {}

    void begin(int width, int height) override;
    OKLab bias(int x, int y) const override;
    void feedback(int, int, const OKLab&) override {}  // no propagation, same as Bayer
    std::string name() const override { return "blue-noise"; }
    void set_strength(float strength) override { strength_ = strength; }  // pattern amplitude
    void set_pattern_origin(int ox, int oy) override { ox_ = ox; oy_ = oy; }

private:
    float strength_;
    int ox_ = 0, oy_ = 0;
};

}  // namespace aic
