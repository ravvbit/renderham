// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
// Tests for the HAM6 encoder's lookahead search (HAM6EncoderOptions::lookahead).
//
// The lookahead is only trustworthy if it is a strict generalization of the
// greedy encoder, so most of these are exact equivalences, not "looks better":
//   1. beam search at depth 1  == greedy search      (same candidates, same ties)
//   2. beam width 1 at any depth == greedy search    (a one-wide beam keeps only
//      the best first step, which is the greedy choice)
//   3. the search is deterministic
//   4. on a hard-edged synthetic image a real lookahead reduces the error
#include <cmath>
#include <cstdio>
#include <string>

#include "aic/aic.hpp"

using namespace aic;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("  %s  %s\n", ok ? "OK  " : "FAIL", what.c_str());
    if (!ok) ++g_failures;
}

// Hard color bars (every channel jumps at once - the worst case for Modify),
// a vertical gradient and a little deterministic noise, so the result
// exercises both Select and all three Modify operations.
Image make_test_image(int w, int h) {
    Image img(w, h);
    uint32_t rng = 12345;
    auto noise = [&] { rng = rng * 1664525u + 1013904223u; return ((rng >> 16) & 0xFF) / 255.f - 0.5f; };
    const float bars[6][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 0}, {0.1f, 0.1f, 0.1f}, {0.9f, 0.4f, 0.7f}};
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float* b = bars[(x / 11) % 6];
            float g = 0.15f + 0.7f * y / h;
            float k = (y < h / 2) ? 1.f : 0.f;  // top half bars, bottom half gradient
            auto mix = [&](float bar) { return std::fmin(1.f, std::fmax(0.f, k * bar + (1.f - k) * g + 0.03f * noise())); };
            img.at(x, y) = RGBf{mix(b[0]), mix(b[1]), mix(b[2])};
        }
    }
    return img;
}

HAM6Frame encode(const Image& img, const std::string& dither, HAM6EncoderOptions opts) {
    auto quantizer = Registry<IPaletteQuantizer>::instance().create("median-cut");
    auto diffuser = Registry<IErrorDiffuser>::instance().create(dither);
    HAM6Encoder enc(*quantizer, *diffuser, opts);
    return enc.encode(img);
}

bool same_frame(const HAM6Frame& a, const HAM6Frame& b) {
    if (a.pixel_codes != b.pixel_codes) return false;
    for (size_t i = 0; i < a.palette.colors.size(); ++i) {
        const auto &p = a.palette.colors[i], &q = b.palette.colors[i];
        if (p.r4 != q.r4 || p.g4 != q.g4 || p.b4 != q.b4) return false;
    }
    return true;
}

double mean_error(const Image& ref, const HAM6Frame& frame) {
    Image dec = decode_ham6(frame);
    double sum = 0;
    for (int y = 0; y < ref.height(); ++y)
        for (int x = 0; x < ref.width(); ++x)
            sum += std::sqrt(distance2(srgb_to_oklab(ref.at(x, y)), srgb_to_oklab(dec.at(x, y))));
    return sum / (ref.width() * ref.height());
}

}  // namespace

int main() {
    const Image img = make_test_image(96, 64);
    const char* ditherers[] = {"none", "floyd-steinberg", "bayer", "atkinson", "checks-lines"};

    std::printf("1. beam search at depth 1 == greedy search\n");
    for (const char* d : ditherers) {
        for (bool lsm : {false, true}) {
            HAM6EncoderOptions greedy;
            greedy.line_start_modify = lsm;
            HAM6EncoderOptions beam = greedy;
            beam.lookahead_always_beam = true;
            check(same_frame(encode(img, d, greedy), encode(img, d, beam)),
                  std::string(d) + (lsm ? ", line-start-modify" : ""));
        }
    }

    std::printf("2. beam width 1 at depth 3 == greedy search\n");
    for (const char* d : ditherers) {
        HAM6EncoderOptions greedy;
        HAM6EncoderOptions narrow = greedy;
        narrow.lookahead = 3;
        narrow.lookahead_beam = 1;
        check(same_frame(encode(img, d, greedy), encode(img, d, narrow)), d);
    }

    std::printf("3. determinism\n");
    {
        HAM6EncoderOptions o;
        o.lookahead = 3;
        check(same_frame(encode(img, "floyd-steinberg", o), encode(img, "floyd-steinberg", o)), "depth 3 twice");
    }

    std::printf("4. lookahead reduces the error on a hard-edged image\n");
    for (const char* d : {"none", "floyd-steinberg"}) {
        HAM6EncoderOptions greedy;
        HAM6EncoderOptions la = greedy;
        la.lookahead = 3;
        double eg = mean_error(img, encode(img, d, greedy));
        double el = mean_error(img, encode(img, d, la));
        std::printf("       %-16s greedy %.5f  lookahead 3 %.5f\n", d, eg, el);
        check(el < eg, std::string(d) + ": lookahead 3 < greedy");
    }

    std::printf(g_failures == 0 ? "alle Tests bestanden\n" : "%d Test(s) FEHLGESCHLAGEN\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
