// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
// Tests for aic::canvas_mapping (aic/canvas_mapping.hpp): where a point of the source lands on
// the canvas the pipeline builds.
//
// The mapping is checked against the real thing: a bright square is put at a known place of a
// dark source, the pipeline converts it, and the centre of the bright area on the resulting canvas
// must be where the mapping says the square's centre goes - for every scaling mode, with and
// without a crop box, with a box that reaches past the source, and with non-square pixels.
// On top of that: the map is its own inverse, zoom ratios follow the scale, and a source or
// crop box without area has no mapping.
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>

#include "aic/aic.hpp"

using namespace aic;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("  %s  %s\n", ok ? "OK  " : "FAIL", what.c_str());
    if (!ok) ++g_failures;
}

constexpr int kSourceW = 300, kSourceH = 200;

// Dark picture with one bright square, `size` pixels wide, whose top-left corner is (x0, y0).
Image marker_source(int x0, int y0, int size) {
    Image img(kSourceW, kSourceH);
    for (int y = 0; y < kSourceH; ++y)
        for (int x = 0; x < kSourceW; ++x) img.at(x, y) = RGBf{0.f, 0.f, 0.f};
    for (int y = y0; y < y0 + size; ++y)
        for (int x = x0; x < x0 + size; ++x)
            if (x >= 0 && y >= 0 && x < kSourceW && y < kSourceH) img.at(x, y) = RGBf{1.f, 1.f, 1.f};
    return img;
}

CropSpec pixel_crop(float x, float y, float w, float h) {
    CropSpec c;
    c.x = {x, false};
    c.y = {y, false};
    c.width = {w, false};
    c.height = {h, false};
    return c;
}

// Centre of the bright area of the canvas, weighted by brightness.
bool canvas_centroid(const Image& canvas, double* cx, double* cy) {
    double sum = 0, sx = 0, sy = 0;
    for (int y = 0; y < canvas.height(); ++y)
        for (int x = 0; x < canvas.width(); ++x) {
            const double v = canvas.at(x, y).g;
            sum += v;
            sx += v * (x + 0.5);
            sy += v * (y + 0.5);
        }
    if (sum < 1.0) return false;
    *cx = sx / sum;
    *cy = sy / sum;
    return true;
}

struct Case {
    const char* name;
    int target_w, target_h;
    FitMode mode;
    float pixel_aspect;
    std::optional<CropSpec> crop;
};

// Converts the marker with the case's options and compares where it lands with the mapping.
void check_against_pipeline(const Case& c, int marker_x, int marker_y, int marker_size) {
    const Image source = marker_source(marker_x, marker_y, marker_size);

    ConversionOptions opts;
    opts.target_width = c.target_w;
    opts.target_height = c.target_h;
    opts.fit_mode = c.mode;
    opts.pixel_aspect_ratio = c.pixel_aspect;
    opts.crop = c.crop;
    opts.keep_canvas = true;
    const PreviewResult result = ConversionPipeline().preview_source(source, opts);
    check(result.success, std::string(c.name) + ": the pipeline converts the picture");
    if (!result.success) return;

    ResizeOptions resize;
    resize.target_width = c.target_w;
    resize.target_height = c.target_h;
    resize.mode = c.mode;
    resize.pixel_aspect_ratio = c.pixel_aspect;
    const std::optional<CanvasMapping> m = canvas_mapping(kSourceW, kSourceH, c.crop, resize);
    check(m.has_value(), std::string(c.name) + ": there is a mapping");
    if (!m) return;

    double cx = 0, cy = 0;
    if (!canvas_centroid(result.canvas, &cx, &cy)) {
        check(false, std::string(c.name) + ": the marker is visible on the canvas");
        return;
    }
    const CanvasPoint expected = m->to_canvas({marker_x + marker_size / 2.0, marker_y + marker_size / 2.0});
    const double dx = cx - expected.x, dy = cy - expected.y;
    char text[200];
    std::snprintf(text, sizeof text, "%s: marker at (%.2f, %.2f), mapping says (%.2f, %.2f)", c.name, cx, cy,
                  expected.x, expected.y);
    check(std::fabs(dx) < 0.6 && std::fabs(dy) < 0.6, text);
}

void test_against_pipeline() {
    std::printf("the mapping agrees with where the pipeline puts a marker\n");
    // The marker sits left of and above the middle, so a swapped axis or a wrong sign shows.
    const int mx = 70, my = 40, ms = 40;

    check_against_pipeline({"fit, picture wider than canvas", 128, 64, FitMode::Fit, 1.f, std::nullopt}, mx, my, ms);
    check_against_pipeline({"fit, canvas wider (pillarbox)", 160, 64, FitMode::Fit, 1.f, std::nullopt}, mx, my, ms);
    check_against_pipeline({"fill, overhang at top and bottom", 128, 64, FitMode::Fill, 1.f, std::nullopt}, mx, my, ms);
    // Here only the middle third of the width is on the canvas, so the marker moves there.
    check_against_pipeline({"fill, overhang at the sides", 64, 128, FitMode::Fill, 1.f, std::nullopt}, 130, 60, ms);
    check_against_pipeline({"stretch", 128, 64, FitMode::Stretch, 1.f, std::nullopt}, mx, my, ms);
    check_against_pipeline({"fit, non-square pixels (1.2)", 128, 64, FitMode::Fit, 1.2f, std::nullopt}, mx, my, ms);
    check_against_pipeline({"fit, non-square pixels (0.5)", 128, 128, FitMode::Fit, 0.5f, std::nullopt}, mx, my, ms);
    check_against_pipeline({"crop box, fit", 128, 64, FitMode::Fit, 1.f, pixel_crop(40, 20, 150, 100)}, mx, my, ms);
    check_against_pipeline({"crop box, fill", 128, 64, FitMode::Fill, 1.f, pixel_crop(40, 20, 150, 100)}, mx, my, ms);
    check_against_pipeline({"crop box, stretch", 128, 64, FitMode::Stretch, 1.f, pixel_crop(40, 20, 150, 100)}, mx, my,
                           ms);
    check_against_pipeline({"crop box in percent", 128, 64, FitMode::Fit, 1.f, [] {
                                CropSpec c;
                                c.x = {10.f, true};
                                c.y = {5.f, true};
                                c.width = {60.f, true};
                                c.height = {80.f, true};
                                return std::optional<CropSpec>(c);
                            }()},
                           mx, my, ms);
    // The box starts left of and above the source: those parts are filled, and the marker still
    // has to land where the shifted box puts it.
    check_against_pipeline({"crop box reaching past the top left corner", 128, 64, FitMode::Fit, 1.f,
                            pixel_crop(-30, -20, 200, 130)},
                           10, 10, 30);
    check_against_pipeline({"crop box reaching past the bottom right corner", 128, 64, FitMode::Fill, 1.f,
                            pixel_crop(150, 100, 200, 130)},
                           250, 150, 30);
}

void test_round_trip_and_zoom() {
    std::printf("round trip and zoom\n");
    ResizeOptions resize;
    resize.target_width = 320;
    resize.target_height = 256;
    resize.mode = FitMode::Fit;
    resize.pixel_aspect_ratio = 1.1f;
    const auto m = canvas_mapping(4000, 3000, pixel_crop(500, 250, 2400, 1900), resize);
    check(m.has_value(), "mapping for a large source with a crop box");
    if (!m) return;

    const CanvasPoint points[] = {{0, 0}, {500, 250}, {1234.5, 987.25}, {3999.5, 2999.5}, {-100, -50}};
    bool round_trip_ok = true;
    for (const CanvasPoint& p : points) {
        const CanvasPoint back = m->to_source(m->to_canvas(p));
        if (std::fabs(back.x - p.x) > 1e-9 || std::fabs(back.y - p.y) > 1e-9) round_trip_ok = false;
    }
    check(round_trip_ok, "to_source undoes to_canvas");

    // The crop box's corners land on the corners of the scaled picture.
    const ResizeGeometry g = resize_geometry(2400, 1900, resize);
    const CanvasPoint top_left = m->to_canvas({500, 250});
    const CanvasPoint bottom_right = m->to_canvas({500 + 2400, 250 + 1900});
    check(std::fabs(top_left.x - g.offset_x) < 1e-9 && std::fabs(top_left.y - g.offset_y) < 1e-9,
          "the crop box's top left corner lands at the offset");
    check(std::fabs(bottom_right.x - (g.offset_x + g.scaled_width)) < 1e-9 &&
              std::fabs(bottom_right.y - (g.offset_y + g.scaled_height)) < 1e-9,
          "the crop box's bottom right corner lands at offset + scaled size");

    // The overload for ConversionOptions reads the same fields the pipeline does.
    {
        ConversionOptions opts;
        opts.target_width = resize.target_width;
        opts.target_height = resize.target_height;
        opts.fit_mode = resize.mode;
        opts.pixel_aspect_ratio = resize.pixel_aspect_ratio;
        opts.crop = pixel_crop(500, 250, 2400, 1900);
        const auto from_options = canvas_mapping(4000, 3000, opts);
        check(from_options.has_value() && *from_options == *m, "the overload for ConversionOptions gives the same mapping");
        opts.crop.reset();
        const auto uncropped = canvas_mapping(4000, 3000, opts);
        check(uncropped.has_value() && *uncropped != *m, "and it follows the crop");
    }

    check(std::fabs(m->zoom_to_source(m->zoom_to_canvas(0.37)) - 0.37) < 1e-12, "zoom conversion round-trips");
    // A picture shown at 0.2 screen pixels per source pixel, scaled to a canvas that has fewer
    // pixels per source pixel, needs a proportionally larger canvas zoom to look the same size.
    check(m->scale_x < 1.0 && m->zoom_to_canvas(0.2) > 0.2, "a downscaled canvas is shown larger to look the same size");
    check(std::fabs(m->zoom_to_canvas(0.2) * m->scale_x - 0.2) < 1e-12,
          "one source pixel is the same number of screen pixels in both views");
}

void test_no_mapping() {
    std::printf("no mapping without area\n");
    ResizeOptions resize;
    check(!canvas_mapping(0, 100, std::nullopt, resize).has_value(), "empty source");
    check(!canvas_mapping(100, 0, std::nullopt, resize).has_value(), "empty source (height)");
    check(!canvas_mapping(100, 100, pixel_crop(10, 10, 0, 50), resize).has_value(), "crop box with no width");
    check(!canvas_mapping(100, 100, pixel_crop(10, 10, 50, 0), resize).has_value(), "crop box with no height");
    ResizeOptions no_canvas;
    no_canvas.target_width = 0;
    check(!canvas_mapping(100, 100, std::nullopt, no_canvas).has_value(), "canvas without area");
    check(canvas_mapping(100, 100, std::nullopt, resize).has_value(), "but a normal source has one");
}

}  // namespace

int main() {
    test_against_pipeline();
    test_round_trip_and_zoom();
    test_no_mapping();
    if (g_failures) {
        std::printf("\n%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("\nall checks passed\n");
    return 0;
}
