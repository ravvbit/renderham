// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/pipeline.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "aic/ham_maps.hpp"
#include "aic/ilbm_writer.hpp"
#include "aic/importance.hpp"
#include "aic/registry.hpp"

namespace aic {

namespace {

bool in_range(float v, float lo, float hi) { return std::isfinite(v) && v >= lo && v <= hi; }

std::string range_message(const char* field, double lo, double hi) {
    std::ostringstream os;
    os << field << " must be between " << lo << " and " << hi;
    return os.str();
}

}  // namespace

std::string validate_options(const ConversionOptions& o) {
    using namespace limits;
    if (o.target_width < kMinDimension || o.target_width > kMaxDimension)
        return range_message("target width", kMinDimension, kMaxDimension);
    if (o.target_height < kMinDimension || o.target_height > kMaxDimension)
        return range_message("target height", kMinDimension, kMaxDimension);
    if (!in_range(o.pixel_aspect_ratio, kMinPixelAspect, kMaxPixelAspect))
        return range_message("pixel aspect", kMinPixelAspect, kMaxPixelAspect);
    if (!in_range(o.color_correction.brightness, kMinBrightness, kMaxBrightness))
        return range_message("brightness", kMinBrightness, kMaxBrightness);
    if (!in_range(o.color_correction.contrast, kMinContrast, kMaxContrast))
        return range_message("contrast", kMinContrast, kMaxContrast);
    if (!in_range(o.color_correction.gamma, kMinGamma, kMaxGamma))
        return range_message("gamma", kMinGamma, kMaxGamma);
    if (!in_range(o.color_correction.saturation, kMinSaturation, kMaxSaturation))
        return range_message("saturation", kMinSaturation, kMaxSaturation);
    if (!in_range(o.chroma_weight, kMinChromaWeight, kMaxChromaWeight))
        return range_message("chroma weight", kMinChromaWeight, kMaxChromaWeight);
    if (!in_range(o.dither_strength, 0.0f, 1.0f)) return range_message("dither strength", 0.0, 1.0);
    if (o.lookahead < 0 || o.lookahead > kMaxLookahead) return range_message("lookahead", 0, kMaxLookahead);
    if (o.lookahead_beam < kMinBeam || o.lookahead_beam > kMaxBeam)
        return range_message("lookahead beam", kMinBeam, kMaxBeam);
    if (o.palette_refinement_iterations < 0 || o.palette_refinement_iterations > kMaxPaletteIterations)
        return range_message("palette refinement iterations", 0, kMaxPaletteIterations);
    return {};
}

std::optional<Image> ConversionPipeline::load_source(const std::string& input_path, const ConversionOptions& options,
                                                      std::string* error_message) const {
    std::string load_err;
    LoadOptions load_opts;
    load_opts.apply_exif_orientation = options.apply_exif_orientation;
    load_opts.alpha_background = options.alpha_background;
    auto loaded = Image::load(input_path, &load_err, load_opts);
    if (!loaded) {
        if (error_message) *error_message = "failed to load '" + input_path + "': " + load_err;
        return std::nullopt;
    }
    if (!options.source_transform.is_identity()) *loaded = options.source_transform.apply(*loaded);
    return loaded;
}

PreviewResult ConversionPipeline::preview(const std::string& input_path, const ConversionOptions& options) const {
    PreviewResult result;

    if (std::string bad = validate_options(options); !bad.empty()) {
        result.error_message = "invalid option: " + bad;
        return result;
    }

    std::string load_err;
    auto loaded = load_source(input_path, options, &load_err);
    if (!loaded) {
        result.error_message = load_err;
        return result;
    }
    return preview_source(*loaded, options);
}

PreviewResult ConversionPipeline::preview_source(const Image& source, const ConversionOptions& options) const {
    PreviewResult result;

    if (std::string bad = validate_options(options); !bad.empty()) {
        result.error_message = "invalid option: " + bad;
        return result;
    }

    const Image* pre_scale = &source;
    Image cropped;  // only populated (and pointed to) if options.crop is set
    if (options.crop.has_value()) {
        std::string crop_err;
        auto c = crop_image(source, *options.crop, options.crop_fill_color, &crop_err);
        if (!c) {
            result.error_message = "crop failed: " + crop_err;
            return result;
        }
        cropped = std::move(*c);
        pre_scale = &cropped;
    }

    ResizeOptions resize_opts;
    resize_opts.target_width = options.target_width;
    resize_opts.target_height = options.target_height;
    resize_opts.mode = options.fit_mode;
    resize_opts.pad_color = options.pad_color;
    resize_opts.pixel_aspect_ratio = options.pixel_aspect_ratio;
    Image canvas = resize_image(*pre_scale, resize_opts);
    apply_color_correction(canvas, options.color_correction);

    if (!Registry<IPaletteQuantizer>::instance().contains(options.quantizer_name)) {
        result.error_message = "unknown palette quantizer '" + options.quantizer_name + "'";
        return result;
    }
    if (!Registry<IErrorDiffuser>::instance().contains(options.ditherer_name)) {
        result.error_message = "unknown ditherer '" + options.ditherer_name + "'";
        return result;
    }
    if (!Registry<IImportanceMap>::instance().contains(options.importance_name)) {
        result.error_message = "unknown importance map '" + options.importance_name + "'";
        return result;
    }
    auto quantizer = Registry<IPaletteQuantizer>::instance().create(options.quantizer_name);
    auto diffuser = Registry<IErrorDiffuser>::instance().create(options.ditherer_name);
    auto importance = Registry<IImportanceMap>::instance().create(options.importance_name);
    diffuser->set_strength(options.dither_strength);

    HAM6EncoderOptions encoder_opts;
    encoder_opts.palette_refinement_iterations = options.palette_refinement_iterations;
    encoder_opts.importance = importance.get();
    encoder_opts.weights.chroma = options.chroma_weight;
    encoder_opts.locked_palette = options.locked_palette;
    encoder_opts.fixed_colors = options.fixed_colors;
    encoder_opts.line_start_modify = options.line_start_modify;
    encoder_opts.lookahead = std::max(1, options.lookahead);
    encoder_opts.lookahead_beam = options.lookahead_beam;
    encoder_opts.progress = options.progress;
    if (options.anchor_dither_to_source) {
        // Canvas position of the source's top-left corner, in whole pixels: where the crop box
        // starts in the source, scaled like the picture was, plus where the scaled crop
        // landed on the canvas (padding or overhang). With frame after frame panning over the
        // same source, this origin moves exactly as the content does.
        CropBox box{0, 0, source.width(), source.height()};
        if (options.crop.has_value()) {
            if (auto resolved = resolve_crop_box(source.width(), source.height(), *options.crop)) box = *resolved;
        }
        const ResizeGeometry g = resize_geometry(box.width, box.height, resize_opts);
        const double scale_x = static_cast<double>(g.scaled_width) / box.width;
        const double scale_y = static_cast<double>(g.scaled_height) / box.height;
        encoder_opts.dither_origin_x = static_cast<int>(std::lround(box.x * scale_x)) - g.offset_x;
        encoder_opts.dither_origin_y = static_cast<int>(std::lround(box.y * scale_y)) - g.offset_y;
    }
    HAM6Encoder encoder(*quantizer, *diffuser, encoder_opts);

    // One check before the (slow) encoder starts: loading and resizing a big
    // picture can take a moment, and a cancel that came meanwhile need not wait
    // for the first scanline.
    if (options.progress && !options.progress(0.f)) {
        result.cancelled = true;
        result.error_message = "cancelled";
        return result;
    }
    try {
        result.frame = encoder.encode(canvas);
    } catch (const OperationCancelled&) {
        result.cancelled = true;
        result.error_message = "cancelled";
        return result;
    }
    result.decoded = decode_ham6(result.frame);
    if (options.keep_canvas) result.canvas = canvas;
    result.lace = options.lace.has_value() ? *options.lace : (result.frame.height >= 400);

    // Quality metric: mean OKLab distance between source canvas and what
    // HAM6 actually reproduced - useful for A/B-ing ditherers/quantizers
    // without opening an emulator.
    double sum = 0.0;
    for (int y = 0; y < canvas.height(); ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            OKLab a = srgb_to_oklab(canvas.at(x, y));
            OKLab b = srgb_to_oklab(result.decoded.at(x, y));
            sum += std::sqrt(static_cast<double>(distance2(a, b)));
        }
    }
    result.mean_oklab_error = sum / (static_cast<double>(canvas.width()) * canvas.height());
    if (options.quality_metrics) result.metrics = compute_quality_metrics(canvas, result.decoded);

    if (!options.preview_png_path.empty()) result.decoded.save_png(options.preview_png_path);
    if (!options.canvas_png_path.empty()) canvas.save_png(options.canvas_png_path);
    if (!options.ham_map_png_path.empty()) make_ham_map(result.frame).save_png(options.ham_map_png_path);
    if (!options.error_map_png_path.empty()) make_error_map(canvas, result.decoded).save_png(options.error_map_png_path);

    result.success = true;
    return result;
}

ConversionResult ConversionPipeline::convert(const std::string& input_path, const std::string& output_path,
                                              const ConversionOptions& options) const {
    ConversionResult result;

    PreviewResult pv = preview(input_path, options);
    if (!pv.success) {
        result.cancelled = pv.cancelled;
        result.error_message = pv.error_message;
        return result;
    }

    IlbmWriteOptions write_opts;
    write_opts.compress = options.compress;
    write_opts.lace = pv.lace;
    std::string write_err;
    if (!write_ham6_ilbm(pv.frame, output_path, write_opts, &write_err)) {
        result.error_message = "failed to write '" + output_path + "': " + write_err;
        return result;
    }

    result.success = true;
    result.mean_oklab_error = pv.mean_oklab_error;
    result.metrics = pv.metrics;
    result.lace = pv.lace;
    result.palette = pv.frame.palette;
    return result;
}

}  // namespace aic
