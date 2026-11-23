// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/ilbm_writer.hpp
//
// Writes a standard IFF ILBM file (FORM/ILBM: BMHD, CMAP, CAMG, BODY)
// with the CAMG viewport-mode HAM bit set, so any existing Amiga ILBM
// viewer (Deluxe Paint, Multiview, etc.) can load it without knowing
// anything about this project.

#include <string>

#include "aic/ham6_encoder.hpp"

namespace aic {

struct IlbmWriteOptions {
    bool compress = true;  // ByteRun1 (IFF's standard RLE)
    bool lace = false;     // sets CAMG's LACE (interlace) viewport-mode bit
};

// Returns true on success; writes error_out on failure.
bool write_ham6_ilbm(const HAM6Frame& frame, const std::string& path,
                      const IlbmWriteOptions& opts = {}, std::string* error_out = nullptr);

}  // namespace aic
