#pragma once
#ifndef _GLPK_RINGBUFFER_H
#define _GLPK_RINGBUFFER_H

#include <algorithm>
#include <array>
#include <memory>
#include <mutex>

/// Multi-consumer, multi-producer, condition_variable signalled Shared
/// ringbuffer (contiguous memory; mutex)
template <typename T, size_t SIZE>
class RingBuffer {
    constexpr static bool divides_evenly(int v) { return (static_cast<size_t>(-1) % v) == v - 1; }

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

        // read_idx_ & write_idx_ are unsigned so this subtraction is in the
        // group $\mathbb{Z}_{2^64}$, and so is overrun safe as long as $buf_.size() \divides 2^64$
        if (write_idx_ - read_idx_ >= buf_.size()) {
            return false;
        }

        buf_[write_idx_ % buf_.size()] = std::move(val);
        ++write_idx_;

        // unlock before notify so we don't put the notified thread back to sleep immediately
        guard.unlock();
        notifier_.notify_one();
        return true;
    }

    /// Move the value into the queue
    ///
    /// @param[in] val - the value to enqueue
    void enqueue(T&& val) {
        while (!enqueue_nonblocking(std::move(val)))
            ;
    }

    /// Dequeue an element into val
    ///
    /// @param[out] val - place to put item from the queue
    ///
    /// @returns true if there was something to dequeue, false otherwise
    bool dequeue_nonblocking(T& val) {
        std::unique_lock<std::mutex> guard{lock_};
        if (!available()) {
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
        while (!available()) {
            notifier_.wait(guard);
        }
        return unlocked_dequeue();
    }

    inline bool available() { return write_idx_ - read_idx_ != 0; }

 private:
    inline T&& unlocked_dequeue() { return std::move(buf_[read_idx_++]); }

    std::mutex lock_;
    std::condition_variable notifier_;
    size_t read_idx_;
    size_t write_idx_;
    std::array<T, SIZE> buf_;
};

#endif
