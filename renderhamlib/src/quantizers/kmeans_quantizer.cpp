// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/quantizers/kmeans_quantizer.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>

#include "aic/registry.hpp"

namespace aic {
namespace {
constexpr int kLloydIterations = 10;
}  // namespace

Palette KMeansQuantizer::quantize(const Image& image, int num_colors, const std::vector<float>& importance_weights,
                                   const PerceptualWeights& perceptual_weights) const {
    std::vector<OKLab> pts;
    pts.reserve(static_cast<size_t>(image.width()) * image.height());
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) pts.push_back(srgb_to_oklab(image.at(x, y)));
    const int n = static_cast<int>(pts.size());

    // Seed: weighted greedy farthest-point selection (deterministic
    // stand-in for k-means++'s random sampling). First center is the
    // point farthest from the weighted global mean; each subsequent
    // center is the point with the largest importance-weighted squared
    // distance to its nearest already-chosen center.
    std::vector<OKLab> centers;
    centers.reserve(static_cast<size_t>(num_colors));

    OKLab mean{0.f, 0.f, 0.f};
    float wsum = 0.f;
    for (int i = 0; i < n; ++i) {
        mean = mean + pts[static_cast<size_t>(i)] * importance_weights[static_cast<size_t>(i)];
        wsum += importance_weights[static_cast<size_t>(i)];
    }
    if (wsum > 0.f) mean = mean * (1.f / wsum);

    int first_idx = 0;
    float best_d = -1.f;
    for (int i = 0; i < n; ++i) {
        float d = distance2(pts[static_cast<size_t>(i)], mean);
        if (d > best_d) { best_d = d; first_idx = i; }
    }
    centers.push_back(pts[static_cast<size_t>(first_idx)]);

    std::vector<float> min_dist2(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) min_dist2[static_cast<size_t>(i)] = distance2(pts[static_cast<size_t>(i)], centers[0]);

    while (static_cast<int>(centers.size()) < num_colors && static_cast<int>(centers.size()) < n) {
        int best_idx = 0;
        float best_val = -1.f;
        for (int i = 0; i < n; ++i) {
            float v = min_dist2[static_cast<size_t>(i)] * importance_weights[static_cast<size_t>(i)];
            if (v > best_val) { best_val = v; best_idx = i; }
        }
        centers.push_back(pts[static_cast<size_t>(best_idx)]);
        for (int i = 0; i < n; ++i) {
            float nd = distance2(pts[static_cast<size_t>(i)], centers.back());
            if (nd < min_dist2[static_cast<size_t>(i)]) min_dist2[static_cast<size_t>(i)] = nd;
        }
    }

    // Lloyd's iteration: assign, re-average (importance-weighted), repeat.
    std::vector<int> assignment(static_cast<size_t>(n), 0);
    const int k = static_cast<int>(centers.size());
    for (int iter = 0; iter < kLloydIterations; ++iter) {
        for (int i = 0; i < n; ++i) {
            int best_c = 0;
            float best_d2 = std::numeric_limits<float>::max();
            for (int c = 0; c < k; ++c) {
                float d2 = distance2(pts[static_cast<size_t>(i)], centers[static_cast<size_t>(c)]);
                if (d2 < best_d2) { best_d2 = d2; best_c = c; }
            }
            assignment[static_cast<size_t>(i)] = best_c;
        }

        std::vector<OKLab> sum(static_cast<size_t>(k), OKLab{0.f, 0.f, 0.f});
        std::vector<float> wsum2(static_cast<size_t>(k), 0.f);
        for (int i = 0; i < n; ++i) {
            float w = importance_weights[static_cast<size_t>(i)];
            int c = assignment[static_cast<size_t>(i)];
            sum[static_cast<size_t>(c)] = sum[static_cast<size_t>(c)] + pts[static_cast<size_t>(i)] * w;
            wsum2[static_cast<size_t>(c)] += w;
        }
        for (int c = 0; c < k; ++c) {
            // Empty clusters are left at their previous position rather
            // than reseeded here - HAM6Encoder's own refinement pass
            // (which runs after this seed) already handles that case
            // more usefully, since it can reseed toward whatever the
            // real encoder's worst-served pixel turns out to be.
            if (wsum2[static_cast<size_t>(c)] > 0.f) {
                centers[static_cast<size_t>(c)] = sum[static_cast<size_t>(c)] * (1.f / wsum2[static_cast<size_t>(c)]);
            }
        }
    }

    Palette pal;
    for (auto& c : centers) pal.colors.push_back(quantize_oklab_to_amiga(c, perceptual_weights));
    while (static_cast<int>(pal.colors.size()) < num_colors) {
        pal.colors.push_back(pal.colors.empty() ? AmigaColor{0, 0, 0} : pal.colors.back());
    }
    return pal;
}

static AutoRegister<IPaletteQuantizer, KMeansQuantizer> _reg("kmeans");

}  // namespace aic
