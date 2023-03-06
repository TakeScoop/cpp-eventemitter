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
#include <memory>

#include "async_event_emitting_reentrant_cpp_worker.hpp"
#include "async_queued_progress_worker.hpp"
#include "cpp_emitter.h"
#include "eventemitter_impl.hpp"

namespace NodeEvent {
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

template <size_t SIZE>
using ExecutionProgressSender =
    AsyncQueuedProgressWorker<NodeEvent::EventEmitter::ProgressReport,
                              SIZE>::ExecutionProgressSender;

template <size_t SIZE>
AsyncEventEmittingReentrantCWorker<SIZE>::AsyncEventEmittingReentrantCWorker(
    Nan::Callback *callback, std::shared_ptr<NodeEvent::EventEmitter> emitter)
    : AsyncQueuedProgressWorker<EventEmitter::ProgressReport, SIZE>(callback),
      emitter_(emitter) {}

template <size_t SIZE>
void AsyncEventEmittingReentrantCWorker<SIZE>::HandleProgressCallback(
    const EventEmitter::ProgressReport *report, size_t size) {
  UNUSED(size);
  Nan::HandleScope scope;
  v8::Isolate *isolate = v8::Isolate::GetCurrent();

  auto &[event, value] = *report;
  emitter_->emit(this->async_resource, scope, isolate, event, value);
}

template <size_t SIZE>
using EventEmitterFunctionReentrant =
    std::function<int(const ExecutionProgressSender<SIZE> *sender,
                      const std::string event, EventValue value)>;

template <size_t SIZE>
void AsyncEventEmittingReentrantCWorker<SIZE>::ExecuteWithEmitter(
    const ExecutionProgressSender *sender,
    EventEmitterFunctionReentrant fn) = 0;

template <size_t SIZE>
void AsyncEventEmittingReentrantCWorker<SIZE>::Execute(
    const ExecutionProgressSender &sender) {
  ExecuteWithEmitter(&sender, this->reentrant_emit);
}

template <size_t SIZE>
int AsyncEventEmittingReentrantCWorker<SIZE>::reentrant_emit(
    const ExecutionProgressSender *sender, const std::string event,
    EventValue value) {
  if (sender) {
    auto reports = new EventEmitter::ProgressReport[1];
    reports[0] = EventEmitter::ProgressReport{event, value};
    return sender->Send(reports, 1);
  }
  return false;
}

} // namespace NodeEvent
