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

#include <algorithm>
#include <array>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "shared_ringbuffer.hpp"

#ifdef HAVE_BOOST

/// XXX(jrb): This fundamentally changes the behavior of this ringbuffer to
/// single-producer/single-consumer. You must ensure that there can only ever be
/// a single producer at a time via external synchronization.
///
/// Sadly, cannot use the multi-producer/multi-consumer because
/// boost::lockfree::queue doesn't allow non-trivial destructors.
#include <boost/lockfree/policies.hpp>
#include <boost/lockfree/spsc_queue.hpp>
////single-consumer, single-producer low-latency lockfree ringbuffer from boost
template <typename T, size_t SIZE>
using RingBuffer =
    boost::lockfree::spsc_queue<T, boost::lockfree::capacity<SIZE>>;

#else

template <typename T, size_t SIZE>
RingBuffer<T, SIZE>::RingBuffer()
    : lock_(), notifier_(), read_idx_(0), write_idx_(0), buf_() {}

template <typename T, size_t SIZE> bool RingBuffer<T, SIZE>::push(T &&val) {
  std::unique_lock<std::mutex> guard{lock_};
  return unlocked_enqueue(std::move(val));
}

template <typename T, size_t SIZE>
void RingBuffer<T, SIZE>::push_blocking(T &&val) {
  std::unique_lock<std::mutex> guard{lock_};
  while (!unlocked_enqueue(std::move(val))) {
    notifier_.wait(guard);
  }
}

template <typename T, size_t SIZE> bool RingBuffer<T, SIZE>::pop(T &val) {
  std::unique_lock<std::mutex> guard{lock_};
  if (!unlocked_read_available()) {
    return false;
  }
  val = unlocked_dequeue();
  return true;
}

template <typename T, size_t SIZE> T &&RingBuffer<T, SIZE>::pop_blocking() {
  std::unique_lock<std::mutex> guard{lock_};
  while (!unlocked_read_available()) {
    notifier_.wait(guard);
  }
  return unlocked_dequeue();
}

template <typename T, size_t SIZE>
inline bool RingBuffer<T, SIZE>::read_available() {
  std::unique_lock<std::mutex> guard{lock_};
  return unlocked_read_available();
}

template <typename T, size_t SIZE>
inline bool RingBuffer<T, SIZE>::unlocked_enqueue(T &&val) {
  // read_idx_ & write_idx_ are unsigned so this subtraction is in the
  // group $\mathbb{Z}_{2^64}$, and so is overrun safe as long as $buf_.size()
  // \divides 2^64$
  if (write_idx_ - read_idx_ >= buf_.size()) {
    return false;
  }

  buf_[write_idx_ % buf_.size()] = std::move(val);
  ++write_idx_;

  // Notify under mutex to ensure consistent behavior; rely on wait-morphing
  // for performance
  notifier_.notify_one();
  return true;
}

template <typename T, size_t SIZE>
inline T &&RingBuffer<T, SIZE>::unlocked_dequeue() {
  auto &&v = std::move(buf_[read_idx_++ % buf_.size()]);
  // notify all blocked writers
  notifier_.notify_all();
  return std::move(v);
}

template <typename T, size_t SIZE>
inline bool RingBuffer<T, SIZE>::unlocked_read_available() {
  return write_idx_ - read_idx_ != 0;
}

#endif
