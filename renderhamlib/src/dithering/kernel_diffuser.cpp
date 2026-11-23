// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/dithering/kernel_diffuser.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace aic {

KernelErrorDiffuser::KernelErrorDiffuser(std::vector<DiffusionTap> taps, std::string name)
    : taps_(std::move(taps)), name_(std::move(name)) {
    for (const auto& t : taps_) {
        reach_ = std::max(reach_, std::abs(t.dx));
        lookahead_ = std::max(lookahead_, t.dy);
    }
}

void KernelErrorDiffuser::begin(int width, int) {
    width_ = width;
    rows_.assign(static_cast<size_t>(lookahead_) + 1, std::vector<OKLab>(width_ + 2 * reach_, OKLab{0.f, 0.f, 0.f}));
}

void KernelErrorDiffuser::begin_row(int) {
    // Row 0 (fully consumed by now) is dropped; every later row shifts
    // down one slot; a fresh empty row appears at the far end.
    for (int i = 0; i < lookahead_; ++i) rows_[static_cast<size_t>(i)] = std::move(rows_[static_cast<size_t>(i + 1)]);
    rows_[static_cast<size_t>(lookahead_)].assign(width_ + 2 * reach_, OKLab{0.f, 0.f, 0.f});
}

OKLab KernelErrorDiffuser::bias(int x, int) const { return rows_[0][static_cast<size_t>(x + reach_)]; }

void KernelErrorDiffuser::feedback(int x, int, const OKLab& error) {
    for (const auto& t : taps_) {
        rows_[static_cast<size_t>(t.dy)][static_cast<size_t>(x + reach_ + t.dx)] =
            rows_[static_cast<size_t>(t.dy)][static_cast<size_t>(x + reach_ + t.dx)] + error * (t.weight * strength_);
    }
}

}  // namespace aic
