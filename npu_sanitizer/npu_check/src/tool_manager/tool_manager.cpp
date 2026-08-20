// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "tool_manager/tool_manager.h"

#include <array>
#include <cstring>
#include <exception>
#include <filesystem>
#include <sstream>

namespace npu::sanitizer {
namespace {

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
    if (config_.toolName != "memcheck") {
        error = "unsupported tool '" + config_.toolName + "'; current implementation supports memcheck";
        return false;
    }
    memcheck_ = std::make_unique<Memcheck>(config_.strict);
    logger_.Debug("memcheck instance created");
    AclsanStatus status = aclsanSubscribe(&subscriber_, &ToolManager::Callback, this);
    if (status != ACLSAN_STATUS_SUCCESS) {
        error = StatusMessage("aclsanSubscribe", status);
        return false;
    }
    subscribed_ = true;
    logger_.Info("sanitizer callback subscriber registered");
    return EnableMemcheckCallbacks(error);
}

bool ToolManager::EnableMemcheckCallbacks(std::string& error)
{
    for (const auto& callback : kMemcheckCallbacks) {
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
        const MemcheckStats stats = memcheck_ != nullptr ? memcheck_->Stats() : MemcheckStats{};
        analysisComplete = stats.pendingDeviceOperations == 0 && stats.droppedDeviceOperations == 0 &&
                           malformedCallbacks_ == 0 && frameworkErrors_ == 0;
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
    bool malformed = false;
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        switch (domain) {
            case ACLSAN_CB_DOMAIN_RESOURCE: {
                const auto* data = ValidateCallbackData<AclsanResourceData>(cbdata);
                malformed = data == nullptr;
                if (data != nullptr && cbid == ACLSAN_CBID_RESOURCE_MEMORY_ALLOC) {
                    memcheck_->OnAllocation(*data);
                    std::ostringstream message;
                    message << "memory alloc resource=" << data->resourceId << " device=" << data->deviceId
                            << " address=" << data->ptr << " bytes=" << data->bytes
                            << " result=" << data->common.result;
                    logger_.Info(message.str());
                } else if (data != nullptr && cbid == ACLSAN_CBID_RESOURCE_MEMORY_FREE) {
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
                    if (data != nullptr) {
                        memcheck_->QueueDeviceMemoryAccess(*data);
                        std::ostringstream message;
                        message << "device memory access launch=" << data->header.launchId << " pc=0x" << std::hex
                                << data->header.pc << std::dec << " device=" << data->header.deviceId
                                << " core=" << data->header.coreId << " address=0x" << std::hex << data->address
                                << std::dec << " access_mode=" << data->accessMode << " layout=" << data->layoutKind;
                        logger_.Debug(message.str());
                    } else {
                        malformed = true;
                    }
                }
                break;
            }
            case ACLSAN_CB_DOMAIN_SYNCHRONIZE: {
                const auto* data = ValidateCallbackData<AclsanSynchronizeData>(cbdata);
                malformed = data == nullptr;
                if (data != nullptr && data->common.result == 0) {
                    reports = memcheck_->OnSynchronization();
                    std::ostringstream message;
                    message << "synchronization completed reports=" << reports.size() << " stream=" << data->stream;
                    logger_.Info(message.str());
                } else if (data != nullptr) {
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
        // Source locations belong to sanitizer_api. Once its public PC query contract is
        // available, populate report.common.exec and the device call stack here before rendering.
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
    output << "tool=memcheck session=" << server_.SessionId() << " api_version=" << ACLSAN_API_VERSION
           << " callbacks=" << kMemcheckCallbacks.size() << " compile_options=" << config_.compileOptions.size();
    return output.str();
}

std::string ToolManager::BuildSummaryMessage() const
{
    std::lock_guard<std::mutex> stateLock(stateMutex_);
    const MemcheckStats stats = memcheck_ != nullptr ? memcheck_->Stats() : MemcheckStats{};
    std::ostringstream output;
    output << "tool=memcheck"
           << " allocations=" << stats.allocations << " frees=" << stats.frees
           << " device_operations=" << stats.deviceOperations << " synchronizations=" << stats.synchronizationEvents
           << " errors=" << stats.errors << " warnings=" << stats.warnings
           << " pending_device_operations=" << stats.pendingDeviceOperations
           << " dropped_device_operations=" << stats.droppedDeviceOperations << " callbacks=" << callbackCount_.load()
           << " malformed_callbacks=" << malformedCallbacks_ << " framework_errors=" << frameworkErrors_
           << " dropped_messages=" << server_.DroppedMessages();
    return output.str();
}

} // namespace npu::sanitizer
