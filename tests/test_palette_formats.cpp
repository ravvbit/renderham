// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
// Palette files (aic/palette_io.hpp): ILBM, JASC PAL and GIMP GPL.
//  - a round trip through each format gives back exactly the same 16 colors,
//  - files written by other programs (CRLF line ends, comments, names with spaces, a byte order
//    mark, values that are not multiples of 17) are read,
//  - the format is recognized by content, and chosen for writing by the file name's extension,
//  - damaged or wrong files are refused with a message that says what is wrong (and where).

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
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

std::string tmp(const std::string& name) { return std::string(RENDERHAM_TMPDIR) + "/" + name; }

void write_raw(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::string read_raw(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// 16 colors that exercise the extremes and the middle: black, white, every primary and secondary,
// and a few arbitrary ones.
aic::Palette sample_palette() {
    aic::Palette p;
    const int v[16][3] = {{0, 0, 0},   {15, 15, 15}, {15, 0, 0},  {0, 15, 0}, {0, 0, 15}, {15, 15, 0},
                          {0, 15, 15}, {15, 0, 15},  {15, 8, 0},  {1, 2, 3},  {4, 5, 6},  {7, 8, 9},
                          {10, 11, 12}, {13, 14, 15}, {9, 0, 6},  {2, 12, 7}};
    for (const auto& c : v)
        p.colors.push_back(aic::AmigaColor{static_cast<uint8_t>(c[0]), static_cast<uint8_t>(c[1]), static_cast<uint8_t>(c[2])});
    return p;
}

bool same(const aic::Palette& a, const aic::Palette& b) {
    if (a.colors.size() != b.colors.size()) return false;
    for (size_t i = 0; i < a.colors.size(); ++i)
        if (!(a.colors[i] == b.colors[i])) return false;
    return true;
}

// Does loading `path` fail, with a message that contains `needle`?
void check_refused(const std::string& path, const std::string& needle, const std::string& what) {
    aic::Palette out;
    std::string err;
    const bool ok = aic::load_palette(path, &out, &err);
    check(!ok, what + ": refused");
    check(err.find(needle) != std::string::npos, what + ": message mentions '" + needle + "' (was: '" + err + "')");
}

std::string jasc_with(const std::vector<std::string>& color_lines, const std::string& count = "16",
                      const std::string& version = "0100", const char* eol = "\r\n") {
    std::string s = std::string("JASC-PAL") + eol + version + eol + count + eol;
    for (const auto& l : color_lines) s += l + eol;
    return s;
}

std::vector<std::string> gray_lines(int n) {
    std::vector<std::string> lines;
    for (int i = 0; i < n; ++i) lines.push_back(std::to_string(i * 17) + " " + std::to_string(i * 17) + " " + std::to_string(i * 17));
    return lines;
}

}  // namespace

int main() {
    const aic::Palette sample = sample_palette();

    // --- which format does a file name mean? -------------------------------------------------
    check(aic::palette_format_for_path("a/b/colors.pal") == aic::PaletteFileFormat::JascPal, "extension .pal -> JASC");
    check(aic::palette_format_for_path("colors.PAL") == aic::PaletteFileFormat::JascPal, "extension .PAL (upper case) -> JASC");
    check(aic::palette_format_for_path("colors.gpl") == aic::PaletteFileFormat::GimpGpl, "extension .gpl -> GIMP");
    check(aic::palette_format_for_path("colors.Gpl") == aic::PaletteFileFormat::GimpGpl, "extension .Gpl -> GIMP");
    check(aic::palette_format_for_path("colors.iff") == aic::PaletteFileFormat::Ilbm, "extension .iff -> ILBM");
    check(aic::palette_format_for_path("colors.ilbm") == aic::PaletteFileFormat::Ilbm, "extension .ilbm -> ILBM");
    check(aic::palette_format_for_path("colors") == aic::PaletteFileFormat::Ilbm, "no extension -> ILBM");
    check(aic::palette_format_for_path("dir.pal/colors") == aic::PaletteFileFormat::Ilbm,
          "a dot in a folder name is not an extension");
    check(aic::palette_format_for_path("colors.txt") == aic::PaletteFileFormat::Ilbm, "unknown extension -> ILBM (as before)");

    // The table: every extension maps back to its own format, lower case, no dots, no repeats.
    {
        std::vector<std::string> seen;
        for (const aic::PaletteFormatInfo& f : aic::supported_palette_formats()) {
            check(!f.extensions.empty(), std::string(f.name) + ": has extensions");
            for (const std::string& e : f.extensions) {
                check(aic::palette_format_for_path("x." + e) == f.id, std::string(f.name) + ": ." + e + " maps to its own format");
                check(!e.empty() && e.find('.') == std::string::npos, "extension '" + e + "' has no dot");
                for (char c : e) check(!(c >= 'A' && c <= 'Z'), "extension '" + e + "' is lower case");
                for (const auto& s : seen) check(s != e, "extension '" + e + "' is listed once");
                seen.push_back(e);
            }
        }
    }

    // --- round trips ----------------------------------------------------------------------
    for (const char* name : {"rt.iff", "rt.pal", "rt.gpl"}) {
        const std::string path = tmp(name);
        std::string err;
        check(aic::save_palette(sample, path, &err), std::string(name) + ": saved (" + err + ")");
        aic::Palette back;
        check(aic::load_palette(path, &back, &err), std::string(name) + ": loaded (" + err + ")");
        check(same(sample, back), std::string(name) + ": the 16 colors come back exactly");
    }
    // Explicit format beats the extension, and the content (not the name) decides when reading.
    {
        const std::string path = tmp("explicit_but_named.iff");
        std::string err;
        check(aic::save_palette(sample, path, aic::PaletteFileFormat::GimpGpl, &err), "explicit GIMP format into a .iff name");
        check(read_raw(path).rfind("GIMP Palette", 0) == 0, "the file really is a GIMP palette");
        aic::Palette back;
        check(aic::load_palette(path, &back, &err) && same(sample, back), "read by content although named .iff");
    }

    // --- what the files look like -----------------------------------------------------------
    {
        const std::string jasc = read_raw(tmp("rt.pal"));
        check(jasc.rfind("JASC-PAL\r\n0100\r\n16\r\n", 0) == 0, "JASC header (with CRLF)");
        check(jasc.find("\r\n255 136 0\r\n") != std::string::npos, "$F80 is written as 255 136 0 (4-bit v * 17)");
        const std::string gpl = read_raw(tmp("rt.gpl"));
        check(gpl.rfind("GIMP Palette\nName: rt\nColumns: 4\n", 0) == 0, "GIMP header names the file");
        check(gpl.find("255 136   0\t$F80\n") != std::string::npos, "GIMP color line with the Amiga hex as its name");
    }

    // --- files from other programs ----------------------------------------------------------
    {
        // JASC with LF line ends, blank lines at the end, trailing spaces.
        aic::Palette p;
        write_raw(tmp("lf.pal"), jasc_with(gray_lines(16), "16", "0100", "\n") + "\n\n");
        check(aic::load_palette(tmp("lf.pal"), &p) && p.colors.size() == 16 && p.colors[5] == aic::AmigaColor{5, 5, 5},
              "JASC with LF ends and trailing blank lines");
        // Values that are not multiples of 17 are rounded: 8-bit 128 is 7.5 of 15 -> 8 (lround rounds half up).
        std::vector<std::string> odd = gray_lines(15);
        odd.push_back("128 127 1");
        write_raw(tmp("odd.pal"), jasc_with(odd));
        check(aic::load_palette(tmp("odd.pal"), &p) && p.colors[15] == aic::AmigaColor{8, 7, 0},
              "8-bit values are rounded to the nearest 4-bit value (128 127 1 -> 8 7 0)");
        // Byte order mark in front.
        write_raw(tmp("bom.pal"), "\xEF\xBB\xBF" + jasc_with(gray_lines(16)));
        check(aic::load_palette(tmp("bom.pal"), &p), "JASC with a UTF-8 byte order mark");
        // GIMP with comments, blank lines, names with spaces, tabs between the numbers, no Name/Columns.
        std::string gpl = "GIMP Palette\r\n# made by hand\r\n\r\n";
        for (int i = 0; i < 16; ++i)
            gpl += std::to_string(i * 17) + "\t" + std::to_string(255 - i * 17) + "  0 Color number " + std::to_string(i) + "\r\n";
        write_raw(tmp("hand.gpl"), gpl);
        check(aic::load_palette(tmp("hand.gpl"), &p) && p.colors[3] == aic::AmigaColor{3, 12, 0} && p.colors[15] == aic::AmigaColor{15, 0, 0},
              "GIMP palette with comments, tabs and names with spaces");
        // Colors without a name.
        std::string noname = "GIMP Palette\n";
        for (int i = 0; i < 16; ++i) noname += "0 0 " + std::to_string(i * 17) + "\n";
        write_raw(tmp("noname.gpl"), noname);
        check(aic::load_palette(tmp("noname.gpl"), &p) && p.colors[9] == aic::AmigaColor{0, 0, 9}, "GIMP colors without names");
    }

    // --- refused files ----------------------------------------------------------------------
    {
        write_raw(tmp("few.pal"), jasc_with(gray_lines(8), "8"));
        check_refused(tmp("few.pal"), "16-color palette", "JASC with 8 colors");
        write_raw(tmp("many.pal"), jasc_with(gray_lines(17), "17"));
        check_refused(tmp("many.pal"), "file has 17", "JASC with 17 colors");
        write_raw(tmp("short.pal"), jasc_with(gray_lines(12), "16"));
        check_refused(tmp("short.pal"), "has 12", "JASC that ends early");
        write_raw(tmp("extra.pal"), jasc_with(gray_lines(17), "16"));
        check_refused(tmp("extra.pal"), "Line 20", "JASC with more lines than announced");
        write_raw(tmp("ver.pal"), jasc_with(gray_lines(16), "16", "0200"));
        check_refused(tmp("ver.pal"), "version", "JASC with another version");
        write_raw(tmp("nocount.pal"), "JASC-PAL\r\n0100\r\nsixteen\r\n");
        check_refused(tmp("nocount.pal"), "Line 3", "JASC with a count that is no number");
        std::vector<std::string> bad = gray_lines(16);
        bad[6] = "10 20";
        write_raw(tmp("two.pal"), jasc_with(bad));
        check_refused(tmp("two.pal"), "Line 10", "JASC line with two numbers");
        bad[6] = "10 20 300";
        write_raw(tmp("big.pal"), jasc_with(bad));
        check_refused(tmp("big.pal"), "outside 0-255", "JASC value above 255");
        bad[6] = "10 -2 30";
        write_raw(tmp("neg.pal"), jasc_with(bad));
        check_refused(tmp("neg.pal"), "outside 0-255", "JASC negative value");
        bad[6] = "10 20 30 40";
        write_raw(tmp("four.pal"), jasc_with(bad));
        check_refused(tmp("four.pal"), "after the three color values", "JASC line with a fourth value");
        bad[6] = "ten 20 30";
        write_raw(tmp("word.pal"), jasc_with(bad));
        check_refused(tmp("word.pal"), "Line 10", "JASC line with a word");
        write_raw(tmp("head.pal"), "JASC-PAL\r\n");
        check_refused(tmp("head.pal"), "Truncated", "JASC with only the first header line");

        std::string g8 = "GIMP Palette\n";
        for (int i = 0; i < 8; ++i) g8 += "1 2 3 x\n";
        write_raw(tmp("few.gpl"), g8);
        check_refused(tmp("few.gpl"), "file has 8", "GIMP with 8 colors");
        std::string gbad = "GIMP Palette\nName: x\n";
        for (int i = 0; i < 16; ++i) gbad += (i == 4 ? std::string("1.5 2 3 half\n") : std::string("1 2 3 ok\n"));
        write_raw(tmp("float.gpl"), gbad);
        check_refused(tmp("float.gpl"), "Line 7", "GIMP with a fractional value");
        write_raw(tmp("empty.gpl"), "GIMP Palette\n");
        check_refused(tmp("empty.gpl"), "file has 0", "GIMP without colors");

        write_raw(tmp("text.txt"), "hello, this is no palette\n");
        check_refused(tmp("text.txt"), "Not a palette file", "plain text");
        write_raw(tmp("empty.bin"), "");
        check_refused(tmp("empty.bin"), "Not a palette file", "empty file");
        write_raw(tmp("tiny.bin"), "JA");
        check_refused(tmp("tiny.bin"), "Not a palette file", "two bytes");
        write_raw(tmp("huge.pal"), jasc_with(gray_lines(16)) + std::string(70 * 1024, ' '));
        check_refused(tmp("huge.pal"), "too large", "a 70 KB text file");
        check_refused(tmp("does_not_exist.pal"), "Could not open", "missing file");
        // An ILBM that is not a palette carrier keeps its old message.
        write_raw(tmp("form.iff"), std::string("FORM\0\0\0\4ILBM", 12));
        check_refused(tmp("form.iff"), "No CMAP", "ILBM without a CMAP chunk");
    }

    // --- writing refuses what it cannot write ----------------------------------------------
    {
        std::string err;
        check(!aic::save_palette(sample, tmp("no_such_dir/x.pal"), &err) && !err.empty(), "saving into a missing folder fails with a message");
        check(!aic::save_palette(sample, tmp("no_such_dir/x.gpl"), &err) && !err.empty(), "same for GIMP");
        aic::Palette short_palette;
        short_palette.colors.assign(8, aic::AmigaColor{});
        check(!aic::save_palette(short_palette, tmp("short_out.pal"), &err) && err.find("16 colors") != std::string::npos,
              "a palette that does not have 16 colors is not written");
    }

    if (g_failures) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "palette formats: all checks passed\n";
    return 0;
}
