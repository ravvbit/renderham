// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/dithering/kernel_diffuser.hpp
//
// Every error-diffusion dithering algorithm (Floyd-Steinberg, Atkinson,
// Sierra, Jarvis-Judice-Ninke, ...) is fully described by a small list
// of (dx, dy, weight) taps: after choosing a pixel's output color, its
// residual error gets scattered to each tap's relative position,
// scaled by that tap's weight. Rather than writing a near-duplicate
// row-buffer class per algorithm (see FloydSteinbergDiffuser, which
// predates this and is left as-is since it's already covered by
// tests), this base class handles the general case - arbitrary lookahead
// rows and arbitrary left/right reach - so a new kernel-based ditherer
// is just a tap list plus a name (see atkinson.hpp for the pattern).
//
// Weights don't have to sum to 1.0: Atkinson's sum to 0.75 on purpose
// (it discards a quarter of the error, which is exactly what gives it
// its higher-contrast, less "muddy" look compared to full diffusion).

#include <string>
#include <vector>

#include "aic/dither.hpp"

namespace aic {

struct DiffusionTap {
    int dx;        // horizontal offset from the current pixel
    int dy;        // vertical offset (0 = same row, must be paired with dx > 0)
    float weight;  // fraction of the residual error added at this tap
};

class KernelErrorDiffuser : public IErrorDiffuser {
public:
    KernelErrorDiffuser(std::vector<DiffusionTap> taps, std::string name);

    void begin(int width, int height) override;
    void begin_row(int y) override;
    OKLab bias(int x, int y) const override;
    void feedback(int x, int y, const OKLab& error) override;
    std::string name() const override { return name_; }
    void set_strength(float strength) override { strength_ = strength; }

private:
    std::vector<DiffusionTap> taps_;
    std::string name_;

    float strength_ = 1.0f;  // scales every tap's weight, see IErrorDiffuser::set_strength
    int width_ = 0;
    int reach_ = 1;     // padding needed on each side = max(|dx|) across taps
    int lookahead_ = 1;  // number of rows beyond the current one = max(dy) across taps

    // rows_[0] is the row currently being read via bias(); rows_[k] for
    // k>0 accumulates error for k rows ahead. Rotated in begin_row().
    std::vector<std::vector<OKLab>> rows_;
};

}  // namespace aic
