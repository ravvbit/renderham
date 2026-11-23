// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/quality_metrics.hpp
//
// Quality figures for a conversion: how far is what HAM6 reproduced from the picture the
// encoder was given (the canvas). All of them are measured in OKLab.
//
//   mean_error     Mean OKLab distance per pixel. Every pixel counts on its own, so
//                  it punishes dithering: a dither pattern trades exactness of each
//                  pixel for a good average over the neighborhood, and this figure only
//                  sees the first half of that bargain.
//   blurred_error  Mean OKLab distance AFTER both pictures were blurred with a Gaussian
//                  of sigma 1.5 px - roughly what eye and CRT do with neighboring pixels
//                  at a normal viewing distance. This rewards a dither that gets the local
//                  average right and is the fairer figure to compare ditherers with.
//                  (scripts/ham_eval.py has measured this as "err_blur" from the start; same
//                  definition, so the numbers are comparable.)
//   ssim           Structural similarity (Wang et al. 2004) of the OKLab lightness: 1 means
//                  identical, lower means the local structure (edges, texture, contrast)
//                  is reproduced worse. Looks at what the first two do not: a smeared edge or
//                  a lost texture hurts here even if the average error is small.
//                  11x11 Gaussian window (sigma 1.5), C1 = 0.01^2, C2 = 0.03^2 for a
//                  lightness range of 1, mean over the whole picture, reflected borders.
//
// The picture borders are reflected for the blurs (not zero-padded), so a flat picture
// stays flat up to its edge.

#include "aic/image.hpp"

namespace aic {

// Sigma of the Gaussian that blurred_error and ssim use, in pixels.
constexpr float kPerceptualBlurSigma = 1.5f;

struct QualityMetrics {
    bool valid = false;  // false if the metrics were not computed (or the sizes differ)
    double mean_error = 0.0;
    double blurred_error = 0.0;
    double ssim = 0.0;
};

// reference and decoded must have the same, non-zero size; otherwise the result has valid == false.
QualityMetrics compute_quality_metrics(const Image& reference, const Image& decoded);

}  // namespace aic
