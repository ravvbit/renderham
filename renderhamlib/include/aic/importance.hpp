// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/importance.hpp
//
// A per-pixel weight map, independent of any particular quantizer or
// encoder. The same weights feed BOTH a weighted palette seed (see
// MedianCutQuantizer) AND HAM6Encoder's iterative refinement loop, so a
// strategy like "center matters more" only needs to be implemented once.

#include <string>
#include <vector>

#include "aic/image.hpp"

namespace aic {

class IImportanceMap {
public:
    virtual ~IImportanceMap() = default;

    // Returns width*height weights (row-major, matching Image), all > 0.
    virtual std::vector<float> compute(const Image& image) const = 0;
    virtual std::string name() const = 0;
};

}  // namespace aic
