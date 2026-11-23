// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/ham_maps.hpp"

#include <algorithm>
#include <cmath>

namespace aic {

Image make_ham_map(const HAM6Frame& frame) {
    Image map(frame.width, frame.height);
    for (int y = 0; y < frame.height; ++y) {
        for (int x = 0; x < frame.width; ++x) {
            const uint8_t code = frame.pixel_codes[static_cast<size_t>(y) * frame.width + x];
            // Same mode numbering as apply_code() in ham6_encoder.cpp:
            // 0 = Select, 1 = modify blue, 2 = modify red, 3 = modify green.
            switch ((code >> 4) & 3) {
                case 1: map.at(x, y) = RGBf{0.f, 0.f, 1.f}; break;
                case 2: map.at(x, y) = RGBf{1.f, 0.f, 0.f}; break;
                case 3: map.at(x, y) = RGBf{0.f, 1.f, 0.f}; break;
                default: map.at(x, y) = RGBf{0.f, 0.f, 0.f}; break;
            }
        }
    }
    return map;
}

Image make_error_map(const Image& reference, const Image& decoded) {
    if (reference.empty() || decoded.empty() || reference.width() != decoded.width() ||
        reference.height() != decoded.height()) {
        return Image();
    }
    Image map(decoded.width(), decoded.height());
    for (int y = 0; y < decoded.height(); ++y) {
        for (int x = 0; x < decoded.width(); ++x) {
            float d = std::sqrt(distance2(srgb_to_oklab(reference.at(x, y)), srgb_to_oklab(decoded.at(x, y))));
            float v = std::min(1.f, d / kErrorMapFullScale);
            map.at(x, y) = RGBf{v, v, v};
        }
    }
    return map;
}

}  // namespace aic
