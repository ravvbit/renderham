// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/color_space.hpp
//
// All perceptual color matching in this library happens in OKLab space
// (Björn Ottosson, 2020) rather than raw RGB, because Euclidean distance
// in OKLab correlates much better with what the human eye perceives as
// "close" than distance in gamma-encoded RGB does. This directly feeds
// the palette quantizer and the HAM6 encoder's candidate search.

#include <cmath>
#include <algorithm>

namespace aic {

// Working color in linear or gamma space, components normalized to [0,1].
struct RGBf {
    float r = 0.f, g = 0.f, b = 0.f;

    RGBf operator+(const RGBf& o) const { return {r + o.r, g + o.g, b + o.b}; }
    RGBf operator-(const RGBf& o) const { return {r - o.r, g - o.g, b - o.b}; }
    RGBf operator*(float s) const { return {r * s, g * s, b * s}; }
};

// Perceptually (approximately) uniform color space used for all
// distance/error computations.
struct OKLab {
    float L = 0.f, a = 0.f, b = 0.f;

    OKLab operator+(const OKLab& o) const { return {L + o.L, a + o.a, b + o.b}; }
    OKLab operator-(const OKLab& o) const { return {L - o.L, a - o.a, b - o.b}; }
    OKLab operator*(float s) const { return {L * s, a * s, b * s}; }
};

inline float srgb_to_linear(float c) {
    c = std::clamp(c, 0.f, 1.f);
    return (c <= 0.04045f) ? (c / 12.92f)
                            : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

inline float linear_to_srgb(float c) {
    c = std::clamp(c, 0.f, 1.f);
    return (c <= 0.0031308f) ? (c * 12.92f)
                              : (1.055f * std::pow(c, 1.f / 2.4f) - 0.055f);
}

inline RGBf srgb_to_linear(const RGBf& c) {
    return {srgb_to_linear(c.r), srgb_to_linear(c.g), srgb_to_linear(c.b)};
}

inline RGBf linear_to_srgb(const RGBf& c) {
    return {linear_to_srgb(c.r), linear_to_srgb(c.g), linear_to_srgb(c.b)};
}

// Linear RGB -> OKLab
inline OKLab linear_to_oklab(const RGBf& c) {
    float l = 0.4122214708f * c.r + 0.5363325363f * c.g + 0.0514459929f * c.b;
    float m = 0.2119034982f * c.r + 0.6806995451f * c.g + 0.1073969566f * c.b;
    float s = 0.0883024619f * c.r + 0.2817188376f * c.g + 0.6299787005f * c.b;

    float l_ = std::cbrt(l);
    float m_ = std::cbrt(m);
    float s_ = std::cbrt(s);

    return OKLab{
        0.2104542553f * l_ + 0.7936177850f * m_ - 0.0040720468f * s_,
        1.9779984951f * l_ - 2.4285922050f * m_ + 0.4505937099f * s_,
        0.0259040371f * l_ + 0.7827717662f * m_ - 0.8086757660f * s_
    };
}

// OKLab -> linear RGB
inline RGBf oklab_to_linear(const OKLab& c) {
    float l_ = c.L + 0.3963377774f * c.a + 0.2158037573f * c.b;
    float m_ = c.L - 0.1055613458f * c.a - 0.0638541728f * c.b;
    float s_ = c.L - 0.0894841775f * c.a - 1.2914855480f * c.b;

    float l = l_ * l_ * l_;
    float m = m_ * m_ * m_;
    float s = s_ * s_ * s_;

    return RGBf{
        +4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
        -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
        -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s
    };
}

inline OKLab srgb_to_oklab(const RGBf& srgb) { return linear_to_oklab(srgb_to_linear(srgb)); }
inline RGBf oklab_to_srgb(const OKLab& lab) { return linear_to_srgb(oklab_to_linear(lab)); }

inline float distance2(const OKLab& a, const OKLab& b) {
    float dL = a.L - b.L, da = a.a - b.a, db = a.b - b.b;
    return dL * dL + da * da + db * db;
}

// Configurable perceptual weighting for the encoder's internal decisions
// (candidate search, palette fitting). Kept separate from plain
// distance2()/mean_oklab_error reporting so quality numbers stay
// comparable across different weight settings - the encoder's opinion
// of "close" can change; the yardstick used to report results doesn't.
struct PerceptualWeights {
    // Scales the a/b (chroma) contribution relative to L (luminance).
    // 1.0 = unweighted (equivalent to distance2, the original/default
    // behavior). <1 deprioritizes color accuracy in favor of luminance
    // accuracy - the same reasoning chroma-subsampled video/JPEG rely
    // on: the eye resolves fine luminance detail far better than fine
    // chrominance detail, so on ties/trade-offs it's worth spending
    // HAM6's limited per-pixel budget on getting L right first.
    float chroma = 1.0f;
};

inline float weighted_distance2(const OKLab& a, const OKLab& b, const PerceptualWeights& w) {
    float dL = a.L - b.L, da = a.a - b.a, db = a.b - b.b;
    return dL * dL + w.chroma * (da * da + db * db);
}

}  // namespace aic
