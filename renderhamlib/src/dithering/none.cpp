// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/dithering/none.hpp"

#include "aic/registry.hpp"

namespace aic {
static AutoRegister<IErrorDiffuser, NoneDiffuser> _reg("none");
}  // namespace aic
