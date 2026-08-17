/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <utility>

namespace npu_compute::aclpti::data::detail {

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {}

    bool TryPush(T value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_ || queue_.size() == capacity_) {
            return false;
        }
        queue_.push_back(std::move(value));
        readable_.notify_one();
        return true;
    }

    bool Push(T value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        writable_.wait(lock, [this] { return closed_ || queue_.size() < capacity_; });
        if (closed_) {
            return false;
        }
        queue_.push_back(std::move(value));
        readable_.notify_one();
        return true;
    }

    bool Pop(T& value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        readable_.wait(lock, [this] { return closed_ || !queue_.empty(); });
        if (queue_.empty()) {
            return false;
        }
        value = std::move(queue_.front());
        queue_.pop_front();
        writable_.notify_one();
        return true;
    }

    void Close()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        readable_.notify_all();
        writable_.notify_all();
    }

private:
    const std::size_t capacity_;
    std::mutex mutex_;
    std::condition_variable readable_;
    std::condition_variable writable_;
    std::deque<T> queue_;
    bool closed_ = false;
};

} // namespace npu_compute::aclpti::data::detail
