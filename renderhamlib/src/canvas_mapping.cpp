// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/canvas_mapping.hpp"

namespace aic {

std::optional<CanvasMapping> canvas_mapping(int source_width, int source_height, const std::optional<CropSpec>& crop,
                                            const ResizeOptions& resize) {
    if (source_width <= 0 || source_height <= 0 || resize.target_width <= 0 || resize.target_height <= 0)
        return std::nullopt;

    CropBox box{0, 0, source_width, source_height};
    if (crop.has_value()) {
        const std::optional<CropBox> resolved = resolve_crop_box(source_width, source_height, *crop);
        if (!resolved) return std::nullopt;
        box = *resolved;
    }

    // The same two calls the pipeline makes: crop_image() cuts the box out (parts outside the
    // source are filled, so the box keeps its size), resize_image() scales it to
    // `scaled_width x scaled_height` and puts it at (offset_x, offset_y) - a negative offset in
    // fill mode, where the overhang falls off the canvas.
    const ResizeGeometry g = resize_geometry(box.width, box.height, resize);

    CanvasMapping m;
    m.box_x = box.x;
    m.box_y = box.y;
    m.scale_x = static_cast<double>(g.scaled_width) / box.width;
    m.scale_y = static_cast<double>(g.scaled_height) / box.height;
    m.offset_x = g.offset_x;
    m.offset_y = g.offset_y;
    return m;
}

std::optional<CanvasMapping> canvas_mapping(int source_width, int source_height, const ConversionOptions& options) {
    // The fields the pipeline hands to resize_image().
    ResizeOptions resize;
    resize.target_width = options.target_width;
    resize.target_height = options.target_height;
    resize.mode = options.fit_mode;
    resize.pad_color = options.pad_color;
    resize.pixel_aspect_ratio = options.pixel_aspect_ratio;
    return canvas_mapping(source_width, source_height, options.crop, resize);
}

}  // namespace aic
