#pragma once
#ifndef _NODE_EVENT_EVENTEMITTER_IMPL_H
#define _NODE_EVENT_EVENTEMITTER_IMPL_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <nan.h>
#include <uv.h>

#include "uv_rwlock_adaptor.hpp"
#include "shared_lock.hpp"
#include "shared_ringbuffer.hpp"

namespace NodeEvent {
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif
/// A type for implementing things that behave like EventEmitter in node
class EventEmitter {
 public:
     /// A report type, consisting of a key and a value
     typedef std::pair<std::string, std::string> ProgressReport;

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
        /// @param[in] callback - the callback to fire, should take a single argument (which will be a string)
        explicit Receiver(Nan::Callback* callback) : callback_(callback) {}

        /// notify the callback by building an AsyncWorker and scheduling it via Nan::AsyncQueueWorker()
        ///
        /// @param[in] value - the string value to send to the callback
        void notify(const std::string& value) {
            v8::Local<v8::Value> info[] = {Nan::New<v8::String>(value).ToLocalChecked()};
            callback_->Call(1, info);
        }

     private:
        Nan::Callback* callback_;
    };

    /// ReceiverList is a list of receivers. Access to the list is proteced via shared mutex
    class ReceiverList {
     public:
        ReceiverList() : receivers_list_(), receivers_list_lock_() {}

        /// Adds a callback to the end of the receivers_list
        ///
        /// @param[in] cb - the callback to add to the list
        void emplace_back(Nan::Callback* cb) {
            std::lock_guard<std::mutex> guard{receivers_list_lock_};
            receivers_list_.emplace_back(std::make_shared<Receiver>(cb));
        }

        /// notify all receivers
        ///
        /// @param[in] value - the string to send to all receivers
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
    std::unordered_map<std::string, std::shared_ptr<ReceiverList>> receivers_;
};

}  // namespace NodeEvent

#endif
