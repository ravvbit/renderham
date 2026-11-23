// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/ham_maps.hpp
//
// Two diagnostic images for judging and tuning a conversion (ham_convert
// offers the same pair; the legend below follows its documented one).
//
//   HAM map    one pixel per HAM6 pixel showing WHICH operation produced it:
//                black = Select (a palette color)   red   = Modify red
//                green = Modify green               blue  = Modify blue
//              Long runs of one color are cheap HAM; dense flicker between
//              the Modify colors marks the areas that will fringe.
//   Error map  grayscale; brightness = OKLab distance between the canvas the
//              encoder was given and what HAM6 actually reproduced, linear
//              from black (exact) to white (kErrorMapFullScale or worse). The
//              scale is FIXED, not normalized per image, so maps of different
//              settings - or different images - are directly comparable.

#include "aic/ham6_encoder.hpp"
#include "aic/image.hpp"

namespace aic {

// OKLab distance that maps to full white in the error map. Typical mean
// error is ~0.01-0.02, the worst 1% of pixels ~0.05-0.07.
constexpr float kErrorMapFullScale = 0.10f;

Image make_ham_map(const HAM6Frame& frame);

// Returns an empty Image if the two sizes differ (or either is empty).
Image make_error_map(const Image& reference, const Image& decoded);

}  // namespace aic
