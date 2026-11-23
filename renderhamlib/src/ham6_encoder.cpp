// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/ham6_encoder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>

namespace aic {
namespace {

// AmigaColor <-> OKLab LUT and index helper now live in palette.hpp
// (amiga_lab_lut() / amiga_lut_index()), shared with
// quantize_oklab_to_amiga() so palette fitting and per-pixel candidate
// search always agree on the same color data.

inline uint8_t make_code(int mode, int data) { return static_cast<uint8_t>((mode << 4) | data); }

AmigaColor apply_code(uint8_t code, const AmigaColor& held, const Palette& pal) {
    int mode = (code >> 4) & 3;
    int data = code & 0xF;
    switch (mode) {
        case 0: return pal.colors[data];
        case 1: return AmigaColor{held.r4, held.g4, static_cast<uint8_t>(data)};
        case 2: return AmigaColor{static_cast<uint8_t>(data), held.g4, held.b4};
        default: return AmigaColor{held.r4, static_cast<uint8_t>(data), held.b4};
    }
}

// Estimated cost of one pass relative to a greedy pass (= 1), only used to
// spread the progress report evenly over the passes: it decides how the bar
// moves, never what the encoder does. Fitted to single-pass timings of a
// 640x512 picture (greedy about 0.06 s): depth 2/3/4/5 at beam 8 took 20/42/65/91
// greedy passes, depth 3 at beam 2/4/8/16 took 17/23/41/70. The fit
// (depth-1)/2 * (10 + 3.7*beam) lands within 10 percent of all of them.
float pass_cost(int lookahead_depth, int beam) {
    if (lookahead_depth <= 1) return 1.f;
    return 0.5f * static_cast<float>(lookahead_depth - 1) * (10.f + 3.7f * static_cast<float>(std::clamp(beam, 1, 32)));
}

}  // namespace

HAM6Encoder::HAM6Encoder(const IPaletteQuantizer& quantizer, IErrorDiffuser& diffuser, HAM6EncoderOptions options)
    : quantizer_(quantizer), diffuser_(diffuser), options_(options) {}

namespace {

constexpr int kMaxBeam = 32;

struct BeamState {
    int idx;          // held color as 12-bit lattice index
    uint8_t first;    // the operation chosen for the FIRST pixel of this sequence
    float cost;
};

struct Candidate {
    float cost;
    int idx;
    uint16_t seq;     // generation order: makes every tie-break deterministic
    uint8_t first;
};

inline bool cand_less(const Candidate& a, const Candidate& b) {
    return a.cost < b.cost || (a.cost == b.cost && a.seq < b.seq);
}

}  // namespace

uint8_t HAM6Encoder::choose_lookahead(const OKLab& biased0, const std::vector<OKLab>& row_targets, int x, int y,
                                      int width, const AmigaColor& held, const std::vector<int>& pal_idx,
                                      int depth) const {
    const auto& lut = amiga_lab_lut();
    const PerceptualWeights& pw = options_.weights;
    const int beam_width = std::min(kMaxBeam, std::max(1, options_.lookahead_beam));
    const int n = std::min(depth, width - x);

    // Scratch space, reused across calls (this runs once per pixel, so
    // allocating per call would dominate). thread_local keeps the encoder
    // usable from several threads at once later.
    thread_local std::array<Candidate, kMaxBeam * 64> cand;
    thread_local std::array<int, kMaxBeam * 64> uniq;
    thread_local std::array<uint32_t, 4096> stamp{};
    thread_local std::array<int, 4096> slot{};
    thread_local uint32_t cur = 0;

    BeamState beam[kMaxBeam];
    int nbeam = 1;
    beam[0] = BeamState{amiga_lut_index(held), 0, 0.f};

    for (int d = 0; d < n; ++d) {
        const OKLab t = (d == 0) ? biased0 : diffuser_.adjust_target(x + d, y, row_targets[static_cast<size_t>(x + d)]);
        const float wd = (d == 0) ? 1.f : options_.lookahead_future_weight;
        const bool allow_modify = (x + d > 0) || options_.line_start_modify;

        float set_cost[16];
        for (int i = 0; i < 16; ++i) set_cost[i] = wd * weighted_distance2(t, lut[pal_idx[i]], pw);

        int nc = 0;
        uint16_t seq = 0;
        for (int s = 0; s < nbeam; ++s) {
            const BeamState& st = beam[s];
            // Same generation order as the greedy search in run_pass: the 16
            // Selects first, then Modify blue/red/green per value - so with
            // depth 1 both pick exactly the same operation, ties included.
            for (int i = 0; i < 16; ++i) {
                cand[nc++] = Candidate{st.cost + set_cost[i], pal_idx[i], seq++, d == 0 ? make_code(0, i) : st.first};
            }
            if (allow_modify) {
                const int base = st.idx;
                for (int v = 0; v < 16; ++v) {
                    int ib = (base & 0xFF0) | v;
                    cand[nc++] = Candidate{st.cost + wd * weighted_distance2(t, lut[ib], pw), ib, seq++,
                                           d == 0 ? make_code(1, v) : st.first};
                    int ir = (v << 8) | (base & 0x0FF);
                    cand[nc++] = Candidate{st.cost + wd * weighted_distance2(t, lut[ir], pw), ir, seq++,
                                           d == 0 ? make_code(2, v) : st.first};
                    int ig = (base & 0xF00) | (v << 4) | (base & 0x00F);
                    cand[nc++] = Candidate{st.cost + wd * weighted_distance2(t, lut[ig], pw), ig, seq++,
                                           d == 0 ? make_code(3, v) : st.first};
                }
            }
        }

        // The future depends ONLY on the held color, so of all sequences that
        // end in the same color just the cheapest one needs to survive.
        if (++cur == 0) { stamp.fill(0); cur = 1; }
        int nu = 0;
        for (int c = 0; c < nc; ++c) {
            const int idx = cand[c].idx;
            if (stamp[idx] != cur) {
                stamp[idx] = cur;
                slot[idx] = nu;
                uniq[nu++] = c;
            } else if (cand_less(cand[c], cand[uniq[slot[idx]]])) {
                uniq[slot[idx]] = c;
            }
        }

        if (d == n - 1) {  // last level: the best complete sequence wins
            int best = uniq[0];
            for (int u = 1; u < nu; ++u) {
                if (cand_less(cand[uniq[u]], cand[best])) best = uniq[u];
            }
            return cand[best].first;
        }

        if (nu > beam_width) {
            std::nth_element(uniq.begin(), uniq.begin() + beam_width, uniq.begin() + nu,
                             [&](int a, int b) { return cand_less(cand[a], cand[b]); });
            nu = beam_width;
        }
        for (int k = 0; k < nu; ++k) {
            const Candidate& c = cand[uniq[k]];
            beam[k] = BeamState{c.idx, c.first, c.cost};
        }
        nbeam = nu;
    }
    return 0;  // unreachable: n >= 1
}

HAM6Encoder::PassResult HAM6Encoder::run_pass(const Image& image, const Palette& palette,
                                               const std::vector<float>& weights, int lookahead_depth,
                                               float progress_begin, float progress_end) const {
    const int width = image.width(), height = image.height();
    PassResult result;
    result.pixel_codes.assign(static_cast<size_t>(width) * height, 0);
    result.cluster_sum.assign(16, OKLab{0.f, 0.f, 0.f});
    result.cluster_weight.assign(16, 0.f);

    const auto& lut = amiga_lab_lut();
    const PerceptualWeights& pw = options_.weights;
    std::vector<int> pal_idx(16);
    for (int i = 0; i < 16; ++i) pal_idx[i] = amiga_lut_index(palette.colors[i]);

    diffuser_.begin(width, height);

    const bool use_lookahead = lookahead_depth > 1 || options_.lookahead_always_beam;
    std::vector<OKLab> row_targets;
    if (use_lookahead) row_targets.resize(static_cast<size_t>(width));

    for (int y = 0; y < height; ++y) {
        if (options_.progress &&
            !options_.progress(progress_begin + (progress_end - progress_begin) * static_cast<float>(y) / height)) {
            throw OperationCancelled();
        }
        diffuser_.begin_row(y);
        if (use_lookahead) {
            for (int x = 0; x < width; ++x) row_targets[static_cast<size_t>(x)] = srgb_to_oklab(image.at(x, y));
        }
        // Line start: the border color (register 0), see line_start_modify.
        // Only observable if the first pixel may be a Modify.
        AmigaColor held = palette.colors[0];

        for (int x = 0; x < width; ++x) {
            const size_t p = static_cast<size_t>(y) * width + x;
            OKLab target = use_lookahead ? row_targets[static_cast<size_t>(x)] : srgb_to_oklab(image.at(x, y));
            OKLab biased = diffuser_.adjust_target(x, y, target);

            uint8_t best_code = 0;
            float best_dist = std::numeric_limits<float>::max();

            if (use_lookahead) {
                best_code = choose_lookahead(biased, row_targets, x, y, width, held, pal_idx, lookahead_depth);
            } else {

            // Mode 00: select one of the 16 base palette colors. Always
            // available, and - unless options_.line_start_modify - the ONLY
            // option for the leftmost pixel of a scanline (no "held"
            // predecessor to modify; with the option, the predecessor is
            // the border color, palette register 0).
            for (int i = 0; i < 16; ++i) {
                float d = weighted_distance2(biased, lut[pal_idx[i]], pw);
                if (d < best_dist) {
                    best_dist = d;
                    best_code = make_code(0, i);
                }
            }

            // Modes 01/10/11: hold two channels of the previous pixel,
            // brute-force all 16 values of the third.
            if (x > 0 || options_.line_start_modify) {
                int base = amiga_lut_index(held);
                for (int v = 0; v < 16; ++v) {
                    int idx_blue = (base & 0xFF0) | v;
                    float db = weighted_distance2(biased, lut[idx_blue], pw);
                    if (db < best_dist) { best_dist = db; best_code = make_code(1, v); }

                    int idx_red = (v << 8) | (base & 0x0FF);
                    float dr = weighted_distance2(biased, lut[idx_red], pw);
                    if (dr < best_dist) { best_dist = dr; best_code = make_code(2, v); }

                    int idx_green = (base & 0xF00) | (v << 4) | (base & 0x00F);
                    float dg = weighted_distance2(biased, lut[idx_green], pw);
                    if (dg < best_dist) { best_dist = dg; best_code = make_code(3, v); }
                }
            }
            }  // greedy search

            AmigaColor actual = apply_code(best_code, held, palette);
            result.pixel_codes[p] = best_code;
            diffuser_.feedback(x, y, biased - lut[amiga_lut_index(actual)]);

            float w = weights[p];
            if (((best_code >> 4) & 3) == 0) {
                int idx = best_code & 0xF;
                // Accumulate the UNBIASED target (not dither-adjusted) so
                // refinement fits the palette to the true source colors
                // that chose it, not to transient dither noise.
                result.cluster_sum[idx] = result.cluster_sum[idx] + target * w;
                result.cluster_weight[idx] += w;
            }

            // Worst-pixel tracking also uses the configured perceptual
            // weighting, so empty-cluster reseeding (below) chases the
            // same notion of "bad" the candidate search above optimizes
            // for, not a different unweighted one.
            float src_dist2 = weighted_distance2(target, lut[amiga_lut_index(actual)], pw);
            float weighted_dist2 = src_dist2 * w;
            if (weighted_dist2 > result.worst_pixel_dist2) {
                result.worst_pixel_dist2 = weighted_dist2;
                result.worst_pixel_lab = target;
            }

            held = actual;
        }
    }
    return result;
}

Palette HAM6Encoder::refit_palette(const Palette& current, const PassResult& stats) const {
    Palette next = current;
    for (int i = 0; i < 16; ++i) {
        if (stats.cluster_weight[i] > 0.f) {
            OKLab mean = stats.cluster_sum[i] * (1.f / stats.cluster_weight[i]);
            next.colors[i] = quantize_oklab_to_amiga(mean, options_.weights);
        } else if (stats.worst_pixel_dist2 >= 0.f) {
            // Genuinely empty cluster: nobody selected this entry this
            // pass. Reseed at the worst-served (importance- and
            // perceptually-weighted) pixel instead of leaving it to
            // stagnate - the classic k-means empty-cluster fix. Tried
            // generalizing this to unconditionally steal the least-used
            // cluster every pass (to give importance weighting more
            // direct pull on allocation, not just on cluster means);
            // that destabilized convergence and measured WORSE on the
            // center-weighted stress test than doing nothing, so it was
            // reverted. The weighted seed (MedianCutQuantizer) and
            // weighted mean above remain the primary levers importance
            // weighting has.
            next.colors[i] = quantize_oklab_to_amiga(stats.worst_pixel_lab, options_.weights);
        }
    }
    return next;
}

HAM6Frame HAM6Encoder::encode(const Image& image) const {
    HAM6Frame frame;
    frame.width = image.width();
    frame.height = image.height();
    diffuser_.set_pattern_origin(options_.dither_origin_x, options_.dither_origin_y);

    std::vector<float> weights = options_.importance
                                      ? options_.importance->compute(image)
                                      : std::vector<float>(static_cast<size_t>(frame.width) * frame.height, 1.0f);

    Palette palette =
        options_.locked_palette ? *options_.locked_palette : quantizer_.quantize(image, 16, weights, options_.weights);
    // Applied both right after the seed/lock and after every
    // refinement pass below, so fixed entries never drift and every
    // unfixed entry is fitted around their correct final values.
    auto apply_fixed_colors = [&](Palette& p) {
        for (size_t i = 0; i < options_.fixed_colors.size() && i < p.colors.size(); ++i) {
            if (options_.fixed_colors[i]) p.colors[i] = *options_.fixed_colors[i];
        }
    };
    apply_fixed_colors(palette);

    int refinement_iterations = options_.locked_palette ? 0 : options_.palette_refinement_iterations;
    const int refinement_depth = options_.lookahead_in_refinement ? options_.lookahead : 1;

    // Progress is weighted by what a pass costs: a lookahead pass is several
    // times as expensive as a greedy one, so counting passes would make the bar
    // crawl through the (fast) refinement and then stall on the final pass.
    const float refinement_cost = pass_cost(refinement_depth, options_.lookahead_beam);
    const float final_cost = pass_cost(options_.lookahead, options_.lookahead_beam);
    const float total_cost = refinement_cost * static_cast<float>(refinement_iterations) + final_cost;
    float done_cost = 0.f;
    for (int iter = 0; iter < refinement_iterations; ++iter) {
        PassResult trial = run_pass(image, palette, weights, refinement_depth, done_cost / total_cost,
                                    (done_cost + refinement_cost) / total_cost);
        done_cost += refinement_cost;
        palette = refit_palette(palette, trial);
        apply_fixed_colors(palette);
    }

    PassResult final_pass = run_pass(image, palette, weights, options_.lookahead, done_cost / total_cost, 1.f);
    frame.palette = palette;
    frame.pixel_codes = std::move(final_pass.pixel_codes);
    return frame;
}

Image decode_ham6(const HAM6Frame& frame) {
    Image out(frame.width, frame.height);
    for (int y = 0; y < frame.height; ++y) {
        // Per the ILBM spec a Modify as the first pixel of a line modifies
        // the border color (register 0). Frames from an encoder with
        // line_start_modify off never do that, so this is only observable
        // for frames produced with the option on.
        AmigaColor held = frame.palette.colors[0];
        for (int x = 0; x < frame.width; ++x) {
            uint8_t code = frame.pixel_codes[static_cast<size_t>(y) * frame.width + x];
            AmigaColor c = apply_code(code, held, frame.palette);
            out.at(x, y) = c.to_srgb();
            held = c;
        }
    }
    return out;
}

}  // namespace aic
