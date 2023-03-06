/*
 * Copyright 2017 Scoop Technologies
 * Copyright 2023 Totto16
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once
#ifndef _NODE_EVENT_ASYNC_EVENT_EMITTING_REENTRANT_C_WORKER
#define _NODE_EVENT_ASYNC_EVENT_EMITTING_REENTRANT_C_WORKER

#include <functional>
#include <memory>

#include "async_queued_progress_worker.hpp"
#include "cpp_emitter.h"
#include "eventemitter_impl.hpp"

namespace NodeEvent {
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

template <size_t SIZE>
class AsyncEventEmittingReentrantCWorker
    : public AsyncQueuedProgressWorker<EventEmitter::ProgressReport, SIZE> {
public:
  typedef typename AsyncQueuedProgressWorker<EventEmitter::ProgressReport,
                                             SIZE>::ExecutionProgressSender
      ExecutionProgressSender;
  /// @param[in] callback - the callback to invoke after Execute completes.
  /// (unless overridden, is called from
  ///                      HandleOKCallback with no arguments, and called from
  ///                      HandleErrorCallback with the errors reported (if any)
  /// @param[in] emitter - The emitter object to use for notifying JS callbacks
  /// for given events.
  AsyncEventEmittingReentrantCWorker(Nan::Callback *callback,
                                     std::shared_ptr<EventEmitter> emitter)
      : AsyncQueuedProgressWorker<EventEmitter::ProgressReport, SIZE>(callback),
        emitter_(emitter) {}

  /// emit the EventEmitter::ProgressReport as an event via the given emitter,
  /// ignores whether or not the emit is successful
  ///
  /// @param[in] report - an array (of size 1) of a EventEmitter::ProgressReport
  /// (which is a pair, where first is the "key" and
  ///                     second is the "value")
  /// @param[in] size - size of the array (should always be 1)
  virtual void
  HandleProgressCallback(const EventEmitter::ProgressReport *report,
                         size_t size) override {
    UNUSED(size);
    Nan::HandleScope scope;
    v8::Isolate *isolate = v8::Isolate::GetCurrent();

    auto &[event, value] = *report;
    emitter_->emit(this->async_resource, scope, isolate, event, value);
  }

  using EventEmitterFunctionReentrant =
      std::function<int(const ExecutionProgressSender *sender,
                        const std::string event, EventValue value)>;

  /// The work you need to happen in a worker thread
  ///
  /// @param[in] sender - An object you must pass as the first argument of fn
  /// @param[in] fn - Function suitable for passing to multi-threaded C code
  virtual void ExecuteWithEmitter(const ExecutionProgressSender *sender,
                                  EventEmitterFunctionReentrant fn) = 0;

private:
  virtual void Execute(const ExecutionProgressSender &sender) override {
    ExecuteWithEmitter(&sender, this->reentrant_emit);
  }

  static int reentrant_emit(const ExecutionProgressSender *sender,
                            const std::string event, EventValue value) {
    if (sender) {
      auto reports = new EventEmitter::ProgressReport[1];
      reports[0] = EventEmitter::ProgressReport{event, value};
      return sender->Send(reports, 1);
    }
    return false;
  }

  std::shared_ptr<EventEmitter> emitter_;
};

} // namespace NodeEvent

#endif
