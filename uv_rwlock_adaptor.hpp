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
#ifndef _NODE_EVENT_UVRWLOCK_ADAPTOR_H
#define _NODE_EVENT_UVRWLOCK_ADAPTOR_H

#include <utility>

#include <uv.h>

namespace NodeEvent {
/// To avoid requiring c++14, using an adaptor for uv locks to make them behave
/// like std::mutex so they can be used with std::lock_guard and
/// std::unique_lock and std:shared_lock (but not using std::shared_lock to
/// avoid c++14)
class uv_rwlock {
public:
  uv_rwlock();
  ~uv_rwlock() noexcept;

  uv_rwlock(const uv_rwlock &other) = delete;
  uv_rwlock &operator=(const uv_rwlock &other) = delete;

  uv_rwlock(uv_rwlock &&other);

  uv_rwlock &operator=(uv_rwlock &&other);

  void swap(uv_rwlock &other);

  void lock();
  void unlock();
  bool try_lock();
  void lock_shared();
  void unlock_shared();
  bool try_lock_shared();

private:
  uv_rwlock_t lock_;
};

void swap(uv_rwlock &lhs, uv_rwlock &rhs);

} // namespace NodeEvent

#endif
