// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/palette_io.hpp
//
// Standalone palette files, for reuse across conversions (and, later,
// across the frames of an animation - see the "Lock Palette" GUI
// feature this exists to support).
//
// Three formats:
//  - ILBM (.iff, .ilbm, .lbm): the native one. Not a bespoke format: a saved
//    palette is a genuine, minimal IFF FORM ILBM (a 1x1 placeholder bitmap plus
//    the real CMAP) written through the same write_ham6_ilbm() this project
//    already uses for real output, so it opens in any real Amiga ILBM viewer too.
//  - JASC PAL (.pal): the text palette of Paint Shop Pro, also read by many
//    other tools ("JASC-PAL", "0100", the number of colors, one "R G B" line each).
//  - GIMP palette (.gpl): GIMP's, Inkscape's and Krita's text palette
//    ("GIMP Palette", header lines, then "R G B name" lines).
// The two text formats hold 8-bit values; the Amiga's 4-bit colors are written
// as v*17 (so $F80 becomes 255 136 0) and read back by rounding to the nearest
// 4-bit value, which makes a round trip exact.
//
// Reading recognizes the format by the file's content, not by its name. Writing
// picks the format from the file name's extension (see palette_format_for_path),
// or takes it explicitly.

#include <string>
#include <vector>

#include "aic/palette.hpp"

namespace aic {

enum class PaletteFileFormat { Ilbm, JascPal, GimpGpl };

// One kind of palette file; supported_palette_formats() is the list the GUI builds its
// file dialogs from.
struct PaletteFormatInfo {
    PaletteFileFormat id;
    const char* name;                      // for people: "IFF ILBM"
    std::vector<std::string> extensions;   // lower case, without the dot; the first is the default
};
const std::vector<PaletteFormatInfo>& supported_palette_formats();

// The format a file with this name is written in: by extension, ignoring case
// (.pal = JASC, .gpl = GIMP, anything else - .iff, .ilbm, no extension, ... - ILBM).
PaletteFileFormat palette_format_for_path(const std::string& path);

// Writes `palette` in the format that palette_format_for_path(path) names. Returns true on
// success; writes error_out on failure.
bool save_palette(const Palette& palette, const std::string& path, std::string* error_out = nullptr);

// Same, in an explicitly chosen format (whatever the extension says).
bool save_palette(const Palette& palette, const std::string& path, PaletteFileFormat format,
                  std::string* error_out = nullptr);

// Reads a palette from an ILBM file (as written by save_palette, or any other standard
// ILBM - real Amiga art is fair game too: its CMAP chunk), a JASC PAL file or a GIMP
// palette. Requires exactly 16 colors, matching HAM6's base palette size. Returns true on
// success; writes error_out on failure (missing file, unknown format, no CMAP chunk, a line
// that is not a color - the message names the line - or the wrong color count).
bool load_palette(const std::string& path, Palette* out, std::string* error_out = nullptr);

}  // namespace aic
