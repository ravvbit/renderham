// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
// Quality profiles (aic/quality_profile.hpp): Fast / Standard / Best.
//  - "standard" is exactly what a default-constructed ConversionOptions already has,
//  - every profile is a valid set of options (known quantizer, values inside the limits),
//  - a profile changes its four settings and nothing else,
//  - matching_quality_profile() recognizes a profile's settings, and nothing that deviates,
//  - on a test picture the profiles do what their names say: best < standard <= fast in error.

#include <algorithm>
#include <iostream>
#include <string>

#include "aic/aic.hpp"

namespace {

int g_failures = 0;
void check(bool ok, const std::string& what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << "\n";
        ++g_failures;
    }
}

}  // namespace

int main() {
    const auto& profiles = aic::quality_profiles();
    check(profiles.size() == 3, "there are three profiles");

    // --- standard = defaults ----------------------------------------------------------------
    const aic::ConversionOptions defaults;
    {
        aic::ConversionOptions o;
        aic::apply_quality_profile(aic::quality_profile(aic::QualityProfile::Standard), &o);
        check(o.quantizer_name == defaults.quantizer_name, "standard: quantizer is the library default");
        check(o.palette_refinement_iterations == defaults.palette_refinement_iterations, "standard: refinement passes are the library default");
        check(o.lookahead == defaults.lookahead, "standard: lookahead is the library default");
        check(o.lookahead_beam == defaults.lookahead_beam, "standard: lookahead beam is the library default");
    }

    // --- the table --------------------------------------------------------------------------
    for (const aic::QualityProfileInfo& p : profiles) {
        const std::string name = p.key;
        check(aic::find_quality_profile(name) == &p, name + ": found by its key");
        check(&aic::quality_profile(p.id) == &p, name + ": found by its id");
        check(std::string(p.name).size() > 0, name + ": has a display name");
        for (char c : name) check(!(c >= 'A' && c <= 'Z') && c != ' ', name + ": key is lower case without spaces");

        aic::ConversionOptions o;
        aic::apply_quality_profile(p, &o);
        check(aic::validate_options(o).empty(), name + ": options are valid (" + aic::validate_options(o) + ")");
        const auto quantizers = aic::Registry<aic::IPaletteQuantizer>::instance().names();
        check(std::find(quantizers.begin(), quantizers.end(), o.quantizer_name) != quantizers.end(), name + ": the quantizer exists");
        check(o.lookahead >= 0 && o.lookahead <= 4, name + ": lookahead is within what the GUI offers (0 = off, 2-4)");
        check(o.lookahead != 1, name + ": lookahead 1 is spelled 0");
        check(aic::matching_quality_profile(o) == p.id, name + ": recognized again after applying it");
    }
    check(aic::find_quality_profile("") == nullptr && aic::find_quality_profile("Fast") == nullptr &&
              aic::find_quality_profile("medium") == nullptr,
          "unknown keys are not found (keys are exact)");
    // Distinct, so a match is unambiguous.
    for (size_t i = 0; i < profiles.size(); ++i)
        for (size_t j = i + 1; j < profiles.size(); ++j) {
            aic::ConversionOptions a, b;
            aic::apply_quality_profile(profiles[i], &a);
            aic::apply_quality_profile(profiles[j], &b);
            check(aic::matching_quality_profile(a) != aic::matching_quality_profile(b), std::string(profiles[i].key) + " and " + profiles[j].key + " differ");
        }

    // --- a profile touches its four settings only ------------------------------------------------
    {
        aic::ConversionOptions o;
        o.target_width = 640;
        o.target_height = 400;
        o.ditherer_name = "atkinson";
        o.dither_strength = 0.5f;
        o.importance_name = "center";
        o.chroma_weight = 0.7f;
        o.compress = false;
        o.line_start_modify = true;
        o.color_correction.gamma = 1.4f;
        const aic::ConversionOptions before = o;
        aic::apply_quality_profile(aic::quality_profile(aic::QualityProfile::Best), &o);
        check(o.target_width == before.target_width && o.target_height == before.target_height, "profile leaves the size alone");
        check(o.ditherer_name == before.ditherer_name && o.dither_strength == before.dither_strength, "profile leaves the dithering alone");
        check(o.importance_name == before.importance_name && o.chroma_weight == before.chroma_weight, "profile leaves color focus and chroma weight alone");
        check(o.compress == before.compress && o.line_start_modify == before.line_start_modify, "profile leaves the file options alone");
        check(o.color_correction.gamma == before.color_correction.gamma, "profile leaves the color correction alone");
        check(o.lookahead == 4 && o.palette_refinement_iterations == 8, "but the profile's own settings are set");
    }

    // --- matching ----------------------------------------------------------------------------
    {
        aic::ConversionOptions o;  // = standard
        check(aic::matching_quality_profile(o) == aic::QualityProfile::Standard, "default options match standard");
        o.lookahead = 1;
        check(aic::matching_quality_profile(o) == aic::QualityProfile::Standard, "lookahead 1 counts as off");
        o.lookahead = 3;
        check(!aic::matching_quality_profile(o).has_value(), "lookahead 3 matches no profile");
        o = aic::ConversionOptions();
        o.palette_refinement_iterations = 3;
        check(!aic::matching_quality_profile(o).has_value(), "3 passes match no profile");
        o = aic::ConversionOptions();
        o.quantizer_name = "octree";
        check(!aic::matching_quality_profile(o).has_value(), "another quantizer matches no profile");
        o = aic::ConversionOptions();
        o.lookahead_beam = 16;
        check(!aic::matching_quality_profile(o).has_value(), "another beam width matches no profile");
        o = aic::ConversionOptions();
        o.dither_strength = 0.3f;  // not part of a profile
        check(aic::matching_quality_profile(o) == aic::QualityProfile::Standard, "settings outside the profile do not matter");
    }

    // --- they do what their names say --------------------------------------------------------------
    {
        const std::string input = std::string(RENDERHAM_TESTDATA) + "/test_input.png";
        aic::ConversionPipeline pipeline;
        double error[3] = {0, 0, 0};
        for (size_t i = 0; i < profiles.size(); ++i) {
            aic::ConversionOptions o;
            aic::apply_quality_profile(profiles[i], &o);
            const aic::PreviewResult r = pipeline.preview(input, o);
            check(r.success, std::string(profiles[i].key) + ": converts (" + r.error_message + ")");
            error[i] = r.mean_oklab_error;
        }
        std::cout << "mean OKLab error on test_input.png: fast " << error[0] << ", standard " << error[1] << ", best " << error[2] << "\n";
        check(error[2] < error[1], "best has less error than standard");
        check(error[1] <= error[0], "standard has no more error than fast");
    }

    if (g_failures) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "profiles: all checks passed\n";
    return 0;
}
