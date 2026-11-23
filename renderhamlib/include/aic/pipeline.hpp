// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/pipeline.hpp
//
// The one class a frontend (CLI today, GUI now) actually needs to know
// about. It resolves algorithm names via the Registry, so adding a new
// ditherer or quantizer never requires changing this file.

#include <array>
#include <functional>
#include <optional>
#include <string>

#include "aic/color_correction.hpp"
#include "aic/crop.hpp"
#include "aic/ham6_encoder.hpp"
#include "aic/limits.hpp"
#include "aic/quality_metrics.hpp"
#include "aic/transform.hpp"
#include "aic/resize.hpp"

namespace aic {

struct ConversionOptions {
    // Applied first, before any scaling, if set.
    std::optional<CropSpec> crop;

    int target_width = 320;
    int target_height = 256;
    FitMode fit_mode = FitMode::Fit;
    RGBf pad_color{0.f, 0.f, 0.f};
    float pixel_aspect_ratio = 1.0f;

    // Applied to the resized canvas, right before quantization/dithering -
    // i.e. visible only in the HAM6-encoded output (and any preview
    // decoded from it). A caller's own copy of the source image, shown
    // unmodified elsewhere (e.g. a GUI's source panel), never passes
    // through this pipeline and is therefore never touched by it.
    ColorCorrectionOptions color_correction;

    // See HAM6EncoderOptions for the exact semantics of both - passed
    // straight through, just exposed at the pipeline level so CLI/GUI
    // callers don't need to touch HAM6EncoderOptions directly.
    std::optional<Palette> locked_palette;
    std::array<std::optional<AmigaColor>, 16> fixed_colors{};

    // Fill color for any crop-box pixel that falls outside the source
    // image (see crop_image) - lets an animation keyframe's crop box
    // extend past an edge, or lie entirely outside the source, for a
    // "wander into/out of frame" effect. Black by default, matching
    // real Amiga hardware where color register 0 also doubles as the
    // border/background color outside the actual bitmap area.
    RGBf crop_fill_color{};

    // Rotation/mirroring chosen by the user, applied right after loading (and
    // after the automatic EXIF turn). Crop boxes are given in the coordinates
    // of the transformed image, i.e. of what the user sees.
    ImageTransform source_transform;

    // How the source file is read, see LoadOptions in image.hpp.
    bool apply_exif_orientation = true;
    RGBf alpha_background{0.f, 0.f, 0.f};

    // Defaults measured on four photos at 320x256 (README, "kmeans und Lookahead 3 als
    // Standard"): kmeans beats median-cut by about 2 percent mean error (better on every photo).
    // A lookahead of 3 would take about 4 percent more off, but at roughly 6 times the run time,
    // which makes every preview and every animation frame that much slower: it is therefore an
    // option to switch on (README, "Lookahead wieder aus"), not the default.
    std::string quantizer_name = "kmeans";
    std::string ditherer_name = "floyd-steinberg";
    std::string importance_name = "uniform";

    // 0..1, passed to IErrorDiffuser::set_strength: how much of the
    // dithering effect applies (error-propagation factor for error
    // diffusers, pattern amplitude for ordered ones). 1 = default.
    float dither_strength = 1.0f;

    // For a sequence of frames that crop different parts of one picture (an animation):
    // make an ordered ditherer's pattern (Bayer, blue-noise, checks) follow the picture, as
    // if it were printed on it, instead of staying fixed on the screen. With the pattern
    // fixed to the screen, a pan makes every dot of the dither flip from frame to frame;
    // attached to the picture, the dots travel with the content and the dither stays calm.
    // The pattern's origin is the canvas position of the source's top-left corner, rounded
    // to whole pixels - exact for pans, which is what Ken Burns animations mostly are (a zoom
    // moves content at different speeds, there the pattern can only follow one point). No
    // effect on error-diffusion ditherers (their noise comes from the content, there is no
    // pattern to move) and none on a single picture without a crop (origin 0 unless fit
    // mode letterboxes it; with this off, frames are exactly as before). SequenceOptions
    // turns it on for every frame.
    bool anchor_dither_to_source = false;

    // See HAM6EncoderOptions::line_start_modify (off by default: a rule of
    // the ILBM format that some decoders, e.g. netpbm, do not follow).
    bool line_start_modify = false;

    // See HAM6EncoderOptions::lookahead / lookahead_beam. 0 or 1 = off (greedy, the default);
    // 2..8 = look that many pixels ahead (3 is a good compromise, about 6 times slower).
    int lookahead = 0;
    int lookahead_beam = 8;
    int palette_refinement_iterations = 4;

    // Scales chroma (a/b) contribution relative to luminance (L) in the
    // encoder's internal color-distance decisions. 1.0 = standard
    // unweighted OKLab distance (default/original behavior). <1
    // deprioritizes color accuracy in favor of luminance accuracy - see
    // PerceptualWeights in color_space.hpp for the reasoning.
    float chroma_weight = 1.0f;

    bool compress = true;

    // CAMG LACE (interlace) bit. nullopt = auto-detect: on when
    // target_height >= 400, since that's well beyond what a
    // non-interlaced OCS PAL frame can show - such a tall image only
    // makes sense as an interlaced one. An explicit true/false always
    // overrides the heuristic.
    std::optional<bool> lace;

    // Optional debug/QA output: a decoded PNG preview of exactly what the
    // Amiga will display (round-tripped through the HAM6 encoding). Only
    // used by convert(); preview() always returns the decoded image in
    // memory regardless of this field.
    std::string preview_png_path;

    // Optional debug/QA output: the exact canvas the encoder saw (after
    // crop, resize and color correction - i.e. the reference the
    // reported mean_oklab_error is measured against). Lets external
    // tooling (scripts/ham_eval.py) compare decoded output against the
    // same reference the encoder used instead of re-deriving it.
    std::string canvas_png_path;

    // Diagnostic PNG outputs, see ham_maps.hpp: which HAM operation produced
    // each pixel, and how far each pixel is from the canvas.
    std::string ham_map_png_path;
    std::string error_map_png_path;

    // Keep the encoder's canvas in PreviewResult::canvas (needed to build an
    // error map later). Off by default: a canvas is as big as the decoded
    // image, which adds up over the hundreds of frames of an animation
    // preview that never look at it.
    bool keep_canvas = false;

    // Also compute the perceptual quality figures (aic/quality_metrics.hpp: blurred error and
    // SSIM) into PreviewResult::metrics / ConversionResult::metrics. Off by default: they cost
    // about 50 ms for a 320x256 picture and 120 ms for an interlaced one (measured, Release
    // build) - roughly a third of a conversion without lookahead. Fine once per picture, not
    // worth paying for each of the hundreds of frames of an animation, which never look at
    // them. The CLI and the GUI's single-picture preview switch it on; the mean error is
    // always computed.
    bool quality_metrics = false;

    // Progress report and cancellation, for a caller that runs the pipeline on
    // a worker thread (the GUI). Called on that thread, first once before the
    // encoder starts and then at the start of every scanline of every encoder
    // pass, with the fraction of the encoder's work that is done (0..1, see
    // HAM6EncoderOptions::progress). Returning false stops the conversion:
    // PreviewResult::cancelled is set and nothing is written. Never changes the
    // result of a conversion that runs to the end. Keep it cheap.
    std::function<bool(float fraction)> progress;
};

// Result of running the full pipeline up to (but not including) writing
// an IFF file - everything a GUI needs to show a live preview without
// touching disk. convert() is a thin wrapper: preview() + write the file.
struct PreviewResult {
    bool success = false;
    std::string error_message;
    HAM6Frame frame;
    Image decoded;  // decode_ham6(frame) - what the Amiga will actually show
    Image canvas;   // what the encoder was given (the reference for mean_oklab_error); empty unless options.keep_canvas
    double mean_oklab_error = 0.0;
    QualityMetrics metrics;  // valid only if options.quality_metrics
    bool lace = false;
    // The progress callback asked to stop. success is false then, and
    // error_message just says "cancelled" - not an error worth showing.
    bool cancelled = false;
};

struct ConversionResult {
    bool success = false;
    bool cancelled = false;  // see PreviewResult::cancelled
    std::string error_message;
    double mean_oklab_error = 0.0;  // average per-pixel OKLab distance, encoder vs. original
    QualityMetrics metrics;         // valid only if options.quality_metrics
    bool lace = false;              // the actual LACE decision that was written (auto or override)
    Palette palette;                // the palette this conversion actually used (quantized or locked)
};

// Checks the options against aic::limits (sizes, ranges, NaN). Returns an
// empty string when everything is acceptable, otherwise a message naming the
// offending field. preview()/convert() call it first, so every frontend is
// protected; the CLI also uses the same limits for its own messages.
std::string validate_options(const ConversionOptions& options);

class ConversionPipeline {
public:
    // Load -> optional crop -> resize -> HAM6-encode. No disk writes.
    PreviewResult preview(const std::string& input_path, const ConversionOptions& options) const;

    // preview() split in its two halves, for callers that convert the same
    // picture many times (the frames of an animation differ only in the crop
    // box): load_source() does what preview() does first - read the file (EXIF
    // turn, transparency blended over options.alpha_background) and apply
    // options.source_transform - and preview_source() runs everything after
    // that. preview(path, o) is exactly load_source() + preview_source().
    // Both are const and thread-safe; preview_source() only reads `source`, so
    // several threads can share one loaded image.
    std::optional<Image> load_source(const std::string& input_path, const ConversionOptions& options,
                                     std::string* error_message) const;
    PreviewResult preview_source(const Image& source, const ConversionOptions& options) const;

    // preview() + write the resulting IFF file to output_path.
    ConversionResult convert(const std::string& input_path, const std::string& output_path,
                              const ConversionOptions& options) const;
};

}  // namespace aic
