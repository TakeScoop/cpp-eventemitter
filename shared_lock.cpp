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

#include <utility>

#include "shared_lock.hpp"

namespace NodeEvent {

template <class T>
shared_lock<T>::shared_lock(T &shared_mutex)
    : shared_lock_(shared_mutex), locked_{false} {
  lock();
}

template <class T> shared_lock<T>::~shared_lock() noexcept { unlock(); }

template <class T>
shared_lock<T>::shared_lock(shared_lock<T> &&other)
    : shared_lock_(std::move(other.shared_lock_)),
      locked_(std::move(other.locked_)) {}

template <class T>
shared_lock<T> &shared_lock<T>::operator=(shared_lock<T> &&other) {
  shared_lock_ = std::move(other.shared_lock_);
  locked_ = std::move(other.locked_);
  return *this;
}

template <class T> void shared_lock<T>::swap(shared_lock<T> &other) {
  std::swap(this->shared_lock_, other.shared_lock_);
}
template <class T> inline void shared_lock<T>::lock() {
  shared_lock_.lock_shared();
  locked_ = true;
}

template <class T> inline void shared_lock<T>::unlock() {
  if (locked_) {
    locked_ = false;
    shared_lock_.unlock_shared();
  }
}

template <class T> inline void shared_lock<T>::try_lock() {
  shared_lock_.try_lock_shared();
}

template <class T> void swap(shared_lock<T> &lhs, shared_lock<T> &rhs) {
  lhs.swap(rhs);
}

} // namespace NodeEvent
