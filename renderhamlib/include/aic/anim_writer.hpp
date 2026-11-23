// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/anim_writer.hpp
//
// Writes a genuine Amiga IFF FORM ANIM (multi-frame, delta-compressed)
// from a sequence of HAM6 frames, using ANIM-5 ("byte vertical delta",
// devised by Jim Kent) - the most common and most widely supported
// ANIM compression mode. See the README for the exact encoding and how
// it was independently verified (a from-scratch decoder that
// reconstructs every frame from the written file and compares it
// byte-for-byte against the source frames).

#include <string>
#include <vector>

#include "aic/ham6_encoder.hpp"

namespace aic {

// ANIM timing is counted in jiffies of 1/60 s (ANHD reltime).
constexpr int kAnimJiffiesPerSecond = 60;

// Jiffies per frame for a playback speed in frames per second: 60/fps rounded to
// the nearest whole jiffy, at least 1 (the fastest speed an ANIM can express is one
// frame per jiffy = 60 fps). fps is clamped to 1..60. Speeds that divide 60 (60, 30,
// 20, 15, 12, 10, 6, 5, 4, 3, 2, 1) come out exact; any other is the nearest
// representable one.
int anim_jiffies_for_fps(int fps);

struct AnimWriteOptions {
    bool compress = true;  // ByteRun1 for the first (full) frame's BODY
    bool lace = false;     // sets CAMG's LACE (interlace) viewport-mode bit

    // Per-frame display duration, in jiffies (1/60 second) - the unit
    // real ANIM players read directly. 2 = 30fps, the Amiga's
    // traditional ANIM playback rate.
    int reltime_jiffies = 2;
};

// `frames` must have at least 2 entries, and every frame must share
// the same width, height, and 16-color palette (this project's
// RunAnimationExport already guarantees the palette across an
// animation's frames; width/height come from one fixed target
// resolution). Returns true on success; writes error_out on failure,
// including if any frame's dimensions/palette don't match frame 0's.
bool write_ham6_anim(const std::vector<HAM6Frame>& frames, const std::string& path, const AnimWriteOptions& opts,
                      std::string* error_out = nullptr);

}  // namespace aic
