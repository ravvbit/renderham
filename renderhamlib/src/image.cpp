// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/image.hpp"

#include <algorithm>
#include <cstddef>

#include "aic/exif.hpp"
#include "aic/transform.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace aic {

const std::vector<ImageFormat>& supported_image_formats() {
    static const std::vector<ImageFormat> formats = {
        {"PNG", {"png"}, "image/png"},
        {"JPEG", {"jpg", "jpeg"}, "image/jpeg"},
        {"BMP", {"bmp"}, "image/bmp"},
        {"TGA", {"tga"}, "image/x-tga"},
        {"GIF (first frame)", {"gif"}, "image/gif"},
        {"Photoshop PSD (flattened picture)", {"psd"}, "image/vnd.adobe.photoshop"},
        {"PPM (binary)", {"ppm"}, "image/x-portable-pixmap"},
        {"PGM (binary)", {"pgm"}, "image/x-portable-graymap"},
    };
    return formats;
}

Image::Image(int width, int height)
    : width_(width), height_(height), pixels_(static_cast<size_t>(width) * height) {}

RGBf& Image::at(int x, int y) { return pixels_[static_cast<size_t>(y) * width_ + x]; }
const RGBf& Image::at(int x, int y) const { return pixels_[static_cast<size_t>(y) * width_ + x]; }

std::optional<Image> Image::load(const std::string& path, std::string* error_out, const LoadOptions& options) {
    int w = 0, h = 0, channels = 0;
    // Always decode to RGBA so the alpha channel can be blended explicitly
    // (forcing 3 channels just dropped it). `channels` still reports what the
    // file really had.
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data) {
        if (error_out) *error_out = stbi_failure_reason() ? stbi_failure_reason() : "unknown stb_image error";
        return std::nullopt;
    }
    const bool has_alpha = (channels == 2 || channels == 4);
    const RGBf bg = options.alpha_background;

    Image img(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const unsigned char* p = data + (static_cast<size_t>(y) * w + x) * 4;
            RGBf c{p[0] / 255.f, p[1] / 255.f, p[2] / 255.f};
            if (has_alpha && p[3] != 255) {
                const float a = p[3] / 255.f;
                c = RGBf{c.r * a + bg.r * (1.f - a), c.g * a + bg.g * (1.f - a), c.b * a + bg.b * (1.f - a)};
            }
            img.at(x, y) = c;
        }
    }
    stbi_image_free(data);

    if (options.apply_exif_orientation) {
        const int orientation = read_exif_orientation(path);
        if (orientation != 1) return ImageTransform::from_exif(orientation).apply(img);
    }
    return img;
}

bool Image::save_png(const std::string& path) const {
    if (empty()) return false;
    std::vector<unsigned char> buf(static_cast<size_t>(width_) * height_ * 3);
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const RGBf& c = at(x, y);
            size_t idx = (static_cast<size_t>(y) * width_ + x) * 3;
            buf[idx + 0] = static_cast<unsigned char>(std::clamp(c.r, 0.f, 1.f) * 255.f + 0.5f);
            buf[idx + 1] = static_cast<unsigned char>(std::clamp(c.g, 0.f, 1.f) * 255.f + 0.5f);
            buf[idx + 2] = static_cast<unsigned char>(std::clamp(c.b, 0.f, 1.f) * 255.f + 0.5f);
        }
    }
    return stbi_write_png(path.c_str(), width_, height_, 3, buf.data(), width_ * 3) != 0;
}

}  // namespace aic
