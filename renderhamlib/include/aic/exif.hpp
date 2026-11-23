// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/exif.hpp - just enough EXIF to find the orientation tag of a JPEG.
//
// stb_image ignores EXIF, so a portrait photo from a phone (stored sideways
// with orientation = 6) used to be converted lying on its side. Only the
// orientation is read; everything else is ignored.

#include <cstddef>
#include <cstdint>
#include <string>

namespace aic {

// Scans the JPEG segments in `data` for an Exif APP1 block and returns the
// orientation tag (1..8). Returns 1 (= upright, no change) when there is no
// EXIF block, no orientation tag, a value outside 1..8, or the data is
// malformed/truncated. Never reads past `size`.
int parse_exif_orientation(const uint8_t* data, size_t size);

// Reads the first part of the file (the EXIF block always sits near the
// start of a JPEG) and returns parse_exif_orientation() of it; 1 if the file
// cannot be read or is not a JPEG.
int read_exif_orientation(const std::string& path);

}  // namespace aic
