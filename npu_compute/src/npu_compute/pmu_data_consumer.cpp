/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "pmu_data_consumer.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

namespace npu_compute {
namespace {

constexpr std::size_t kQueueCapacity = 1024;

enum class ConsumerState { Created, Running, Stopping, Stopped };

} // namespace

class PmuDataConsumer::Impl {
public:
    explicit Impl(Processor processor) : processor_(std::move(processor)) {}

    ~Impl() { ShutdownAndDrain(); }

    aclptiResult Start()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == ConsumerState::Running) {
            return ACLPTI_SUCCESS;
        }
        if (state_ != ConsumerState::Created || !processor_) {
            return ACLPTI_ERROR_INVALID_STATE;
        }
        state_ = ConsumerState::Running;
        try {
            worker_ = std::thread(&Impl::ConsumeLoop, this);
        } catch (...) {
            state_ = ConsumerState::Stopped;
            return ACLPTI_ERROR_INTERNAL;
        }
        return ACLPTI_SUCCESS;
    }

    aclptiResult Submit(std::shared_ptr<const aclptiPmuDataResult> result)
    {
        if (!result) {
            return ACLPTI_ERROR_INVALID_PARAMETER;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != ConsumerState::Running) {
            return ACLPTI_ERROR_INVALID_STATE;
        }
        if (queue_.size() == kQueueCapacity) {
            if (firstError_ == ACLPTI_SUCCESS) {
                firstError_ = ACLPTI_ERROR_QUEUE_FULL;
            }
            return ACLPTI_ERROR_QUEUE_FULL;
        }
        queue_.push_back(std::move(result));
        readable_.notify_one();
        return ACLPTI_SUCCESS;
    }

    aclptiResult ShutdownAndDrain()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (state_ == ConsumerState::Created) {
                state_ = ConsumerState::Stopped;
                return ACLPTI_SUCCESS;
            }
            if (state_ == ConsumerState::Stopped) {
                return firstError_;
            }
            state_ = ConsumerState::Stopping;
            readable_.notify_all();
        }
        if (worker_.joinable()) {
            worker_.join();
        }
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = ConsumerState::Stopped;
        return firstError_;
    }

private:
    void ConsumeLoop()
    {
        while (true) {
            std::shared_ptr<const aclptiPmuDataResult> result;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                readable_.wait(lock, [this] { return state_ != ConsumerState::Running || !queue_.empty(); });
                if (queue_.empty()) {
                    return;
                }
                result = std::move(queue_.front());
                queue_.pop_front();
            }

            aclptiResult status = ACLPTI_ERROR_INTERNAL;
            try {
                status = processor_(std::move(result));
            } catch (...) {
            }
            if (status != ACLPTI_SUCCESS) {
                std::lock_guard<std::mutex> lock(mutex_);
                if (firstError_ == ACLPTI_SUCCESS) {
                    firstError_ = status;
                }
            }
        }
    }

    Processor processor_;
    std::mutex mutex_;
    std::condition_variable readable_;
    std::deque<std::shared_ptr<const aclptiPmuDataResult>> queue_;
    ConsumerState state_ = ConsumerState::Created;
    aclptiResult firstError_ = ACLPTI_SUCCESS;
    std::thread worker_;
};

std::shared_ptr<PmuDataConsumer> PmuDataConsumer::Create(Processor processor)
{
    if (!processor) {
        return nullptr;
    }
    return std::shared_ptr<PmuDataConsumer>(new PmuDataConsumer(std::move(processor)));
}

PmuDataConsumer::PmuDataConsumer(Processor processor) : impl_(std::make_unique<Impl>(std::move(processor))) {}

PmuDataConsumer::~PmuDataConsumer() = default;

aclptiResult PmuDataConsumer::Start() { return impl_->Start(); }

aclptiResult PmuDataConsumer::Submit(std::shared_ptr<const aclptiPmuDataResult> result)
{
    return impl_->Submit(std::move(result));
}

aclptiResult PmuDataConsumer::ShutdownAndDrain() { return impl_->ShutdownAndDrain(); }

} // namespace npu_compute
