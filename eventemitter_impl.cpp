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
#include <unordered_map>
#include <vector>

#include <nan.h>
#include <uv.h>

#include "constructable.hpp"
#include "eventemitter_impl.hpp"
#include "shared_lock.hpp"
#include "shared_ringbuffer.hpp"
#include "uv_rwlock_adaptor.hpp"

namespace NodeEvent {
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

typedef std::pair<std::string, std::shared_ptr<Constructable>> ProgressReport;

EventEmitter::InvalidEvent::InvalidEvent(const std::string &msg)
    : std::runtime_error(msg) {}

EventEmitter::EventEmitter() : receivers_lock_(), receivers_() {}
EventEmitter::~EventEmitter() noexcept = default;

void EventEmitter::on(const std::string &ev, Nan::Callback *cb) {
  std::unique_lock<uv_rwlock> master_lock{receivers_lock_};
  auto it = receivers_.find(ev);

  if (it == receivers_.end()) {
    receivers_.emplace(ev, std::make_shared<ReceiverList>());
    it = receivers_.find(ev);
  }
  master_lock.unlock();

  it->second->emplace_back(cb);
}

void EventEmitter::removeAllListenersForEvent(const std::string &ev) {
  std::unique_lock<uv_rwlock> master_lock{receivers_lock_};
  auto it = receivers_.find(ev);

  if (it != receivers_.end()) {
    receivers_.erase(it);
  }
}

void EventEmitter::removeAllListeners() {
  std::unique_lock<uv_rwlock> master_lock{receivers_lock_};
  receivers_.clear();
}

std::vector<std::string> EventEmitter::eventNames() {
  std::unique_lock<uv_rwlock> master_lock{receivers_lock_};
  std::vector<std::string> keys;

  for (auto it : receivers_) {
    keys.emplace_back(it.first);
  }
  return keys;
}

bool EventEmitter::emit(Nan::AsyncResource *async_resource,
                        Nan::HandleScope &scope, v8::Isolate *isolate,
                        const std::string &event, EventValue value) const {
  shared_lock<uv_rwlock> master_lock{receivers_lock_};
  auto it = receivers_.find(event);
  if (it == receivers_.end()) {
    return false;
  }
  master_lock.unlock();

  it->second->emit(async_resource, scope, isolate, value);
  return true;
}

EventEmitter::Receiver::Receiver(Nan::Callback *callback)
    : callback_(callback) {}

void EventEmitter::Receiver::notify(Nan::AsyncResource *async_resource,
                                    Nan::HandleScope &scope,
                                    v8::Isolate *isolate,
                                    const EventValue value) const {
  v8::Local<v8::Value> info[] = {value->construct(scope, isolate)};

  // or  Nan::TypeError("First argument must be string");

  callback_->Call(1, info, async_resource);
}

EventEmitter::Receiver::~Receiver() {
  callback_->Reset();
  delete callback_;
}

EventEmitter::ReceiverList::ReceiverList()
    : receivers_list_(), receivers_list_lock_() {}

void EventEmitter::ReceiverList::emplace_back(Nan::Callback *cb) {
  std::lock_guard<uv_rwlock> guard{receivers_list_lock_};
  receivers_list_.emplace_back(std::make_shared<Receiver>(cb));
}

void EventEmitter::ReceiverList::emit(Nan::AsyncResource *async_resource,
                                      Nan::HandleScope &scope,
                                      v8::Isolate *isolate,
                                      EventValue value) const {
  shared_lock<uv_rwlock> guard{receivers_list_lock_};
  for (auto &receiver : receivers_list_) {
    receiver->notify(async_resource, scope, isolate, value);
  }
}

} // namespace NodeEvent
