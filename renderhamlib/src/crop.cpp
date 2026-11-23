// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/crop.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <regex>

namespace aic {
namespace {

bool parse_value(const std::string& s, CropValue& out) {
    if (s.empty()) return false;
    std::string num = s;
    bool pct = false;
    if (num.back() == '%') {
        pct = true;
        num.pop_back();
    }
    if (num.empty()) return false;
    try {
        size_t consumed = 0;
        float v = std::stof(num, &consumed);
        if (consumed != num.size()) return false;
        out.value = v;
        out.is_percent = pct;
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

std::optional<CropSpec> parse_crop_spec(const std::string& text, std::string* error_out) {
    // WxH+X+Y - W/H: non-negative, optional '%'; X/Y: mandatory sign, optional '%'.
    static const std::regex re(
        R"(^\s*(\d+(?:\.\d+)?%?)x(\d+(?:\.\d+)?%?)([+-]\d+(?:\.\d+)?%?)([+-]\d+(?:\.\d+)?%?)\s*$)");
    std::smatch m;
    if (!std::regex_match(text, m, re)) {
        if (error_out) {
            *error_out = "malformed crop geometry '" + text +
                         "' (expected WxH+X+Y, e.g. 320x200+10+5 or 50%x50%+25%+25%)";
        }
        return std::nullopt;
    }

    CropSpec spec;
    if (!parse_value(m[1].str(), spec.width) || !parse_value(m[2].str(), spec.height) ||
        !parse_value(m[3].str(), spec.x) || !parse_value(m[4].str(), spec.y)) {
        if (error_out) *error_out = "malformed crop geometry '" + text + "'";
        return std::nullopt;
    }
    if (spec.width.value <= 0.f || spec.height.value <= 0.f) {
        if (error_out) *error_out = "crop width/height must be positive in '" + text + "'";
        return std::nullopt;
    }
    return spec;
}

std::optional<CropBox> resolve_crop_box(int src_width, int src_height, const CropSpec& spec) {
    auto resolve = [](const CropValue& v, int dim) -> float {
        return v.is_percent ? (v.value / 100.f) * static_cast<float>(dim) : v.value;
    };

    CropBox box;
    box.x = static_cast<int>(std::lround(resolve(spec.x, src_width)));
    box.y = static_cast<int>(std::lround(resolve(spec.y, src_height)));
    box.width = static_cast<int>(std::lround(resolve(spec.width, src_width)));
    box.height = static_cast<int>(std::lround(resolve(spec.height, src_height)));
    if (box.width <= 0 || box.height <= 0) return std::nullopt;
    return box;
}

std::optional<Image> crop_image(const Image& src, const CropSpec& spec, RGBf fill_color, std::string* error_out) {
    const std::optional<CropBox> box = resolve_crop_box(src.width(), src.height(), spec);
    if (!box) {
        if (error_out) *error_out = "crop width/height resolved to zero or negative";
        return std::nullopt;
    }
    const int x0 = box->x, y0 = box->y, cw = box->width, ch = box->height;

    // Deliberately no clamping of x0/y0/cw/ch against the source
    // bounds here - the box may extend past any edge, or lie entirely
    // outside the source. Each output pixel either copies its
    // corresponding source pixel, or - if that source coordinate falls
    // outside the actual source image - gets `fill_color` instead.
    Image out(cw, ch);
    for (int y = 0; y < ch; ++y) {
        const int sy = y0 + y;
        const bool y_in = sy >= 0 && sy < src.height();
        for (int x = 0; x < cw; ++x) {
            const int sx = x0 + x;
            out.at(x, y) = (y_in && sx >= 0 && sx < src.width()) ? src.at(sx, sy) : fill_color;
        }
    }
    return out;
}

}  // namespace aic
