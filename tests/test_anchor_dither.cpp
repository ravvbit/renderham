// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
// Tests for anchoring an ordered ditherer's pattern to the source picture
// (IErrorDiffuser::set_pattern_origin, ConversionOptions::anchor_dither_to_source,
// SequenceOptions::anchor_dither).
//
// Three layers:
//  1. The ditherers: a pattern shifted by (ox, oy) must give exactly the bias the
//     unshifted pattern has at (x+ox, y+oy) - for the whole ordered family, with
//     negative origins too. Error diffusers must not react to the call at all.
//  2. The pipeline: frames of a pan. The overlap of two frames (same content, a few
//     pixels apart) must differ much less with the pattern anchored than without -
//     that is the point of the feature - and error diffusion must be unaffected.
//  3. The promises that keep old results valid: off by default, no effect without a
//     crop, and a shift by a whole pattern period changes nothing.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "aic/aic.hpp"

using namespace aic;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("  %s  %s\n", ok ? "OK  " : "FAIL", what.c_str());
    if (!ok) ++g_failures;
}

const std::string kInput = std::string(RENDERHAM_TESTDATA) + "/test_input.png";  // 640x400

bool same_oklab(const OKLab& a, const OKLab& b) { return a.L == b.L && a.a == b.a && a.b == b.b; }

// ---------------------------------------------------------------------------
// 1. The ditherers
// ---------------------------------------------------------------------------

void test_pattern_shift() {
    std::printf("pattern origin shifts the pattern (ordered ditherers)\n");
    const char* names[] = {"bayer", "blue-noise", "checks", "checks-lines", "checks-lines-mixed"};
    const int origins[][2] = {{0, 0}, {3, 0}, {0, 5}, {7, 9}, {8, 8}, {-5, -11}, {-1, 6}, {123, -77}};
    const OKLab target{0.55f, 0.03f, -0.04f};

    for (const char* name : names) {
        bool all_ok = true;
        for (const auto& o : origins) {
            auto shifted = Registry<IErrorDiffuser>::instance().create(name);
            auto plain = Registry<IErrorDiffuser>::instance().create(name);
            shifted->begin(64, 64);
            plain->begin(64, 64);
            shifted->set_strength(1.f);
            plain->set_strength(1.f);
            shifted->set_pattern_origin(o[0], o[1]);
            for (int y = 0; y < 24 && all_ok; ++y) {
                for (int x = 0; x < 24; ++x) {
                    if (!same_oklab(shifted->adjust_target(x, y, target),
                                    plain->adjust_target(x + o[0], y + o[1], target))) {
                        all_ok = false;
                        std::printf("      %s: mismatch at (%d,%d), origin (%d,%d)\n", name, x, y, o[0], o[1]);
                        break;
                    }
                }
            }
        }
        check(all_ok, std::string(name) + ": bias at (x,y) with origin == unshifted bias at (x+ox,y+oy)");
    }

    // The point of shifting: an origin that is not a whole period really moves the pattern
    // (otherwise the check above would be satisfied by a ditherer that ignores the call).
    // Swept over many colors: a single color can sit where both phases round to the same
    // 4-bit level, and the two line phases would then look alike.
    for (const char* name : names) {
        auto a = Registry<IErrorDiffuser>::instance().create(name);
        auto b = Registry<IErrorDiffuser>::instance().create(name);
        a->begin(64, 64);
        b->begin(64, 64);
        a->set_strength(1.f);
        b->set_strength(1.f);
        b->set_pattern_origin(1, 1);
        int differing = 0;
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                const float v = 0.05f + 0.9f * static_cast<float>((x * 16 + y) % 37) / 36.f;
                const OKLab t = srgb_to_oklab(RGBf{v, 1.f - v, 0.5f * v + 0.2f});
                if (!same_oklab(a->adjust_target(x, y, t), b->adjust_target(x, y, t))) ++differing;
            }
        }
        // The checker moves by 2 with (1,1): same parity, identical - correct for a 2-periodic pattern.
        if (std::string(name) == "checks") {
            check(differing == 0, "checks: origin (1,1) is a whole period of the checkerboard (identical)");
        } else {
            check(differing > 0, std::string(name) + ": origin (1,1) changes the pattern");
        }
    }
}

void test_error_diffusers_ignore_origin() {
    std::printf("error diffusers ignore the origin\n");
    const char* names[] = {"floyd-steinberg", "atkinson", "sierra-lite", "jarvis-judice-ninke", "none"};
    const OKLab target{0.5f, 0.02f, 0.01f};
    const OKLab err{0.01f, -0.002f, 0.003f};
    for (const char* name : names) {
        auto a = Registry<IErrorDiffuser>::instance().create(name);
        auto b = Registry<IErrorDiffuser>::instance().create(name);
        a->begin(16, 16);
        b->begin(16, 16);
        a->set_strength(1.f);
        b->set_strength(1.f);
        b->set_pattern_origin(5, -3);
        bool same = true;
        for (int y = 0; y < 8; ++y) {
            a->begin_row(y);
            b->begin_row(y);
            for (int x = 0; x < 16; ++x) {
                same = same && same_oklab(a->adjust_target(x, y, target), b->adjust_target(x, y, target));
                a->feedback(x, y, err);
                b->feedback(x, y, err);
            }
        }
        check(same, std::string(name) + ": identical with and without set_pattern_origin");
    }
}

// ---------------------------------------------------------------------------
// 2. The pipeline: a pan
// ---------------------------------------------------------------------------

CropSpec crop_at(float x, float y, float w, float h) {
    CropSpec c;
    c.width = {w, false};
    c.height = {h, false};
    c.x = {x, false};
    c.y = {y, false};
    return c;
}

// Mean absolute difference (0..255 scale, all three channels) of the overlap of two decoded
// frames of a pan: `a` is the earlier frame, `b` the one whose crop box moved `shift` canvas
// pixels to the right, so b's column x shows what a has at column x+shift. The outermost columns and the
// first few columns are left out: a HAM line starts from the palette, the first pixels of a
// row carry no history yet.
double overlap_difference(const Image& a, const Image& b, int shift) {
    double sum = 0.0;
    long n = 0;
    for (int y = 0; y < a.height(); ++y) {
        for (int x = 8; x + shift < a.width() - 8; ++x) {
            const RGBf& p = a.at(x + shift, y);
            const RGBf& q = b.at(x, y);
            sum += std::fabs(p.r - q.r) + std::fabs(p.g - q.g) + std::fabs(p.b - q.b);
            n += 3;
        }
    }
    return n ? 255.0 * sum / static_cast<double>(n) : 0.0;
}

struct PanFrames {
    Image first, second;
    bool ok = false;
};

// Two frames of one source with the crop box moved by `src_shift` source pixels. The second frame
// uses the first one's palette, as an animation does - otherwise the palettes would differ.
PanFrames render_pan(const std::string& dither, bool anchor, float crop_w, float crop_h, int width, int height,
                     float src_shift) {
    ConversionPipeline pipeline;
    ConversionOptions o;
    o.target_width = width;
    o.target_height = height;
    o.ditherer_name = dither;
    o.lookahead = 0;  // the test is about the dither pattern; greedy keeps it quick and unambiguous
    o.anchor_dither_to_source = anchor;

    PanFrames out;
    o.crop = crop_at(40.f, 50.f, crop_w, crop_h);
    PreviewResult a = pipeline.preview(kInput, o);
    if (!a.success) return out;
    o.crop = crop_at(40.f + src_shift, 50.f, crop_w, crop_h);
    o.locked_palette = a.frame.palette;
    PreviewResult b = pipeline.preview(kInput, o);
    if (!b.success) return out;
    out.first = a.decoded;
    out.second = b.decoded;
    out.ok = true;
    return out;
}

void test_pan_flicker(const char* label, float crop_w, float crop_h, int width, int height, float src_shift,
                      int canvas_shift) {
    std::printf("pan, %s\n", label);
    for (const char* dither : {"bayer", "blue-noise", "checks-lines"}) {
        PanFrames fixed = render_pan(dither, false, crop_w, crop_h, width, height, src_shift);
        PanFrames moved = render_pan(dither, true, crop_w, crop_h, width, height, src_shift);
        if (!fixed.ok || !moved.ok) {
            check(false, std::string(dither) + ": frames rendered");
            continue;
        }
        const double d_fixed = overlap_difference(fixed.first, fixed.second, canvas_shift);
        const double d_anchor = overlap_difference(moved.first, moved.second, canvas_shift);
        std::printf("      %-12s overlap difference: pattern on the screen %.2f, on the picture %.2f (0-255)\n", dither,
                    d_fixed, d_anchor);
        // checks-lines depends on y alone: a horizontal pan cannot flicker it either way (the
        // anchored pattern may start on the other line parity, which is why not bit-equal).
        if (std::string(dither) == "checks-lines") {
            check(std::fabs(d_fixed - d_anchor) < 0.5, "checks-lines: a horizontal pan is about the same either way");
        } else {
            // Measured: about 9 without anchoring against 0.02 with it (no-dither baseline: exactly 0).
            check(d_anchor < 0.1 * d_fixed, std::string(dither) + ": anchored overlap differs at least 10 times less");
        }
    }

    // Error diffusion has no pattern, so the flag must not change a single pixel.
    for (const char* dither : {"floyd-steinberg", "atkinson"}) {
        PanFrames fixed = render_pan(dither, false, crop_w, crop_h, width, height, src_shift);
        PanFrames moved = render_pan(dither, true, crop_w, crop_h, width, height, src_shift);
        const bool same = fixed.ok && moved.ok && overlap_difference(fixed.second, moved.second, 0) == 0.0 &&
                          overlap_difference(fixed.first, moved.first, 0) == 0.0;
        check(same, std::string(dither) + ": the flag changes nothing");
    }
}

// ---------------------------------------------------------------------------
// 3. Promises about the old behavior
// ---------------------------------------------------------------------------

bool same_image(const Image& a, const Image& b) {
    if (a.width() != b.width() || a.height() != b.height()) return false;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x) {
            const RGBf &p = a.at(x, y), &q = b.at(x, y);
            if (p.r != q.r || p.g != q.g || p.b != q.b) return false;
        }
    return true;
}

PreviewResult preview_with(const std::string& dither, bool anchor, std::optional<CropSpec> crop, FitMode fit,
                           int width = 160, int height = 100) {
    ConversionPipeline pipeline;
    ConversionOptions o;
    o.target_width = width;
    o.target_height = height;
    o.ditherer_name = dither;
    o.lookahead = 0;
    o.fit_mode = fit;
    o.crop = crop;
    o.anchor_dither_to_source = anchor;
    return pipeline.preview(kInput, o);
}

void test_old_results_stay() {
    std::printf("old results stay valid\n");
    {
        ConversionOptions defaults;
        check(!defaults.anchor_dither_to_source, "ConversionOptions: anchoring is off by default");
        SequenceOptions seq;
        check(seq.anchor_dither, "SequenceOptions: anchoring is on by default");
    }
    for (const char* dither : {"bayer", "blue-noise", "checks"}) {
        const std::string d = dither;
        // No crop, fill the canvas (stretch): the source's corner is the canvas corner, origin (0,0).
        check(same_image(preview_with(d, false, std::nullopt, FitMode::Stretch).decoded,
                         preview_with(d, true, std::nullopt, FitMode::Stretch).decoded),
              d + ": no crop, stretched -> identical");
        // A crop box at the source's origin: also (0,0).
        const CropSpec at_origin = crop_at(0.f, 0.f, 320.f, 200.f);
        check(same_image(preview_with(d, false, at_origin, FitMode::Stretch).decoded,
                         preview_with(d, true, at_origin, FitMode::Stretch).decoded),
              d + ": crop at the source's corner -> identical");
        // Crop box moved by whole pattern periods (16 and 8 px at 1:1, period 8 and 2): the same pattern phase.
        if (d == "bayer") {
            const CropSpec shifted = crop_at(16.f, 8.f, 160.f, 100.f);
            check(same_image(preview_with(d, false, shifted, FitMode::Stretch).decoded,
                             preview_with(d, true, shifted, FitMode::Stretch).decoded),
                  "bayer: crop moved by whole 8x8 periods -> identical");
        }
        // A crop that is NOT a whole period away must differ - otherwise the option does nothing.
        // (37 + 22 is odd: the checkerboard has period 2 in x+y, an even sum would not move it.)
        const CropSpec off = crop_at(37.f, 22.f, 160.f, 100.f);
        check(!same_image(preview_with(d, false, off, FitMode::Stretch).decoded,
                          preview_with(d, true, off, FitMode::Stretch).decoded),
              d + ": crop at (37,22) -> pattern really moved");
    }
}

}  // namespace

int main() {
    test_pattern_shift();
    test_error_diffusers_ignore_origin();
    // 1:1 - the crop is as large as the canvas; moving the crop 3 source pixels moves the content 3 canvas pixels.
    test_pan_flicker("1:1, 3 px", 320.f, 200.f, 320, 200, 3.f, 3);
    // 2:1 reduction - 6 source pixels are 3 canvas pixels. Checks the rounding of the scaled crop position.
    test_pan_flicker("2:1 reduction, 6 source px = 3 px", 480.f, 300.f, 240, 150, 6.f, 3);
    test_old_results_stay();

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL OK" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
