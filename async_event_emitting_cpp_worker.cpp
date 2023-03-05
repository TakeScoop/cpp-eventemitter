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

#include <functional>

#include "async_event_emitting_cpp_worker.hpp"
#include "async_queued_progress_worker.hpp"
#include "constructable.hpp"
#include "cpp_emitter.h"
#include "eventemitter_impl.hpp"

namespace NodeEvent {
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

using namespace NodeEvent;

template <size_t SIZE>
AsyncEventEmittingCWorker<SIZE>::AsyncEventEmittingCWorker(
    Nan::Callback *callback, std::shared_ptr<NodeEvent::EventEmitter> emitter)
    : AsyncQueuedProgressWorker<EventEmitter::ProgressReport, SIZE>(callback),
      emitter_(emitter) {}

template <size_t SIZE>
void AsyncEventEmittingCWorker<SIZE>::ExecuteWithEmitter(
    EventEmitterFunction fn) = 0;

template <size_t SIZE>
void AsyncEventEmittingCWorker<SIZE>::HandleProgressCallback(
    const EventEmitter::ProgressReport *report, size_t size) {
  UNUSED(size);
  Nan::HandleScope scope;
  v8::Isolate *isolate = v8::Isolate::GetCurrent();

  auto &[event, value] = *report;
  emitter_->emit(this->async_resource, scope, isolate, event, value);
}

template <size_t SIZE>
void AsyncEventEmittingCWorker<SIZE>::Execute(
    const typename AsyncQueuedProgressWorker<
        EventEmitter::ProgressReport, SIZE>::ExecutionProgressSender &sender) {
  // XXX(jrb): This will not work if the C library is multithreaded, as the
  // c_emitter_func_ will be uninitialized in any threads other than the one
  // we're running in right now
  emitterFunc([&sender](const std::string event, EventValue value) -> int {
    // base class uses delete[], so we have to make sure we use new[]
    auto reports = new EventEmitter::ProgressReport[1];
    reports[0] = EventEmitter::ProgressReport{event, value};

    return sender.Send(reports, 1);
  });
  ExecuteWithEmitter(this->emit);
}

template <size_t SIZE>
EventEmitterFunction
AsyncEventEmittingCWorker<SIZE>::emitterFunc(EventEmitterFunction fn) {
  // XXX(jrb): This will not work if the C library is multithreaded
  static thread_local EventEmitterFunction c_emitter_func_ = nullptr;
  if (fn != nullptr) {
    c_emitter_func_ = fn;
  }
  return c_emitter_func_;
}

template <size_t SIZE>
int AsyncEventEmittingCWorker<SIZE>::emit(const std::string event,
                                          EventValue value) {
  return emitterFunc(nullptr)(event, value);
}
std::shared_ptr<EventEmitter> emitter_;
}; // namespace NodeEvent
