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
#ifndef _NODE_EVENT_RINGBUFFER_H
#define _NODE_EVENT_RINGBUFFER_H

#include <algorithm>
#include <array>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

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

/// Multi-consumer, multi-producer, condition_variable signalled Shared
/// ringbuffer (contiguous memory; mutex)
template <typename T, size_t SIZE> class RingBuffer {
  constexpr static bool divides_evenly(size_t v) {
    return (static_cast<size_t>(-1) % v) == v - 1;
  }
  static_assert(divides_evenly(SIZE),
                "SIZE does not divide $2^{sizeof(size_t)*8}$, so behavior on "
                "overrun would be erratic");

public:
  RingBuffer();

  /// Move val into the ringbuffer. If this fails, the object is still moved
  /// (and so, effectively lost at this point). If this is a problem, make a
  /// copy and move that copy
  ///
  /// @param[in] val - the value to try to push
  ///
  /// @returns true if successful, false if the buffer was full
  bool push(T &&val);

  /// Move the value into the queue
  /// (no boost equiv)
  /// @param[in] val - the value to enqueue
  void push_blocking(T &&val);

  /// Dequeue an element into val
  ///
  /// @param[out] val - place to put item from the queue
  ///
  /// @returns true if there was something to dequeue, false otherwise
  bool pop(T &val);

  /// Blocking attempt to dequeue an item
  /// (no boost equiv)
  /// @returns the dequeued element
  T &&pop_blocking();

  /// @returns true if the ringbuffer is empty
   bool read_available();

private:
  bool unlocked_enqueue(T &&val);

  T &&unlocked_dequeue();

   bool unlocked_read_available();

  std::mutex lock_;
  std::condition_variable notifier_;
  size_t read_idx_;
  size_t write_idx_;
  std::array<T, SIZE> buf_;
};
#endif

#endif
