// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#pragma once
// BackgroundJob.hpp
//
// One worker thread at a time for the GUI's long-running work (conversions,
// animation renders), with the three things such work needs:
//   - a cancel flag the worker polls,
//   - a way to hand results back to the GUI thread (Context::post),
//   - a completion callback that runs on the GUI thread.
//
// The worker must not touch any wx object - it works on plain copies of what
// it needs and reports back through Context::post. Everything else on this
// class is for the GUI thread only.
//
// Stale messages are dropped: CancelAndWait() (or starting the next job)
// bumps a generation counter, and every posted function checks that its job is
// still the current one when it finally runs on the GUI thread. So a result
// that was queued just before the user cancelled can never overwrite what they
// look at now.

#include <wx/event.h>

#include <atomic>
#include <functional>
#include <thread>

class BackgroundJob {
public:
    struct Context {
        const std::atomic<bool>& cancel;  // set by RequestCancel()/CancelAndWait()
        // Runs `fn` on the GUI thread, later - unless this job was cancelled and
        // discarded in the meantime. Never blocks. Safe from any thread.
        std::function<void(std::function<void()> fn)> post;
    };
    using Work = std::function<void(Context& ctx)>;

    explicit BackgroundJob(wxEvtHandler* owner) : owner_(owner) {}
    ~BackgroundJob() { CancelAndWait(); }
    BackgroundJob(const BackgroundJob&) = delete;
    BackgroundJob& operator=(const BackgroundJob&) = delete;

    bool IsRunning() const { return running_; }

    // Starts `work` on a new thread; when it has returned, `done` runs on the GUI
    // thread. Only one job at a time: call CancelAndWait() first if one runs.
    void Start(Work work, std::function<void()> done);

    // Asks the worker to stop. It does so at its own pace; `done` still runs,
    // and the worker decides what it reports (the library calls say "cancelled").
    void RequestCancel() { cancel_ = true; }

    // Stops the job, waits for the thread, and discards everything it still had
    // queued - `done` does not run. For when the result is no longer wanted
    // (new image, window closing). The worker polls the flag between scanlines,
    // so this returns within a few milliseconds.
    void CancelAndWait();

private:
    wxEvtHandler* owner_;
    std::thread thread_;
    std::atomic<bool> cancel_{false};
    bool running_ = false;      // GUI thread only
    unsigned generation_ = 0;   // GUI thread only (posted functions read it there)
};
