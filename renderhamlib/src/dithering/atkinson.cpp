// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/dithering/atkinson.hpp"

#include "aic/registry.hpp"

namespace aic {
static AutoRegister<IErrorDiffuser, AtkinsonDiffuser> _reg("atkinson");
}  // namespace aic
