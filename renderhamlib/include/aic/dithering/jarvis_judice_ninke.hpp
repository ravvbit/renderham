// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
#include "aic/dithering/kernel_diffuser.hpp"

namespace aic {

// Jarvis-Judice-Ninke: a wide 12-tap kernel spread over the current and
// next TWO rows. Produces the smoothest gradients of the diffusion
// ditherers here (most closely approximates true error minimization),
// at the cost of the widest error reach - worth comparing directly
// against Floyd-Steinberg/Sierra Lite on HAM6 fringing, since spreading
// error further means more neighboring pixels can be pushed toward a
// multi-channel jump.
class JarvisJudiceNinkeDiffuser : public KernelErrorDiffuser {
public:
    JarvisJudiceNinkeDiffuser()
        : KernelErrorDiffuser({{1, 0, 7.f / 48}, {2, 0, 5.f / 48},
                                {-2, 1, 3.f / 48}, {-1, 1, 5.f / 48}, {0, 1, 7.f / 48}, {1, 1, 5.f / 48}, {2, 1, 3.f / 48},
                                {-2, 2, 1.f / 48}, {-1, 2, 3.f / 48}, {0, 2, 5.f / 48}, {1, 2, 3.f / 48}, {2, 2, 1.f / 48}},
                               "jarvis-judice-ninke") {}
};

}  // namespace aic
