// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/quality_metrics.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

#include "aic/color_space.hpp"

namespace aic {
namespace {

// One channel of an image, row-major. double, not float: the SSIM takes variances as
// E[x^2] - E[x]^2, and with float planes that difference is off by about 3e-8 - small
// against C2 = 9e-4 only until a flat area (variance 0) is compared, where it showed as
// an error of 3e-5 in the SSIM.
struct Plane {
    int w = 0, h = 0;
    std::vector<double> v;
    Plane() = default;
    Plane(int width, int height) : w(width), h(height), v(static_cast<size_t>(width) * height, 0.0) {}
    double& at(int x, int y) { return v[static_cast<size_t>(y) * w + x]; }
    double at(int x, int y) const { return v[static_cast<size_t>(y) * w + x]; }
};

// Index of a border-reflected sample (edge pixel not repeated: -1 -> 1, n -> n-2), for any
// distance outside, however small the image is.
int reflect(int i, int n) {
    if (n == 1) return 0;
    const int period = 2 * (n - 1);
    i %= period;
    if (i < 0) i += period;
    return i < n ? i : period - i;
}

std::vector<double> gaussian_kernel(float sigma, int& radius) {
    radius = static_cast<int>(std::ceil(3.f * sigma));
    std::vector<double> k(static_cast<size_t>(2 * radius + 1));
    double sum = 0.0;
    for (int i = -radius; i <= radius; ++i) {
        const double t = i / static_cast<double>(sigma);
        const double wgt = std::exp(-0.5 * t * t);
        k[static_cast<size_t>(i + radius)] = wgt;
        sum += wgt;
    }
    for (double& f : k) f /= sum;
    return k;
}

// Separable Gaussian blur, reflected borders.
Plane blur(const Plane& src, const std::vector<double>& k, int radius) {
    Plane tmp(src.w, src.h), out(src.w, src.h);
    for (int y = 0; y < src.h; ++y) {
        for (int x = 0; x < src.w; ++x) {
            double acc = 0.0;
            for (int i = -radius; i <= radius; ++i) acc += k[static_cast<size_t>(i + radius)] * src.at(reflect(x + i, src.w), y);
            tmp.at(x, y) = acc;
        }
    }
    for (int y = 0; y < src.h; ++y) {
        for (int x = 0; x < src.w; ++x) {
            double acc = 0.0;
            for (int i = -radius; i <= radius; ++i) acc += k[static_cast<size_t>(i + radius)] * tmp.at(x, reflect(y + i, src.h));
            out.at(x, y) = acc;
        }
    }
    return out;
}

struct LabPlanes {
    Plane L, a, b;
};

LabPlanes to_oklab(const Image& img) {
    LabPlanes p{Plane(img.width(), img.height()), Plane(img.width(), img.height()), Plane(img.width(), img.height())};
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const OKLab c = srgb_to_oklab(img.at(x, y));
            p.L.at(x, y) = c.L;
            p.a.at(x, y) = c.a;
            p.b.at(x, y) = c.b;
        }
    }
    return p;
}

}  // namespace

QualityMetrics compute_quality_metrics(const Image& reference, const Image& decoded) {
    QualityMetrics m;
    if (reference.empty() || decoded.empty() || reference.width() != decoded.width() ||
        reference.height() != decoded.height()) {
        return m;
    }
    const int w = reference.width(), h = reference.height();
    const double n = static_cast<double>(w) * h;

    const LabPlanes ref = to_oklab(reference);
    const LabPlanes dec = to_oklab(decoded);

    // 1. Per-pixel distance.
    double sum = 0.0;
    for (size_t i = 0; i < ref.L.v.size(); ++i) {
        const double dl = ref.L.v[i] - dec.L.v[i], da = ref.a.v[i] - dec.a.v[i], db = ref.b.v[i] - dec.b.v[i];
        sum += std::sqrt(dl * dl + da * da + db * db);
    }
    m.mean_error = sum / n;

    // 2. Distance after the same blur on both.
    int radius = 0;
    const std::vector<double> kernel = gaussian_kernel(kPerceptualBlurSigma, radius);
    const Plane rL = blur(ref.L, kernel, radius), ra = blur(ref.a, kernel, radius), rb = blur(ref.b, kernel, radius);
    const Plane dL = blur(dec.L, kernel, radius), da = blur(dec.a, kernel, radius), db = blur(dec.b, kernel, radius);
    double bsum = 0.0;
    for (size_t i = 0; i < rL.v.size(); ++i) {
        const double x = rL.v[i] - dL.v[i], y = ra.v[i] - da.v[i], z = rb.v[i] - db.v[i];
        bsum += std::sqrt(x * x + y * y + z * z);
    }
    m.blurred_error = bsum / n;

    // 3. SSIM of the lightness. The local means are the blurs computed above (rL, dL); only the
    //    second moments are new.
    Plane rr(w, h), dd(w, h), rd(w, h);
    for (size_t i = 0; i < rr.v.size(); ++i) {
        const double x = ref.L.v[i], y = dec.L.v[i];
        rr.v[i] = x * x;
        dd.v[i] = y * y;
        rd.v[i] = x * y;
    }
    const Plane Exx = blur(rr, kernel, radius), Eyy = blur(dd, kernel, radius), Exy = blur(rd, kernel, radius);
    constexpr double kC1 = 0.01 * 0.01, kC2 = 0.03 * 0.03;
    double ssum = 0.0;
    for (size_t i = 0; i < rL.v.size(); ++i) {
        const double mx = rL.v[i], my = dL.v[i];
        const double vx = Exx.v[i] - mx * mx, vy = Eyy.v[i] - my * my, cxy = Exy.v[i] - mx * my;
        ssum += ((2.0 * mx * my + kC1) * (2.0 * cxy + kC2)) / ((mx * mx + my * my + kC1) * (vx + vy + kC2));
    }
    m.ssim = ssum / n;
    m.valid = true;
    return m;
}

}  // namespace aic
