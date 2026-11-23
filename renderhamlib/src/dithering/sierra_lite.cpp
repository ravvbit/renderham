// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/dithering/sierra_lite.hpp"

#include "aic/registry.hpp"

namespace aic {
static AutoRegister<IErrorDiffuser, SierraLiteDiffuser> _reg("sierra-lite");
}  // namespace aic
