// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
// File names with non-ASCII characters (umlauts, accents, CJK). The library takes paths as UTF-8
// strings; this runs every place that opens a file with such a name, for reading and for writing.
// It matters on Windows, where the narrow file functions would read the bytes in the ANSI code page.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "aic/aic.hpp"

namespace fs = std::filesystem;

namespace {

int g_failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << "\n";
        ++g_failures;
    }
}

// The UTF-8 spellings used below ("Bücher", "Straße", Japanese for "Japan"), written as escapes so
// that this file does not depend on the source character set of the compiler.
const std::string kFolder = "B\xC3\xBC" "cher-\xE6\x97\xA5\xE6\x9C\xAC";
const std::string kSource = "Stra\xC3\x9F" "e-\xC3\xA9.jpg";
const std::string kIlbm = "Ausgabe-\xC3\xBC.iff";
const std::string kAnim = "Anim-\xC3\xB6.anim";
const std::string kPng = "Bild-\xC3\xA4.png";
const std::string kPalette = "Farben-\xC3\x9F.pal";

fs::path to_path(const std::string& utf8) { return fs::u8path(utf8); }
std::string join(const std::string& dir, const std::string& name) { return dir + "/" + name; }
bool exists_nonempty(const std::string& utf8) {
    std::error_code ec;
    return fs::exists(to_path(utf8), ec) && fs::file_size(to_path(utf8), ec) > 0;
}

}  // namespace

int main() {
    const std::string testdata = RENDERHAM_TESTDATA;
    const std::string dir = join(RENDERHAM_TMPDIR, kFolder);
    std::error_code ec;
    fs::remove_all(to_path(dir), ec);
    fs::create_directories(to_path(dir), ec);
    check(!ec && fs::is_directory(to_path(dir)), "could not create the test folder " + dir);
    if (g_failures) return 1;

    // A JPEG with an EXIF orientation tag (6), copied to a name with non-ASCII characters.
    const std::string source = join(dir, kSource);
    fs::copy_file(to_path(testdata + "/exif_orient6.jpg"), to_path(source), fs::copy_options::overwrite_existing, ec);
    check(!ec, "could not copy the test image: " + ec.message());
    if (g_failures) return 1;

    // Reading: EXIF reader and image loader.
    check(aic::read_exif_orientation(source) == 6, "read_exif_orientation() opens a file with non-ASCII name");
    std::string error;
    auto loaded = aic::Image::load(source, &error);
    check(loaded.has_value(), "Image::load() opens a file with non-ASCII name: " + error);
    auto plain = aic::Image::load(testdata + "/exif_orient6.jpg");
    check(plain && loaded && loaded->width() == plain->width() && loaded->height() == plain->height(),
          "the picture read from the non-ASCII name is the same size as the original");

    // Writing: PNG (stb), ILBM, and the palette files; then reading them back.
    if (loaded) {
        const std::string png = join(dir, kPng);
        check(loaded->save_png(png) && exists_nonempty(png), "Image::save_png() writes a file with non-ASCII name");
        auto back = aic::Image::load(png, &error);
        check(back && back->width() == loaded->width() && back->height() == loaded->height(),
              "the PNG just written can be read back: " + error);
    }

    aic::ConversionPipeline pipeline;
    aic::ConversionOptions opts;
    opts.target_width = 64;
    const std::string ilbm = join(dir, kIlbm);
    aic::ConversionResult result = pipeline.convert(source, ilbm, opts);
    check(result.success, "convert() from and to non-ASCII names: " + result.error_message);
    check(exists_nonempty(ilbm), "the ILBM file is written under its non-ASCII name");

    aic::Palette from_ilbm;
    check(aic::load_palette(ilbm, &from_ilbm, &error), "load_palette() opens the ILBM: " + error);
    const std::string pal = join(dir, kPalette);
    check(aic::save_palette(from_ilbm, pal, &error) && exists_nonempty(pal), "save_palette() writes a .pal: " + error);
    aic::Palette from_pal;
    check(aic::load_palette(pal, &from_pal, &error), "load_palette() reads the .pal back: " + error);
    bool same = from_ilbm.colors.size() == from_pal.colors.size();
    for (size_t i = 0; same && i < from_ilbm.colors.size(); ++i) same = from_ilbm.colors[i] == from_pal.colors[i];
    check(same, "the palette survives the round trip");

    // Writing: ANIM (two frames of the same picture).
    aic::PreviewResult pv = pipeline.preview(source, opts);
    check(pv.success, "preview() from a non-ASCII name: " + pv.error_message);
    if (pv.success) {
        const std::string anim = join(dir, kAnim);
        std::vector<aic::HAM6Frame> frames{pv.frame, pv.frame};
        check(aic::write_ham6_anim(frames, anim, aic::AnimWriteOptions{}, &error) && exists_nonempty(anim),
              "write_ham6_anim() writes a file with non-ASCII name: " + error);
    }

    fs::remove_all(to_path(dir), ec);
    if (g_failures == 0) std::cout << "OK: non-ASCII file names\n";
    return g_failures == 0 ? 0 : 1;
}
