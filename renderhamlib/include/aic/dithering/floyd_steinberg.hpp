// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
#include <vector>

#include "aic/dither.hpp"

namespace aic {

// Classic Floyd-Steinberg error diffusion (7/16, 3/16, 5/16, 1/16).
// Note: on HAM6 this tends to produce more fringing than ditherers
// designed for HAM (see the CLI --help notes), because it deliberately
// creates high-frequency pixel-to-pixel variation - exactly what forces
// multi-channel jumps HAM can't do in a single pixel. Still a solid,
// well-understood default and a useful baseline for A/B comparisons.
class FloydSteinbergDiffuser : public IErrorDiffuser {
public:
    void begin(int width, int height) override;
    void begin_row(int y) override;
    OKLab bias(int x, int y) const override;
    void feedback(int x, int y, const OKLab& error) override;
    std::string name() const override { return "floyd-steinberg"; }
    void set_strength(float strength) override { strength_ = strength; }

private:
    float strength_ = 1.0f;  // scales the propagated error, see IErrorDiffuser::set_strength
    int width_ = 0;
    // 1px padding on each side so diagonal neighbor writes never need
    // bounds checks.
    std::vector<OKLab> curr_row_;
    std::vector<OKLab> next_row_;
};

}  // namespace aic
