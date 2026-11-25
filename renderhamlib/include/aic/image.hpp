// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/image.hpp
//
// Deliberately "dumb": a width/height buffer of gamma-encoded (sRGB)
// float pixels in [0,1], plus file I/O. It knows nothing about Amiga,
// HAM, palettes or dithering - those live in their own modules so this
// class stays reusable (e.g. for a future GUI's preview).

#include <string>
#include <vector>
#include <optional>

#include "aic/color_space.hpp"

namespace aic {

// What load() does besides decoding the file.
struct LoadOptions {
    // JPEG only: turn the pixels upright according to the EXIF orientation tag
    // (phones store portrait photos sideways and only set the tag). What every
    // image viewer does, so it is on by default.
    bool apply_exif_orientation = true;

    // Pixels with alpha < 1 are blended over this color (gamma-encoded sRGB,
    // blended in encoded space like web browsers and image viewers do). Without
    // it the hidden RGB values of transparent pixels showed through - a
    // fully transparent "red" pixel came out red. Black matches the Amiga
    // border (palette color 0); only used where the file has an alpha channel.
    RGBf alpha_background{0.f, 0.f, 0.f};
};

// One kind of image file that Image::load() reads. This table is the single list of what the
// programs offer: the GUI builds its "Open" filter from it, and the desktop entry of the Linux
// packages (MimeType=, packaging/linux/) must name exactly these MIME types - a test checks that.
//
// stb_image can read a few more (HDR, Softimage PIC, ASCII PNM): they are left out on purpose,
// since HDR pictures would come out clipped to the 0..1 range and the others are hardly used.
struct ImageFormat {
    const char* name;                      // for people: "PNG", "Photoshop (flattened)"
    std::vector<std::string> extensions;   // lower case, without the dot
    const char* mime_type;                 // shared-mime-info name
};

// The formats Image::load() is meant to read (and keeps working for, see tests/test_formats.cpp).
const std::vector<ImageFormat>& supported_image_formats();

class Image {
public:
    Image() = default;
    Image(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }
    bool empty() const { return width_ <= 0 || height_ <= 0; }

    RGBf& at(int x, int y);
    const RGBf& at(int x, int y) const;

    // Loads an image file (the formats of supported_image_formats(): PNG, JPEG, BMP, TGA, GIF
    // (first frame), Photoshop PSD (the flattened picture), binary PPM/PGM), applying
    // `options` (EXIF orientation, alpha background). The format is recognized from the file's
    // content, not from its name.
    // Returns std::nullopt and fills error_out on failure.
    static std::optional<Image> load(const std::string& path, std::string* error_out = nullptr,
                                     const LoadOptions& options = {});

    // Writes a PNG purely for debugging/preview purposes (not part of the
    // Amiga pipeline).
    bool save_png(const std::string& path) const;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<RGBf> pixels_;
};

}  // namespace aic
