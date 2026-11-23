// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
#include "aic/dithering/kernel_diffuser.hpp"

namespace aic {

// Bill Atkinson's dithering algorithm (original Macintosh). Diffuses
// only 6/8 = 75% of the residual error - the rest is simply dropped.
// That deliberate loss is what gives Atkinson its characteristic higher
// contrast, "cleaner" look compared to full-error methods like
// Floyd-Steinberg, and - relevant here - injects less error into
// neighboring pixels overall, which can mean fewer forced multi-channel
// HAM6 jumps (see the fringing discussion in the design notes).
class AtkinsonDiffuser : public KernelErrorDiffuser {
public:
    AtkinsonDiffuser()
        : KernelErrorDiffuser({{1, 0, 1.f / 8}, {2, 0, 1.f / 8},
                                {-1, 1, 1.f / 8}, {0, 1, 1.f / 8}, {1, 1, 1.f / 8},
                                {0, 2, 1.f / 8}},
                               "atkinson") {}
};

}  // namespace aic
