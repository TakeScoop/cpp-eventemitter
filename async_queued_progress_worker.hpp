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
#ifndef _NODE_EVENT_ASYNC_QUEUED_PROGRESS_WORKER_H
#define _NODE_EVENT_ASYNC_QUEUED_PROGRESS_WORKER_H

#include <functional>
#include <memory>
#include <string>

#include <nan.h>
#include <uv.h>

#include "constructable.hpp"
#include "shared_ringbuffer.hpp"

namespace NodeEvent {
/// Unfortunately, the AsyncProgressWorker in NAN uses a single element which it
/// populates and notifies the handler to check. If lots of things happen
/// quickly, that element will be overwritten before the handler has a chance to
/// notice, and events will be lost.
///
/// AsyncQueuedProgressWorker provides a ringbuffer (to avoid reallocations and
/// poor locality of reference) to help prevent lost progress
template <class T, size_t SIZE>
class AsyncQueuedProgressWorker : public Nan::AsyncWorker {
public:
  class ExecutionProgressSender {
  public:
    /// Send enqueus data to the AsyncQueuedProgressWorker bound to this
    /// instance.
    ///
    /// @param[in] data - data to send, must be array (because it will be free'd
    /// via delete[])
    /// @param[in] count - size of array
    ///
    /// @returns true if successfully enqueued, false otherwise
    bool Send(const T *data, size_t count) const;

  private:
    friend void AsyncQueuedProgressWorker::Execute();
    explicit ExecutionProgressSender(AsyncQueuedProgressWorker &worker);
    ExecutionProgressSender() = delete;
    AsyncQueuedProgressWorker &worker_;
  };

  using EventEmitterFunctionReentrant =
      std::function<int(const ExecutionProgressSender *sender,
                        const std::string event, const EventValue value)>;

  /// @param[in] callback - the callback to invoke after Execute completes.
  /// (unless overridden, is called from
  ///                      HandleOKCallback with no arguments, and called from
  ///                      HandleErrorCallback with the errors reported (if any)
  explicit AsyncQueuedProgressWorker(Nan::Callback *callback);

  /// same as AsyncWorker's, except checks if callback is set first
  virtual void HandleOKCallback() override;

  /// same as AsyncWorker's, except checks if callback is set first
  virtual void HandleErrorCallback() override;

  /// close our async_t handle and free resources (via AsyncClose method)
  virtual void Destroy() override;

  /// Will receive a progress sender, which you can "Send" to.
  ///
  /// @param[in] progress - an ExecutionProgressSender bound to this instance
  virtual void Execute(const ExecutionProgressSender &progress) = 0;

  /// Should be set to handle progress reports as they become available
  ///
  /// @param[in] data - The data (must be an array, is free'd with delete[],
  /// consistent with the Nan API)
  /// @param[in] size - size of the array
  virtual void HandleProgressCallback(const T *data, size_t size) = 0;

  /// Execute implements the Nan::AsyncWorker interface. It should not be
  /// overridden, override virtual void Execute(const ExecutionProgressSender&
  /// progress) instead
  void Execute() final override;

private:
  void HandleProgressQueue();

  bool SendProgress(const T *data, size_t size);

  // This is invoked as an effect if calling uv_async_send(async_), so executes
  // on the thread that the default loop is running on, so it can safely touch
  // v8 data structures
  static NAUV_WORK_CB(asyncNotifyProgressQueue);

  // This is invoked after Destroy(), which executes on the thread that the
  // default loop is running on, and so can touch v8 data structures
  static void AsyncClose(uv_handle_t *handle);

  RingBuffer<std::pair<const T *, size_t>, SIZE> buffer_;
  std::unique_ptr<uv_async_t> async_;
};

} // namespace NodeEvent

#endif
