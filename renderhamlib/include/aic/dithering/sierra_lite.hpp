// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
#include "aic/dithering/kernel_diffuser.hpp"

namespace aic {

// Sierra Lite (aka "Sierra-2-4A"): a lightweight 3-tap alternative to
// Floyd-Steinberg with the same total error diffused (weights sum to
// 1.0), just distributed more narrowly. Often a good practical
// middle-ground: cheaper and less prone to the directional artifacts
// FS can show, without JJN/Stucki's wider (and more fringing-prone on
// HAM6) reach.
class SierraLiteDiffuser : public KernelErrorDiffuser {
public:
    SierraLiteDiffuser()
        : KernelErrorDiffuser({{1, 0, 2.f / 4}, {-1, 1, 1.f / 4}, {0, 1, 1.f / 4}}, "sierra-lite") {}
};

}  // namespace aic
