// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#ifndef NPU_CHECK_TOOL_MANAGER_TOOL_MANAGER_H
#define NPU_CHECK_TOOL_MANAGER_TOOL_MANAGER_H

#include "aclsan/aclsan_api.h"
#include "checker/memcheck.h"
#include "wire_protocol.h"
#include "ipc/uds_server.h"
#include "diagnostic/source_resolver.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace npu::sanitizer {

class ToolManager {
public:
    ToolManager() = default;
    ~ToolManager() noexcept;
    ToolManager(const ToolManager&) = delete;
    ToolManager& operator=(const ToolManager&) = delete;

    int Initialize();
    void Finalize();
    bool IsInitialized() const;

private:
    class ActiveCallbackGuard {
    public:
        explicit ActiveCallbackGuard(ToolManager& service) noexcept : service_(service) {}
        ~ActiveCallbackGuard() noexcept;
        ActiveCallbackGuard(const ActiveCallbackGuard&) = delete;
        ActiveCallbackGuard& operator=(const ActiveCallbackGuard&) = delete;

    private:
        ToolManager& service_;
    };

    static void Callback(
        void* userdata, AclsanCallbackDomain domain, AclsanCallbackId cbid, const void* cbdata) noexcept;
    void OnCallback(AclsanCallbackDomain domain, AclsanCallbackId cbid, const void* cbdata);
    void OnCallbackException(const char* reason) noexcept;
    bool EnterCallback();
    void LeaveCallback();
    bool ConfigureSanitizer(std::string& error);
    bool BuildLaunchConfig(std::string& error);
    bool EnableMemcheckCallbacks(std::string& error);
    void RollbackSanitizer();
    void PublishDiagnostics(std::vector<Diagnostic> diagnostics);
    void PublishMalformed(AclsanCallbackDomain domain, AclsanCallbackId cbid, const char* reason);
    std::string BuildReadyMessage() const;
    std::string BuildSummaryMessage() const;

    template <typename T>
    static const T* ValidateCallbackData(const void* cbdata)
    {
        if (cbdata == nullptr) {
            return nullptr;
        }
        const auto* typed = static_cast<const T*>(cbdata);
        if (typed->common.version != ACLSAN_API_VERSION || typed->common.size < sizeof(T)) {
            return nullptr;
        }
        return typed;
    }

    static const AclsanDeviceMemoryAccessData* ValidateDeviceMemoryAccessData(const void* cbdata)
    {
        if (cbdata == nullptr) {
            return nullptr;
        }
        const auto* typed = static_cast<const AclsanDeviceMemoryAccessData*>(cbdata);
        if (typed->header.version != ACLSAN_API_VERSION || typed->header.size < sizeof(AclsanDeviceMemoryAccessData)) {
            return nullptr;
        }
        return typed;
    }

    mutable std::mutex lifecycleMutex_;
    std::mutex callbackMutex_;
    std::condition_variable callbacksDrained_;
    mutable std::mutex stateMutex_;
    uint64_t activeCallbacks_ = 0;
    bool stopping_ = false;
    bool initialized_ = false;
    bool subscribed_ = false;

    ipc::ToolConfig config_{};
    AclsanLaunchConfig launchConfig_{};
    ipc::UdsServer server_{};
    AclsanSubscriberHandle subscriber_ = ACLSAN_INVALID_SUBSCRIBER_HANDLE;
    std::unique_ptr<Memcheck> memcheck_;
    SourceResolver sourceResolver_{};
    std::atomic<uint64_t> diagnosticOrdinal_{0};
    uint64_t malformedCallbacks_ = 0;
    uint64_t frameworkErrors_ = 0;
};

} // namespace npu::sanitizer

#endif
