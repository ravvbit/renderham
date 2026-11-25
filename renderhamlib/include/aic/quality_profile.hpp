// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/quality_profile.hpp
//
// Three ready-made bundles of the settings that trade run time against quality - the palette
// quantizer, the number of palette refinement passes and the lookahead of the HAM encoder -
// so a frontend can offer "Fast / Standard / Best" instead of three separate controls.
// A profile only sets those four fields of ConversionOptions (quantizer_name,
// palette_refinement_iterations, lookahead, lookahead_beam); everything else (dithering, size,
// color correction, ...) is left as it is.
//
// The figures behind the choice (320x256, Release build, wall time of a whole run, best of three;
// 4 photographs / 7 further test pictures (textures, a test chart, ...); see README):
//   fast      kmeans, 1 pass            the same error as standard (photos +-0.0 %, others -1.9 to
//                                       +0.3 %), about a quarter less time (0.73x / 0.81x): one pass does
//                                       nearly all of what the later passes do
//   standard  kmeans, 4 passes          the library defaults
//   best      kmeans, 8 passes, look 4  photos: 3.6 to 6.6 % less error (mean 4.8 %), SSIM +0.006, about
//                                       7 times the time; the other pictures: 0.9 % less on average
//                                       (from -4.1 to +0.3 %), about 7 times the time

#include <optional>
#include <string>
#include <vector>

#include "aic/pipeline.hpp"

namespace aic {

enum class QualityProfile { Fast, Standard, Best };

struct QualityProfileInfo {
    QualityProfile id;
    const char* key;   // the name on the command line: "fast", "standard", "best"
    const char* name;  // for people (English; a GUI translates it itself): "Fast"
    // The settings. lookahead 0 = off (0 and 1 both mean greedy).
    const char* quantizer;
    int palette_refinement_iterations;
    int lookahead;
    int lookahead_beam;
};

// All profiles, fastest first.
const std::vector<QualityProfileInfo>& quality_profiles();

// By key ("fast", "standard", "best"); nullptr if there is no such profile.
const QualityProfileInfo* find_quality_profile(const std::string& key);
const QualityProfileInfo& quality_profile(QualityProfile id);

// Sets the four fields of `options` to the profile's values.
void apply_quality_profile(const QualityProfileInfo& profile, ConversionOptions* options);

// The profile whose four settings `options` has exactly, if any (a GUI shows "Custom" otherwise).
// A lookahead of 1 counts as off, like the encoder treats it.
std::optional<QualityProfile> matching_quality_profile(const ConversionOptions& options);

}  // namespace aic
