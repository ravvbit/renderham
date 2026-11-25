// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
// renderHAM - command-line frontend for renderhamlib.
//
// Deliberately thin: every line of actual conversion logic lives in the
// library so a future GUI can call aic::ConversionPipeline directly and
// stay in sync with the CLI for free.

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "aic/aic.hpp"

namespace {

void print_usage(const char* prog) {
    std::cout <<
        "Usage: " << prog << " -i <input> -o <output.iff> [options]\n\n"
        "Options:\n"
        "  -i, --input <path>        Source image (PNG, JPEG, BMP, TGA, GIF, PSD, PPM, PGM)\n"
        "  -o, --output <path>       Output IFF ILBM (HAM6) file\n"
        "      --background <color>  Color that transparent areas of the source are blended\n"
        "                            over: R,G,B (0-255) or '#RRGGBB' (quote it in a shell;\n"
        "                            default 0,0,0 = black)\n"
        "      --ignore-exif         Do not rotate JPEGs according to their EXIF orientation\n"
        "                            (by default a sideways-stored phone photo is turned upright)\n"
        "      --rotate <deg>        Rotate the source clockwise by 0, 90, 180 or 270 degrees\n"
        "                            (after the EXIF turn, before --crop)\n"
        "      --flip-h              Mirror the source left <-> right (applied before --rotate)\n"
        "      --flip-v              Mirror the source top <-> bottom (applied before --rotate)\n"
        "      --crop <geometry>     Crop before scaling, ImageMagick-style WxH+X+Y.\n"
        "                            Each value may be a pixel count or a percent of\n"
        "                            the source dimension (e.g. 320x200+10+5,\n"
        "                            50%x50%+25%+25%, or 640x50%+0+10%)\n"
        "      --width <n>           Target width, 1-8192 (default 320)\n"
        "      --height <n>          Target height, 1-8192 (default 256)\n"
        "      --fit <mode>          stretch | fit | fill (default fit)\n"
        "      --pixel-aspect <f>    Display pixel aspect ratio correction, 0.1-10 (default 1.0)\n"
        "      --brightness <f>      Output-only brightness offset, -1 to 1 (default 0.0)\n"
        "      --contrast <f>        Output-only contrast factor, 0-4 (default 1.0)\n"
        "      --gamma <f>           Output-only gamma curve, 0.1-5 (default 1.0)\n"
        "      --saturation <f>      Output-only saturation factor, 0-4 (default 1.0)\n"
        "      --dither <name>       Ditherer to use (default floyd-steinberg)\n"
        "      --dither-strength <n> Dither strength in percent, 0-100 (default 100). For\n"
        "                            error diffusion this is the share of the error that is\n"
        "                            propagated to neighbors, for ordered dithering the\n"
        "                            pattern amplitude; lower values mean less noise\n"
        "      --quantizer <name>    Palette quantizer to use (default kmeans)\n"
        "      --color-focus <name>  Per-pixel palette-priority map: uniform | center |\n"
        "                            edge | center-edge | golden-ratio (default uniform)\n"
        "      --palette-iterations <n>  Iterative palette refinement passes\n"
        "                            0-64 (default 4, 0 = seed palette only)\n"
        "      --chroma-weight <f>   Chroma vs. luminance weight, 0-4, in the encoder's\n"
        "                            color distance (default 1.0 = unweighted;\n"
        "                            <1 prioritizes luminance accuracy over color)\n"
        "      --load-palette <path.iff>  Load and lock the palette from a saved\n"
        "                            palette file instead of quantizing a new one\n"
        "                            (also disables --palette-iterations refinement)\n"
        "      --save-palette <path.iff>  Save the palette this conversion actually\n"
        "                            used, as a standalone loadable/openable ILBM\n"
        "      --fix-color0 <r>,<g>,<b>  Force palette index 0 to this exact 8-bit\n"
        "                            RGB value (also the border/background color\n"
        "                            outside the bitmap on real Amiga hardware)\n"
        "      --lookahead <n>       Look n pixels ahead when choosing each HAM operation\n"
        "                            (0-8, default 0 = off; 0 or 1 = greedy, the fastest; 3 is a\n"
        "                            good compromise). Starts the Modify chain before an edge\n"
        "                            instead of after it: less smearing, about 6 times slower\n"
        "                            than greedy\n"
        "      --lookahead-beam <n>  Beam width of the lookahead search, 1-32 (default 8)\n"
        "      --line-start-modify   Let a scanline start with a Modify of the border color\n"
        "                            (palette color 0), as the ILBM spec allows. Slightly\n"
        "                            better at the left edge, but decoders that start every\n"
        "                            line from black (e.g. netpbm ilbmtoppm) misrender it;\n"
        "                            no difference if color 0 is black. Default: off\n"
        "      --anchor-dither       Attach the pattern of an ordered ditherer (bayer, blue-noise,\n"
        "                            checks...) to the picture instead of the screen: with --crop,\n"
        "                            the pattern starts where the crop box sits in the source, so\n"
        "                            frames cropped at different positions of one picture (a pan,\n"
        "                            made by a script) get a dither that moves with the content\n"
        "                            and does not flicker. No effect without --crop; none on\n"
        "                            error-diffusion ditherers. Default: off\n"
        "      --no-compress         Disable ByteRun1 RLE compression of BODY\n"
        "      --lace                Force the CAMG LACE (interlace) bit on\n"
        "      --no-lace             Force the CAMG LACE bit off\n"
        "                            (default: auto - on when --height >= 400)\n"
        "      --preview <path.png>  Also write a decoded PNG preview\n"
        "      --ham-map <path.png>  Also write a map of the HAM operation used per pixel\n"
        "                            (black = Select, red/green/blue = Modify R/G/B)\n"
        "      --error-map <path.png>  Also write an error map: brightness = distance from\n"
        "                            the canvas, white = 0.10 OKLab or more (fixed scale)\n"
        "      --canvas-png <path.png>  Also write the canvas the encoder saw (after\n"
        "                            crop/resize/color correction) - the reference the\n"
        "                            reported error is measured against (for QA tooling)\n"
        "      --list-dithers        List available ditherer names and exit\n"
        "      --list-quantizers     List available quantizer names and exit\n"
        "      --list-color-focus    List available color-focus map names and exit\n"
        "      --version             Print the version, license and origin, and exit\n"
        "  -h, --help                Show this help\n\n"
        "renderHAM " RENDERHAM_VERSION " - AI-generated software (Claude, Anthropic), MIT License.\n";
}

[[noreturn]] void die(const std::string& message) {
    std::cerr << "error: " << message << "\n";
    std::exit(1);
}

bool need_arg(int i, int argc, const char* flag) {
    if (i + 1 >= argc) die(std::string(flag) + " requires an argument");
    return true;
}

// Strict number parsing: the whole text must be a number inside [lo, hi].
// (atoi/atof turned "abc" into 0 and accepted any magnitude silently.)
int parse_int(const char* flag, const char* text, int lo, int hi) {
    char* end = nullptr;
    errno = 0;
    long v = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE || v < lo || v > hi) {
        die(std::string(flag) + " expects an integer between " + std::to_string(lo) + " and " + std::to_string(hi) +
            ", got '" + text + "'");
    }
    return static_cast<int>(v);
}

float parse_float(const char* flag, const char* text, float lo, float hi) {
    char* end = nullptr;
    errno = 0;
    float v = std::strtof(text, &end);
    if (end == text || *end != '\0' || errno == ERANGE || !std::isfinite(v) || v < lo || v > hi) {
        std::ostringstream os;
        os << flag << " expects a number between " << lo << " and " << hi << ", got '" << text << "'";
        die(os.str());
    }
    return v;
}

// "R,G,B" (each 0-255) or "#RRGGBB" / "RRGGBB" -> gamma-encoded RGB in 0..1.
aic::RGBf parse_color(const char* flag, const char* text) {
    int r = 0, g = 0, b = 0;
    char trailing = 0;
    if (std::sscanf(text, "%d,%d,%d%c", &r, &g, &b, &trailing) == 3 && r >= 0 && r <= 255 && g >= 0 && g <= 255 &&
        b >= 0 && b <= 255) {
        return aic::RGBf{r / 255.f, g / 255.f, b / 255.f};
    }
    const char* hex = (text[0] == '#') ? text + 1 : text;
    if (std::strlen(hex) == 6 && std::strspn(hex, "0123456789abcdefABCDEF") == 6) {
        unsigned v = static_cast<unsigned>(std::strtoul(hex, nullptr, 16));
        return aic::RGBf{((v >> 16) & 0xff) / 255.f, ((v >> 8) & 0xff) / 255.f, (v & 0xff) / 255.f};
    }
    die(std::string(flag) + " expects R,G,B (each 0-255) or #RRGGBB, got '" + text + "'");
}

}  // namespace

int main(int argc, char** argv) {
    std::string input_path, output_path, save_palette_path;
    aic::ConversionOptions opts;
    opts.quality_metrics = true;  // the CLI reports the perceptual figures, too (~50 ms at 320x256)
    int rotate_quarter_turns = 0;
    bool flip_h = false, flip_v = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (a == "--version") {
            // First line "<name> <version>" and the lines after it are what help2man expects.
            std::cout << "renderHAM " << RENDERHAM_VERSION << "\n"
                      << RENDERHAM_COPYRIGHT << ". Released under the " << RENDERHAM_LICENSE_NAME << ".\n"
                      << "AI-generated software: written by Claude (Anthropic) under human direction.\n"
                      << "See README.md for what that means.\n";
            return 0;
        } else if (a == "--list-dithers") {
            for (const auto& n : aic::Registry<aic::IErrorDiffuser>::instance().names()) std::cout << n << "\n";
            return 0;
        } else if (a == "--list-quantizers") {
            for (const auto& n : aic::Registry<aic::IPaletteQuantizer>::instance().names()) std::cout << n << "\n";
            return 0;
        } else if (a == "--list-color-focus") {
            for (const auto& n : aic::Registry<aic::IImportanceMap>::instance().names()) std::cout << n << "\n";
            return 0;
        } else if ((a == "-i" || a == "--input") && need_arg(i, argc, a.c_str())) {
            input_path = argv[++i];
        } else if ((a == "-o" || a == "--output") && need_arg(i, argc, a.c_str())) {
            output_path = argv[++i];
        } else if (a == "--crop" && need_arg(i, argc, a.c_str())) {
            std::string crop_err;
            auto spec = aic::parse_crop_spec(argv[++i], &crop_err);
            if (!spec) {
                std::cerr << "error: " << crop_err << "\n";
                return 1;
            }
            opts.crop = spec;
        } else if (a == "--width" && need_arg(i, argc, a.c_str())) {
            opts.target_width = parse_int("--width", argv[++i], aic::limits::kMinDimension, aic::limits::kMaxDimension);
        } else if (a == "--height" && need_arg(i, argc, a.c_str())) {
            opts.target_height = parse_int("--height", argv[++i], aic::limits::kMinDimension, aic::limits::kMaxDimension);
        } else if (a == "--pixel-aspect" && need_arg(i, argc, a.c_str())) {
            opts.pixel_aspect_ratio = parse_float("--pixel-aspect", argv[++i], aic::limits::kMinPixelAspect, aic::limits::kMaxPixelAspect);
        } else if (a == "--brightness" && need_arg(i, argc, a.c_str())) {
            opts.color_correction.brightness = parse_float("--brightness", argv[++i], aic::limits::kMinBrightness, aic::limits::kMaxBrightness);
        } else if (a == "--contrast" && need_arg(i, argc, a.c_str())) {
            opts.color_correction.contrast = parse_float("--contrast", argv[++i], aic::limits::kMinContrast, aic::limits::kMaxContrast);
        } else if (a == "--gamma" && need_arg(i, argc, a.c_str())) {
            opts.color_correction.gamma = parse_float("--gamma", argv[++i], aic::limits::kMinGamma, aic::limits::kMaxGamma);
        } else if (a == "--saturation" && need_arg(i, argc, a.c_str())) {
            opts.color_correction.saturation = parse_float("--saturation", argv[++i], aic::limits::kMinSaturation, aic::limits::kMaxSaturation);
        } else if (a == "--fit" && need_arg(i, argc, a.c_str())) {
            std::string m = argv[++i];
            if (m == "stretch") opts.fit_mode = aic::FitMode::Stretch;
            else if (m == "fit") opts.fit_mode = aic::FitMode::Fit;
            else if (m == "fill") opts.fit_mode = aic::FitMode::Fill;
            else { std::cerr << "error: unknown --fit mode '" << m << "'\n"; return 1; }
        } else if (a == "--dither" && need_arg(i, argc, a.c_str())) {
            opts.ditherer_name = argv[++i];
        } else if (a == "--dither-strength" && need_arg(i, argc, a.c_str())) {
            opts.dither_strength = parse_float("--dither-strength", argv[++i], 0.0f, 100.0f) / 100.f;
        } else if (a == "--quantizer" && need_arg(i, argc, a.c_str())) {
            opts.quantizer_name = argv[++i];
        } else if (a == "--color-focus" && need_arg(i, argc, a.c_str())) {
            opts.importance_name = argv[++i];
        } else if (a == "--palette-iterations" && need_arg(i, argc, a.c_str())) {
            opts.palette_refinement_iterations = parse_int("--palette-iterations", argv[++i], 0, aic::limits::kMaxPaletteIterations);
        } else if (a == "--chroma-weight" && need_arg(i, argc, a.c_str())) {
            opts.chroma_weight = parse_float("--chroma-weight", argv[++i], aic::limits::kMinChromaWeight, aic::limits::kMaxChromaWeight);
        } else if (a == "--load-palette" && need_arg(i, argc, a.c_str())) {
            aic::Palette loaded;
            std::string load_err;
            if (!aic::load_palette(argv[++i], &loaded, &load_err)) {
                std::cerr << "error: failed to load palette: " << load_err << "\n";
                return 1;
            }
            opts.locked_palette = loaded;
        } else if (a == "--save-palette" && need_arg(i, argc, a.c_str())) {
            save_palette_path = argv[++i];
        } else if (a == "--fix-color0" && need_arg(i, argc, a.c_str())) {
            int r, g, b;
            char trailing = 0;
            const char* text = argv[++i];
            if (std::sscanf(text, "%d,%d,%d%c", &r, &g, &b, &trailing) != 3 || r < 0 || r > 255 || g < 0 ||
                g > 255 || b < 0 || b > 255) {
                die(std::string("--fix-color0 expects R,G,B with each value 0-255 (e.g. 0,0,0), got '") + text + "'");
            }
            auto to4bit = [](int v8) {
                int v4 = static_cast<int>(std::lround(std::clamp(v8, 0, 255) / 17.0));
                return static_cast<uint8_t>(std::clamp(v4, 0, 15));
            };
            opts.fixed_colors[0] = aic::AmigaColor{to4bit(r), to4bit(g), to4bit(b)};
        } else if (a == "--background" && need_arg(i, argc, a.c_str())) {
            opts.alpha_background = parse_color("--background", argv[++i]);
        } else if (a == "--rotate" && need_arg(i, argc, a.c_str())) {
            const char* text = argv[++i];
            int deg = parse_int("--rotate", text, -360, 360);
            if (deg % 90 != 0) die(std::string("--rotate expects 0, 90, 180 or 270 degrees (clockwise), got '") + text + "'");
            rotate_quarter_turns = deg / 90;
        } else if (a == "--flip-h") {
            flip_h = true;
        } else if (a == "--flip-v") {
            flip_v = true;
        } else if (a == "--ignore-exif") {
            opts.apply_exif_orientation = false;
        } else if (a == "--no-compress") {
            opts.compress = false;
        } else if (a == "--lookahead" && need_arg(i, argc, a.c_str())) {
            opts.lookahead = parse_int("--lookahead", argv[++i], 0, aic::limits::kMaxLookahead);
        } else if (a == "--anchor-dither") {
            opts.anchor_dither_to_source = true;
        } else if (a == "--lookahead-beam" && need_arg(i, argc, a.c_str())) {
            opts.lookahead_beam = parse_int("--lookahead-beam", argv[++i], aic::limits::kMinBeam, aic::limits::kMaxBeam);
        } else if (a == "--line-start-modify") {
            opts.line_start_modify = true;
        } else if (a == "--lace") {
            opts.lace = true;
        } else if (a == "--no-lace") {
            opts.lace = false;
        } else if (a == "--preview" && need_arg(i, argc, a.c_str())) {
            opts.preview_png_path = argv[++i];
        } else if (a == "--ham-map" && need_arg(i, argc, a.c_str())) {
            opts.ham_map_png_path = argv[++i];
        } else if (a == "--error-map" && need_arg(i, argc, a.c_str())) {
            opts.error_map_png_path = argv[++i];
        } else if (a == "--canvas-png" && need_arg(i, argc, a.c_str())) {
            opts.canvas_png_path = argv[++i];
        } else {
            std::cerr << "error: unknown argument '" << a << "'\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (input_path.empty() || output_path.empty()) {
        std::cerr << "error: --input and --output are required\n\n";
        print_usage(argv[0]);
        return 1;
    }

    // Mirror first, then rotate - the same order the GUI buttons compose in.
    aic::ImageTransform user_transform;
    if (flip_h) user_transform = user_transform.then(aic::ImageTransform::flip_horizontal());
    if (flip_v) user_transform = user_transform.then(aic::ImageTransform::flip_vertical());
    opts.source_transform = user_transform.then(aic::ImageTransform::rotate_cw(rotate_quarter_turns));

    aic::ConversionPipeline pipeline;
    aic::ConversionResult result = pipeline.convert(input_path, output_path, opts);

    if (!result.success) {
        std::cerr << "conversion failed: " << result.error_message << "\n";
        return 1;
    }

    if (!save_palette_path.empty()) {
        std::string save_err;
        if (!aic::save_palette(result.palette, save_palette_path, &save_err)) {
            std::cerr << "error: failed to save palette: " << save_err << "\n";
            return 1;
        }
    }

    std::cout << "wrote " << output_path
              << " (" << opts.target_width << "x" << opts.target_height
              << (opts.source_transform.is_identity() ? "" : ", transformed")
              << (opts.crop.has_value() ? ", crop=on" : "")
              << ", dither=" << opts.ditherer_name
              << (opts.line_start_modify ? ", line-start-modify=on" : "")
              << (opts.anchor_dither_to_source ? ", anchor-dither=on" : "")
              << (opts.lookahead > 1 ? ", lookahead=" + std::to_string(opts.lookahead) : std::string())
              << ", dither-strength=" << static_cast<int>(opts.dither_strength * 100.f + 0.5f) << "%"
              << ", quantizer=" << opts.quantizer_name
              << ", importance=" << opts.importance_name
              << ", palette-iterations=" << opts.palette_refinement_iterations
              << ", chroma-weight=" << opts.chroma_weight
              << ", lace=" << (result.lace ? "on" : "off")
              << ", mean OKLab error=" << result.mean_oklab_error;
    if (result.metrics.valid) {
        // Perceptual figures (aic/quality_metrics.hpp). After the mean error, which scripts parse.
        std::cout << ", blurred error=" << result.metrics.blurred_error << ", SSIM=" << result.metrics.ssim;
    }
    std::cout << ")\n";
    return 0;
}
