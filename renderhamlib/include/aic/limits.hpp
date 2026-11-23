// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/limits.hpp - accepted value ranges for ConversionOptions.
//
// One place for the numbers, shared by the library's validate_options()
// and by the command line parser, so a rejected value always reports the
// same range everywhere. The ranges are deliberately wider than what the
// GUI sliders offer: they exist to stop nonsense (zero or negative sizes,
// NaN, a width that overflows the 16-bit IFF header), not to restrict.

namespace aic::limits {

// BMHD stores width/height as UWORD, so 65535 is the hard format limit.
// 8192 is far beyond any real Amiga mode (max. about 1440x580) and keeps
// run time and memory sane.
constexpr int kMinDimension = 1;
constexpr int kMaxDimension = 8192;
constexpr int kMaxIffDimension = 65535;

constexpr int kMaxLookahead = 8;  // 0 and 1 both mean greedy
constexpr int kMinBeam = 1;
constexpr int kMaxBeam = 32;
constexpr int kMaxPaletteIterations = 64;

constexpr float kMinPixelAspect = 0.1f;
constexpr float kMaxPixelAspect = 10.0f;
constexpr float kMinBrightness = -1.0f;
constexpr float kMaxBrightness = 1.0f;
constexpr float kMinContrast = 0.0f;
constexpr float kMaxContrast = 4.0f;
constexpr float kMinGamma = 0.1f;
constexpr float kMaxGamma = 5.0f;
constexpr float kMinSaturation = 0.0f;
constexpr float kMaxSaturation = 4.0f;
constexpr float kMinChromaWeight = 0.0f;
constexpr float kMaxChromaWeight = 4.0f;

}  // namespace aic::limits
