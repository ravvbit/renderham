// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/quantizers/octree_quantizer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>

#include "aic/registry.hpp"

namespace aic {
namespace {

constexpr int kMaxDepth = 8;  // one bit per level per channel, matching 8-bit sRGB

struct OctreeNode {
    std::unique_ptr<OctreeNode> children[8];
    bool is_leaf = false;
    double r = 0.0, g = 0.0, b = 0.0, weight = 0.0;
};

void InsertPixel(OctreeNode* root, int r8, int g8, int b8, float weight,
                  std::array<std::vector<OctreeNode*>, kMaxDepth + 1>& reducible, int& leaf_count) {
    OctreeNode* node = root;
    for (int level = 0; level < kMaxDepth; ++level) {
        int shift = kMaxDepth - 1 - level;
        int idx = (((r8 >> shift) & 1) << 2) | (((g8 >> shift) & 1) << 1) | ((b8 >> shift) & 1);
        if (!node->children[idx]) {
            node->children[idx] = std::make_unique<OctreeNode>();
            OctreeNode* child = node->children[idx].get();
            if (level == kMaxDepth - 1) {
                child->is_leaf = true;
                ++leaf_count;
            } else {
                reducible[static_cast<size_t>(level + 1)].push_back(child);
            }
        }
        node = node->children[idx].get();
    }
    node->r += r8 * weight;
    node->g += g8 * weight;
    node->b += b8 * weight;
    node->weight += weight;
}

// Merges the deepest reducible node's children into itself, turning it
// into a leaf. Always picking the DEEPEST available level guarantees
// that node's children are themselves already leaves by construction
// (any non-leaf child would sit at a deeper reducible level and would
// therefore have been processed first).
bool ReduceDeepest(std::array<std::vector<OctreeNode*>, kMaxDepth + 1>& reducible, int& leaf_count) {
    for (int level = kMaxDepth - 1; level >= 1; --level) {
        auto& bucket = reducible[static_cast<size_t>(level)];
        if (bucket.empty()) continue;
        OctreeNode* node = bucket.back();
        bucket.pop_back();

        int merged = 0;
        for (auto& child : node->children) {
            if (child) {
                node->r += child->r;
                node->g += child->g;
                node->b += child->b;
                node->weight += child->weight;
                ++merged;
                child.reset();
            }
        }
        node->is_leaf = true;
        leaf_count += 1 - merged;  // node itself becomes one leaf, its `merged` leaf children disappear
        return true;
    }
    return false;
}

void CollectLeaves(OctreeNode* node, std::vector<OctreeNode*>& leaves) {
    if (node->is_leaf) {
        leaves.push_back(node);
        return;
    }
    for (auto& c : node->children)
        if (c) CollectLeaves(c.get(), leaves);
}

}  // namespace

Palette OctreeQuantizer::quantize(const Image& image, int num_colors, const std::vector<float>& importance_weights,
                                   const PerceptualWeights& perceptual_weights) const {
    auto root = std::make_unique<OctreeNode>();
    std::array<std::vector<OctreeNode*>, kMaxDepth + 1> reducible;
    int leaf_count = 0;

    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const RGBf& c = image.at(x, y);
            int r8 = std::clamp(static_cast<int>(c.r * 255.f + 0.5f), 0, 255);
            int g8 = std::clamp(static_cast<int>(c.g * 255.f + 0.5f), 0, 255);
            int b8 = std::clamp(static_cast<int>(c.b * 255.f + 0.5f), 0, 255);
            float w = importance_weights[static_cast<size_t>(y) * image.width() + x];
            InsertPixel(root.get(), r8, g8, b8, w, reducible, leaf_count);
        }
    }

    while (leaf_count > num_colors) {
        if (!ReduceDeepest(reducible, leaf_count)) break;  // safety net; shouldn't trigger
    }

    std::vector<OctreeNode*> leaves;
    CollectLeaves(root.get(), leaves);

    Palette pal;
    for (auto* leaf : leaves) {
        if (leaf->weight <= 0.0) continue;
        RGBf avg{static_cast<float>(leaf->r / leaf->weight / 255.0), static_cast<float>(leaf->g / leaf->weight / 255.0),
                  static_cast<float>(leaf->b / leaf->weight / 255.0)};
        pal.colors.push_back(quantize_oklab_to_amiga(srgb_to_oklab(avg), perceptual_weights));
    }
    while (static_cast<int>(pal.colors.size()) < num_colors) {
        pal.colors.push_back(pal.colors.empty() ? AmigaColor{0, 0, 0} : pal.colors.back());
    }
    // Extremely unlikely (would need >16 exactly-representable leaves
    // surviving reduction due to a bug), but guard the HAM6 palette
    // size invariant anyway rather than let a malformed palette through.
    if (static_cast<int>(pal.colors.size()) > num_colors) pal.colors.resize(static_cast<size_t>(num_colors));

    return pal;
}

static AutoRegister<IPaletteQuantizer, OctreeQuantizer> _reg("octree");

}  // namespace aic
