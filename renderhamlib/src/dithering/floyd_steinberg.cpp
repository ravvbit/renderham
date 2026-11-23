// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/dithering/floyd_steinberg.hpp"

#include "aic/registry.hpp"

namespace aic {

void FloydSteinbergDiffuser::begin(int width, int) {
    width_ = width;
    curr_row_.assign(width + 2, OKLab{0.f, 0.f, 0.f});
    next_row_.assign(width + 2, OKLab{0.f, 0.f, 0.f});
}

void FloydSteinbergDiffuser::begin_row(int) {
    curr_row_.swap(next_row_);
    next_row_.assign(width_ + 2, OKLab{0.f, 0.f, 0.f});
}

OKLab FloydSteinbergDiffuser::bias(int x, int) const {
    return curr_row_[x + 1];
}

void FloydSteinbergDiffuser::feedback(int x, int, const OKLab& raw_err) {
    const OKLab err = raw_err * strength_;
    curr_row_[x + 2] = curr_row_[x + 2] + err * (7.f / 16.f);
    next_row_[x + 0] = next_row_[x + 0] + err * (3.f / 16.f);
    next_row_[x + 1] = next_row_[x + 1] + err * (5.f / 16.f);
    next_row_[x + 2] = next_row_[x + 2] + err * (1.f / 16.f);
}

static AutoRegister<IErrorDiffuser, FloydSteinbergDiffuser> _reg("floyd-steinberg");

}  // namespace aic
