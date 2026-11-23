// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// aic/sequence.hpp - render the frames of an animation, on several threads.
//
// The frames of a crop animation are the same picture under different crop
// boxes, all encoded with ONE palette (frame 0's, so the sequence does not
// flicker between slightly different "optimal" palettes). That makes the
// frames independent of each other as soon as frame 0 is done, so everything
// after it can run in parallel - and since a frame's result depends only on its
// own crop box and that palette, the output is bit-for-bit the same whether it
// is rendered on one thread or on eight (the test suite checks exactly that).
//
// Order of work: the source is loaded once (not once per frame); frame 0 is
// rendered first, alone; its actual palette is locked for frames 1..N-1, which
// then are spread over the worker threads. If the caller already locked a
// palette (ConversionOptions::locked_palette) there is nothing to wait for and
// all frames run in parallel from the start.

#include <atomic>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "aic/crop.hpp"
#include "aic/pipeline.hpp"

namespace aic {

struct SequenceOptions {
    // Number of threads that render frames at the same time; 0 = one per
    // hardware thread. 1 gives the plain sequential behavior.
    int threads = 0;

    // Attach the pattern of an ordered ditherer to the picture for every frame, so that it
    // travels with the content instead of flickering over it (see
    // ConversionOptions::anchor_dither_to_source). On by default: all frames of a sequence
    // show the same picture, which is exactly what this is for. Turn it off to get frames
    // that are identical to separately converted ones.
    bool anchor_dither = true;

    // Polled between scanlines. Set it (from any thread) to stop the sequence
    // as soon as possible; render_sequence() then returns with cancelled set.
    const std::atomic<bool>* cancel = nullptr;

    // Called for every finished frame, with the frame's index in `crops`. Frames
    // other than the first finish in no particular order. Calls are serialised
    // (never two at once), so the callback may write files or fill a vector
    // that was sized beforehand without a lock of its own. It may move out of
    // `frame` what it wants to keep. Return an empty string to go on, or an
    // error message to stop the whole sequence with that error.
    std::function<std::string(std::size_t index, PreviewResult& frame)> on_frame;

    // Overall progress, 0..1: finished frames plus the partial progress of the
    // running ones. Called from the worker threads, possibly several at the
    // same time and very often (once per scanline): keep it to an atomic store.
    std::function<void(float fraction)> on_progress;
};

struct SequenceResult {
    bool success = false;
    bool cancelled = false;        // stopped through SequenceOptions::cancel
    std::string error_message;     // set when !success && !cancelled
    std::size_t failed_index = 0;  // frame the error belongs to (lowest, if several failed)
    std::size_t frames_done = 0;   // frames that finished and were passed to on_frame
    // The palette the whole sequence was encoded with: frame 0's palette (the
    // locked one if the caller gave one, with fixed colors applied).
    std::optional<Palette> palette;
};

// Renders one frame per entry of `crops` (the crop of base.crop is ignored) and
// reports each through options.on_frame. base is used for every frame, as is,
// except for the crop and - from frame 1 on - the locked palette. Thread-safe
// with respect to `pipeline`, which is only used through its const interface.
SequenceResult render_sequence(const ConversionPipeline& pipeline, const std::string& input_path,
                               const ConversionOptions& base, const std::vector<CropSpec>& crops,
                               const SequenceOptions& options = {});

}  // namespace aic
