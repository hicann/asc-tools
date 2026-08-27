// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "tool_manager/tool_manager.h"

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>

namespace npu::sanitizer {
namespace {

std::string StatusReason(AclsanStatus status) { return "api_status_" + std::to_string(static_cast<uint32_t>(status)); }

bool HasCallStackFrames(AclsanStatus status)
{
    return status == ACLSAN_STATUS_SUCCESS || status == ACLSAN_STATUS_ERROR_MAX_LIMIT_REACHED;
}

std::string FormatCallStackReport(AclsanStatus status, const AclsanDeviceCallStack& callStack)
{
    std::ostringstream output;
    output << "[CALL-STACK] pc=0x" << std::hex << callStack.pc << std::dec;
    if (!HasCallStackFrames(status) || callStack.depth == 0) {
        output << " status=unavailable reason=" << StatusReason(status) << '\n';
        return output.str();
    }
    output << " status=available binary_id=" << callStack.binaryId;
    if ((callStack.flags & ACLSAN_CALL_STACK_FLAG_TRUNCATED) != 0) {
        output << " truncated=true";
    }
    output << '\n';
    for (uint32_t index = 0; index < callStack.depth; ++index) {
        const AclsanDeviceCallStackFrame& frame = callStack.frames[index];
        output << "  #" << index << ' ' << frame.functionName << " at " << frame.fileName << ':' << frame.line << ':'
               << frame.column << '\n';
    }
    return output.str();
}

std::vector<aclsan::cann::ReportFrame> MakeReportFrames(const AclsanDeviceCallStack& callStack)
{
    const uint32_t depth = std::min(callStack.depth, static_cast<uint32_t>(ACLSAN_CALL_STACK_MAX_DEPTH));
    std::vector<aclsan::cann::ReportFrame> frames;
    frames.reserve(depth);
    for (uint32_t index = 0; index < depth; ++index) {
        const AclsanDeviceCallStackFrame& source = callStack.frames[index];
        aclsan::cann::ReportFrame frame;
        frame.pc = callStack.pc;
        frame.function = source.functionName;
        frame.file = source.fileName;
        frame.line = source.line;
        frame.column = source.column;
        frame.inlineDepth = source.inlineDepth;
        frames.push_back(std::move(frame));
    }
    return frames;
}

void PopulateDeviceCallStack(aclsan::cann::NpusanMemcheckReport& report) noexcept
{
    if (report.common.exec.pc == 0 || report.common.stackCount > aclsan::cann::kNpusanReportStackMax) {
        return;
    }

    std::uint32_t stackIndex = report.common.stackCount;
    for (std::uint32_t index = 0; index < report.common.stackCount && index < aclsan::cann::kNpusanReportStackMax;
         ++index) {
        if (report.common.stacks[index].role == aclsan::cann::ReportStackRole::kFaultDevice) {
            stackIndex = index;
            break;
        }
    }

    if (stackIndex == report.common.stackCount && report.common.stackCount >= aclsan::cann::kNpusanReportStackMax) {
        return;
    }

    try {
        auto callStack = std::make_unique<AclsanDeviceCallStack>();
        const AclsanStatus status = aclsanGetDeviceCallStack(report.common.exec.pc, callStack.get());
        std::string callStackText = FormatCallStackReport(status, *callStack);
        std::vector<aclsan::cann::ReportFrame> frames;
        if (HasCallStackFrames(status)) {
            frames = MakeReportFrames(*callStack);
        }

        auto& stack = report.common.stacks[stackIndex];
        stack.rawText.swap(callStackText);
        stack.role = aclsan::cann::ReportStackRole::kFaultDevice;
        if (!frames.empty()) {
            stack.frames.swap(frames);
        }
        const bool hasStructuredFrames = !stack.frames.empty();
        stack.format =
            hasStructuredFrames ? aclsan::cann::ReportStackFormat::kBoth : aclsan::cann::ReportStackFormat::kRawText;
        if (callStack->binaryId != 0) {
            report.common.exec.binaryId = callStack->binaryId;
        }
        if (stackIndex == report.common.stackCount) {
            ++report.common.stackCount;
        }
    } catch (...) {
        return;
    }
}

void PopulateSyncPointCallStack(
    aclsan::cann::NpusanReportCommon& common, aclsan::cann::NpusanSyncPoint& point,
    aclsan::cann::ReportStackRole role) noexcept
{
    if (!point.hasExecContext || point.exec.pc == 0 || common.stackCount > aclsan::cann::kNpusanReportStackMax) {
        return;
    }

    std::uint32_t stackIndex = common.stackCount;
    for (std::uint32_t index = 0; index < common.stackCount && index < aclsan::cann::kNpusanReportStackMax; ++index) {
        if (common.stacks[index].role == role) {
            stackIndex = index;
            break;
        }
    }
    if (stackIndex == common.stackCount && common.stackCount >= aclsan::cann::kNpusanReportStackMax) {
        return;
    }

    try {
        auto callStack = std::make_unique<AclsanDeviceCallStack>();
        const AclsanStatus status = aclsanGetDeviceCallStack(point.exec.pc, callStack.get());
        std::string callStackText = FormatCallStackReport(status, *callStack);
        std::vector<aclsan::cann::ReportFrame> frames;
        if (HasCallStackFrames(status)) {
            frames = MakeReportFrames(*callStack);
        }

        auto& stack = common.stacks[stackIndex];
        stack.rawText.swap(callStackText);
        stack.role = role;
        stack.frames.swap(frames);
        stack.format =
            stack.frames.empty() ? aclsan::cann::ReportStackFormat::kRawText : aclsan::cann::ReportStackFormat::kBoth;
        if (callStack->binaryId != 0) {
            point.exec.binaryId = callStack->binaryId;
        }
        point.stackRole = role;
        if (stackIndex == common.stackCount) {
            ++common.stackCount;
        }
    } catch (...) {
        return;
    }
}

void PopulateDeviceCallStack(aclsan::cann::NpusanSynccheckReport& report) noexcept
{
    PopulateSyncPointCallStack(report.common, report.triggerPoint, aclsan::cann::ReportStackRole::kSyncTrigger);
    report.common.exec = report.triggerPoint.exec;
    PopulateSyncPointCallStack(report.common, report.relatedPoint, aclsan::cann::ReportStackRole::kSyncRelated);
}

struct CallbackSpec {
    AclsanCallbackDomain domain;
    AclsanCallbackId cbid;
};

constexpr std::array<CallbackSpec, 4> kMemcheckCallbacks{{
    {ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC},
    {ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_FREE},
    {ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_MEMORY_ACCESS},
    {ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END},
}};

constexpr std::array<CallbackSpec, 2> kSynccheckCallbacks{{
    {ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_SYNC},
    {ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END},
}};

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
    if (!server_.StartAndHandshake(config_, error)) {
        LogHandshakeFailure(error);
        server_.Shutdown({}, {});
        return 1;
    }
    if (!InitializeLogger(error)) {
        server_.SendInitializationError(error);
        server_.Shutdown({}, "status=initialization_failed");
        return 1;
    }
    logger_.Info("UDS handshake completed");
    std::ostringstream configMessage;
    configMessage << "tool configuration tool=" << config_.toolName << " strict=" << (config_.strict ? "true" : "false")
                  << " keep_temp=" << (config_.keepTemp ? "true" : "false") << " work_dir=" << config_.workDir
                  << " probe_cache_dir=" << config_.probeCacheDir << " report_file=" << config_.logFile;
    logger_.Info(configMessage.str());
    if (!ConfigureSanitizer(error)) {
        logger_.Error(error);
        server_.SendInitializationError(error);
        RollbackSanitizer();
        server_.Shutdown({}, "status=initialization_failed");
        return 1;
    }
    if (!server_.SendReady(BuildReadyMessage(), error)) {
        logger_.Error(error);
        RollbackSanitizer();
        server_.Shutdown({}, "status=transport_failed");
        return 1;
    }
    logger_.SetErrorSink([this](const std::string& message) { server_.SendFlowError(message); });
    initialized_ = true;
    logger_.Info("npu_check initialization completed");
    return 0;
}

void ToolManager::LogHandshakeFailure(const std::string& reason) noexcept
{
    try {
        std::string ignored;
        const std::filesystem::path path = std::filesystem::current_path() / "npu_check.log";
        if (logger_.Path().empty() && !logger_.Open(path.string(), logging::Logger::ConfiguredLevel(), ignored)) {
            return;
        }
        logger_.Error("UDS handshake failed: " + reason);
        logger_.Flush();
    } catch (...) {
        return;
    }
}

bool ToolManager::InitializeLogger(std::string& error)
{
    const std::filesystem::path directory =
        config_.workDir.empty() ? std::filesystem::current_path() : std::filesystem::path(config_.workDir);
    const std::string path = (directory / "npu_check.log").string();
    if (!logger_.Open(path, logging::Logger::ConfiguredLevel(), error)) {
        return false;
    }
    return true;
}

bool ToolManager::ConfigureSanitizer(std::string& error)
{
    if (config_.toolName != "memcheck" && config_.toolName != "synccheck") {
        error = "unsupported tool '" + config_.toolName + "'; current implementation supports memcheck and synccheck";
        return false;
    }
    if (config_.toolName == "memcheck") {
        memcheck_ = std::make_unique<Memcheck>(config_.strict);
        logger_.Debug("memcheck instance created");
    } else {
        synccheck_ = std::make_unique<Synccheck>(&logger_);
        logger_.Debug("synccheck instance created");
    }
    AclsanStatus status = aclsanSubscribe(&subscriber_, &ToolManager::Callback, this);
    if (status != ACLSAN_STATUS_SUCCESS) {
        error = StatusMessage("aclsanSubscribe", status);
        return false;
    }
    subscribed_ = true;
    logger_.Info("sanitizer callback subscriber registered");
    return EnableCallbacks(error);
}

bool ToolManager::EnableCallbacks(std::string& error)
{
    const auto enable = [this, &error](const auto& callbacks) {
        for (const auto& callback : callbacks) {
            const AclsanStatus status = aclsanEnableCallback(1, subscriber_, callback.domain, callback.cbid);
            if (status != ACLSAN_STATUS_SUCCESS) {
                error = StatusMessage("aclsanEnableCallback", status);
                return false;
            }
            std::ostringstream message;
            message << "callback enabled domain=" << static_cast<uint32_t>(callback.domain)
                    << " cbid=" << static_cast<uint32_t>(callback.cbid);
            logger_.Debug(message.str());
        }
        return true;
    };
    return config_.toolName == "memcheck" ? enable(kMemcheckCallbacks) : enable(kSynccheckCallbacks);
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
    memcheck_.reset();
    synccheck_.reset();
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

    const std::string summary = BuildSummaryMessage();
    logger_.Info(summary);
    bool analysisComplete = false;
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        if (memcheck_ != nullptr) {
            const MemcheckStats stats = memcheck_->Stats();
            analysisComplete = stats.pendingDeviceOperations == 0 && stats.droppedDeviceOperations == 0;
        } else if (synccheck_ != nullptr) {
            const SynccheckStats stats = synccheck_->Stats();
            analysisComplete = stats.pendingOpens == 0 && stats.invalidEvents == 0;
        }
        analysisComplete = analysisComplete && malformedCallbacks_ == 0 && frameworkErrors_ == 0;
    }
    std::ostringstream sessionEnd;
    const bool transportComplete = server_.TransportComplete() && server_.DroppedMessages() == 0;
    sessionEnd << "status="
               << (unsubscribeStatus == ACLSAN_STATUS_SUCCESS && transportComplete && analysisComplete ? "complete" :
                                                                                                         "incomplete")
               << " aclsan_unsubscribe=" << static_cast<int>(unsubscribeStatus)
               << " dropped_messages=" << server_.DroppedMessages()
               << " analysis_complete=" << (analysisComplete ? "true" : "false");
    server_.Shutdown(summary, sessionEnd.str());
    logger_.Info(sessionEnd.str());
    logger_.Flush();
    memcheck_.reset();
    synccheck_.reset();
    initialized_ = false;
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
    std::vector<aclsan::cann::NpusanMemcheckReport> reports;
    std::vector<aclsan::cann::NpusanSynccheckReport> syncReports;
    bool malformed = false;
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        switch (domain) {
            case ACLSAN_CB_DOMAIN_RESOURCE: {
                const auto* data = ValidateCallbackData<AclsanResourceData>(cbdata);
                malformed = data == nullptr;
                if (data != nullptr && memcheck_ != nullptr && cbid == ACLSAN_CBID_RESOURCE_MEMORY_ALLOC) {
                    memcheck_->OnAllocation(*data);
                    std::ostringstream message;
                    message << "memory alloc resource=" << data->resourceId << " device=" << data->deviceId
                            << " address=" << data->ptr << " bytes=" << data->bytes
                            << " result=" << data->common.result;
                    logger_.Info(message.str());
                } else if (data != nullptr && memcheck_ != nullptr && cbid == ACLSAN_CBID_RESOURCE_MEMORY_FREE) {
                    memcheck_->OnFree(*data);
                    std::ostringstream message;
                    message << "memory free resource=" << data->resourceId << " device=" << data->deviceId
                            << " address=" << data->ptr << " result=" << data->common.result;
                    logger_.Info(message.str());
                }
                break;
            }
            case ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION: {
                if (cbid == ACLSAN_CBID_DEVICE_MEMORY_ACCESS) {
                    const auto* data = ValidateDeviceMemoryAccessData(cbdata);
                    if (data != nullptr && memcheck_ != nullptr) {
                        memcheck_->QueueDeviceMemoryAccess(*data);
                        std::ostringstream message;
                        message << "device memory access launch=" << data->header.launchId << " pc=0x" << std::hex
                                << data->header.pc << std::dec << " device=" << data->header.deviceId
                                << " core=" << data->header.phyCoreId << " address=0x" << std::hex << data->address
                                << std::dec << " access_mode=" << data->accessMode << " layout=" << data->layoutKind;
                        logger_.Debug(message.str());
                    } else {
                        malformed = true;
                    }
                } else if (cbid == ACLSAN_CBID_DEVICE_SYNC) {
                    const auto* data = static_cast<const AclsanDeviceSyncData*>(cbdata);
                    if (data != nullptr && synccheck_ != nullptr) {
                        syncReports = synccheck_->OnDeviceSync(*data);
                    } else {
                        malformed = true;
                    }
                }
                break;
            }
            case ACLSAN_CB_DOMAIN_SYNCHRONIZE: {
                const auto* data = ValidateCallbackData<AclsanSynchronizeData>(cbdata);
                malformed = data == nullptr;
                if (data != nullptr && memcheck_ != nullptr && data->common.result == 0) {
                    reports = memcheck_->OnSynchronization();
                    std::ostringstream message;
                    message << "synchronization completed reports=" << reports.size() << " stream=" << data->stream;
                    logger_.Info(message.str());
                }
                if (data != nullptr && synccheck_ != nullptr) {
                    syncReports = synccheck_->OnSynchronization();
                    std::ostringstream message;
                    message << "synchronization observed reports=" << syncReports.size() << " stream=" << data->stream
                            << " result=" << data->common.result;
                    logger_.Info(message.str());
                }
                if (data != nullptr && data->common.result != 0) {
                    std::ostringstream message;
                    message << "synchronization failed result=" << data->common.result << " stream=" << data->stream;
                    logger_.Warning(message.str());
                }
                break;
            }
            default:
                break;
        }
    }
    if (malformed) {
        {
            std::lock_guard<std::mutex> stateLock(stateMutex_);
            ++malformedCallbacks_;
        }
        PublishMalformed(domain, cbid, "null, truncated, or incompatible callback data");
    } else {
        PublishDiagnostics(std::move(reports));
        PublishSynccheckReports(std::move(syncReports));
    }
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
        logger_.Error(message);
    } catch (...) {
        // Error reporting is best effort inside a noexcept runtime callback.
        return;
    }
}

void ToolManager::PublishDiagnostics(std::vector<aclsan::cann::NpusanMemcheckReport> reports)
{
    for (auto& report : reports) {
        PopulateDeviceCallStack(report);
        std::string rendered;
        const auto status =
            aclsan::cann::RenderNpusanReportRecord(aclsan::cann::NpusanReportRecord::From(report), {}, &rendered);
        if (status != aclsan::cann::ReportRenderStatus::kSuccess) {
            std::ostringstream message;
            message << "report rendering failed report_id=" << report.common.reportId
                    << " status=" << static_cast<int>(status);
            {
                std::lock_guard<std::mutex> stateLock(stateMutex_);
                ++frameworkErrors_;
            }
            logger_.Error(message.str());
            continue;
        }
        logger_.Info("diagnostic report generated report_id=" + std::to_string(report.common.reportId));
        if (!server_.Publish(ipc::MessageType::DIAGNOSTIC, rendered)) {
            logger_.Error("failed to queue diagnostic report for UDS delivery");
        }
    }
}

void ToolManager::PublishSynccheckReports(std::vector<aclsan::cann::NpusanSynccheckReport> reports)
{
    for (auto& report : reports) {
        PopulateDeviceCallStack(report);
        std::string rendered;
        const auto status =
            aclsan::cann::RenderNpusanReportRecord(aclsan::cann::NpusanReportRecord::From(report), {}, &rendered);
        if (status != aclsan::cann::ReportRenderStatus::kSuccess) {
            std::ostringstream message;
            message << "synccheck report rendering failed report_id=" << report.common.reportId
                    << " status=" << static_cast<int>(status);
            {
                std::lock_guard<std::mutex> stateLock(stateMutex_);
                ++frameworkErrors_;
            }
            logger_.Error(message.str());
            continue;
        }
        logger_.Info("synccheck diagnostic report generated report_id=" + std::to_string(report.common.reportId));
        if (!server_.Publish(ipc::MessageType::DIAGNOSTIC, rendered)) {
            logger_.Error("failed to queue synccheck diagnostic report for UDS delivery");
        }
    }
}

void ToolManager::PublishMalformed(AclsanCallbackDomain domain, AclsanCallbackId cbid, const char* reason)
{
    std::ostringstream output;
    output << "[NPU-CHECK-MALFORMED-CALLBACK] domain=" << static_cast<uint32_t>(domain) << " cbid=" << cbid
           << " reason=" << reason;
    logger_.Error(output.str());
}

void ToolManager::LogCallback(AclsanCallbackDomain domain, AclsanCallbackId cbid, const void* cbdata)
{
    const uint64_t count = ++callbackCount_;
    std::ostringstream message;
    message << "cbdata received count=" << count << " domain=" << static_cast<uint32_t>(domain)
            << " cbid=" << static_cast<uint32_t>(cbid) << " address=" << cbdata;
    logger_.Debug(message.str());
}

std::string ToolManager::BuildReadyMessage() const
{
    std::ostringstream output;
    const size_t callbackCount =
        config_.toolName == "memcheck" ? kMemcheckCallbacks.size() : kSynccheckCallbacks.size();
    output << "tool=" << config_.toolName << " session=" << server_.SessionId() << " api_version=" << ACLSAN_API_VERSION
           << " callbacks=" << callbackCount << " compile_options=" << config_.compileOptions.size();
    return output.str();
}

std::string ToolManager::BuildSummaryMessage() const
{
    std::lock_guard<std::mutex> stateLock(stateMutex_);
    std::ostringstream output;
    output << "tool=" << config_.toolName;
    if (memcheck_ != nullptr) {
        const MemcheckStats stats = memcheck_->Stats();
        output << " allocations=" << stats.allocations << " frees=" << stats.frees
               << " device_operations=" << stats.deviceOperations << " synchronizations=" << stats.synchronizationEvents
               << " errors=" << stats.errors << " warnings=" << stats.warnings
               << " pending_device_operations=" << stats.pendingDeviceOperations
               << " dropped_device_operations=" << stats.droppedDeviceOperations;
    } else {
        const SynccheckStats stats = synccheck_ != nullptr ? synccheck_->Stats() : SynccheckStats{};
        const uint64_t errors = stats.duplicateOpens + stats.unmatchedCloses + stats.unconsumedOpens;
        output << " sync_events=" << stats.syncEvents << " synchronizations=" << stats.synchronizationEvents
               << " matched_pairs=" << stats.matchedPairs << " duplicate_opens=" << stats.duplicateOpens
               << " unmatched_closes=" << stats.unmatchedCloses << " unconsumed_opens=" << stats.unconsumedOpens
               << " invalid_events=" << stats.invalidEvents << " pending_opens=" << stats.pendingOpens
               << " errors=" << errors << " warnings=0";
    }
    output << " callbacks=" << callbackCount_.load() << " malformed_callbacks=" << malformedCallbacks_
           << " framework_errors=" << frameworkErrors_ << " dropped_messages=" << server_.DroppedMessages();
    return output.str();
}

} // namespace npu::sanitizer
