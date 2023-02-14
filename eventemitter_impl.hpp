/*
 * Copyright 2017 Scoop Technologies
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
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

    /// Remove all listeners for a given event
    ///
    /// @param[in] ev - event name
    virtual void removeAllListenersForEvent(const std::string& ev) {
        std::unique_lock<uv_rwlock> master_lock{receivers_lock_};
        auto it = receivers_.find(ev);

        if (it != receivers_.end()) {
            receivers_.erase(it);
        }
    }

    /// Remove all listeners for all events
    virtual void removeAllListeners() {
        std::unique_lock<uv_rwlock> master_lock{receivers_lock_};
        receivers_.clear();
    }

    // Return a list of all eventNames
    virtual std::vector<std::string> eventNames() {
        std::unique_lock<uv_rwlock> master_lock{receivers_lock_};
        std::vector<std::string> keys;

        for (auto it : receivers_) {
            keys.emplace_back(it.first);
        }
        return keys;
    }

    /// Emit a value to any registered callbacks for the event
    ///
    /// @param[in] ev - event name
    /// @param[in] value - a string to emit
    ///
    /// @returns true if the event has listeners, false otherwise
    virtual bool emit(Nan::AsyncResource *async_resource, const std::string &ev, const std::string &value) const {
        shared_lock<uv_rwlock> master_lock{receivers_lock_};
        auto it = receivers_.find(ev);
        if (it == receivers_.end()) {
            return false;
        }
        master_lock.unlock();

        it->second->emit(async_resource, value);
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
        void notify(Nan::AsyncResource* async_resource, const std::string& value) const {
            v8::Local<v8::Value> info[] = {Nan::New<v8::String>(value).ToLocalChecked()};
            callback_->Call(1, info, async_resource);
        }

        ~Receiver() {
            callback_->Reset();
            delete callback_;
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
            std::lock_guard<uv_rwlock> guard{receivers_list_lock_};
            receivers_list_.emplace_back(std::make_shared<Receiver>(cb));
        }

        /// notify all receivers
        ///
        /// @param[in] value - the string to send to all receivers
        void emit(Nan::AsyncResource* async_resource, const std::string& value) const {
            shared_lock<uv_rwlock> guard{receivers_list_lock_};
            for (auto& receiver : receivers_list_) {
                receiver->notify(async_resource, value);
            }
        }

     private:
        std::vector<std::shared_ptr<Receiver>> receivers_list_;
        mutable uv_rwlock receivers_list_lock_;
    };

    mutable uv_rwlock receivers_lock_;
    std::unordered_map<std::string, std::shared_ptr<ReceiverList>> receivers_;
};

}  // namespace NodeEvent

#endif
