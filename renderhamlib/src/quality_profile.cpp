// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/quality_profile.hpp"

namespace aic {

const std::vector<QualityProfileInfo>& quality_profiles() {
    // "standard" must stay equal to the defaults of ConversionOptions (tests/test_profiles.cpp
    // checks it); change them together.
    static const std::vector<QualityProfileInfo> profiles = {
        {QualityProfile::Fast, "fast", "Fast", "kmeans", 1, 0, 8},
        {QualityProfile::Standard, "standard", "Standard", "kmeans", 4, 0, 8},
        {QualityProfile::Best, "best", "Best quality", "kmeans", 8, 4, 8},
    };
    return profiles;
}

const QualityProfileInfo* find_quality_profile(const std::string& key) {
    for (const QualityProfileInfo& p : quality_profiles())
        if (key == p.key) return &p;
    return nullptr;
}

const QualityProfileInfo& quality_profile(QualityProfile id) {
    for (const QualityProfileInfo& p : quality_profiles())
        if (p.id == id) return p;
    return quality_profiles().front();  // not reached: every id is in the table
}

void apply_quality_profile(const QualityProfileInfo& profile, ConversionOptions* options) {
    options->quantizer_name = profile.quantizer;
    options->palette_refinement_iterations = profile.palette_refinement_iterations;
    options->lookahead = profile.lookahead;
    options->lookahead_beam = profile.lookahead_beam;
}

std::optional<QualityProfile> matching_quality_profile(const ConversionOptions& options) {
    const int lookahead = options.lookahead <= 1 ? 0 : options.lookahead;
    for (const QualityProfileInfo& p : quality_profiles()) {
        if (options.quantizer_name == p.quantizer && options.palette_refinement_iterations == p.palette_refinement_iterations &&
            lookahead == p.lookahead && options.lookahead_beam == p.lookahead_beam)
            return p.id;
    }
    return std::nullopt;
}

}  // namespace aic
