// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/dithering/checks.hpp
//
// "Checks" dithering: an ordered dither with exactly two threshold
// phases, which nearly doubles the number of shades per channel (16 ->
// 31 for the 4-bit Amiga lattice) by alternating the two neighboring
// shades around every intermediate value. The offset is a quarter of
// one 4-bit step in each direction: a value halfway between two levels
// rounds up in one phase and down in the other (average = the half
// step), values within a quarter step of a level never dither.
//
// The three variants differ only in how the two phases are laid out:
//
//   checks-lines        phase flips from LINE to line, constant within a
//                       line. This is the HAM-specific one: pixels of
//                       different lines are independent in HAM, only
//                       pixels of the same line depend on each other, so
//                       an offset that is the same for the whole line adds
//                       no horizontal pixel-to-pixel variation - nothing
//                       that would force extra Modify chains or fringing.
//   checks-lines-mixed  like checks-lines, but the green channel flips the
//                       opposite way from red and blue. Green carries most
//                       of the luminance, so the line-to-line brightness
//                       flicker drops to ~43% of the plain variant; in
//                       exchange there is a slight line-to-line tint shift.
//   checks              classic checkerboard (x+y parity). Kept as the
//                       reference to compare the line variants against -
//                       it varies horizontally, which HAM dislikes.
//
// The rounding has to act per color channel on the 4-bit lattice, so these
// ditherers work through IErrorDiffuser::adjust_target (they see the
// pixel) rather than the pixel-blind bias(). They hand the encoder the
// lattice color the current phase rounds to; see checks.cpp for why this is
// not simply "target + offset".

#include <string>

#include "aic/dither.hpp"

namespace aic {

class ChecksDiffuser : public IErrorDiffuser {
public:
    enum class Pattern { Lines, LinesMixed, Checker };

    void begin(int, int) override {}
    OKLab bias(int, int) const override { return OKLab{0.f, 0.f, 0.f}; }  // all work is in adjust_target
    OKLab adjust_target(int x, int y, const OKLab& target) const override;
    void feedback(int, int, const OKLab&) override {}  // no propagation
    std::string name() const override { return name_; }
    void set_strength(float strength) override { strength_ = strength; }  // 0 = none ... 1 = recommended mix
    void set_pattern_origin(int ox, int oy) override { ox_ = ox; oy_ = oy; }

protected:
    ChecksDiffuser(Pattern pattern, std::string name) : pattern_(pattern), name_(std::move(name)) {}

private:
    Pattern pattern_;
    std::string name_;
    float strength_ = 1.0f;
    int ox_ = 0, oy_ = 0;
};

class ChecksLinesDiffuser : public ChecksDiffuser {
public:
    ChecksLinesDiffuser() : ChecksDiffuser(Pattern::Lines, "checks-lines") {}
};

class ChecksLinesMixedDiffuser : public ChecksDiffuser {
public:
    ChecksLinesMixedDiffuser() : ChecksDiffuser(Pattern::LinesMixed, "checks-lines-mixed") {}
};

class ChecksCheckerDiffuser : public ChecksDiffuser {
public:
    ChecksCheckerDiffuser() : ChecksDiffuser(Pattern::Checker, "checks") {}
};

}  // namespace aic
