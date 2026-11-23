// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/palette.hpp

#include <cstdint>
#include <string>
#include <vector>

#include "aic/color_space.hpp"
#include "aic/image.hpp"

namespace aic {

// A single OCS color register: 4 bits per channel (0-15), i.e. one of the
// chipset's 4096 representable colors (12-bit RGB / "RGB444").
struct AmigaColor {
    uint8_t r4 = 0, g4 = 0, b4 = 0;

    bool operator==(const AmigaColor& o) const { return r4 == o.r4 && g4 == o.g4 && b4 == o.b4; }

    // sRGB [0,1] float, for preview/PNG export and OKLab distance calc.
    //
    // Uses nibble replication (v*17, so 15 -> 255 exactly) rather than
    // left-shift (v<<4, so 15 -> 240) to map the 4-bit register value to
    // 8 bits. This is a PC-side display convention only - real OCS
    // hardware has no such conversion, it drives the DAC directly from
    // the 4-bit value, so it's unaffected either way. It matters here
    // because it's applied uniformly to every one of the 4096 candidate
    // colors during HAM6 encoding, so it doesn't bias which candidate
    // wins; it only affects how --preview (and other *rendering* tools)
    // draw the result. Cross-checking against netpbm's ilbmtoppm showed
    // it uses v<<4 for HAM "modify" pixel values specifically, so a
    // side-by-side comparison of --preview against ilbmtoppm's output
    // will show small (a few percent) brightness differences on
    // modified channels - that's this convention difference, not a
    // decoding bug (verified bit-exact against a from-scratch row-aware
    // ByteRun1 decompressor first).
    RGBf to_srgb() const {
        return RGBf{r4 / 15.f, g4 / 15.f, b4 / 15.f};
    }
};

// 4096-entry lookup table (index = r4<<8|g4<<4|b4) mapping every
// representable Amiga color to its OKLab equivalent. A function-local
// static in an inline function is guaranteed to be the single, shared
// instance across every translation unit that includes this header, so
// this is built exactly once per process despite living in a header.
inline const std::vector<OKLab>& amiga_lab_lut() {
    static const std::vector<OKLab> lut = [] {
        std::vector<OKLab> t(16 * 16 * 16);
        for (int r = 0; r < 16; ++r)
            for (int g = 0; g < 16; ++g)
                for (int b = 0; b < 16; ++b) {
                    AmigaColor c{static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)};
                    t[(r << 8) | (g << 4) | b] = srgb_to_oklab(c.to_srgb());
                }
        return t;
    }();
    return lut;
}

inline int amiga_lut_index(const AmigaColor& c) { return (c.r4 << 8) | (c.g4 << 4) | c.b4; }

// Finds the AmigaColor whose OKLab value is closest to `target`, by
// exhaustive search over all 4096 representable colors (cheap: this
// runs a handful of times per palette fit, not per pixel).
//
// This replaced an earlier version that rounded each sRGB channel to
// its nearest 4-bit value independently. That's NOT equivalent to
// minimizing OKLab distance, because OKLab is a nonlinear transform of
// RGB - independent per-channel rounding in sRGB space doesn't reliably
// land on the true nearest color in the space every other distance
// comparison in this codebase (candidate search, importance weighting,
// quality reporting) actually uses. Since the encoder already builds
// this exact LUT for its own per-pixel candidate search, reusing it
// here makes palette-color selection consistent with everything else
// instead of quietly using a different, weaker metric.
inline AmigaColor quantize_oklab_to_amiga(const OKLab& target, const PerceptualWeights& weights = {}) {
    const auto& lut = amiga_lab_lut();
    int best_idx = 0;
    float best_dist = weighted_distance2(target, lut[0], weights);
    for (int i = 1; i < 4096; ++i) {
        float d = weighted_distance2(target, lut[i], weights);
        if (d < best_dist) {
            best_dist = d;
            best_idx = i;
        }
    }
    return AmigaColor{static_cast<uint8_t>((best_idx >> 8) & 0xF), static_cast<uint8_t>((best_idx >> 4) & 0xF),
                       static_cast<uint8_t>(best_idx & 0xF)};
}

struct Palette {
    std::vector<AmigaColor> colors;  // 16 entries for HAM6's base palette
};

// Strategy interface: anything that can turn an image into an N-color
// Amiga palette. Swap implementations without touching the encoder.
//
// importance_weights.size() must equal image.width()*image.height()
// (row-major). Pass a uniform vector of 1s for unweighted behavior -
// implementations are expected to degenerate to their old unweighted
// form in that case. perceptual_weights governs how a computed target
// color gets rounded to a storable AmigaColor (see
// quantize_oklab_to_amiga above) - pass {} for the standard/default
// unweighted metric.
class IPaletteQuantizer {
public:
    virtual ~IPaletteQuantizer() = default;
    virtual Palette quantize(const Image& image, int num_colors, const std::vector<float>& importance_weights,
                              const PerceptualWeights& perceptual_weights) const = 0;
    virtual std::string name() const = 0;
};

}  // namespace aic
