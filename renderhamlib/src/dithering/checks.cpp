// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/dithering/checks.hpp"

#include <algorithm>

#include "aic/registry.hpp"

namespace aic {
namespace {

// The two phases shift the rounding threshold by a quarter of a 4-bit
// step in either direction.
constexpr float kPhaseOffset = 0.25f;

// How far "strength 100%" moves the target from the continuous color toward
// the phase-rounded lattice color. Calibrated empirically (see the README
// measurement series, four photos): err_blur is lowest around 0.5 and flat
// between 0.4 and 0.6, while outliers (err_p99) and per-pixel error keep
// rising above that - fully snapping to the lattice (1.0) is worse on every
// metric because it takes the encoder's own nearest-reachable choice away.
// Same idea as the empirically scaled amplitudes of the Bayer ditherer.
constexpr float kRecommendedBlend = 0.5f;

inline float clamp01(float v) { return std::min(1.f, std::max(0.f, v)); }

// Round one sRGB channel to the 4-bit lattice with the threshold shifted by
// `phase` (+-kPhaseOffset levels), returned as a normalized sRGB value.
inline float round_to_lattice(float srgb, float phase) {
    float level = std::floor(clamp01(srgb) * 15.f + 0.5f + phase);
    return std::min(15.f, std::max(0.f, level)) / 15.f;
}

}  // namespace

OKLab ChecksDiffuser::adjust_target(int x, int y, const OKLab& target) const {
    if (strength_ <= 0.f) return target;  // strength 0 == no dithering at all, bit-identical to "none"

    // Pattern phase: the line parity and the checkerboard parity follow the pattern origin.
    // (x + ox_) & 1 rather than a % 2, so that negative origins behave.
    x += ox_;
    y += oy_;
    float sr, sg, sb;
    switch (pattern_) {
        case Pattern::Lines: {
            float s = (y & 1) ? 1.f : -1.f;
            sr = sg = sb = s;
            break;
        }
        case Pattern::LinesMixed: {
            float s = (y & 1) ? 1.f : -1.f;
            sr = sb = s;
            sg = -s;
            break;
        }
        default: {  // Checker
            float s = ((x + y) & 1) ? 1.f : -1.f;
            sr = sg = sb = s;
            break;
        }
    }

    // The target handed to the encoder is the lattice color this phase
    // rounds to - NOT the continuous color plus an offset. The rounding has
    // to happen in 4-bit code space, where the lattice is uniform: the
    // encoder picks candidates by OKLab distance, which is extremely steep
    // near black, so a code-space offset added to the continuous target
    // (tried first) pushed pure black over the nearest-candidate boundary
    // and turned every other line of black areas dark gray. Rounding here
    // leaves exact lattice values (black, white, every 17-multiple)
    // untouched in both phases.
    //
    // strength blends continuous target (0) -> the recommended mix of
    // continuous and dithered lattice target (1), so the slider stays a
    // continuous "how much" control.
    const float amount = strength_ * kRecommendedBlend;
    RGBf c = oklab_to_srgb(target);
    auto mix = [&](float v, float sign) {
        float lattice = round_to_lattice(v, sign * kPhaseOffset);
        return clamp01(v) + amount * (lattice - clamp01(v));
    };
    return srgb_to_oklab(RGBf{mix(c.r, sr), mix(c.g, sg), mix(c.b, sb)});
}

static AutoRegister<IErrorDiffuser, ChecksLinesDiffuser> _reg_lines("checks-lines");
static AutoRegister<IErrorDiffuser, ChecksLinesMixedDiffuser> _reg_mixed("checks-lines-mixed");
static AutoRegister<IErrorDiffuser, ChecksCheckerDiffuser> _reg_checks("checks");

}  // namespace aic
