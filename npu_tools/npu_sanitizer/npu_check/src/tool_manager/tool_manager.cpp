// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "tool_manager/tool_manager.h"

#include "diagnostic/report/report_normalizer.h"
#include "diagnostic/report/device_call_stack.h"

#include <algorithm>
#include <exception>
#include <cstdlib>
#include "plog_sink.h"
#include <memory>
#include <sstream>

namespace npucheck {
namespace {

std::string StatusMessage(const char* operation, AclsanStatus status)
{
    std::ostringstream output;
    output << operation << " failed with AclsanStatus=" << static_cast<int>(status);
    return output.str();
}

} // namespace

ToolManager::~ToolManager() noexcept
{
    try {
        Finalize();
    } catch (...) {
        // Destruction runs at process exit and must not cross the C injection boundary.
        return;
    }
}

ToolManager::ActiveCallbackGuard::~ActiveCallbackGuard() noexcept { service_.LeaveCallback(); }

int ToolManager::Initialize()
{
    std::lock_guard<std::mutex> lifecycleLock(lifecycleMutex_);
    if (initialized_) {
        return 0;
    }

    std::string error;
    if (!server_.StartAndHandshake(configure_, error)) {
        LogHandshakeFailure(error);
        server_.Shutdown();
        return 1;
    }
    const char* workDir = std::getenv(npucheck::ipc::kWorkDirEnv);
    workDir_ = workDir != nullptr ? workDir : "";
    std::ostringstream handshakeMessage;
    handshakeMessage << "UDS handshake completed session=" << server_.SessionId()
                     << " negotiated_minor=" << server_.NegotiatedMinor();
    npucheck::WritePlog(npucheck::PlogLevel::kInfo, handshakeMessage.str());
    std::ostringstream configMessage;
    configMessage << "tool configuration work_dir=" << workDir_ << " tool_count=" << configure_.tools.size();
    for (const auto& tool : configure_.tools) {
        configMessage << " tool=" << npucheck::ipc::ToolName(tool.toolId) << " option_count=" << tool.options.size();
        for (const auto& option : tool.options) {
            configMessage << " option_id=0x" << std::hex << static_cast<unsigned>(option.optionId) << std::dec;
        }
    }
    npucheck::WritePlog(npucheck::PlogLevel::kInfo, configMessage.str());
    if (!ConfigureSanitizer(error)) {
        npucheck::WritePlog(npucheck::PlogLevel::kError, error);
        server_.SendInitializationError(
            npucheck::ipc::ErrorDomain::kConfiguration, npucheck::ipc::error_code::kToolInitializationFailed, error);
        RollbackSanitizer();
        server_.Shutdown();
        return 1;
    }
    // Ready 不带 payload，会话细节只写 plog。
    npucheck::WritePlog(npucheck::PlogLevel::kInfo, BuildReadyMessage());
    if (!server_.SendReady(error)) {
        npucheck::WritePlog(npucheck::PlogLevel::kError, error);
        RollbackSanitizer();
        server_.Shutdown();
        return 1;
    }
    initialized_ = true;
    npucheck::WritePlog(npucheck::PlogLevel::kInfo, "npu_check initialization completed");
    return 0;
}

void ToolManager::LogHandshakeFailure(const std::string& reason) noexcept
{
    try {
        npucheck::WritePlog(npucheck::PlogLevel::kError, "UDS handshake failed: " + reason);
    } catch (...) {
        return;
    }
}

bool ToolManager::ConfigureSanitizer(std::string& error)
{
    if (configure_.tools.empty()) {
        error = "configure enabled no tool";
        return false;
    }
    for (const auto& tool : configure_.tools) {
        auto checker = CreateChecker(tool.toolId);
        if (!checker) {
            error = std::string("unsupported tool '") + npucheck::ipc::ToolName(tool.toolId) + "'";
            return false;
        }
        checkers_.push_back(std::move(checker));
    }
    AclsanStatus status = aclsanSubscribe(&subscriber_, &ToolManager::Callback, this);
    if (status != ACLSAN_STATUS_SUCCESS) {
        error = StatusMessage("aclsanSubscribe", status);
        return false;
    }
    subscribed_ = true;
    npucheck::WritePlog(npucheck::PlogLevel::kInfo, "sanitizer callback subscriber registered");
    return EnableCallbacks(error);
}

bool ToolManager::EnableCallbacks(std::string& error)
{
    const auto required = RequiredCallbacks(checkers_);

    for (const auto& callback : required) {
        const AclsanStatus status = aclsanEnableCallback(1, subscriber_, callback.domain, callback.cbid);
        if (status != ACLSAN_STATUS_SUCCESS) {
            error = StatusMessage("aclsanEnableCallback", status);
            return false;
        }
        std::ostringstream message;
        message << "callback enabled domain=" << static_cast<uint32_t>(callback.domain)
                << " cbid=" << static_cast<uint32_t>(callback.cbid);
        npucheck::WritePlog(npucheck::PlogLevel::kDebug, message.str());
    }
    return true;
}

void ToolManager::RollbackSanitizer()
{
    {
        std::lock_guard<std::mutex> callbackLock(callbackMutex_);
        stopping_ = true;
    }
    if (subscribed_) {
        (void)aclsanUnsubscribe(subscriber_);
        subscribed_ = false;
        subscriber_ = nullptr;
    }
    {
        std::unique_lock<std::mutex> callbackLock(callbackMutex_);
        callbacksDrained_.wait(callbackLock, [this] { return activeCallbacks_ == 0; });
    }
    checkers_.clear();
}

void ToolManager::Finalize()
{
    std::lock_guard<std::mutex> lifecycleLock(lifecycleMutex_);
    if (!initialized_ && !subscribed_) {
        return;
    }
    {
        std::lock_guard<std::mutex> callbackLock(callbackMutex_);
        stopping_ = true;
    }
    AclsanStatus unsubscribeStatus = ACLSAN_STATUS_SUCCESS;
    if (subscribed_) {
        unsubscribeStatus = aclsanUnsubscribe(subscriber_);
        subscribed_ = false;
        subscriber_ = nullptr;
    }
    {
        std::unique_lock<std::mutex> callbackLock(callbackMutex_);
        callbacksDrained_.wait(callbackLock, [this] { return activeCallbacks_ == 0; });
    }

    // 当前不再发布实时诊断；停掉 publisher 线程后，Result 独占后续线路，且
    // dropped_messages 才是最终值。
    server_.StopPublisher();

    std::vector<npucheck::ReportRecord> reportRecords;
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        reportRecords.swap(reportRecords_);
    }
    std::string renderedReport;
    const auto renderStatus = npucheck::RenderReportBundle(reportRecords, {}, &renderedReport);
    const bool reportBundleAvailable = renderStatus == npucheck::ReportRenderStatus::kSuccess;
    if (!reportBundleAvailable) {
        {
            std::lock_guard<std::mutex> stateLock(stateMutex_);
            ++frameworkErrors_;
        }
        npucheck::WritePlog(
            npucheck::PlogLevel::kError,
            "failed to render the session report bundle status=" + std::to_string(static_cast<int>(renderStatus)));
    } else if (!report_.Append(renderedReport) && !report_.Truncated()) {
        npucheck::WritePlog(npucheck::PlogLevel::kError, "failed to record the rendered session report bundle");
    }

    const std::string summary = BuildSummaryMessage();
    npucheck::WritePlog(npucheck::PlogLevel::kInfo, summary);
    // 多工具时取"全部工具都分析完整"，任一工具留有在途或被丢弃的事件，整份报告就
    // 不能声称完整 —— 这里必须是与，不是二选一。
    bool analysisComplete = true;
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        analysisComplete = std::all_of(
            checkers_.begin(), checkers_.end(), [](const auto& checker) { return checker->AnalysisComplete(); });
        analysisComplete = analysisComplete && malformedCallbacks_ == 0 && frameworkErrors_ == 0;
    }
    const bool truncated = report_.Truncated();
    std::ostringstream sessionEnd;
    const bool transportComplete = server_.TransportComplete() && server_.DroppedMessages() == 0;
    sessionEnd << "status="
               << (unsubscribeStatus == ACLSAN_STATUS_SUCCESS && transportComplete && analysisComplete ? "complete" :
                                                                                                         "incomplete")
               << " aclsan_unsubscribe=" << static_cast<int>(unsubscribeStatus)
               << " dropped_messages=" << server_.DroppedMessages()
               << " analysis_complete=" << (analysisComplete ? "true" : "false")
               << " report_truncated=" << (truncated ? "true" : "false");

    // 汇总与完整性信息进报告正文，而不是单独的消息类型：CLI 不解析报告内容，能不能
    // 信任这份结论由 Result 末帧的标志位表达，正文只负责让人读懂发生了什么。
    std::ostringstream trailer;
    trailer << "===== npu_check summary =====\n" << summary << '\n' << sessionEnd.str() << '\n';
    (void)report_.Append(trailer.str());

    const bool hasErrors = HasDetectedErrors();
    if (!reportBundleAvailable || report_.Failed()) {
        // 报告本身没能拼出来，此时宁可什么都不给，也不能把残缺的正文当成结论发出去。
        server_.SendError(
            npucheck::ipc::ErrorDomain::kInternal, npucheck::ipc::error_code::kReportUnavailable,
            "npu_check cannot produce the session report");
    } else {
        const std::string reportText = report_.Take();
        std::string sendError;
        if (!server_.SendResult(reportText, hasErrors, truncated, sendError)) {
            npucheck::WritePlog(npucheck::PlogLevel::kError, "failed to deliver the session report: " + sendError);
        }
    }
    server_.Shutdown();
    npucheck::WritePlog(npucheck::PlogLevel::kInfo, sessionEnd.str());
    checkers_.clear();
    initialized_ = false;
}

bool ToolManager::HasDetectedErrors() const
{
    std::lock_guard<std::mutex> stateLock(stateMutex_);
    return std::any_of(checkers_.begin(), checkers_.end(), [](const auto& checker) { return checker->HasErrors(); });
}

bool ToolManager::IsInitialized() const
{
    std::lock_guard<std::mutex> lock(lifecycleMutex_);
    return initialized_;
}

void ToolManager::Callback(
    void* userdata, AclsanCallbackDomain domain, AclsanCallbackId cbid, const void* cbdata) noexcept
{
    auto* service = static_cast<ToolManager*>(userdata);
    if (service == nullptr) {
        return;
    }
    if (!service->EnterCallback()) {
        return;
    }
    ActiveCallbackGuard callbackGuard(*service);
    try {
        service->OnCallback(domain, cbid, cbdata);
    } catch (const std::exception& exception) {
        service->OnCallbackException(exception.what());
    } catch (...) {
        service->OnCallbackException("unknown exception");
    }
}

bool ToolManager::EnterCallback()
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (stopping_) {
        return false;
    }
    ++activeCallbacks_;
    return true;
}

void ToolManager::LeaveCallback()
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (activeCallbacks_ != 0) {
        --activeCallbacks_;
    }
    if (activeCallbacks_ == 0) {
        callbacksDrained_.notify_all();
    }
}

void ToolManager::OnCallback(AclsanCallbackDomain domain, AclsanCallbackId cbid, const void* cbdata)
{
    LogCallback(domain, cbid, cbdata);
    CheckerReports reports;
    bool malformed = false;
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        for (const auto& checker : checkers_) {
            if (!checker->Accepts(domain, cbid)) {
                continue;
            }
            try {
                if (!checker->OnCallback(domain, cbid, cbdata, reports)) {
                    malformed = true;
                }
            } catch (const std::exception& error) {
                ++frameworkErrors_;
                npucheck::WritePlog(
                    npucheck::PlogLevel::kError, std::string("checker callback failed: ") + error.what());
            } catch (...) {
                ++frameworkErrors_;
                npucheck::WritePlog(npucheck::PlogLevel::kError, "checker callback failed");
            }
        }
        if (malformed)
            ++malformedCallbacks_;
    }
    if (malformed) {
        PublishMalformed(domain, cbid, "null, truncated, or incompatible callback data");
    }
    StoreReports(std::move(reports));
}

void ToolManager::OnCallbackException(const char* reason) noexcept
{
    try {
        {
            std::lock_guard<std::mutex> stateLock(stateMutex_);
            ++frameworkErrors_;
        }
        std::string message = "npu_check callback failed: ";
        message += reason != nullptr ? reason : "unspecified exception";
        npucheck::WritePlog(npucheck::PlogLevel::kError, message);
    } catch (...) {
        // Error reporting is best effort inside a noexcept runtime callback.
        return;
    }
}

void ToolManager::StoreReports(CheckerReports reports)
{
    for (auto& report : reports) {
        std::visit(
            [this](auto& typed) {
                npucheck::PopulateDeviceCallStack(typed);
                NormalizeAndStoreReportRecord(
                    npucheck::NpuCheckReportRecord::From(typed), typed.common.reportId, "checker report");
            },
            report);
    }
}

bool ToolManager::NormalizeAndStoreReportRecord(
    const npucheck::NpuCheckReportRecord& report, uint64_t reportId, const char* what)
{
    npucheck::ReportRecord normalized;
    const auto status = npucheck::detail::NormalizeReport(report, &normalized);
    if (status != npucheck::ReportRenderStatus::kSuccess) {
        std::ostringstream message;
        message << what << " normalization failed report_id=" << reportId << " status=" << static_cast<int>(status);
        {
            std::lock_guard<std::mutex> stateLock(stateMutex_);
            ++frameworkErrors_;
        }
        npucheck::WritePlog(npucheck::PlogLevel::kError, message.str());
        return false;
    }
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        reportRecords_.push_back(std::move(normalized));
    }
    return true;
}

void ToolManager::PublishMalformed(AclsanCallbackDomain domain, AclsanCallbackId cbid, const char* reason)
{
    std::ostringstream output;
    output << "[NPU-CHECK-MALFORMED-CALLBACK] domain=" << static_cast<uint32_t>(domain) << " cbid=" << cbid
           << " reason=" << reason;
    npucheck::WritePlog(npucheck::PlogLevel::kError, output.str());
}

void ToolManager::LogCallback(AclsanCallbackDomain domain, AclsanCallbackId cbid, const void* cbdata)
{
    const uint64_t count = ++callbackCount_;
    std::ostringstream message;
    message << "cbdata received count=" << count << " domain=" << static_cast<uint32_t>(domain)
            << " cbid=" << static_cast<uint32_t>(cbid) << " address=" << cbdata;
    npucheck::WritePlog(npucheck::PlogLevel::kDebug, message.str());
}

std::string ToolManager::BuildReadyMessage() const
{
    std::ostringstream output;
    output << "session=" << server_.SessionId() << " api_version=" << ACLSAN_API_VERSION << " tools=";
    for (size_t index = 0; index < configure_.tools.size(); ++index) {
        output << (index == 0 ? "" : ",") << npucheck::ipc::ToolName(configure_.tools[index].toolId);
    }
    output << " work_dir=" << workDir_;
    return output.str();
}

std::string ToolManager::BuildSummaryMessage() const
{
    std::lock_guard<std::mutex> stateLock(stateMutex_);
    std::ostringstream output;
    for (const auto& checker : checkers_) {
        output << checker->Summary() << '\n';
    }
    output << "callbacks=" << callbackCount_.load() << " malformed_callbacks=" << malformedCallbacks_
           << " framework_errors=" << frameworkErrors_ << " dropped_messages=" << server_.DroppedMessages();
    return output.str();
}

} // namespace npucheck
