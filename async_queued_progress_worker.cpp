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
#include <string>

#include <nan.h>
#include <uv.h>

#include "async_queued_progress_worker.hpp"
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
bool AsyncQueuedProgressWorker<T, SIZE>::ExecutionProgressSender::Send(
    const T *data, size_t count) const {
  return worker_.SendProgress(data, count);
}

template <class T, size_t SIZE>
AsyncQueuedProgressWorker<T, SIZE>::ExecutionProgressSender::
    ExecutionProgressSender(AsyncQueuedProgressWorker &worker)
    : worker_(worker) {}

template <class T, size_t SIZE>
using EventEmitterFunctionReentrant =
    std::function<int(const typename AsyncQueuedProgressWorker<
                          T, SIZE>::ExecutionProgressSender *sender,
                      const std::string event, const EventValue value)>;

template <class T, size_t SIZE>
AsyncQueuedProgressWorker<T, SIZE>::AsyncQueuedProgressWorker(
    Nan::Callback *callback)
    : AsyncWorker(callback), buffer_() {
  async_ = std::unique_ptr<uv_async_t>(new uv_async_t());
  uv_async_init(uv_default_loop(), async_.get(), asyncNotifyProgressQueue);
  async_->data = this;
}

template <class T, size_t SIZE>
void AsyncQueuedProgressWorker<T, SIZE>::HandleOKCallback() {
  Nan::HandleScope scope;
  if (callback) {
#if defined(NODE_8_0_MODULE_VERSION) && (NODE_8_0_MODULE_VERSION > 51)
    callback->Call(0, NULL, async_resource);
#else
    callback->Call(0, NULL);
#endif
  }
}

template <class T, size_t SIZE>
void AsyncQueuedProgressWorker<T, SIZE>::HandleErrorCallback() {
  Nan::HandleScope scope;
  if (callback) {
    v8::Local<v8::Value> argv[] = {v8::Exception::Error(
        Nan::New<v8::String>(ErrorMessage()).ToLocalChecked())};
    callback->Call(1, argv, async_resource);
  }
}

template <class T, size_t SIZE>
void AsyncQueuedProgressWorker<T, SIZE>::Destroy() {
  // NOTABUG: Nan uses reinterpret_cast to pass uv_async_t around
  uv_close(reinterpret_cast<uv_handle_t *>(async_.get()), AsyncClose);
}

template <class T, size_t SIZE>
void AsyncQueuedProgressWorker<T, SIZE>::Execute(
    const ExecutionProgressSender &progress) = 0;

template <class T, size_t SIZE>
void AsyncQueuedProgressWorker<T, SIZE>::HandleProgressCallback(
    const T *data, size_t size) = 0;

template <class T, size_t SIZE>
void AsyncQueuedProgressWorker<T, SIZE>::Execute() {
  ExecutionProgressSender sender{*this};
  Execute(sender);
}

template <class T, size_t SIZE>
void AsyncQueuedProgressWorker<T, SIZE>::HandleProgressQueue() {
  std::pair<const T *, size_t> elem;
  while (this->buffer_.pop(elem)) {
    HandleProgressCallback(elem.first, elem.second);
    /* if (elem.second > 0) {
      delete[] elem.first;
    } */
  }
}

template <class T, size_t SIZE>
bool AsyncQueuedProgressWorker<T, SIZE>::SendProgress(const T *data,
                                                      size_t size) {
  // TODO: here lies an error
  // use non_blocking and just drop any excessive items
  bool r = buffer_.push({data, size});
  uv_async_send(async_.get());
  return r;
}

#define COMMA ,
// cheesy macro trick

template <class T, size_t SIZE>
NAUV_WORK_CB(
    AsyncQueuedProgressWorker<T COMMA SIZE>::asyncNotifyProgressQueue) {
  auto worker = static_cast<AsyncQueuedProgressWorker *>(async->data);
  worker->HandleProgressQueue();
}

template <class T, size_t SIZE>
void AsyncQueuedProgressWorker<T, SIZE>::AsyncClose(uv_handle_t *handle) {
  auto worker = static_cast<AsyncQueuedProgressWorker *>(handle->data);
  // Destroy happens in the v8 main loop; so we can flush out the Progress
  // queue here before destroying
  if (worker->buffer_.read_available()) {
    worker->HandleProgressQueue();
  }
  delete worker;
}

} // namespace NodeEvent
