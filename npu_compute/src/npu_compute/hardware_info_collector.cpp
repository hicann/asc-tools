/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hardware_info_collector.h"

#include "hardware_device_api.h"
#include "hardware_info_host.h"
#include "hardware_info_json.h"

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace npu_compute {
namespace {

void DefaultDiagnostic(std::string_view message)
{
    std::fputs("[libnpu-compute] HardwareInfo: ", stderr);
    std::fwrite(message.data(), 1, message.size(), stderr);
    std::fputc('\n', stderr);
}

HardwareInfoDependencies MakeDefaultDependencies()
{
    HardwareInfoDependencies dependencies;
    dependencies.collectHostInfo = [](const std::filesystem::path& outputDirectory, HostInfo* host,
                                      DiagnosticSink* diagnostics) {
        return CollectHostInfo(outputDirectory, host, diagnostics);
    };
    dependencies.deviceApi = std::make_shared<DynamicHardwareDeviceApi>();
    dependencies.publish = PublishHardwareInfoJsonl;
    dependencies.diagnostics = DefaultDiagnostic;
    return dependencies;
}

} // namespace

class HardwareInfoCollector::Impl {
public:
    explicit Impl(HardwareInfoDependencies dependencies) : dependencies_(std::move(dependencies)) {}

    ~Impl() { Stop(); }

    bool Initialize(const std::filesystem::path& outputDirectory, std::string* error)
    {
        std::lock_guard<std::mutex> lifecycleLock(lifecycleMutex_);
        if (error != nullptr) {
            error->clear();
        }
        if (state_.load(std::memory_order_acquire) != HardwareCollectionState::Created) {
            SetError(error, "HardwareInfoCollector is already initialized");
            return false;
        }
        if (!dependencies_.collectHostInfo || dependencies_.deviceApi == nullptr || !dependencies_.publish) {
            state_.store(HardwareCollectionState::Failed, std::memory_order_release);
            SetError(error, "HardwareInfoCollector dependencies are incomplete");
            return false;
        }

        try {
            outputDirectory_ = outputDirectory;
            {
                std::lock_guard<std::mutex> lock(waitMutex_);
                stopRequested_ = false;
            }
            state_.store(HardwareCollectionState::WaitingRuntime, std::memory_order_release);
            worker_ = std::thread(&Impl::Worker, this);
        } catch (const std::exception& exception) {
            state_.store(HardwareCollectionState::Failed, std::memory_order_release);
            SetError(error, std::string("start HardwareInfo worker failed: ") + exception.what());
            return false;
        } catch (...) {
            state_.store(HardwareCollectionState::Failed, std::memory_order_release);
            SetError(error, "start HardwareInfo worker failed: unknown exception");
            return false;
        }
        return true;
    }

    void NotifyRuntimeReady() noexcept
    {
        bool notifyWorker = false;
        try {
            std::lock_guard<std::mutex> lock(waitMutex_);
            HardwareCollectionState expected = HardwareCollectionState::WaitingRuntime;
            notifyWorker = state_.compare_exchange_strong(
                expected, HardwareCollectionState::Collecting, std::memory_order_acq_rel, std::memory_order_acquire);
        } catch (...) {
            state_.store(HardwareCollectionState::Failed, std::memory_order_release);
            Report("notify HardwareInfo worker failed");
            return;
        }
        if (notifyWorker) {
            waitCondition_.notify_one();
        }
    }

    void Stop() noexcept
    {
        std::lock_guard<std::mutex> lifecycleLock(lifecycleMutex_);
        {
            std::lock_guard<std::mutex> lock(waitMutex_);
            stopRequested_ = true;
            HardwareCollectionState expected = HardwareCollectionState::WaitingRuntime;
            state_.compare_exchange_strong(
                expected, HardwareCollectionState::NoRuntimeReady, std::memory_order_acq_rel,
                std::memory_order_acquire);
        }
        waitCondition_.notify_one();

        if (worker_.joinable()) {
            try {
                worker_.join();
            } catch (const std::exception& exception) {
                state_.store(HardwareCollectionState::Failed, std::memory_order_release);
                Report(std::string("join HardwareInfo worker failed: ") + exception.what());
            } catch (...) {
                state_.store(HardwareCollectionState::Failed, std::memory_order_release);
                Report("join HardwareInfo worker failed: unknown exception");
            }
        }
    }

    HardwareCollectionState State() const noexcept { return state_.load(std::memory_order_acquire); }

private:
    static void SetError(std::string* error, const std::string& message)
    {
        if (error != nullptr) {
            *error = message;
        }
    }

    void Report(std::string_view message) noexcept
    {
        try {
            if (dependencies_.diagnostics) {
                dependencies_.diagnostics(message);
            }
        } catch (...) {
        }
    }

    void Worker() noexcept
    {
        try {
            {
                std::unique_lock<std::mutex> lock(waitMutex_);
                waitCondition_.wait(lock, [this] {
                    return stopRequested_ ||
                           state_.load(std::memory_order_acquire) == HardwareCollectionState::Collecting;
                });
            }

            if (state_.load(std::memory_order_acquire) != HardwareCollectionState::Collecting) {
                return;
            }
            Collect();
        } catch (const std::exception& exception) {
            state_.store(HardwareCollectionState::Failed, std::memory_order_release);
            Report(std::string("HardwareInfo collection failed: ") + exception.what());
        } catch (...) {
            state_.store(HardwareCollectionState::Failed, std::memory_order_release);
            Report("HardwareInfo collection failed: unknown exception");
        }
    }

    void Collect()
    {
        HardwareInfoSnapshot snapshot;
        DiagnosticSink* diagnostics = dependencies_.diagnostics ? &dependencies_.diagnostics : nullptr;
        if (!dependencies_.collectHostInfo(outputDirectory_, &snapshot.host, diagnostics)) {
            Fail("Host HardwareInfo collection failed");
            return;
        }
        if (!CollectDevice0Info(
                *dependencies_.deviceApi, &snapshot.device, &snapshot.cpu, &snapshot.aiCore, &snapshot.memory,
                diagnostics)) {
            Fail("Device HardwareInfo collection failed");
            return;
        }

        std::string jsonl;
        std::string error;
        if (!SerializeHardwareInfoJsonl(snapshot, &jsonl, &error)) {
            Fail("HardwareInfo JSONL serialization failed: " + error);
            return;
        }

        const PublishResult result = dependencies_.publish(outputDirectory_, jsonl, &error);
        if (result == PublishResult::Failed) {
            Fail("HardwareInfo publication failed: " + error);
            return;
        }
        state_.store(HardwareCollectionState::Completed, std::memory_order_release);
    }

    void Fail(const std::string& message) noexcept
    {
        state_.store(HardwareCollectionState::Failed, std::memory_order_release);
        Report(message);
    }

    HardwareInfoDependencies dependencies_;
    std::filesystem::path outputDirectory_;
    std::atomic<HardwareCollectionState> state_{HardwareCollectionState::Created};
    std::mutex lifecycleMutex_;
    std::mutex waitMutex_;
    std::condition_variable waitCondition_;
    bool stopRequested_ = false;
    std::thread worker_;
};

HardwareInfoCollector::HardwareInfoCollector() : impl_(std::make_unique<Impl>(MakeDefaultDependencies())) {}

HardwareInfoCollector::HardwareInfoCollector(HardwareInfoDependencies dependencies)
    : impl_(std::make_unique<Impl>(std::move(dependencies)))
{}

HardwareInfoCollector::~HardwareInfoCollector() = default;

bool HardwareInfoCollector::Initialize(const std::filesystem::path& outputDirectory, std::string* error)
{
    return impl_->Initialize(outputDirectory, error);
}

void HardwareInfoCollector::NotifyRuntimeReady() noexcept { impl_->NotifyRuntimeReady(); }

void HardwareInfoCollector::Stop() noexcept { impl_->Stop(); }

HardwareCollectionState HardwareInfoCollector::State() const noexcept { return impl_->State(); }

} // namespace npu_compute
