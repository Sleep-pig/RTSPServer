#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

template <typename T>
class RingBuffer {
public:
    RingBuffer(size_t size = 60) :
        buffer_(size),
        get_index_(0),
        put_index_(0),
        capacity_(size),
        nums_data_(0) {}

    ~RingBuffer() = default;

    bool Push(const T& item) {
        return PushData(std::forward<T>(item));
    }
    bool Push(T&& item) {
        return PushData(item);
    }
    bool Pop(T& item) {
        if (nums_data_ == 0) {
            return false;
        }
        item = buffer_[get_index_];
        get_index_ = (get_index_ + 1) % capacity_;
        --nums_data_;
        return true;
    }

    bool isEmpty() const {
        return nums_data_ == 0;
    }

    bool isFull() const {
        return nums_data_ >= capacity_;
    }

    size_t size() const {
        return nums_data_;
    }

private:
    bool PushData(const T& item) {
        if (nums_data_ >= capacity_) {
            return false;
        }
        buffer_[put_index_] = item;
        put_index_ = (put_index_ + 1) % capacity_;
        ++nums_data_;
        return true;
    }

    std::vector<T> buffer_;
    size_t get_index_;
    size_t put_index_;
    size_t capacity_;
    std::atomic<int> nums_data_;
};