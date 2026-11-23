// SPDX-License-Identifier: MIT
// Copyright (c) 2026 rawbit (@ravvbit). AI-generated with Claude (Anthropic) under human direction; see README.md.
#include "BackgroundJob.hpp"

#include <utility>

void BackgroundJob::Start(Work work, std::function<void()> done) {
    if (running_) CancelAndWait();
    const unsigned gen = ++generation_;
    cancel_ = false;
    running_ = true;
    wxEvtHandler* owner = owner_;

    thread_ = std::thread([this, owner, gen, work = std::move(work), done = std::move(done)]() mutable {
        Context ctx{cancel_, nullptr};
        ctx.post = [this, owner, gen](std::function<void()> fn) {
            // CallAfter() is documented to be usable from other threads: it queues an
            // event for the GUI thread and returns at once.
            owner->CallAfter([this, gen, fn = std::move(fn)] {
                if (gen == generation_) fn();
            });
        };
        work(ctx);
        ctx.post([this, done = std::move(done)] {
            // The worker has posted its last message and is about to return, so the
            // join is immediate.
            if (thread_.joinable()) thread_.join();
            running_ = false;
            done();
        });
    });
}

void BackgroundJob::CancelAndWait() {
    if (!running_) return;
    ++generation_;  // whatever the worker still has queued is now stale
    cancel_ = true;
    if (thread_.joinable()) thread_.join();
    running_ = false;
}
