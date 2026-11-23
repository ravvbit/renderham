// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
// Tests for progress/cancellation and for the threaded animation renderer.
//
// The central claim of render_sequence() is that threads change only the speed:
// every frame must come out bit-for-bit as it does when the frames are rendered
// one after another through ConversionPipeline::preview() (frame 0 first, its
// palette locked for the others) - which is what the GUI did before it got a
// worker thread. So most checks here are exact comparisons against that
// reference loop, with 1, 2 and 4 threads.
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "aic/aic.hpp"

using namespace aic;

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("  %s  %s\n", ok ? "OK  " : "FAIL", what.c_str());
    if (!ok) ++g_failures;
}

const std::string kInput = std::string(RENDERHAM_TESTDATA) + "/test_input.png";

ConversionOptions base_options() {
    ConversionOptions o;
    o.target_width = 96;
    o.target_height = 80;
    o.lookahead = 3;  // the default, and the slow path the threads are for
    return o;
}

// A pan across the test picture, the last frame partly outside the source.
std::vector<CropSpec> make_crops(int n) {
    std::vector<CropSpec> crops;
    for (int i = 0; i < n; ++i) {
        CropSpec c;
        c.width = {320.f, false};
        c.height = {200.f, false};
        c.x = {static_cast<float>(i * 330 / (n - 1)), false};
        c.y = {static_cast<float>(i * 5), false};
        crops.push_back(c);
    }
    return crops;
}

bool same_palette(const Palette& a, const Palette& b) {
    for (size_t i = 0; i < a.colors.size(); ++i)
        if (a.colors[i].r4 != b.colors[i].r4 || a.colors[i].g4 != b.colors[i].g4 || a.colors[i].b4 != b.colors[i].b4)
            return false;
    return true;
}

bool same_frame(const HAM6Frame& a, const HAM6Frame& b) {
    return a.width == b.width && a.height == b.height && a.pixel_codes == b.pixel_codes &&
           same_palette(a.palette, b.palette);
}

// What RunAnimationExport did before the worker thread: one preview() per frame,
// frame 0's actual palette locked for the rest. `anchor` mirrors SequenceOptions::anchor_dither
// (which render_sequence applies to every frame; on by default).
std::vector<PreviewResult> reference_loop(const ConversionPipeline& pipeline, const ConversionOptions& base,
                                          const std::vector<CropSpec>& crops, bool anchor = true) {
    std::vector<PreviewResult> out;
    std::optional<Palette> shared = base.locked_palette;
    for (size_t i = 0; i < crops.size(); ++i) {
        ConversionOptions o = base;
        o.crop = crops[i];
        o.anchor_dither_to_source = anchor;
        if (shared) o.locked_palette = shared;
        out.push_back(pipeline.preview(kInput, o));
        if (!shared) shared = out.back().frame.palette;
    }
    return out;
}

struct Collected {
    std::vector<PreviewResult> frames;
    std::vector<int> times_seen;
    std::atomic<int> in_callback{0};
    std::atomic<bool> overlapped{false};
};

SequenceResult run(const ConversionPipeline& p, const ConversionOptions& base, const std::vector<CropSpec>& crops,
                   int threads, Collected& c, bool anchor = true) {
    c.frames.assign(crops.size(), PreviewResult{});
    c.times_seen.assign(crops.size(), 0);
    SequenceOptions so;
    so.threads = threads;
    so.anchor_dither = anchor;
    so.on_frame = [&](size_t i, PreviewResult& f) -> std::string {
        if (c.in_callback.fetch_add(1) != 0) c.overlapped = true;
        c.frames[i] = std::move(f);
        ++c.times_seen[i];
        c.in_callback.fetch_sub(1);
        return {};
    };
    return render_sequence(p, kInput, base, crops, so);
}

bool all_equal(const std::vector<PreviewResult>& a, const std::vector<PreviewResult>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (!a[i].success || !b[i].success || !same_frame(a[i].frame, b[i].frame)) return false;
        if (a[i].lace != b[i].lace || a[i].mean_oklab_error != b[i].mean_oklab_error) return false;
    }
    return true;
}

void test_progress_callback_changes_nothing() {
    std::printf("progress callback\n");
    ConversionPipeline p;
    ConversionOptions o = base_options();
    PreviewResult plain = p.preview(kInput, o);

    std::vector<float> seen;
    ConversionOptions with = o;
    with.progress = [&](float f) { seen.push_back(f); return true; };
    PreviewResult observed = p.preview(kInput, with);

    check(plain.success && observed.success, "both conversions succeed");
    check(same_frame(plain.frame, observed.frame), "result is bit-identical with a progress callback");
    check(plain.mean_oklab_error == observed.mean_oklab_error, "error figure identical");

    bool monotone = true, in_range = true;
    for (size_t i = 0; i < seen.size(); ++i) {
        if (seen[i] < 0.f || seen[i] > 1.f) in_range = false;
        if (i && seen[i] < seen[i - 1]) monotone = false;
    }
    check(!seen.empty() && in_range, "values stay within 0..1");
    check(monotone, "values never go backwards");
    check(!seen.empty() && seen.back() > 0.95f, "the last report is close to 1");
    // 96x80 canvas, 4 refinement passes + 1 final pass, one report per row (plus the initial one)
    check(seen.size() >= 80 * 5, "reported at least once per scanline and pass");
}

void test_cancel_single() {
    std::printf("cancel a single conversion\n");
    ConversionPipeline p;

    ConversionOptions o = base_options();
    o.progress = [](float f) { return f < 0.5f; };
    PreviewResult r = p.preview(kInput, o);
    check(!r.success && r.cancelled, "stopping at 50 percent: not successful, cancelled");
    check(r.error_message == "cancelled", "error message is just 'cancelled'");

    o.progress = [](float) { return false; };
    r = p.preview(kInput, o);
    check(!r.success && r.cancelled, "stopping right away (before the encoder) is cancelled too");

    // a cancel is not an error of the input
    o = base_options();
    o.progress = [](float f) { return f < 0.9f; };
    std::string out = std::string(RENDERHAM_TMPDIR) + "/sequence_cancel.iff";
    std::filesystem::remove(out);
    ConversionResult cr = p.convert(kInput, out, o);
    check(!cr.success && cr.cancelled, "convert(): cancelled");
    check(!std::filesystem::exists(out), "convert(): no file written for a cancelled conversion");

    // ... and the pipeline is still fine afterwards
    r = p.preview(kInput, base_options());
    check(r.success && !r.cancelled, "an ordinary conversion afterwards still works");
}

void test_sequence_equals_reference() {
    std::printf("threaded sequence == sequential reference\n");
    ConversionPipeline p;
    const std::vector<CropSpec> crops = make_crops(9);

    for (const char* ditherer : {"floyd-steinberg", "bayer"}) {
        ConversionOptions o = base_options();
        o.ditherer_name = ditherer;
        std::vector<PreviewResult> ref = reference_loop(p, o, crops);
        bool ref_ok = true;
        for (auto& r : ref) ref_ok = ref_ok && r.success;
        check(ref_ok, std::string("reference frames render (") + ditherer + ")");

        for (int threads : {1, 2, 4}) {
            Collected c;
            SequenceResult sr = run(p, o, crops, threads, c);
            check(sr.success && sr.frames_done == crops.size(),
                  std::string(ditherer) + ", " + std::to_string(threads) + " thread(s): success, all frames done");
            check(all_equal(ref, c.frames), "    every frame bit-identical to the sequential reference");
            bool once = true;
            for (int t : c.times_seen) once = once && t == 1;
            check(once, "    on_frame called exactly once per frame");
            check(!c.overlapped, "    on_frame calls never overlap");
            check(sr.palette && same_palette(*sr.palette, ref[0].frame.palette), "    reported palette is frame 0's");
        }
    }

    // The anchor option: off = frames exactly as separately converted ones; on (the default) moves the
    // pattern of an ordered ditherer with the picture, so for bayer the frames really are different.
    {
        ConversionOptions bo = base_options();
        bo.ditherer_name = "bayer";
        std::vector<PreviewResult> plain = reference_loop(p, bo, crops, false);
        std::vector<PreviewResult> anchored = reference_loop(p, bo, crops, true);
        Collected c;
        SequenceResult sr = run(p, bo, crops, 3, c, false);
        check(sr.success && all_equal(plain, c.frames), "anchor_dither = false: frames identical to unanchored conversions");
        check(!all_equal(plain, anchored), "bayer: anchored frames differ from unanchored ones (the option does something)");
    }

    // frames after the first really do share one palette
    ConversionOptions o = base_options();
    Collected c;
    run(p, o, crops, 3, c);
    bool shared = true;
    for (auto& f : c.frames) shared = shared && same_palette(f.frame.palette, c.frames[0].frame.palette);
    check(shared, "all frames use one palette");
}

void test_locked_palette_runs_in_parallel() {
    std::printf("palette locked by the caller\n");
    ConversionPipeline p;
    const std::vector<CropSpec> crops = make_crops(7);

    // A palette of the caller's own, with colour 0 forced: frame 0 is no longer special.
    ConversionOptions seed = base_options();
    PreviewResult seed_result = p.preview(kInput, seed);
    ConversionOptions o = base_options();
    o.locked_palette = seed_result.frame.palette;
    o.fixed_colors[0] = AmigaColor{3, 5, 7};

    std::vector<PreviewResult> ref = reference_loop(p, o, crops);
    for (int threads : {1, 3}) {
        Collected c;
        SequenceResult sr = run(p, o, crops, threads, c);
        check(sr.success, std::to_string(threads) + " thread(s): success");
        check(all_equal(ref, c.frames), "    bit-identical to the sequential reference");
        check(sr.palette && sr.palette->colors[0].r4 == 3 && sr.palette->colors[0].g4 == 5 &&
                  sr.palette->colors[0].b4 == 7,
              "    reported palette carries the fixed colour");
    }
}

void test_progress_of_sequence() {
    std::printf("sequence progress\n");
    ConversionPipeline p;
    const std::vector<CropSpec> crops = make_crops(6);
    std::atomic<float> last{0.f};
    std::atomic<bool> bad{false};
    SequenceOptions so;
    so.threads = 3;
    so.on_progress = [&](float f) {
        if (f < 0.f || f > 1.f) bad = true;
        last = f;
    };
    SequenceResult sr = render_sequence(p, kInput, base_options(), crops, so);
    check(sr.success, "sequence succeeds");
    check(!bad, "progress stays within 0..1");
    check(last.load() > 0.999f, "progress ends at 1");
}

void test_cancel_sequence() {
    std::printf("cancel a sequence\n");
    ConversionPipeline p;
    const std::vector<CropSpec> crops = make_crops(40);

    for (int threads : {1, 3}) {
        std::atomic<bool> cancel{false};
        SequenceOptions so;
        so.threads = threads;
        so.cancel = &cancel;
        int delivered = 0;
        so.on_frame = [&](size_t, PreviewResult&) -> std::string {
            if (++delivered == 3) cancel = true;
            return {};
        };
        SequenceResult sr = render_sequence(p, kInput, base_options(), crops, so);
        check(!sr.success && sr.cancelled, std::to_string(threads) + " thread(s): cancelled, not successful");
        check(sr.error_message.empty(), "    a cancel carries no error message");
        check(sr.frames_done >= 3 && sr.frames_done < crops.size(), "    stopped early, long before the 40 frames");
        check(delivered == static_cast<int>(sr.frames_done), "    frames_done matches the frames handed out");
    }

    // cancelled before anything started
    std::atomic<bool> already{true};
    SequenceOptions so;
    so.cancel = &already;
    SequenceResult sr = render_sequence(p, kInput, base_options(), crops, so);
    check(sr.cancelled && !sr.success && sr.frames_done == 0, "a flag that is already set does no work");
}

void test_errors() {
    std::printf("errors\n");
    ConversionPipeline p;

    // an error from the consumer (e.g. a full disk) stops the sequence
    {
        const std::vector<CropSpec> crops = make_crops(12);
        SequenceOptions so;
        so.threads = 1;
        so.on_frame = [](size_t i, PreviewResult&) -> std::string { return i == 4 ? "disk full" : std::string(); };
        SequenceResult sr = render_sequence(p, kInput, base_options(), crops, so);
        check(!sr.success && !sr.cancelled, "consumer error: failure, not a cancel");
        check(sr.error_message == "disk full" && sr.failed_index == 4, "    its message and frame index are reported");
        check(sr.frames_done == 4, "    nothing was handed out after it (1 thread)");

        so.threads = 3;
        sr = render_sequence(p, kInput, base_options(), crops, so);
        check(!sr.success && sr.error_message == "disk full", "consumer error with 3 threads");
    }
    // a crop box with no area in the middle of the sequence
    {
        std::vector<CropSpec> crops = make_crops(6);
        crops[3].width = {0.f, false};
        SequenceOptions so;
        so.threads = 1;
        SequenceResult sr = render_sequence(p, kInput, base_options(), crops, so);
        check(!sr.success && sr.failed_index == 3 && sr.error_message.find("crop") != std::string::npos,
              "bad crop box: failed_index 3, message mentions the crop");
        so.threads = 4;
        sr = render_sequence(p, kInput, base_options(), crops, so);
        check(!sr.success && sr.failed_index == 3, "bad crop box with 4 threads: same frame reported");
    }
    {
        SequenceResult sr = render_sequence(p, kInput, base_options(), {});
        check(!sr.success && !sr.error_message.empty(), "no frames: error");
        ConversionOptions bad = base_options();
        bad.lookahead = 99;
        sr = render_sequence(p, kInput, bad, make_crops(3));
        check(!sr.success && sr.error_message.find("lookahead") != std::string::npos, "invalid options are refused");
        sr = render_sequence(p, "/nonexistent/picture.png", base_options(), make_crops(3));
        check(!sr.success && sr.error_message.find("failed to load") != std::string::npos, "missing file: load error");
    }
}

void test_load_source_split() {
    std::printf("preview() == load_source() + preview_source()\n");
    ConversionPipeline p;
    ConversionOptions o = base_options();
    o.source_transform = ImageTransform::rotate_cw(1);
    PreviewResult whole = p.preview(kInput, o);
    std::string err;
    auto src = p.load_source(kInput, o, &err);
    check(src.has_value(), "load_source succeeds");
    PreviewResult split = p.preview_source(*src, o);
    check(whole.success && split.success && same_frame(whole.frame, split.frame),
          "same frame, with a rotated source");
    check(src && src->width() == 400 && src->height() == 640, "load_source already applies the source transform");
}

}  // namespace

int main() {
    test_progress_callback_changes_nothing();
    test_cancel_single();
    test_load_source_split();
    test_sequence_equals_reference();
    test_locked_palette_runs_in_parallel();
    test_progress_of_sequence();
    test_cancel_sequence();
    test_errors();
    if (g_failures) {
        std::printf("\n%d check(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nall sequence checks passed\n");
    return 0;
}
