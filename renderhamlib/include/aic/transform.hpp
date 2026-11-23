// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/transform.hpp - the eight ways to rotate/mirror an image by multiples
// of 90 degrees (the dihedral group D4), as used by EXIF orientation and by
// the user's rotate/flip commands.
//
// A transform is a 2x2 integer matrix acting on pixel coordinates; the
// translation that keeps the result inside the image is derived from the
// image size when it is applied. That makes composition a matrix product and
// lets the very same object move rectangles (crop boxes, animation
// keyframes) together with the image.

#include "aic/image.hpp"

namespace aic {

struct ImageTransform {
    // (x', y') = (a*x + b*y, c*x + d*y) + translation. Identity by default.
    int a = 1, b = 0, c = 0, d = 1;

    // EXIF orientation tag value 1..8 -> the transform that makes the stored
    // pixels upright (6 = rotate 90 degrees clockwise, 3 = 180, 8 = 90 counter-
    // clockwise, 2/4 = mirror horizontally/vertically, 5/7 = the two diagonal
    // mirrors). Any other value gives the identity.
    static ImageTransform from_exif(int orientation);

    static ImageTransform rotate_cw(int quarter_turns);  // any integer, taken modulo 4
    static ImageTransform flip_horizontal();             // left <-> right
    static ImageTransform flip_vertical();               // top <-> bottom

    bool is_identity() const { return a == 1 && b == 0 && c == 0 && d == 1; }
    bool swaps_axes() const { return b != 0; }  // width and height trade places

    // "this, then next".
    ImageTransform then(const ImageTransform& next) const;
    ImageTransform inverse() const;

    bool operator==(const ImageTransform& o) const { return a == o.a && b == o.b && c == o.c && d == o.d; }

    // Size of the result for a source of w x h.
    int result_width(int w, int h) const { return swaps_axes() ? h : w; }
    int result_height(int w, int h) const { return swaps_axes() ? w : h; }

    Image apply(const Image& src) const;

    // Maps a rectangle given in continuous source coordinates (x, y = top left
    // corner, edges between pixels) of a source_w x source_h image to the
    // rectangle covering the same area of the transformed image.
    struct Rect {
        double x = 0, y = 0, w = 0, h = 0;
    };
    Rect map_rect(const Rect& r, int source_w, int source_h) const;
};

}  // namespace aic
