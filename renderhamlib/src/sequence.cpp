// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "aic/sequence.hpp"

#include <algorithm>
#include <memory>
#include <mutex>
#include <thread>

namespace aic {

namespace {

// Everything the worker threads share. Lives on the stack of render_sequence(),
// which joins all threads before returning.
struct Shared {
    const ConversionPipeline& pipeline;
    const Image& source;
    const ConversionOptions& base;
    const std::vector<CropSpec>& crops;
    const SequenceOptions& options;

    // Per-frame progress, so the overall figure can be the sum / N.
    std::unique_ptr<std::atomic<float>[]> frame_progress;
    std::atomic<bool> stop{false};  // an error happened: the other frames give up quickly

    std::mutex mutex;  // guards the members below and serialises on_frame
    SequenceResult result;
    bool have_error = false;

    Shared(const ConversionPipeline& p, const Image& s, const ConversionOptions& b, const std::vector<CropSpec>& c,
           const SequenceOptions& o)
        : pipeline(p), source(s), base(b), crops(c), options(o), frame_progress(new std::atomic<float>[c.size()]) {
        for (std::size_t i = 0; i < c.size(); ++i) frame_progress[i].store(0.f);
    }

    bool cancelled() const { return options.cancel && options.cancel->load(std::memory_order_relaxed); }

    void report() {
        if (!options.on_progress) return;
        float sum = 0.f;
        for (std::size_t i = 0; i < crops.size(); ++i) sum += frame_progress[i].load(std::memory_order_relaxed);
        options.on_progress(std::min(1.f, sum / static_cast<float>(crops.size())));
    }

    void fail(std::size_t index, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex);
        // The lowest failing index wins, so that which error a caller sees does
        // not depend on thread timing.
        if (!have_error || index < result.failed_index) {
            have_error = true;
            result.failed_index = index;
            result.error_message = message;
        }
        stop.store(true);
    }

    // Renders frame `index` with `palette` locked (if given). Returns the
    // finished PreviewResult through `out`; false when the frame was not
    // produced (cancelled, or an error that has been recorded with fail()).
    bool render(std::size_t index, const std::optional<Palette>& palette, PreviewResult& out) {
        ConversionOptions opts = base;
        opts.crop = crops[index];
        opts.anchor_dither_to_source = options.anchor_dither;
        opts.quality_metrics = false;  // nobody looks at them per frame
        opts.keep_canvas = false;  // a sequence never needs the canvas, and it is as big as the frame
        if (palette) opts.locked_palette = palette;
        opts.progress = [this, index](float f) {
            frame_progress[index].store(f, std::memory_order_relaxed);
            report();
            return !stop.load(std::memory_order_relaxed) && !cancelled();
        };

        out = pipeline.preview_source(source, opts);
        if (out.cancelled) return false;
        if (!out.success) {
            fail(index, out.error_message);
            return false;
        }
        frame_progress[index].store(1.f, std::memory_order_relaxed);
        report();
        return true;
    }

    // Hands a finished frame to the caller. Serialised.
    void deliver(std::size_t index, PreviewResult& frame) {
        std::string err;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (stop.load()) return;  // another frame failed meanwhile: do not hand out more
            if (options.on_frame) err = options.on_frame(index, frame);
            if (err.empty()) {
                ++result.frames_done;
                return;
            }
        }
        fail(index, err);
    }
};

}  // namespace

SequenceResult render_sequence(const ConversionPipeline& pipeline, const std::string& input_path,
                               const ConversionOptions& base, const std::vector<CropSpec>& crops,
                               const SequenceOptions& options) {
    SequenceResult result;
    if (crops.empty()) {
        result.error_message = "no frames to render";
        return result;
    }
    if (std::string bad = validate_options(base); !bad.empty()) {
        result.error_message = "invalid option: " + bad;
        return result;
    }
    if (options.cancel && options.cancel->load()) {
        result.cancelled = true;
        return result;
    }

    std::string load_err;
    std::optional<Image> source = pipeline.load_source(input_path, base, &load_err);
    if (!source) {
        result.error_message = load_err;
        return result;
    }

    Shared shared(pipeline, *source, base, crops, options);
    const std::size_t n = crops.size();

    // Frame 0 first, on this thread: its palette is what all other frames lock.
    // With a palette locked by the caller, frame 0 is not special.
    std::optional<Palette> shared_palette = base.locked_palette;
    std::size_t first_parallel = 0;
    if (!shared_palette) {
        PreviewResult f0;
        if (shared.render(0, std::nullopt, f0)) {
            shared_palette = f0.frame.palette;
            shared.result.palette = f0.frame.palette;
            shared.deliver(0, f0);
        }
        first_parallel = 1;
    }

    if (!shared.stop.load() && !shared.cancelled() && first_parallel < n) {
        int threads = options.threads > 0 ? options.threads : static_cast<int>(std::thread::hardware_concurrency());
        threads = std::max(1, std::min<int>(threads, static_cast<int>(n - first_parallel)));

        std::atomic<std::size_t> next{first_parallel};
        auto worker = [&] {
            for (;;) {
                if (shared.stop.load() || shared.cancelled()) return;
                const std::size_t i = next.fetch_add(1);
                if (i >= n) return;
                PreviewResult pv;
                if (!shared.render(i, shared_palette, pv)) return;
                // With a caller-locked palette frame 0 is rendered here like any
                // other; its palette is the one to report.
                if (i == 0) {
                    std::lock_guard<std::mutex> lock(shared.mutex);
                    shared.result.palette = pv.frame.palette;
                }
                shared.deliver(i, pv);
            }
        };

        std::vector<std::thread> pool;
        pool.reserve(static_cast<std::size_t>(threads - 1));
        for (int t = 1; t < threads; ++t) pool.emplace_back(worker);
        worker();  // this thread works too
        for (std::thread& t : pool) t.join();
    }

    result = shared.result;
    if (shared.have_error) {
        result.success = false;
    } else if (result.frames_done == n) {
        result.success = true;  // even if a cancel arrived just after the last frame: everything was delivered
    } else {
        result.cancelled = true;
    }
    return result;
}

}  // namespace aic
