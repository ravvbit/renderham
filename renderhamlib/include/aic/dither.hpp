// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/dither.hpp
//
// Deliberately mode-agnostic: this interface does NOT know about HAM6,
// palettes, or Amiga hardware. It only answers two questions the encoder
// asks it while walking the image left-to-right, top-to-bottom:
//   1. bias(x,y)      - what should be added to the source color at (x,y)
//                        before the encoder searches for the best
//                        reproducible color (accumulated diffusion error,
//                        or a fixed ordered-dither threshold offset)?
//   2. feedback(x,y,e) - here is the residual error (target - what we
//                        actually managed to reproduce); do with it what
//                        your algorithm does (diffuse it, or ignore it).
//
// This is what keeps quantization, dithering and HAM6-encoding a SINGLE
// combined per-pixel decision (see ham6_encoder.hpp) while still letting
// the diffusion *strategy* be swapped freely - the encoder is the only
// place that is inherently HAM-sequential, not the dither math itself.

#include <string>

#include "aic/color_space.hpp"

namespace aic {

class IErrorDiffuser {
public:
    virtual ~IErrorDiffuser() = default;

    // Called once before processing an image of this size.
    virtual void begin(int width, int height) = 0;

    // Called at the start of every scanline (HAM6's state resets per row).
    virtual void begin_row(int /*y*/) {}

    // Bias to add to the source pixel's OKLab value before candidate search.
    virtual OKLab bias(int x, int y) const = 0;

    // What the encoder actually calls: the color to search candidates for,
    // given the source pixel. The default is simply target + bias(x,y),
    // which is all that diffusion and the classic ordered ditherers need.
    // Override it when the adjustment must depend on the pixel itself - e.g.
    // an offset defined per color channel on the 4-bit Amiga lattice, which
    // a pixel-blind OKLab bias cannot express (see dithering/checks.hpp).
    virtual OKLab adjust_target(int x, int y, const OKLab& target) const { return target + bias(x, y); }

    // Residual error (biased_target - actually_reproduced) after the
    // encoder made its choice for (x,y).
    virtual void feedback(int x, int y, const OKLab& error) = 0;

    virtual std::string name() const = 0;

    // Overall strength of the dithering effect, 0..1 (1 = the algorithm's
    // own default behavior, 0 = no dithering effect at all). What it
    // scales depends on the family: error diffusers scale how much of the
    // residual error is propagated to neighbors (a "propagation factor" -
    // lower values trade dither accuracy for less pixel-to-pixel noise,
    // which in HAM6 also means fewer forced multi-channel jumps); ordered
    // ditherers scale the amplitude of their threshold pattern. Ignored by
    // ditherers with nothing to scale (e.g. "none").
    virtual void set_strength(float /*strength*/) {}

    // Shifts an ordered ditherer's pattern: the threshold used for pixel (x,y) is the one
    // the pattern has at (x+ox, y+oy). Everything else about the pattern stays as it is. A
    // sequence of frames that pans over one picture uses it to make the dots travel with
    // the picture instead of staying fixed to the screen (see
    // ConversionOptions::anchor_dither_to_source). Only ditherers whose bias depends on
    // the pixel position alone can honor it - Bayer, blue-noise and the checks family.
    // Error diffusion cannot: its noise is a product of the picture content, so it has no
    // pattern to shift; those ditherers ignore the call (default). Call before begin().
    virtual void set_pattern_origin(int /*ox*/, int /*oy*/) {}
};

}  // namespace aic
