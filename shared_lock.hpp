#pragma once
#ifndef _NODE_EVENT_SHARED_LOCK_H
#define _NODE_EVENT_SHARED_LOCK_H

#include <utility>

namespace NodeEvent {
/// shared_lock is a simplifed implementation of C++14's std::shared_lock. I skipped most of the complexity of that
/// impl because I don't need it
template <class T>
class shared_lock {
 public:
    explicit shared_lock(T& shared_mutex) : shared_lock_(shared_mutex), locked_{false} { lock(); }
    ~shared_lock() noexcept { unlock(); }

    shared_lock(const shared_lock<T>& other) = delete;
    shared_lock& operator=(const shared_lock<T>& other) = delete;

    shared_lock(shared_lock<T>&& other)
        : shared_lock_(std::move(other.shared_lock_)), locked_(std::move(other.locked_)) {}

    shared_lock& operator=(shared_lock<T>&& other) {
        shared_lock_ = std::move(other.shared_lock_);
        locked_ = std::move(other.locked_);
        return *this;
    }

    void swap(shared_lock<T>& other) { std::swap(this->shared_lock_, other.shared_lock_); }

    inline void lock() {
        shared_lock_.lock_shared();
        locked_ = true;
    }

    inline void unlock() {
        if (locked_) {
            locked_ = false;
            shared_lock_.unlock_shared();
        }
    }

    inline void try_lock() { shared_lock_.try_lock_shared(); }

 private:
    T& shared_lock_;
    bool locked_;
};

template <class T>
void swap(shared_lock<T>& lhs, shared_lock<T>& rhs) {
    lhs.swap(rhs);
}

}  // namespace NodeEvent

#endif
