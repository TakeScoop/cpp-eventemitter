#pragma once
#ifndef _GLPK_EVENTEMITTER_H
#define _GLPK_EVENTEMITTER_H

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <nan.h>
#include <node.h>
#include <node_object_wrap.h>
#include <uv.h>

#include "cemitter.h"
#include "shared_ringbuffer.hpp"

namespace NodeEvent {
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

/// To avoid requiring c++14, using an adaptor for uv locks to make them behave like std::mutex so they can be used with
/// std::lock_guard and std::unique_lock and std:shared_lock (but not using std::shared_lock to avoid c++14)
class uv_rwlock {
 public:
    uv_rwlock() { uv_rwlock_init(&lock_); }
    ~uv_rwlock() noexcept { uv_rwlock_destroy(&lock_); }

    uv_rwlock(const uv_rwlock& other) = delete;
    uv_rwlock& operator=(const uv_rwlock& other) = delete;

    uv_rwlock(uv_rwlock&& other) {
        // move other to us
        swap(other);
    }

    uv_rwlock& operator=(uv_rwlock& other) {
        swap(other);
        return *this;
    }

    void swap(uv_rwlock& other) { std::swap(this->lock_, other.lock_); }

    void lock() { uv_rwlock_wrlock(&lock_); }
    void unlock() { uv_rwlock_wrunlock(&lock_); }
    bool try_lock() { return uv_rwlock_trywrlock(&lock_); }
    void lock_shared() { uv_rwlock_rdlock(&lock_); }
    void unlock_shared() { uv_rwlock_rdunlock(&lock_); }
    bool try_lock_shared() { return uv_rwlock_tryrdlock(&lock_); }

 private:
    uv_rwlock_t lock_;
};
}  // namespace NodeEvent

namespace std {
template <>
void swap<NodeEvent::uv_rwlock&>(NodeEvent::uv_rwlock& lhs, NodeEvent::uv_rwlock& rhs) {
    lhs.swap(rhs);
}
}  // namespace std

namespace NodeEvent {

template <class T>
class shared_lock {
 public:
    explicit shared_lock(T& shared_mutex) : shared_lock_(shared_mutex) { shared_lock_.lock_shared(); }
    ~shared_lock() noexcept { shared_lock_.unlock_shared(); }

    shared_lock(const shared_lock<T>& other) = delete;
    shared_lock& operator=(const shared_lock<T>& other) = delete;

    shared_lock(shared_lock<T>&& other) { swap(other); }
    shared_lock& operator=(shared_lock<T>&& other) {
        swap(other);
        return *this;
    }
    void swap(shared_lock<T>& other) { std::swap(this->shared_lock_, other.shared_lock_); }

    inline void lock() { shared_lock_.lock_shared(); }
    inline void unlock() { shared_lock_.unlock_shared(); }
    inline void try_lock() { shared_lock_.try_lock_shared(); }

 private:
    T& shared_lock_;
};
}  // namespace NodeEvent

namespace std {
template <class T>
void swap(NodeEvent::shared_lock<T>& lhs, NodeEvent::shared_lock<T>& rhs) {
    lhs.swap(rhs);
}
}  // namespace std

namespace NodeEvent {

/// A type for implementing things that behave like EventEmitter in node
class EventEmitter {
 public:
    /// An error indicating the event name is not known
    class InvalidEvent : std::runtime_error {
     public:
        explicit InvalidEvent(const std::string& msg) : std::runtime_error(msg) {}
    };

    EventEmitter() : receivers_lock_(), receivers_() {}
    virtual ~EventEmitter() noexcept = default;

    /// Set a callback for a given event name
    ///
    /// @param[in] ev - event name
    /// @param[in] cb - callback
    virtual void on(const std::string& ev, Nan::Callback* cb) {
        std::unique_lock<uv_rwlock> master_lock{receivers_lock_};
        auto it = receivers_.find(ev);

        if (it == receivers_.end()) {
            receivers_.emplace(ev, std::make_shared<ReceiverList>());
            it = receivers_.find(ev);
        }
        master_lock.unlock();

        it->second->emplace_back(cb);
    }

    /// Emit a value to any registered callbacks for the event
    ///
    /// @param[in] ev - event name
    /// @param[in] value - a string to emit
    ///
    /// @returns true if the event has listeners, false otherwise
    virtual bool emit(const std::string& ev, const std::string& value) {
        shared_lock<uv_rwlock> master_lock{receivers_lock_};
        auto it = receivers_.find(ev);
        if (it == receivers_.end()) {
            return false;
        }
        master_lock.unlock();

        it->second->emit(value);
        return true;
    }

 private:
    /// Receiver represents a callback that will receive events that are fired
    class Receiver {
     public:
        /// @params[in] callback - the callback to fire, should take a single argument (which will be a string)
        explicit Receiver(Nan::Callback* callback) : callback_(callback) {}
        /// notify the callback by building an AsyncWorker and scheduling it via Nan::AsyncQueueWorker()
        ///
        /// @param[in] value - the string value to send to the callback
        void notify(const std::string& value) {
            v8::Local<v8::Value> info[] = {Nan::New<v8::String>(value).ToLocalChecked()};
            callback_->Call(1, info);
        }

     private:
        /// A worker is an implementation of Nan::AsyncWorker that uses the HandleOKCallback method (which is
        /// guaranteed to execute in the thread that can access v8 structures) to invoke the callback passed in with
        /// the string value passed to the constructor

        Nan::Callback* callback_;
    };

    /// ReceiverList is a list of receivers. Access to the list is proteced via shared mutex
    class ReceiverList {
     public:
        ReceiverList() : receivers_list_(), receivers_list_lock_() {}
        void emplace_back(Nan::Callback* cb) {
            std::lock_guard<std::mutex> guard{receivers_list_lock_};
            receivers_list_.emplace_back(std::make_shared<Receiver>(cb));
        }
        void emit(const std::string& value) {
            std::lock_guard<std::mutex> guard{receivers_list_lock_};
            for (auto& receiver : receivers_list_) {
                receiver->notify(value);
            }
        }

     private:
        std::vector<std::shared_ptr<Receiver>> receivers_list_;
        std::mutex receivers_list_lock_;
    };

    uv_rwlock receivers_lock_;
    // std::shared_timed_mutex receivers_lock_;
    std::unordered_map<std::string, std::shared_ptr<ReceiverList>> receivers_;
};

typedef std::pair<std::string, std::string> ProgressReport;

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
        void Send(const T* data, size_t count) const { worker_.SendProgress(data, count); }

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
        async_ = new uv_async_t();
        uv_async_init(uv_default_loop(), async_, asyncNotifyProgressQueue);
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
        if (this->buffer_.available()) {
            HandleProgressQueue();
        }
        // NOTABUG: this is how Nan does it...
        uv_close(reinterpret_cast<uv_handle_t*>(async_), AsyncClose);
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
        while (this->buffer_.dequeue_nonblocking(elem)) {
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

    void SendProgress(const T* data, size_t size) {
        // use non_blocking and just drop any excessive items
        buffer_.enqueue_nonblocking({data, size});
        uv_async_send(async_);
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
        // TODO(jrb): see note for async_ as a raw pointer
        // NOTABUG: this is how Nan does it...
        delete reinterpret_cast<uv_async_t*>(handle);
        delete worker;
    }

    RingBuffer<std::pair<const T*, size_t>, SIZE> buffer_;
    // TODO(jrb): std::unique_ptr<uv_async_t> async_;
    // unsure of the lifetime of the object; in several places it looks like leaks of workers would be made far
    // worse due to leaking the underlying handle too; so probably best to delete the handle via Destroy and then go
    // around and try to find all the leaks of the Worker's themselves and delete (or smart-pointer them) them so
    // their DTOR's do get called
    uv_async_t* async_;
};

/// AsyncEventEmittingCWorker is a specialization of an AsyncQueuedProgressWorker which is suitable for invoking
/// single-threaded C library code, and passing to those C functions an emitter which can report back events as they
/// happen. The emitter will enqueue the events to be picked up and handled by the v8 thread, and so will not block
/// the worker thread for longer than 3 mutex acquires. If the number of queued events exceeds SIZE, subsequent
/// events will be silently discarded.
template <size_t SIZE>
class AsyncEventEmittingCWorker : public AsyncQueuedProgressWorker<ProgressReport, SIZE> {
 public:
    /// @param[in] callback - the callback to invoke after Execute completes. (unless overridden, is called from
    ///                      HandleOKCallback with no arguments, and called from HandleErrorCallback with the errors
    ///                      reported (if any)
    /// @param[in] emitter - The emitter object to use for notifying JS callbacks for given events.
    AsyncEventEmittingCWorker(Nan::Callback* callback, std::shared_ptr<EventEmitter> emitter)
        : AsyncQueuedProgressWorker<ProgressReport, SIZE>(callback), emitter_(emitter) {}

    /// The work you need to happen in a worker thread
    /// @param[in] fn - Function suitable for passing to single-threaded C code (uses a thread_local static)
    virtual void ExecuteWithEmitter(eventemitter_fn fn) = 0;

    /// emit the ProgressReport as an event via the given emitter, ignores whether or not the emit is successful
    ///
    /// @param[in] report - an array (of size 1) of a ProgressReport (which is a pair, where first is the "key" and
    ///                     second is the "value")
    /// @param[in] size - size of the array (should always be 1)
    virtual void HandleProgressCallback(const ProgressReport* report, size_t size) override {
        UNUSED(size);
        Nan::HandleScope scope;

        emitter_->emit(report[0].first, report[0].second);
    }

 private:
    virtual void Execute(const typename AsyncQueuedProgressWorker<ProgressReport, SIZE>::ExecutionProgressSender&
                             sender) final override {
        // XXX(jrb): This will not work if the C library is multithreaded, as the c_emitter_func_ will be
        // uninitialized in any threads other than the one we're running in right now
        emitterFunc([&sender](const char* ev, const char* val) {
            // base class uses delete[], so we have to make sure we use new[]
            auto reports = new ProgressReport[1];
            reports[0] = {ev, val};

            sender.Send(reports, 1);
        });
        ExecuteWithEmitter(this->emit);
    }

    static std::function<void(const char*, const char*)> emitterFunc(std::function<void(const char*, const char*)> fn) {
        // XXX(jrb): This will not work if the C library is multithreaded
        static thread_local std::function<void(const char*, const char*)> c_emitter_func_ = nullptr;
        if (fn != nullptr) {
            c_emitter_func_ = fn;
        }
        return c_emitter_func_;
    }
    // XXX(jrb): This will not work if the C library is multithreaded
    static void emit(const char* ev, const char* val) { emitterFunc(nullptr)(ev, val); }
    std::shared_ptr<EventEmitter> emitter_;
};

template <size_t SIZE>
class AsyncEventEmittingReentrantCWorker : public AsyncQueuedProgressWorker<ProgressReport, SIZE> {
 public:
    typedef typename AsyncQueuedProgressWorker<ProgressReport, SIZE>::ExecutionProgressSender ExecutionProgressSender;
    /// @param[in] callback - the callback to invoke after Execute completes. (unless overridden, is called from
    ///                      HandleOKCallback with no arguments, and called from HandleErrorCallback with the errors
    ///                      reported (if any)
    /// @param[in] emitter - The emitter object to use for notifying JS callbacks for given events.
    AsyncEventEmittingReentrantCWorker(Nan::Callback* callback, std::shared_ptr<EventEmitter> emitter)
        : AsyncQueuedProgressWorker<ProgressReport, SIZE>(callback), emitter_(emitter) {}

    /// emit the ProgressReport as an event via the given emitter, ignores whether or not the emit is successful
    ///
    /// @param[in] report - an array (of size 1) of a ProgressReport (which is a pair, where first is the "key" and
    ///                     second is the "value")
    /// @param[in] size - size of the array (should always be 1)
    virtual void HandleProgressCallback(const ProgressReport* report, size_t size) override {
        UNUSED(size);
        Nan::HandleScope scope;
        emitter_->emit(report[0].first, report[0].second);
    }

    /// The work you need to happen in a worker thread
    ///
    /// @param[in] sender - An object you must pass as the first argument of fn
    /// @param[in] fn - Function suitable for passing to single-threaded C code (uses a thread_local static)
    virtual void ExecuteWithEmitter(const ExecutionProgressSender* sender, eventemitter_fn_r fn) = 0;

 private:
    virtual void Execute(const ExecutionProgressSender& sender) override {
        ExecuteWithEmitter(&sender, this->reentrant_emit);
    }

    static void reentrant_emit(void* sender, const char* ev, const char* value) {
        if (sender) {
            auto reports = new ProgressReport[1];
            reports[0] = {ev, value};
            auto emitter = static_cast<ExecutionProgressSender*>(sender);
            emitter->Send(reports, 1);
        }
    }

    std::shared_ptr<EventEmitter> emitter_;
};

}  // namespace NodeEvent

#endif
