// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/quantizers/median_cut_quantizer.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>

#include "aic/registry.hpp"

namespace aic {
namespace {

struct Box {
    std::vector<int> indices;
};

float axis_value(const OKLab& c, int axis) {
    return axis == 0 ? c.L : (axis == 1 ? c.a : c.b);
}

// Unweighted value range - decides which axis to split on. Using the
// raw range (not weight-scaled) is standard median-cut practice and
// keeps box shapes sensible even under heavily skewed weights.
float extent(const std::vector<OKLab>& pts, const std::vector<int>& idx, int axis) {
    float mn = 1e9f, mx = -1e9f;
    for (int i : idx) {
        float v = axis_value(pts[i], axis);
        mn = std::min(mn, v);
        mx = std::max(mx, v);
    }
    return mx - mn;
}

}  // namespace

Palette MedianCutQuantizer::quantize(const Image& image, int num_colors, const std::vector<float>& importance_weights,
                                    const PerceptualWeights& perceptual_weights) const {
    std::vector<OKLab> pts;
    pts.reserve(static_cast<size_t>(image.width()) * image.height());
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x) pts.push_back(srgb_to_oklab(image.at(x, y)));

    std::vector<Box> boxes(1);
    boxes[0].indices.resize(pts.size());
    std::iota(boxes[0].indices.begin(), boxes[0].indices.end(), 0);

    while (static_cast<int>(boxes.size()) < num_colors) {
        int best_box = -1, best_axis = 0;
        float best_extent = -1.f;
        for (int b = 0; b < static_cast<int>(boxes.size()); ++b) {
            if (boxes[b].indices.size() < 2) continue;
            for (int axis = 0; axis < 3; ++axis) {
                float e = extent(pts, boxes[b].indices, axis);
                if (e > best_extent) {
                    best_extent = e;
                    best_box = b;
                    best_axis = axis;
                }
            }
        }
        if (best_box < 0) break;  // nothing left worth splitting

        auto& idx = boxes[best_box].indices;
        std::sort(idx.begin(), idx.end(), [&](int a, int b) {
            return axis_value(pts[a], best_axis) < axis_value(pts[b], best_axis);
        });

        // Weighted median split: walk the sorted box until cumulative
        // weight crosses half the box's total weight, rather than
        // simply splitting at idx.size()/2. Denser-weighted regions
        // (e.g. the image center under CenterWeightedImportance) end up
        // getting finer subdivisions, exactly like they would if there
        // were literally more pixels there.
        float total_w = 0.f;
        for (int i : idx) total_w += importance_weights[static_cast<size_t>(i)];

        size_t split = idx.size() / 2;  // fallback for the degenerate all-zero-weight case
        if (total_w > 0.f) {
            float half = total_w / 2.f, cum = 0.f;
            for (size_t k = 0; k < idx.size(); ++k) {
                cum += importance_weights[static_cast<size_t>(idx[k])];
                if (cum >= half) {
                    split = k + 1;
                    break;
                }
            }
        }
        split = std::clamp(split, size_t{1}, idx.size() - 1);  // both sides must get >=1 point

        Box next_box;
        next_box.indices.assign(idx.begin() + static_cast<std::ptrdiff_t>(split), idx.end());
        idx.resize(split);
        boxes.push_back(std::move(next_box));
    }

    Palette pal;
    for (auto& box : boxes) {
        OKLab sum{0.f, 0.f, 0.f};
        float wsum = 0.f;
        for (int i : box.indices) {
            float w = importance_weights[static_cast<size_t>(i)];
            sum = sum + pts[i] * w;
            wsum += w;
        }
        OKLab mean = (wsum > 0.f) ? sum * (1.f / wsum) : OKLab{0.f, 0.f, 0.f};
        pal.colors.push_back(quantize_oklab_to_amiga(mean, perceptual_weights));
    }
    while (static_cast<int>(pal.colors.size()) < num_colors) {
        pal.colors.push_back(pal.colors.empty() ? AmigaColor{0, 0, 0} : pal.colors.back());
    }
    return pal;
}

static AutoRegister<IPaletteQuantizer, MedianCutQuantizer> _reg("median-cut");

}  // namespace aic
