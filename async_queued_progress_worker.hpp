#pragma once
#ifndef _NODE_EVENT_ASYNC_QUEUED_PROGRESS_WORKER_H
#define _NODE_EVENT_ASYNC_QUEUED_PROGRESS_WORKER_H

#include <functional>
#include <memory>
#include <string>

#include <nan.h>
#include <uv.h>

#include "shared_ringbuffer.hpp"

namespace NodeEvent {
/// Unfortunately, the AsyncProgressWorker in NAN uses a single element which it populates and notifies the handler
/// to check. If lots of things happen quickly, that element will be overwritten before the handler has a chance to
/// notice, and events will be lost.
///
/// AsyncQueuedProgressWorker provides a ringbuffer (to avoid reallocations and poor locality of reference) to help
/// prevent lost progress
template <class T, size_t SIZE>
class AsyncQueuedProgressWorker : public Nan::AsyncWorker {
 public:
    class ExecutionProgressSender {
     public:
        /// Send enqueus data to the AsyncQueuedProgressWorker bound to this instance.
        ///
        /// @param[in] data - data to send, must be array (because it will be free'd via delete[])
        /// @param[in] count - size of array
        ///
        /// @returns true if successfully enqueued, false otherwise
        bool Send(const T* data, size_t count) const { return worker_.SendProgress(data, count); }

     private:
        friend void AsyncQueuedProgressWorker::Execute();
        explicit ExecutionProgressSender(AsyncQueuedProgressWorker& worker) : worker_(worker) {}
        ExecutionProgressSender() = delete;
        AsyncQueuedProgressWorker& worker_;
    };

    /// @param[in] callback - the callback to invoke after Execute completes. (unless overridden, is called from
    ///                      HandleOKCallback with no arguments, and called from HandleErrorCallback with the errors
    ///                      reported (if any)
    explicit AsyncQueuedProgressWorker(Nan::Callback* callback) : AsyncWorker(callback), buffer_() {
        async_ = std::unique_ptr<uv_async_t>(new uv_async_t());
        uv_async_init(uv_default_loop(), async_.get(), asyncNotifyProgressQueue);
        async_->data = this;
    }

    /// same as AsyncWorker's, except checks if callback is set first
    virtual void HandleOKCallback() override {
        Nan::HandleScope scope;
        if (callback) {
            callback->Call(0, NULL);
        }
    }

    /// same as AsyncWorker's, except checks if callback is set first
    virtual void HandleErrorCallback() override {
        Nan::HandleScope scope;
        if (callback) {
            v8::Local<v8::Value> argv[] = {v8::Exception::Error(Nan::New<v8::String>(ErrorMessage()).ToLocalChecked())};
            callback->Call(1, argv);
        }
    }

    /// close our async_t handle and free resources (via AsyncClose method)
    virtual void Destroy() override {
        // Destroy happens in the v8 main loop; so we can flush out the Progress queue here before destroying
        if (this->buffer_.read_available()) {
            HandleProgressQueue();
        }
        // NOTABUG: Nan uses reinterpret_cast to pass uv_async_t around
        uv_close(reinterpret_cast<uv_handle_t*>(async_.get()), AsyncClose);
    }

    /// Will receive a progress sender, which you can "Send" to.
    ///
    /// @param[in] progress - an ExecutionProgressSender bound to this instance
    virtual void Execute(const ExecutionProgressSender& progress) = 0;

    /// Should be set to handle progress reports as they become available
    ///
    /// @param[in] data - The data (must be an array, is free'd with delete[], consistent with the Nan API)
    /// @param[in] size - size of the array
    virtual void HandleProgressCallback(const T* data, size_t size) = 0;

 private:
    void HandleProgressQueue() {
        std::pair<const T*, size_t> elem;
        while (this->buffer_.pop(elem)) {
            HandleProgressCallback(elem.first, elem.second);
            if (elem.second > 0) {
                delete[] elem.first;
            }
        }
    }

    void Execute() final override {
        ExecutionProgressSender sender{*this};
        Execute(sender);
    }

    bool SendProgress(const T* data, size_t size) {
        // use non_blocking and just drop any excessive items
        bool r = buffer_.push({data, size});
        uv_async_send(async_.get());
        return r;
    }

    // This is invoked as an effect if calling uv_async_send(async_), so executes on the thread that the default
    // loop is running on, so it can safely touch v8 data structures
    static NAUV_WORK_CB(asyncNotifyProgressQueue) {
        auto worker = static_cast<AsyncQueuedProgressWorker*>(async->data);
        worker->HandleProgressQueue();
    }

    // This is invoked after Destroy(), which executes on the thread that the default loop is running on, and so can
    // touch v8 data structures
    static void AsyncClose(uv_handle_t* handle) {
        auto worker = static_cast<AsyncQueuedProgressWorker*>(handle->data);
        delete worker;
    }

    RingBuffer<std::pair<const T*, size_t>, SIZE> buffer_;
    std::unique_ptr<uv_async_t> async_;
};

}  // namespace NodeEvent

#endif
