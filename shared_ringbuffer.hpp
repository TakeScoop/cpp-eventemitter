#pragma once
#ifndef _GLPK_RINGBUFFER_H
#define _GLPK_RINGBUFFER_H

#include <algorithm>
#include <condition_variable>
#include <array>
#include <memory>
#include <mutex>
#include <thread>

/// Multi-consumer, multi-producer, condition_variable signalled Shared
/// ringbuffer (contiguous memory; mutex)
template <typename T, size_t SIZE>
class RingBuffer {
    constexpr static bool divides_evenly(size_t v) { return (static_cast<size_t>(-1) % v) == v - 1; }

    static_assert(divides_evenly(SIZE), "SIZE does not divide 2^64, so behavior on overrun would be erratic");

 public:
    RingBuffer() : lock_(), notifier_(), read_idx_(0), write_idx_(0), buf_() {}

    /// Move val into the ringbuffer. If this fails, the object is still moved
    /// (and so, effectively lost at this point). If this is a problem, make a
    /// copy and move that copy
    ///
    /// @param[in] val - the value to try to push
    ///
    /// @returns true if successful, false if the buffer was full
    bool enqueue_nonblocking(T&& val) {
        std::unique_lock<std::mutex> guard{lock_};
        return unlocked_enqueue(std::move(val));
    }

    /// Move the value into the queue
    ///
    /// @param[in] val - the value to enqueue
    void enqueue(T&& val) {
        std::unique_lock<std::mutex> guard{lock_};
        while (!unlocked_enqueue(std::move(val))) {
            notifier_.wait(guard);
        }
    }

    /// Dequeue an element into val
    ///
    /// @param[out] val - place to put item from the queue
    ///
    /// @returns true if there was something to dequeue, false otherwise
    bool dequeue_nonblocking(T& val) {
        std::unique_lock<std::mutex> guard{lock_};
        if (!unlocked_available()) {
            return false;
        }
        val = unlocked_dequeue();
        return true;
    }

    /// Blocking attempt to dequeue an item
    ///
    /// @returns the dequeued element
    T&& dequeue() {
        std::unique_lock<std::mutex> guard{lock_};
        while (!unlocked_available()) {
            notifier_.wait(guard);
        }
        return unlocked_dequeue();
    }

    inline bool available() {
        std::unique_lock<std::mutex> guard{lock_};
        return unlocked_available();
    }


 private:
    inline bool unlocked_enqueue(T&& val) {
        // read_idx_ & write_idx_ are unsigned so this subtraction is in the
        // group $\mathbb{Z}_{2^64}$, and so is overrun safe as long as $buf_.size() \divides 2^64$
        if (write_idx_ - read_idx_ >= buf_.size()) {
            return false;
        }

        buf_[write_idx_ % buf_.size()] = std::move(val);
        ++write_idx_;

        // Notify under mutex to ensure consistent behavior; rely on wait-morphing for performance
        notifier_.notify_one();
        return true;
    }

    inline T&& unlocked_dequeue() {
        auto&& v = std::move(buf_[read_idx_++ % buf_.size()]);
        // notify all blocked writers
        notifier_.notify_all();
        return std::move(v);
    }

    inline bool unlocked_available() { return write_idx_ - read_idx_ != 0; }

    std::mutex lock_;
    std::condition_variable notifier_;
    size_t read_idx_;
    size_t write_idx_;
    std::array<T, SIZE> buf_;
};

#endif
