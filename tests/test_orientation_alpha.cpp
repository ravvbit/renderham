// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
// EXIF orientation, ImageTransform and alpha blending.

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "aic/aic.hpp"

namespace {

int g_failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << "\n";
        ++g_failures;
    }
}
bool near(float a, float b, float tol = 0.02f) { return std::fabs(a - b) <= tol; }
bool near_color(const aic::RGBf& c, float r, float g, float b, float tol = 0.06f) {
    return near(c.r, r, tol) && near(c.g, g, tol) && near(c.b, b, tol);
}

// Minimal JPEG prefix: SOI + APP1(Exif, TIFF with one IFD0 entry: orientation) + EOI.
std::vector<uint8_t> fake_jpeg(int orientation, bool big_endian, uint16_t type = 3) {
    std::vector<uint8_t> t;  // TIFF block
    auto u16 = [&](uint16_t v) {
        if (big_endian) { t.push_back(v >> 8); t.push_back(v & 0xff); }
        else { t.push_back(v & 0xff); t.push_back(v >> 8); }
    };
    auto u32 = [&](uint32_t v) {
        if (big_endian) { t.push_back(v >> 24); t.push_back((v >> 16) & 0xff); t.push_back((v >> 8) & 0xff); t.push_back(v & 0xff); }
        else { t.push_back(v & 0xff); t.push_back((v >> 8) & 0xff); t.push_back((v >> 16) & 0xff); t.push_back(v >> 24); }
    };
    t.push_back(big_endian ? 'M' : 'I'); t.push_back(big_endian ? 'M' : 'I');
    u16(42); u32(8);
    u16(2);                                   // two entries: an unrelated one first, then orientation
    u16(0x010F); u16(2); u32(1); u32(0);      // Make (ASCII), irrelevant
    u16(0x0112); u16(type); u32(1);
    u16(static_cast<uint16_t>(orientation)); u16(0);  // SHORT value, left-aligned in the 4-byte field
    u32(0);                                   // next IFD
    std::vector<uint8_t> j = {0xFF, 0xD8, 0xFF, 0xE1};
    const size_t len = 2 + 6 + t.size();
    j.push_back(static_cast<uint8_t>(len >> 8)); j.push_back(static_cast<uint8_t>(len & 0xff));
    for (char c : std::string("Exif\0\0", 6)) j.push_back(static_cast<uint8_t>(c));
    j.insert(j.end(), t.begin(), t.end());
    j.push_back(0xFF); j.push_back(0xD9);
    return j;
}

aic::Image make_test_image(int w, int h) {  // every pixel unique
    aic::Image img(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) img.at(x, y) = aic::RGBf{x / 100.f, y / 100.f, 0.5f};
    return img;
}
bool same_pixel(const aic::RGBf& a, const aic::RGBf& b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
bool same_image(const aic::Image& a, const aic::Image& b) {
    if (a.width() != b.width() || a.height() != b.height()) return false;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x)
            if (!same_pixel(a.at(x, y), b.at(x, y))) return false;
    return true;
}
aic::Image sub_image(const aic::Image& src, int x0, int y0, int w, int h) {
    aic::Image out(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) out.at(x, y) = src.at(x0 + x, y0 + y);
    return out;
}

}  // namespace

int main() {
    // ---- EXIF parser ----
    for (int o = 1; o <= 8; ++o) {
        for (bool big : {false, true}) {
            auto j = fake_jpeg(o, big);
            check(aic::parse_exif_orientation(j.data(), j.size()) == o,
                  "orientation " + std::to_string(o) + (big ? " (big endian)" : " (little endian)"));
        }
    }
    {
        auto j = fake_jpeg(9, false);
        check(aic::parse_exif_orientation(j.data(), j.size()) == 1, "value 9 is ignored");
        j = fake_jpeg(0, true);
        check(aic::parse_exif_orientation(j.data(), j.size()) == 1, "value 0 is ignored");
        j = fake_jpeg(6, false, /*type=*/4);  // LONG instead of SHORT: not what the spec says
        check(aic::parse_exif_orientation(j.data(), j.size()) == 1, "wrong tag type is ignored");
        const uint8_t no_exif[] = {0xFF, 0xD8, 0xFF, 0xDA, 0x00, 0x02, 0xFF, 0xD9};
        check(aic::parse_exif_orientation(no_exif, sizeof no_exif) == 1, "JPEG without EXIF -> 1");
        const uint8_t png[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        check(aic::parse_exif_orientation(png, sizeof png) == 1, "non-JPEG -> 1");
        check(aic::parse_exif_orientation(nullptr, 0) == 1, "empty -> 1");
        // Every truncation of a valid block must be safe (no reads beyond `size`).
        auto full = fake_jpeg(6, true);
        bool safe = true;
        for (size_t n = 0; n < full.size(); ++n) {
            std::vector<uint8_t> cut(full.begin(), full.begin() + static_cast<long>(n));
            int r = aic::parse_exif_orientation(cut.data(), cut.size());
            if (r < 1 || r > 8) safe = false;
        }
        check(safe, "truncated data never crashes or returns garbage");
    }

    // ---- ImageTransform ----
    {
        const aic::Image src = make_test_image(5, 3);
        // explicit meaning of a few
        aic::Image r6 = aic::ImageTransform::from_exif(6).apply(src);  // 90 degrees clockwise
        check(r6.width() == 3 && r6.height() == 5, "rotate cw swaps size");
        bool ok6 = true;
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 5; ++x) ok6 = ok6 && same_pixel(r6.at(3 - 1 - y, x), src.at(x, y));
        check(ok6, "orientation 6 = rotate 90 clockwise (top-left goes to top-right)");
        aic::Image r8 = aic::ImageTransform::from_exif(8).apply(src);  // 90 counter-clockwise
        bool ok8 = true;
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 5; ++x) ok8 = ok8 && same_pixel(r8.at(y, 5 - 1 - x), src.at(x, y));
        check(ok8, "orientation 8 = rotate 90 counter-clockwise");
        aic::Image r3 = aic::ImageTransform::from_exif(3).apply(src);
        check(same_pixel(r3.at(0, 0), src.at(4, 2)) && same_pixel(r3.at(4, 2), src.at(0, 0)), "orientation 3 = 180 degrees");
        aic::Image r2 = aic::ImageTransform::from_exif(2).apply(src);
        check(same_pixel(r2.at(0, 1), src.at(4, 1)), "orientation 2 = mirror horizontally");
        aic::Image r4 = aic::ImageTransform::from_exif(4).apply(src);
        check(same_pixel(r4.at(1, 0), src.at(1, 2)), "orientation 4 = mirror vertically");
        aic::Image r5 = aic::ImageTransform::from_exif(5).apply(src);
        check(r5.width() == 3 && same_pixel(r5.at(1, 4), src.at(4, 1)), "orientation 5 = transpose");
        aic::Image r7 = aic::ImageTransform::from_exif(7).apply(src);
        check(r7.width() == 3 && same_pixel(r7.at(3 - 1 - 1, 5 - 1 - 4), src.at(4, 1)), "orientation 7 = anti-transpose");

        // group properties over all 8 x 8 combinations
        std::vector<aic::ImageTransform> all;
        for (int o = 1; o <= 8; ++o) all.push_back(aic::ImageTransform::from_exif(o));
        bool compose_ok = true, inverse_ok = true;
        for (const auto& p : all) {
            inverse_ok = inverse_ok && same_image(p.inverse().apply(p.apply(src)), src);
            for (const auto& q : all)
                compose_ok = compose_ok && same_image(p.then(q).apply(src), q.apply(p.apply(src)));
        }
        check(compose_ok, "then() equals applying one after the other (64 combinations)");
        check(inverse_ok, "inverse() undoes every transform");
        check(aic::ImageTransform::rotate_cw(4).is_identity() && aic::ImageTransform::rotate_cw(-1) == aic::ImageTransform::from_exif(8),
              "rotate_cw wraps modulo 4 and -1 is a counter-clockwise turn");
        check(aic::ImageTransform::rotate_cw(1).then(aic::ImageTransform::rotate_cw(1)) == aic::ImageTransform::from_exif(3),
              "two quarter turns are 180 degrees");

        // map_rect: cropping before or after the transform gives the same pixels
        bool rect_ok = true;
        const int rx = 1, ry = 0, rw = 3, rh = 2;
        for (const auto& t : all) {
            auto m = t.map_rect({double(rx), double(ry), double(rw), double(rh)}, src.width(), src.height());
            aic::Image transformed = t.apply(src);
            aic::Image a = t.apply(sub_image(src, rx, ry, rw, rh));
            aic::Image b = sub_image(transformed, int(std::lround(m.x)), int(std::lround(m.y)), int(std::lround(m.w)),
                                     int(std::lround(m.h)));
            rect_ok = rect_ok && same_image(a, b);
        }
        check(rect_ok, "map_rect follows the pixels for all 8 transforms");
    }

    // ---- loading real files ----
    const std::string dir = RENDERHAM_TESTDATA;
    {
        std::string err;
        auto img = aic::Image::load(dir + "/exif_orient6.jpg", &err);
        check(img.has_value(), "exif_orient6.jpg loads: " + err);
        if (img) {
            check(img->width() == 100 && img->height() == 200, "orientation 6 JPEG is turned upright (100x200)");
            check(near_color(img->at(50, 20), 0.9f, 0.08f, 0.08f), "red half ends up on top");
            check(near_color(img->at(50, 180), 0.08f, 0.08f, 0.9f), "blue half ends up at the bottom");
        }
        aic::LoadOptions raw;
        raw.apply_exif_orientation = false;
        auto un = aic::Image::load(dir + "/exif_orient6.jpg", &err, raw);
        check(un && un->width() == 200 && un->height() == 100, "apply_exif_orientation=false keeps the stored layout");
        check(un && near_color(un->at(20, 50), 0.9f, 0.08f, 0.08f), "stored layout: red on the left");

        auto r3 = aic::Image::load(dir + "/exif_orient3.jpg", &err);
        check(r3 && r3->width() == 120 && r3->height() == 60, "orientation 3 keeps the size");
        check(r3 && near_color(r3->at(110, 50), 0.08f, 0.78f, 0.08f), "180 degrees: the marker moves to the bottom right");
        check(r3 && near_color(r3->at(5, 5), 0.78f, 0.78f, 0.78f), "180 degrees: top left is now plain");
    }
    {
        std::string err;
        auto black = aic::Image::load(dir + "/alpha_samples.png", &err);
        check(black.has_value(), "alpha_samples.png loads: " + err);
        if (black) {
            check(near_color(black->at(0, 0), 0.502f, 0.502f, 0.502f), "white at 50 % alpha over black = mid grey");
            check(near_color(black->at(1, 0), 0.f, 0.f, 0.f), "fully transparent red becomes the background (black), not red");
            check(near_color(black->at(2, 0), 0.f, 0.f, 1.f), "opaque pixel unchanged");
            check(near_color(black->at(3, 0), 0.f, 0.251f, 0.f), "green at 25 % alpha over black");
        }
        aic::LoadOptions white;
        white.alpha_background = aic::RGBf{1.f, 1.f, 1.f};
        auto w = aic::Image::load(dir + "/alpha_samples.png", &err, white);
        if (w) {
            check(near_color(w->at(0, 0), 1.f, 1.f, 1.f), "white at 50 % alpha over white stays white");
            check(near_color(w->at(1, 0), 1.f, 1.f, 1.f), "fully transparent -> white background");
            check(near_color(w->at(3, 0), 0.749f, 1.f, 0.749f), "green at 25 % alpha over white");
        }
    }

    // ---- the pipeline applies ConversionOptions::source_transform ----
    {
        // 80x40: red | blue on top, green | yellow below
        aic::Image quad(80, 40);
        for (int y = 0; y < 40; ++y)
            for (int x = 0; x < 80; ++x) {
                const bool right = x >= 40, bottom = y >= 20;
                quad.at(x, y) = bottom ? (right ? aic::RGBf{1, 1, 0} : aic::RGBf{0, 1, 0})
                                       : (right ? aic::RGBf{0, 0, 1} : aic::RGBf{1, 0, 0});
            }
        const std::string path = std::string(RENDERHAM_TMPDIR) + "/quad.png";
        check(quad.save_png(path), "quad test image written");

        auto corners = [&](const aic::ImageTransform& t) {  // TL TR BL BR as letters
            aic::ConversionOptions o;
            o.target_width = o.target_height = 64;
            o.fit_mode = aic::FitMode::Stretch;
            o.ditherer_name = "none";
            o.keep_canvas = true;
            o.source_transform = t;
            aic::ConversionPipeline pipeline;
            auto pv = pipeline.preview(path, o);
            std::string out;
            if (!pv.success) return std::string("error: ") + pv.error_message;
            auto letter = [](const aic::RGBf& c) {
                if (c.r > .5f && c.g < .5f && c.b < .5f) return 'R';
                if (c.r < .5f && c.g < .5f && c.b > .5f) return 'B';
                if (c.r < .5f && c.g > .5f && c.b < .5f) return 'G';
                if (c.r > .5f && c.g > .5f && c.b < .5f) return 'Y';
                return '?';
            };
            const aic::Image& cv = pv.canvas;
            const int w = cv.width(), h = cv.height();
            for (auto [x, y] : {std::pair{8, 8}, {w - 9, 8}, {8, h - 9}, {w - 9, h - 9}}) out += letter(cv.at(x, y));
            return out;
        };
        check(corners({}) == "RBGY", "no transform: RBGY, got " + corners({}));
        check(corners(aic::ImageTransform::rotate_cw(1)) == "GRYB", "rotate right: GRYB, got " + corners(aic::ImageTransform::rotate_cw(1)));
        check(corners(aic::ImageTransform::rotate_cw(2)) == "YGBR", "rotate 180: YGBR");
        check(corners(aic::ImageTransform::rotate_cw(3)) == "BYRG", "rotate left: BYRG");
        check(corners(aic::ImageTransform::flip_horizontal()) == "BRYG", "mirror horizontally: BRYG");
        check(corners(aic::ImageTransform::flip_vertical()) == "GYRB", "mirror vertically: GYRB");
        check(corners(aic::ImageTransform::flip_horizontal().then(aic::ImageTransform::rotate_cw(1))) == "YBGR",
              "mirror, then rotate right: YBGR");
    }

    if (g_failures) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "orientation/alpha: all checks passed\n";
    return 0;
}
