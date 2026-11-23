// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/dithering/blue_noise.hpp"

#include <cmath>

#include "aic/registry.hpp"

namespace aic {
namespace {

// Interleaved Gradient Noise (Jorge Jimenez, "Next Generation Post
// Processing in Call of Duty: Advanced Warfare", 2014) - a fast
// analytic approximation of blue noise, widely used in real-time
// rendering for exactly this reason.
float InterleavedGradientNoise(int x, int y) {
    float v = 52.9829189f * std::fmod(0.06711056f * static_cast<float>(x) + 0.00583715f * static_cast<float>(y), 1.0f);
    return v - std::floor(v);  // fractional part, in [0,1)
}

// Same empirically-scaled OKLab amplitudes as BayerDiffuser, so the two
// ordered ditherers are directly comparable at the same --dither-strength
// setting (not currently CLI-exposed, but keeps the constants meaningful
// relative to each other for anyone reading the source).
constexpr float kAmpL = 0.045f;
constexpr float kAmpAB = 0.012f;

}  // namespace

void BlueNoiseDiffuser::begin(int, int) {}

OKLab BlueNoiseDiffuser::bias(int x, int y) const {
    float t = InterleavedGradientNoise(x + ox_, y + oy_) - 0.5f;  // in [-0.5, 0.5)
    return OKLab{t * kAmpL * strength_, t * kAmpAB * strength_, t * kAmpAB * strength_};
}

static AutoRegister<IErrorDiffuser, BlueNoiseDiffuser> _reg("blue-noise");

}  // namespace aic
