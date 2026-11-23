// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/ham6_encoder.hpp
//
// Implements HAM6 as what it actually is: a 12-bit RGB signal with
// per-pixel delta compression. Each output pixel is one of 64 codes:
//   mode 00 (2 high bits) -> data = index 0..15 into the 16-color palette
//   mode 01               -> data = new 4-bit BLUE value, R/G held
//   mode 10               -> data = new 4-bit RED value,  G/B held
//   mode 11               -> data = new 4-bit GREEN value, R/B held
// ("held" = the previous pixel's actual displayed color, in the same
// scanline; the leftmost pixel of every line has no predecessor and is
// therefore always forced into select-mode.)
//
// Quantization, dithering and the select/modify decision are NOT three
// separate passes - seperating them would mean dithering against a
// target that ignores what HAM can actually reach from the previous
// pixel's real (already-dithered) color. Instead, for every pixel this
// encoder brute-forces all 64 reachable codes, scores each in OKLab
// space against the (dither-biased) target, keeps the best, and only
// *then* feeds the true residual back into the IErrorDiffuser. That is
// the "combined algorithm" from the design discussion.
//
// On top of that single-pass encode, the palette itself can be
// iteratively refined (see HAM6EncoderOptions::palette_refinement_iterations):
// after an initial seed from the IPaletteQuantizer, the encoder runs
// full trial passes - using the SAME real diffuser the final output
// will use - and re-fits each palette entry to the (importance-weighted)
// mean of the pixels that actually chose it via select-mode. Because
// the trial passes go through the real diffuser, refinement is
// automatically dithering-aware with no special-casing.

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

#include "aic/dither.hpp"
#include "aic/image.hpp"
#include "aic/importance.hpp"
#include "aic/palette.hpp"

namespace aic {

struct HAM6Frame {
    int width = 0;
    int height = 0;
    Palette palette;                  // 16 base colors
    std::vector<uint8_t> pixel_codes;  // width*height, one 6-bit HAM code (0..63) per pixel
};

// Thrown by HAM6Encoder::encode when HAM6EncoderOptions::progress asks to stop.
// ConversionPipeline catches it, so frontends see PreviewResult::cancelled.
class OperationCancelled : public std::runtime_error {
public:
    OperationCancelled() : std::runtime_error("cancelled") {}
};

struct HAM6EncoderOptions {
    // 0 disables refinement: the seed palette from IPaletteQuantizer is
    // used as-is (original single-pass behavior).
    int palette_refinement_iterations = 4;

    // nullptr = uniform weighting (every pixel counts equally, both for
    // a weighted quantizer seed and for refinement re-fitting).
    const IImportanceMap* importance = nullptr;

    // Governs how "close" two colors are considered throughout the
    // encoder's own decisions (per-pixel candidate search, palette
    // fitting). {} = standard unweighted OKLab distance. See
    // PerceptualWeights in color_space.hpp.
    PerceptualWeights weights{};

    // If set, used as the encoder's palette directly instead of calling
    // the IPaletteQuantizer, and palette_refinement_iterations is
    // ignored (treated as 0) - refitting a palette the caller
    // explicitly locked would defeat the point of locking it. Meant
    // for reusing one palette across several conversions (e.g. the
    // frames of a future animation feature), so consecutive results
    // don't each pick a slightly different "optimal" palette.
    std::optional<Palette> locked_palette;

    // For any index with a value set, that palette entry is forced to
    // the exact given value - applied to the initial (seeded or
    // locked) palette and re-applied after every refinement pass, so
    // it never drifts and every other (unfixed) entry is fitted around
    // the correct final fixed values, not temporary placeholders.
    // Index 0 is relevant because color 0 also doubles as the Amiga's
    // border/background color outside the actual bitmap area; the
    // other 15 are just as individually fixable.
    std::array<std::optional<AmigaColor>, 16> fixed_colors{};

    // Allow the first pixel of a scanline to be a Modify operation. Per
    // the ILBM specification a Modify at the start of a line modifies
    // the border color (palette register 0), so the "held" color a line
    // starts with is palette.colors[0]. Off by default: that is a rule of
    // the format, but not every decoder follows it - netpbm's ilbmtoppm,
    // for one, starts every line from black and would render such pixels
    // (and the modify chain after them) wrongly. When palette color 0 is
    // black both conventions agree and the option is risk-free. With the
    // option off the first pixel is always a Select, so the held color
    // at line start is irrelevant and every decoder agrees.
    bool line_start_modify = false;

    // How many consecutive pixels the encoder considers when it picks the
    // operation for one pixel. 1 = greedy: the operation with the smallest
    // error for this pixel alone (the original behavior). >1 = lookahead: the
    // best SEQUENCE over the next N pixels is searched and only its first
    // operation is kept, then the search starts again from the next pixel.
    //
    // This matters because Modify changes only ONE color channel per pixel,
    // so a color jump that differs from the held color in several channels
    // takes up to three pixels to reach; a greedy encoder only starts
    // reaching for it once the edge is already there, which smears
    // (fringing). A lookahead can start the Modify chain a pixel or two
    // BEFORE the edge. Cost grows with depth and beam width.
    int lookahead = 1;

    // Beam width of the lookahead search: how many partial sequences
    // (distinct held colors) survive from one level to the next. Wider =
    // closer to the exhaustive optimum, slower. Clamped to 1..32.
    int lookahead_beam = 8;

    // Weight of the pixels AFTER the current one in the sequence cost
    // (current pixel = 1). Below 1 trusts the prediction of future pixels
    // less, since for error-diffusion ditherers their targets are only
    // approximations (the error of the pixels in between is not known yet).
    // Measured (4 photos, Floyd-Steinberg, depth 3): 1.0 is best; 0.5, 0.75
    // and 1.25 are all slightly worse.
    float lookahead_future_weight = 1.0f;

    // Use the lookahead in the palette-refinement passes too, not only in the
    // final one. Off by default: measured, it buys only 0.6% lower error but
    // makes the conversion about 4x slower, because every refinement pass is
    // then as expensive as the final one.
    bool lookahead_in_refinement = false;

    // Shifts the pattern of an ordered ditherer by this many pixels, see
    // IErrorDiffuser::set_pattern_origin. (0, 0) = the pattern as it always was.
    int dither_origin_x = 0;
    int dither_origin_y = 0;

    // Progress report and cancellation for callers that run the encoder on a
    // worker thread. Called at the start of every scanline of every pass with
    // the fraction of the whole encode that is done (0..1, increasing; the
    // refinement passes and the final pass are weighted by their estimated
    // cost, so the bar moves about evenly). Returning false aborts the encode:
    // encode() throws OperationCancelled. Runs on the thread that called
    // encode(); keep it cheap (an atomic store and a flag check). Empty (the
    // default) = no reports and no overhead; the encoder's output is the same
    // either way.
    std::function<bool(float fraction)> progress;

    // For tests: run the beam search even at depth 1. It must then choose
    // exactly the operations the greedy search does (same candidates, same
    // tie-breaking) - the proof that the lookahead is a strict generalization.
    bool lookahead_always_beam = false;
};

class HAM6Encoder {
public:
    HAM6Encoder(const IPaletteQuantizer& quantizer, IErrorDiffuser& diffuser, HAM6EncoderOptions options = {});

    // image must already be exactly the target canvas size (see resize_image).
    // Throws OperationCancelled only if options.progress returned false.
    HAM6Frame encode(const Image& image) const;

private:
    struct PassResult {
        std::vector<uint8_t> pixel_codes;
        // Per-palette-index weighted OKLab sum/weight of pixels that
        // reached that index via select-mode (mode 00) during this pass.
        std::vector<OKLab> cluster_sum;
        std::vector<float> cluster_weight;
        // Worst-scoring pixel this pass (importance-weighted error), for
        // empty-cluster reseeding.
        OKLab worst_pixel_lab{0.f, 0.f, 0.f};
        float worst_pixel_dist2 = -1.f;
    };

    // progress_begin/progress_end: the slice of the overall 0..1 progress this
    // pass covers, see HAM6EncoderOptions::progress.
    PassResult run_pass(const Image& image, const Palette& palette, const std::vector<float>& weights,
                        int lookahead_depth, float progress_begin, float progress_end) const;

    // Lookahead search for one pixel (see HAM6EncoderOptions::lookahead).
    // biased0 is the already dither-adjusted target of pixel x; row_targets
    // holds the unadjusted OKLab targets of the whole row, from which the
    // targets of the following pixels are derived.
    uint8_t choose_lookahead(const OKLab& biased0, const std::vector<OKLab>& row_targets, int x, int y, int width,
                             const AmigaColor& held, const std::vector<int>& pal_idx, int depth) const;
    Palette refit_palette(const Palette& current, const PassResult& stats) const;

    const IPaletteQuantizer& quantizer_;
    IErrorDiffuser& diffuser_;
    HAM6EncoderOptions options_;
};

// Decodes a HAM6Frame back to sRGB, purely for local preview / PSNR-style
// QA (e.g. --preview-png in the CLI) - never used on the Amiga side.
Image decode_ham6(const HAM6Frame& frame);

}  // namespace aic
