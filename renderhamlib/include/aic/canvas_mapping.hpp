// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/canvas_mapping.hpp
//
// Where a point of the source picture ends up on the canvas the HAM encoder works on, and the
// other way round. The pipeline turns the source into the canvas in three steps - crop box,
// scale (stretch, fit with letterbox or fill with overhang, with the pixel aspect ratio), place
// on the canvas - and each of them is a plain shift or scale per axis, so all three together are
// one such map. A viewer that shows the source next to the converted result uses it to keep
// both showing the same part of the picture.
//
// Coordinates are continuous, as in ImageTransform::map_rect: (0, 0) is the top-left corner of
// the first pixel and a pixel spans one unit, so the middle of pixel (3, 5) is (3.5, 5.5).
//
// The source here is the picture the crop box refers to: after ConversionOptions::source_transform
// (rotation, mirroring) has been applied, which is also what the GUI shows.

#include <optional>

#include "aic/crop.hpp"
#include "aic/pipeline.hpp"
#include "aic/resize.hpp"

namespace aic {

struct CanvasPoint {
    double x = 0, y = 0;
};

struct CanvasMapping {
    // canvas = (source - box) * scale + offset, per axis.
    double box_x = 0, box_y = 0;        // top-left corner of the crop box in the source (0, 0 without crop)
    double scale_x = 1, scale_y = 1;    // canvas pixels per source pixel
    double offset_x = 0, offset_y = 0;  // canvas position of the crop box's top-left corner

    bool operator==(const CanvasMapping& o) const {
        return box_x == o.box_x && box_y == o.box_y && scale_x == o.scale_x && scale_y == o.scale_y &&
               offset_x == o.offset_x && offset_y == o.offset_y;
    }
    bool operator!=(const CanvasMapping& o) const { return !(*this == o); }

    CanvasPoint to_canvas(CanvasPoint source) const {
        return {(source.x - box_x) * scale_x + offset_x, (source.y - box_y) * scale_y + offset_y};
    }
    CanvasPoint to_source(CanvasPoint canvas) const {
        return {(canvas.x - offset_x) / scale_x + box_x, (canvas.y - offset_y) / scale_y + box_y};
    }

    // A view that shows the source at `zoom` screen pixels per source pixel shows the same thing at
    // the same size as a view of the canvas at zoom_to_canvas(zoom) - measured along x, where the
    // two views only differ in scale when the picture was stretched or the pixels are not square.
    double zoom_to_canvas(double source_zoom) const { return source_zoom / scale_x; }
    double zoom_to_source(double canvas_zoom) const { return canvas_zoom * scale_x; }
};

// The mapping the pipeline applies to a source of the given size with these crop and resize
// options (the same ones ConversionOptions carries as `crop`, `target_*`, `fit_mode` and
// `pixel_aspect_ratio`). nullopt for an empty source or a crop box with no area.
std::optional<CanvasMapping> canvas_mapping(int source_width, int source_height, const std::optional<CropSpec>& crop,
                                            const ResizeOptions& resize);
// The same for a source converted with `options`.
std::optional<CanvasMapping> canvas_mapping(int source_width, int source_height, const ConversionOptions& options);

}  // namespace aic
