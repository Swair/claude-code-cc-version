// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <array>
#include <mutex>
#include <vector>

#include "common/noncopyable.h"

namespace prosophor {

// ── ListBuffer: thread-safe growing buffer ──

template<typename T>
class ListBuffer : public Noncopyable {
public:
    ListBuffer() = default;

    ListBuffer(ListBuffer&& other) noexcept {
        std::lock_guard<std::mutex> lock(other.mtx_);
        data_ = std::move(other.data_);
    }

    ListBuffer& operator=(ListBuffer&& other) noexcept {
        if (this == &other) return *this;
        std::lock_guard<std::mutex> lock(mtx_);
        std::lock_guard<std::mutex> lock_other(other.mtx_);
        data_ = std::move(other.data_);
        return *this;
    }

    void Push(const T& item) {
        std::lock_guard<std::mutex> lock(mtx_);
        data_.push_back(item);
    }

    void Push(const T* data, size_t count) {
        std::lock_guard<std::mutex> lock(mtx_);
        data_.insert(data_.end(), data, data + count);
    }

    std::vector<T> PopAll() {
        std::lock_guard<std::mutex> lock(mtx_);
        std::vector<T> out;
        std::swap(out, data_);
        return out;
    }

    /// Pop at most n items into pre-allocated out buffer. Returns actual count.
    size_t PopFront(T* out, size_t n) {
        std::lock_guard<std::mutex> lock(mtx_);
        n = std::min(n, data_.size());
        if (n == 0) return 0;
        std::copy_n(data_.begin(), n, out);
        data_.erase(data_.begin(), data_.begin() + n);
        return n;
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(mtx_);
        data_.clear();
    }

    bool Empty() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return data_.empty();
    }

    int Size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return static_cast<int>(data_.size());
    }

private:
    std::vector<T> data_;
    mutable std::mutex mtx_;
};

// ── FixedBuffer: fixed-capacity ring buffer (always keeps last N) ──

/// Non-thread-safe ring buffer. Always keeps the most recent kCapacity items.
template<typename T, int kCapacity>
class FixedBuffer {
public:
    void Push(const T& item) {
        data_[head_] = item;
        head_ = (head_ + 1) % kCapacity;
        if (count_ < kCapacity) count_++;
    }

    std::vector<T> ReadAll() const {
        std::vector<T> out;
        if (count_ == 0) return out;
        out.reserve(count_);
        int start = (head_ - count_ + kCapacity) % kCapacity;
        for (int i = 0; i < count_; i++)
            out.push_back(data_[(start + i) % kCapacity]);
        return out;
    }

    void Clear() {
        head_ = 0;
        count_ = 0;
    }

    int Size() const { return count_; }
    static constexpr int Capacity() { return kCapacity; }

private:
    T data_[kCapacity];
    int head_ = 0;
    int count_ = 0;
};

}  // namespace prosophor
