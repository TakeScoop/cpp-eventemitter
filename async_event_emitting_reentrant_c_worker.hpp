#pragma once
#ifndef _NODE_EVENT_ASYNC_EVENT_EMITTING_REENTRANT_C_WORKER
#define _NODE_EVENT_ASYNC_EVENT_EMITTING_REENTRANT_C_WORKER
#include <functional>
#include <memory>

#include "cemitter.h"
#include "async_queued_progress_worker.hpp"
#include "eventemitter_impl.hpp"

namespace NodeEvent {
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

template <size_t SIZE>
class AsyncEventEmittingReentrantCWorker : public AsyncQueuedProgressWorker<EventEmitter::ProgressReport, SIZE> {
 public:
    typedef typename AsyncQueuedProgressWorker<EventEmitter::ProgressReport, SIZE>::ExecutionProgressSender ExecutionProgressSender;
    /// @param[in] callback - the callback to invoke after Execute completes. (unless overridden, is called from
    ///                      HandleOKCallback with no arguments, and called from HandleErrorCallback with the errors
    ///                      reported (if any)
    /// @param[in] emitter - The emitter object to use for notifying JS callbacks for given events.
    AsyncEventEmittingReentrantCWorker(Nan::Callback* callback, std::shared_ptr<EventEmitter> emitter)
        : AsyncQueuedProgressWorker<EventEmitter::ProgressReport, SIZE>(callback), emitter_(emitter) {}

    /// emit the EventEmitter::ProgressReport as an event via the given emitter, ignores whether or not the emit is successful
    ///
    /// @param[in] report - an array (of size 1) of a EventEmitter::ProgressReport (which is a pair, where first is the "key" and
    ///                     second is the "value")
    /// @param[in] size - size of the array (should always be 1)
    virtual void HandleProgressCallback(const EventEmitter::ProgressReport* report, size_t size) override {
        UNUSED(size);
        Nan::HandleScope scope;
        emitter_->emit(report[0].first, report[0].second);
    }

    /// The work you need to happen in a worker thread
    ///
    /// @param[in] sender - An object you must pass as the first argument of fn
    /// @param[in] fn - Function suitable for passing to multi-threaded C code
    virtual void ExecuteWithEmitter(const ExecutionProgressSender* sender, eventemitter_fn_r fn) = 0;

 private:
    virtual void Execute(const ExecutionProgressSender& sender) override {
        ExecuteWithEmitter(&sender, this->reentrant_emit);
    }

    static int reentrant_emit(const void* sender, const char* ev, const char* value) {
        if (sender) {
            auto reports = new EventEmitter::ProgressReport[1];
            reports[0] = {ev, value};
            auto emitter = static_cast<const ExecutionProgressSender*>(sender);
            return static_cast<int>(emitter->Send(reports, 1));
        }
        return false;
    }

    std::shared_ptr<EventEmitter> emitter_;
};

}  // namespace NodeEvent

#endif
