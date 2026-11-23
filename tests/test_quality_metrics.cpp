// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
// Tests for the perceptual quality figures (aic/quality_metrics.hpp): mean error, blurred error
// (OKLab distance after a sigma 1.5 Gaussian) and SSIM of the OKLab lightness.
//
// Three kinds of evidence, because a metric that only agrees with itself proves nothing:
//  1. Values computed independently: for deterministic float pictures the expected numbers come
//     from a NumPy implementation (scripts/ham_eval.py's blur plus a textbook SSIM, float64),
//     not from this code.
//  2. Cases with a known answer: identical pictures, a constant offset (every figure follows
//     from the OKLab lightness of the two grays), the blur wiping out a checkerboard.
//  3. The pipeline plumbing: off by default, no influence on the encoded frame, never computed
//     for the frames of a sequence.
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

bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

const std::string kInput = std::string(RENDERHAM_TESTDATA) + "/test_input.png";

// The deterministic picture pair of the NumPy reference (see the expected values below):
//   ref(x,y,c) = ((37x + 91y + 53c) mod 251) / 250
//   dec        = clamp(ref + ((13x + 7y + 29c) mod 11 - 5) / 60)
void make_pair(int w, int h, Image& ref, Image& dec) {
    ref = Image(w, h);
    dec = Image(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float r[3], d[3];
            for (int c = 0; c < 3; ++c) {
                r[c] = static_cast<float>(((x * 37 + y * 91 + c * 53) % 251) / 250.0);
                const float delta = static_cast<float>(((x * 13 + y * 7 + c * 29) % 11 - 5) / 60.0);
                d[c] = std::fmin(1.f, std::fmax(0.f, r[c] + delta));
            }
            ref.at(x, y) = RGBf{r[0], r[1], r[2]};
            dec.at(x, y) = RGBf{d[0], d[1], d[2]};
        }
    }
}

Image flat(int w, int h, float v) {
    Image img(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) img.at(x, y) = RGBf{v, v, v};
    return img;
}

void test_against_numpy() {
    std::printf("values from an independent NumPy implementation\n");
    struct Case {
        int w, h;
        double mean, blurred, ssim;
    };
    // Computed with float64 NumPy: OKLab as in ham_eval.py, blur = ham_eval.gaussian_blur (sigma 1.5,
    // reflected borders), SSIM with C1 = 0.01^2, C2 = 0.03^2 on the lightness. 6x4 is smaller than the
    // blur radius (5): the reflection has to work repeatedly there.
    const Case cases[] = {
        {23, 17, 0.039200086, 0.003409152, 0.986732667},
        {40, 31, 0.039482383, 0.003061852, 0.986713676},
        {6, 4, 0.038148565, 0.005444276, 0.992066286},
    };
    for (const Case& c : cases) {
        Image ref, dec;
        make_pair(c.w, c.h, ref, dec);
        const QualityMetrics m = compute_quality_metrics(ref, dec);
        // float32 here against float64 there: about 1e-7 expected; allow 1e-5.
        const std::string size = std::to_string(c.w) + "x" + std::to_string(c.h);
        check(m.valid, size + ": valid");
        check(near(m.mean_error, c.mean, 1e-5), size + ": mean error " + std::to_string(m.mean_error));
        check(near(m.blurred_error, c.blurred, 1e-5), size + ": blurred error " + std::to_string(m.blurred_error));
        check(near(m.ssim, c.ssim, 1e-5), size + ": SSIM " + std::to_string(m.ssim));
    }
}

void test_known_answers() {
    std::printf("cases with a known answer\n");

    // Identical pictures, including degenerate shapes (the reflection must cope with width or height 1).
    for (auto wh : {std::pair<int, int>{30, 20}, {1, 1}, {1, 9}, {9, 1}, {2, 2}}) {
        Image a, b;
        make_pair(wh.first, wh.second, a, b);
        const QualityMetrics m = compute_quality_metrics(a, a);
        check(m.valid && m.mean_error == 0.0 && m.blurred_error == 0.0 && near(m.ssim, 1.0, 1e-9),
              "identical " + std::to_string(wh.first) + "x" + std::to_string(wh.second) +
                  ": errors 0, SSIM 1");
    }

    // Two flat grays: in OKLab a = b = 0 for a gray, so the distance is the lightness difference; the blur
    // does not change a flat picture (reflected borders), and the SSIM has no variance terms:
    // (2 mx my + C1) / (mx^2 + my^2 + C1).
    {
        const float g1 = 0.40f, g2 = 0.55f;
        const double L1 = srgb_to_oklab(RGBf{g1, g1, g1}).L, L2 = srgb_to_oklab(RGBf{g2, g2, g2}).L;
        const QualityMetrics m = compute_quality_metrics(flat(17, 11, g1), flat(17, 11, g2));
        const double expect_ssim = (2 * L1 * L2 + 1e-4) / (L1 * L1 + L2 * L2 + 1e-4);
        check(near(m.mean_error, std::fabs(L1 - L2), 1e-6), "flat grays: mean error = lightness difference");
        check(near(m.blurred_error, std::fabs(L1 - L2), 1e-6), "flat grays: blurred error = the same (blur of a flat picture)");
        check(near(m.ssim, expect_ssim, 1e-6), "flat grays: SSIM = (2 L1 L2 + C1) / (L1^2 + L2^2 + C1)");
    }

    // A checkerboard of +-d around the flat color: every pixel is off by d, the average is right.
    // This is what a dither does, and the reason the blurred error exists.
    {
        const float mid = 0.5f, d = 0.06f;
        Image dither(32, 24);
        for (int y = 0; y < 24; ++y)
            for (int x = 0; x < 32; ++x) {
                const float v = ((x + y) & 1) ? mid + d : mid - d;
                dither.at(x, y) = RGBf{v, v, v};
            }
        const QualityMetrics m = compute_quality_metrics(flat(32, 24, mid), dither);
        std::printf("      checkerboard: mean error %.5f, blurred error %.5f, SSIM %.4f\n", m.mean_error,
                    m.blurred_error, m.ssim);
        check(m.mean_error > 0.02, "checkerboard around the flat color: every pixel is off");
        check(m.blurred_error < 0.1 * m.mean_error, "... but the blurred error is under a tenth of it");
    }

    // Symmetry, and the direction of SSIM: more noise, lower value.
    {
        Image a, b;
        make_pair(40, 31, a, b);
        const QualityMetrics ab = compute_quality_metrics(a, b), ba = compute_quality_metrics(b, a);
        check(near(ab.mean_error, ba.mean_error, 1e-9) && near(ab.blurred_error, ba.blurred_error, 1e-9) &&
                  near(ab.ssim, ba.ssim, 1e-9),
              "swapping the two pictures changes nothing");

        // Smooth reference plus noise of growing size.
        Image ref(48, 36);
        for (int y = 0; y < 36; ++y)
            for (int x = 0; x < 48; ++x) {
                const float v = 0.25f + 0.5f * static_cast<float>(x) / 47.f;
                ref.at(x, y) = RGBf{v, 0.8f * v, 0.6f * v};
            }
        double prev_ssim = 2.0, prev_blur = -1.0;
        bool ssim_falls = true, blur_rises = true;
        for (float amp : {0.0f, 0.02f, 0.05f, 0.10f}) {
            Image noisy = ref;
            for (int y = 0; y < 36; ++y)
                for (int x = 0; x < 48; ++x) {
                    const float n = amp * (static_cast<float>((x * 7 + y * 13) % 17) / 8.f - 1.f);
                    RGBf& p = noisy.at(x, y);
                    p = RGBf{p.r + n, p.g + n, p.b + n};
                }
            const QualityMetrics m = compute_quality_metrics(ref, noisy);
            if (!(m.ssim < prev_ssim)) ssim_falls = false;
            if (!(m.blurred_error > prev_blur)) blur_rises = false;
            prev_ssim = m.ssim;
            prev_blur = m.blurred_error;
        }
        check(ssim_falls, "growing noise: SSIM falls every step");
        check(blur_rises, "growing noise: blurred error rises every step");
    }

    // Unusable input.
    {
        QualityMetrics m = compute_quality_metrics(flat(8, 8, 0.5f), flat(8, 9, 0.5f));
        check(!m.valid, "different sizes: not valid");
        m = compute_quality_metrics(Image(), Image());
        check(!m.valid, "empty pictures: not valid");
    }
}

void test_pipeline_plumbing() {
    std::printf("pipeline\n");
    ConversionPipeline p;
    ConversionOptions o;
    o.target_width = 160;
    o.target_height = 100;
    o.ditherer_name = "floyd-steinberg";

    PreviewResult off = p.preview(kInput, o);
    check(off.success && !off.metrics.valid, "off by default: no metrics");

    o.quality_metrics = true;
    PreviewResult on = p.preview(kInput, o);
    check(on.success && on.metrics.valid, "switched on: metrics valid");
    check(on.frame.pixel_codes == off.frame.pixel_codes && on.mean_oklab_error == off.mean_oklab_error,
          "the switch changes neither the encoded frame nor the mean error");
    check(near(on.metrics.mean_error, on.mean_oklab_error, 1e-9),
          "metrics.mean_error equals the pipeline's mean_oklab_error");
    check(on.metrics.blurred_error < on.metrics.mean_error, "dithered: blurred error is below the per-pixel error");
    check(on.metrics.ssim > 0.0 && on.metrics.ssim < 1.0, "SSIM is strictly between 0 and 1");

    // What the blurred error is for: it rewards the dither that the per-pixel figure punishes.
    ConversionOptions none = o;
    none.ditherer_name = "none";
    PreviewResult nodither = p.preview(kInput, none);
    std::printf("      floyd-steinberg: mean %.5f blurred %.5f SSIM %.4f\n", on.metrics.mean_error,
                on.metrics.blurred_error, on.metrics.ssim);
    std::printf("      none           : mean %.5f blurred %.5f SSIM %.4f\n", nodither.metrics.mean_error,
                nodither.metrics.blurred_error, nodither.metrics.ssim);
    check(nodither.metrics.mean_error < on.metrics.mean_error, "per-pixel error: no dither looks better");
    check(on.metrics.blurred_error < nodither.metrics.blurred_error, "blurred error: the dither looks better");

    // ConversionResult carries them, too.
    const std::string out = std::string(RENDERHAM_TMPDIR) + "/quality_metrics_out.iff";
    ConversionResult cr = p.convert(kInput, out, o);
    check(cr.success && cr.metrics.valid && near(cr.metrics.ssim, on.metrics.ssim, 1e-12),
          "convert() returns the same figures");

    // Sequences never compute them, even when the base options ask.
    SequenceOptions so;
    so.threads = 2;
    int frames = 0;
    bool any_valid = false;
    so.on_frame = [&](size_t, PreviewResult& f) -> std::string {
        ++frames;
        any_valid = any_valid || f.metrics.valid;
        return {};
    };
    ConversionOptions seq = o;
    seq.target_width = 64;
    seq.target_height = 48;
    std::vector<CropSpec> crops(3);
    for (size_t i = 0; i < crops.size(); ++i) {
        crops[i].width = {320.f, false};
        crops[i].height = {200.f, false};
        crops[i].x = {static_cast<float>(i * 20), false};
        crops[i].y = {0.f, false};
    }
    SequenceResult sr = render_sequence(p, kInput, seq, crops, so);
    check(sr.success && frames == 3 && !any_valid, "render_sequence: three frames, none with metrics");
}

}  // namespace

int main() {
    test_against_numpy();
    test_known_answers();
    test_pipeline_plumbing();
    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL OK" : "FAILED", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
