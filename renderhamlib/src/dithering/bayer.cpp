// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/dithering/bayer.hpp"

#include "aic/registry.hpp"

namespace aic {
namespace {

// Standard 8x8 Bayer / ordered-dither threshold matrix (values 0..63).
constexpr int kBayer8x8[8][8] = {
    {0, 32, 8, 40, 2, 34, 10, 42},
    {48, 16, 56, 24, 50, 18, 58, 26},
    {12, 44, 4, 36, 14, 46, 6, 38},
    {60, 28, 52, 20, 62, 30, 54, 22},
    {3, 35, 11, 43, 1, 33, 9, 41},
    {51, 19, 59, 27, 49, 17, 57, 25},
    {15, 47, 7, 39, 13, 45, 5, 37},
    {63, 31, 55, 23, 61, 29, 53, 21},
};

// Empirically-scaled OKLab amplitudes for one full ordered-dither "step".
// Lightness dominates perceived banding, so it gets the largest share;
// a/b get a smaller nudge to also break up hue/chroma banding a little.
constexpr float kAmpL = 0.045f;
constexpr float kAmpAB = 0.012f;

}  // namespace

void BayerDiffuser::begin(int, int) {}

OKLab BayerDiffuser::bias(int x, int y) const {
    float t = (kBayer8x8[(y + oy_) & 7][(x + ox_) & 7] + 0.5f) / 64.f - 0.5f;  // in [-0.5, 0.5)
    return OKLab{t * kAmpL * strength_, t * kAmpAB * strength_, t * kAmpAB * strength_};
}

static AutoRegister<IErrorDiffuser, BayerDiffuser> _reg("bayer");

}  // namespace aic
