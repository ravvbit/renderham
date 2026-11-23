// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/palette_io.hpp
//
// Standalone palette files, for reuse across conversions (and, later,
// across the frames of an animation - see the "Lock Palette" GUI
// feature this exists to support). Not a bespoke format: a saved
// palette is a genuine, minimal IFF FORM ILBM (a 1x1 placeholder
// bitmap plus the real CMAP) written through the same write_ham6_ilbm()
// this project already uses for real output, so a saved palette file
// opens in any real Amiga ILBM viewer too, not just this tool.

#include <string>

#include "aic/palette.hpp"

namespace aic {

// Writes `palette` as a minimal valid ILBM file. Returns true on
// success; writes error_out on failure.
bool save_palette(const Palette& palette, const std::string& path, std::string* error_out = nullptr);

// Reads the CMAP chunk from an IFF FORM ILBM file (as written by
// save_palette, or any other standard ILBM - real Amiga art is fair
// game too). Requires exactly 16 colors, matching HAM6's base palette
// size. Returns true on success; writes error_out on failure (missing
// file, not an ILBM, no CMAP chunk, wrong color count).
bool load_palette(const std::string& path, Palette* out, std::string* error_out = nullptr);

}  // namespace aic
