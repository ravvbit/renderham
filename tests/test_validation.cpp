// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
// Input validation: library (validate_options, writers) and command line.
//
// Regression tests for defects found by probing renderHAM with degenerate
// input: --width 0 / -5 wrote a bogus file with exit 0, --width 100000
// wrote a corrupt IFF header (100000 mod 65536), "--dither-strength abc"
// silently meant 0 %. All of that must now fail with a clear message.

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "aic/aic.hpp"

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << "\n";
        ++g_failures;
    }
}

struct RunResult {
    int exit_code = -1;
    std::string output;  // stdout + stderr
};

RunResult run_cli(const std::string& args) {
    RunResult r;
    std::string cmd = std::string("\"") + RENDERHAM_CLI + "\" " + args + " 2>&1";
    FILE* pipe = POPEN(cmd.c_str(), "r");
    if (!pipe) return r;
    char buf[512];
    while (std::fgets(buf, sizeof buf, pipe)) r.output += buf;
    int status = PCLOSE(pipe);
#ifdef _WIN32
    r.exit_code = status;
#else
    r.exit_code = (status >> 8) & 0xff;
#endif
    return r;
}

void expect_rejected(const std::string& extra_args, const std::string& must_mention) {
    RunResult r = run_cli(std::string("-i \"") + RENDERHAM_TESTDATA + "/test_input.png\" -o \"" + RENDERHAM_TMPFILE +
                          "\" " + extra_args);
    check(r.exit_code == 1, "'" + extra_args + "' must exit with 1, got " + std::to_string(r.exit_code));
    check(r.output.find(must_mention) != std::string::npos,
          "'" + extra_args + "' message must mention '" + must_mention + "', got: " + r.output);
    check(r.output.find("wrote ") == std::string::npos, "'" + extra_args + "' must not report success");
}

}  // namespace

int main() {
    // ---- library ----
    {
        aic::ConversionOptions o;
        check(aic::validate_options(o).empty(), "default options are valid");

        o = {}; o.target_width = 0;
        check(!aic::validate_options(o).empty(), "width 0 rejected");
        o = {}; o.target_height = -5;
        check(!aic::validate_options(o).empty(), "height -5 rejected");
        o = {}; o.target_width = 100000;
        check(!aic::validate_options(o).empty(), "width 100000 rejected");
        o = {}; o.target_width = aic::limits::kMaxDimension;
        check(aic::validate_options(o).empty(), "maximum width accepted");
        o = {}; o.dither_strength = 5.0f;
        check(!aic::validate_options(o).empty(), "dither strength 500 % rejected");
        o = {}; o.chroma_weight = std::nanf("");
        check(!aic::validate_options(o).empty(), "NaN chroma weight rejected");
        o = {}; o.lookahead = 99;
        check(!aic::validate_options(o).empty(), "lookahead 99 rejected");
        o = {}; o.lookahead_beam = 0;
        check(!aic::validate_options(o).empty(), "beam 0 rejected");
        o = {}; o.palette_refinement_iterations = -3;
        check(!aic::validate_options(o).empty(), "negative iteration count rejected");

        // The pipeline refuses before it loads or writes anything.
        aic::ConversionPipeline pipeline;
        o = {}; o.target_width = 0;
        auto pv = pipeline.preview(std::string(RENDERHAM_TESTDATA) + "/test_input.png", o);
        check(!pv.success && pv.error_message.find("target width") != std::string::npos,
              "preview() reports the invalid width, got: " + pv.error_message);

        std::filesystem::remove(RENDERHAM_TMPFILE);
        o = {}; o.target_width = 100000;
        auto cv = pipeline.convert(std::string(RENDERHAM_TESTDATA) + "/test_input.png", RENDERHAM_TMPFILE, o);
        check(!cv.success, "convert() with width 100000 fails");
        check(!std::filesystem::exists(RENDERHAM_TMPFILE), "no file is written for width 100000");

        // The writer protects itself as well (public library API).
        aic::HAM6Frame wide;
        wide.width = 70000;
        wide.height = 1;
        wide.palette.colors.assign(16, aic::AmigaColor{});
        std::string err;
        check(!aic::write_ham6_ilbm(wide, RENDERHAM_TMPFILE, {}, &err) && !err.empty(),
              "ILBM writer rejects width 70000 (16-bit header)");
    }

    // ---- command line ----
    expect_rejected("--width 0", "--width");
    expect_rejected("--width -5", "--width");
    expect_rejected("--width 100000", "--width");
    expect_rejected("--height 0", "--height");
    expect_rejected("--width abc", "--width");
    expect_rejected("--width 12x", "--width");
    expect_rejected("--dither-strength abc", "--dither-strength");
    expect_rejected("--dither-strength 500", "--dither-strength");
    expect_rejected("--dither-strength -1", "--dither-strength");
    expect_rejected("--lookahead 99", "--lookahead");
    expect_rejected("--lookahead-beam 0", "--lookahead-beam");
    expect_rejected("--palette-iterations -3", "--palette-iterations");
    expect_rejected("--gamma nan", "--gamma");
    expect_rejected("--gamma 0", "--gamma");
    expect_rejected("--pixel-aspect 0", "--pixel-aspect");
    expect_rejected("--fix-color0 300,0,0", "--fix-color0");
    expect_rejected("--fix-color0 1,2", "--fix-color0");
    expect_rejected("--fix-color0 1,2,3x", "--fix-color0");
    expect_rejected("--fit crop", "--fit");
    expect_rejected("--rotate 45", "--rotate");
    expect_rejected("--rotate abc", "--rotate");
    expect_rejected("--rotate 450", "--rotate");
    expect_rejected("--background xyz", "--background");
    expect_rejected("--background 1,2", "--background");
    expect_rejected("--background \"#12345\"", "--background");  // quoted: an unquoted # starts a shell comment

    {  // missing argument: one clear message, not a second "unknown argument"
        RunResult r = run_cli("-i x.png -o y.iff --width");
        check(r.exit_code == 1, "missing argument exits with 1");
        check(r.output.find("requires an argument") != std::string::npos, "missing argument is named");
        check(r.output.find("unknown argument") == std::string::npos, "no misleading 'unknown argument'");
    }

    {  // good paths still work
        RunResult v = run_cli("--version");
        check(v.exit_code == 0 && v.output.rfind("renderHAM ", 0) == 0, "--version prints 'renderHAM <ver>'");

        std::filesystem::remove(RENDERHAM_TMPFILE);
        RunResult ok = run_cli(std::string("-i \"") + RENDERHAM_TESTDATA + "/test_input.png\" -o \"" +
                               RENDERHAM_TMPFILE + "\" --width 321 --height 100 --dither-strength 50");
        check(ok.exit_code == 0, "valid conversion succeeds: " + ok.output);
        std::ifstream f(RENDERHAM_TMPFILE, std::ios::binary);
        std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        auto pos = data.find("BMHD");
        bool header_ok = false;
        if (pos != std::string::npos && data.size() >= pos + 12) {
            auto u8 = [&](size_t i) { return static_cast<uint8_t>(data[i]); };
            int w = (u8(pos + 8) << 8) | u8(pos + 9);
            int h = (u8(pos + 10) << 8) | u8(pos + 11);
            header_ok = (w == 321 && h == 100);
        }
        check(header_ok, "BMHD of the valid file says 321x100");
    }

    std::filesystem::remove(RENDERHAM_TMPFILE);
    if (g_failures) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "validation: all checks passed\n";
    return 0;
}
