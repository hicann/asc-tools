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
#include "checker/synccheck.h"
#include "diagnostic/report_renderer.h"
#include "diagnostic/report_buffer.h"
#include "ipc/uds_server.h"
#include "logging/logger.h"
#include "wire_protocol.h"

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
    bool EnableCallbacks(std::string& error);
    // 本次是否启用了某个工具。
    bool IsToolEnabled(ipc::ToolId toolId) const;
    void RollbackSanitizer();
    void LogHandshakeFailure(const std::string& reason) noexcept;
    void PublishDiagnostics(std::vector<aclsan::cann::NpusanMemcheckReport> reports);
    void PublishSynccheckReports(std::vector<aclsan::cann::NpusanSynccheckReport> reports);
    bool NormalizeAndStoreReportRecord(
        const aclsan::cann::NpusanReportRecord& report, uint64_t reportId, const char* what);
    void PublishMalformed(AclsanCallbackDomain domain, AclsanCallbackId cbid, const char* reason);
    bool InitializeLogger(std::string& error);
    void LogCallback(AclsanCallbackDomain domain, AclsanCallbackId cbid, const void* cbdata);
    std::string BuildReadyMessage() const;
    std::string BuildSummaryMessage() const;
    // 把一条已渲染的诊断同时送进两条路径：聚合进最终 Result，并作为实时流发出去。
    void RecordDiagnostic(const std::string& rendered, const char* what);
    // 本次检查是否检出问题，对应 Result 末帧的 kFlagHasErrors。
    bool HasDetectedErrors() const;

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

    // 本次会话启用的工具及其子选项，按 toolId 升序。多个工具可以同时启用。
    ipc::ConfigureRequest configure_{};
    // 工作目录：npu_check.log 与 probe 缓存的落点，由环境变量传入（Configure 只承载
    // 工具与子选项，不承载路径）。
    std::string workDir_;
    ipc::UdsServer server_{};
    // 权威报告的聚合缓冲。callback 线程只往里追加，退出路径上一次性取走发出。
    diagnostic::ReportBuffer report_{};
    // 已成功渲染的报告模板记录，用于在会话结束时追加工具级汇总。
    std::vector<aclsan::cann::ReportRecord> reportRecords_;
    AclsanSubscriberHandle subscriber_ = nullptr;
    std::unique_ptr<Memcheck> memcheck_;
    std::unique_ptr<npucheck::Synccheck> synccheck_;
    logging::Logger logger_{};
    std::atomic<uint64_t> callbackCount_{0};
    uint64_t malformedCallbacks_ = 0;
    uint64_t frameworkErrors_ = 0;
};

} // namespace npu::sanitizer

#endif
