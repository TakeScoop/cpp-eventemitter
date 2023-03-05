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
#ifndef _NODE_EVENT_EMITTING_C_WORKER_H
#define _NODE_EVENT_EMITTING_C_WORKER_H

#include <functional>

#include "async_queued_progress_worker.hpp"
#include "constructable.hpp"
#include "cpp_emitter.h"
#include "eventemitter_impl.hpp"

namespace NodeEvent {
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif
/// AsyncEventEmittingCWorker is a specialization of an
/// AsyncQueuedProgressWorker which is suitable for invoking single-threaded C
/// library code, and passing to those C functions an emitter which can report
/// back events as they happen. The emitter will enqueue the events to be picked
/// up and handled by the v8 thread, and so will not block the worker thread for
/// longer than 3 mutex acquires. If the number of queued events exceeds SIZE,
/// subsequent events will be silently discarded.
template <size_t SIZE>
class AsyncEventEmittingCWorker
    : public AsyncQueuedProgressWorker<EventEmitter::ProgressReport, SIZE> {
public:
  /// @param[in] callback - the callback to invoke after Execute completes.
  /// (unless overridden, is called from
  ///                      HandleOKCallback with no arguments, and called from
  ///                      HandleErrorCallback with the errors reported (if any)
  /// @param[in] emitter - The emitter object to use for notifying JS callbacks
  /// for given events.
  AsyncEventEmittingCWorker(Nan::Callback *callback,
                            std::shared_ptr<EventEmitter> emitter);

  /// The work you need to happen in a worker thread
  /// @param[in] fn - Function suitable for passing to single-threaded C code
  /// (uses a thread_local static)
  virtual void ExecuteWithEmitter(EventEmitterFunction fn) = 0;

  /// emit the ProgressReport as an event via the given emitter, ignores whether
  /// or not the emit is successful
  ///
  /// @param[in] report - an array (of size 1) of a ProgressReport (which is a
  /// pair, where first is the "key" and
  ///                     second is the "value")
  /// @param[in] size - size of the array (should always be 1)
  virtual void
  HandleProgressCallback(const EventEmitter::ProgressReport *report,
                         size_t size) override;

private:
  virtual void
  Execute(const typename AsyncQueuedProgressWorker<
          EventEmitter::ProgressReport, SIZE>::ExecutionProgressSender &sender)
      final override;

  static EventEmitterFunction emitterFunc(EventEmitterFunction fn);

  static int emit(const std::string event, EventValue value);
  std::shared_ptr<EventEmitter> emitter_;
};

} // namespace NodeEvent

#endif
