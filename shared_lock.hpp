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
#ifndef _NODE_EVENT_SHARED_LOCK_H
#define _NODE_EVENT_SHARED_LOCK_H

#include <utility>

namespace NodeEvent {
// TODO replace by std::shared_lock
/// shared_lock is a simplifed implementation of C++14's std::shared_lock. I
/// skipped most of the complexity of that impl because I don't need it
template <class T> class shared_lock {
public:
  explicit shared_lock(T &shared_mutex);
  ~shared_lock() noexcept;

  shared_lock(const shared_lock<T> &other) = delete;
  shared_lock &operator=(const shared_lock<T> &other) = delete;

  shared_lock(shared_lock<T> &&other);

  shared_lock &operator=(shared_lock<T> &&other);

  void swap(shared_lock<T> &other);

  void lock();

  void unlock();

  void try_lock();

private:
  T &shared_lock_;
  bool locked_;
};

template <class T> void swap(shared_lock<T> &lhs, shared_lock<T> &rhs);

} // namespace NodeEvent

#endif
