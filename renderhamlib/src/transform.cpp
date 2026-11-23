// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/transform.hpp"

#include <algorithm>
#include <cmath>

namespace aic {

ImageTransform ImageTransform::from_exif(int orientation) {
    switch (orientation) {
        case 2: return {-1, 0, 0, 1};   // mirror horizontally
        case 3: return {-1, 0, 0, -1};  // 180 degrees
        case 4: return {1, 0, 0, -1};   // mirror vertically
        case 5: return {0, 1, 1, 0};    // transpose
        case 6: return {0, -1, 1, 0};   // 90 clockwise
        case 7: return {0, -1, -1, 0};  // transverse
        case 8: return {0, 1, -1, 0};   // 90 counter-clockwise
        default: return {};
    }
}

ImageTransform ImageTransform::rotate_cw(int quarter_turns) {
    switch (((quarter_turns % 4) + 4) % 4) {
        case 1: return {0, -1, 1, 0};
        case 2: return {-1, 0, 0, -1};
        case 3: return {0, 1, -1, 0};
        default: return {};
    }
}

ImageTransform ImageTransform::flip_horizontal() { return {-1, 0, 0, 1}; }
ImageTransform ImageTransform::flip_vertical() { return {1, 0, 0, -1}; }

ImageTransform ImageTransform::then(const ImageTransform& n) const {
    // next * this
    return {n.a * a + n.b * c, n.a * b + n.b * d, n.c * a + n.d * c, n.c * b + n.d * d};
}

ImageTransform ImageTransform::inverse() const {
    // Orthogonal integer matrix with determinant +-1: inverse = adj / det.
    int det = a * d - b * c;
    return {d * det, -b * det, -c * det, a * det};
}

Image ImageTransform::apply(const Image& src) const {
    if (is_identity() || src.empty()) return src;
    const int w = src.width(), h = src.height();
    Image out(result_width(w, h), result_height(w, h));
    // Pixel-index space: the translation makes the smallest transformed index 0.
    const int tx = -(std::min(0, a * (w - 1)) + std::min(0, b * (h - 1)));
    const int ty = -(std::min(0, c * (w - 1)) + std::min(0, d * (h - 1)));
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            out.at(a * x + b * y + tx, c * x + d * y + ty) = src.at(x, y);
        }
    }
    return out;
}

ImageTransform::Rect ImageTransform::map_rect(const Rect& r, int source_w, int source_h) const {
    // Continuous coordinates: the image occupies [0, W] x [0, H].
    const double tx = -(std::min(0.0, a * double(source_w)) + std::min(0.0, b * double(source_h)));
    const double ty = -(std::min(0.0, c * double(source_w)) + std::min(0.0, d * double(source_h)));
    auto px = [&](double x, double y) { return a * x + b * y + tx; };
    auto py = [&](double x, double y) { return c * x + d * y + ty; };
    const double x0 = px(r.x, r.y), x1 = px(r.x + r.w, r.y + r.h);
    const double y0 = py(r.x, r.y), y1 = py(r.x + r.w, r.y + r.h);
    return {std::min(x0, x1), std::min(y0, y1), std::fabs(x1 - x0), std::fabs(y1 - y0)};
}

}  // namespace aic
