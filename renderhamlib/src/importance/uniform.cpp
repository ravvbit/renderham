// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/importance/uniform.hpp"

#include "aic/registry.hpp"

namespace aic {
static AutoRegister<IImportanceMap, UniformImportance> _reg("uniform");
}  // namespace aic
