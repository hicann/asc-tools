// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "tool_manager/tool_manager.h"

#include "diagnostic/diagnostic.h"

#include <array>
#include <cstring>
#include <exception>
#include <iostream>
#include <sstream>

namespace npu::sanitizer {
namespace {

template <size_t Capacity>
bool CopyString(char (&destination)[Capacity], const std::string& source)
{
    if (source.size() >= Capacity) {
        return false;
    }
    std::memset(destination, 0, Capacity);
    std::memcpy(destination, source.data(), source.size());
    return true;
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
        std::cerr << "npu_check: UDS handshake failed: " << error << '\n';
        server_.Shutdown({}, {});
        return 1;
    }
    if (!ConfigureSanitizer(error)) {
        server_.SendInitializationError(error);
        RollbackSanitizer();
        server_.Shutdown({}, "status=initialization_failed");
        return 1;
    }
    if (!server_.SendReady(BuildReadyMessage(), error)) {
        RollbackSanitizer();
        server_.Shutdown({}, "status=transport_failed");
        return 1;
    }
    initialized_ = true;
    return 0;
}

bool ToolManager::BuildLaunchConfig(std::string& error)
{
    launchConfig_ = {};
    launchConfig_.version = ACLSAN_API_VERSION;
    launchConfig_.size = sizeof(launchConfig_);
    launchConfig_.sessionId = server_.SessionId();
    launchConfig_.strict = config_.strict ? 1u : 0u;
    launchConfig_.keepTemp = config_.keepTemp ? 1u : 0u;
    if (!CopyString(launchConfig_.toolName, config_.toolName) || !CopyString(launchConfig_.logFile, config_.logFile) ||
        !CopyString(launchConfig_.workDir, config_.workDir) ||
        !CopyString(launchConfig_.probeCacheDir, config_.probeCacheDir)) {
        error = "tool configuration path exceeds sanitizer_api limits";
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
    if (!BuildLaunchConfig(error)) {
        return false;
    }

    memcheck_ = std::make_unique<Memcheck>(launchConfig_.strict != 0);
    AclsanStatus status = aclsanSubscribe(&subscriber_, &ToolManager::Callback, this);
    if (status != ACLSAN_STATUS_SUCCESS) {
        error = StatusMessage("aclsanSubscribe", status);
        return false;
    }
    subscribed_ = true;
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
        subscriber_ = ACLSAN_INVALID_SUBSCRIBER_HANDLE;
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
        subscriber_ = ACLSAN_INVALID_SUBSCRIBER_HANDLE;
    }
    {
        std::unique_lock<std::mutex> callbackLock(callbackMutex_);
        callbacksDrained_.wait(callbackLock, [this] { return activeCallbacks_ == 0; });
    }

    const std::string summary = BuildSummaryMessage();
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
    std::vector<Diagnostic> diagnostics;
    bool malformed = false;
    {
        std::lock_guard<std::mutex> stateLock(stateMutex_);
        switch (domain) {
            case ACLSAN_CB_DOMAIN_RESOURCE: {
                const auto* data = ValidateCallbackData<AclsanResourceData>(cbdata);
                malformed = data == nullptr;
                if (data != nullptr && cbid == ACLSAN_CBID_RESOURCE_MEMORY_ALLOC) {
                    memcheck_->OnAllocation(*data);
                } else if (data != nullptr && cbid == ACLSAN_CBID_RESOURCE_MEMORY_FREE) {
                    memcheck_->OnFree(*data);
                }
                break;
            }
            case ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION: {
                if (cbid == ACLSAN_CBID_DEVICE_MEMORY_ACCESS) {
                    const auto* data = ValidateDeviceMemoryAccessData(cbdata);
                    if (data != nullptr) {
                        memcheck_->QueueDeviceMemoryAccess(*data);
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
                    diagnostics = memcheck_->OnSynchronization();
                }
                break;
            }
            default:
                break;
        }
    }
    if (malformed) {
        ++malformedCallbacks_;
        PublishMalformed(domain, cbid, "null, truncated, or incompatible callback data");
    } else {
        PublishDiagnostics(std::move(diagnostics));
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
        (void)server_.Publish(ipc::MessageType::ERROR, message);
    } catch (...) {
        // Error reporting is best effort inside a noexcept runtime callback.
        return;
    }
}

void ToolManager::PublishDiagnostics(std::vector<Diagnostic> diagnostics)
{
    for (auto& diagnostic : diagnostics) {
        if (diagnostic.instruction.present) {
            diagnostic.source = sourceResolver_.Resolve(diagnostic.instruction);
        }
        const uint64_t ordinal = ++diagnosticOrdinal_;
        (void)server_.Publish(ipc::MessageType::DIAGNOSTIC, FormatDiagnostic(diagnostic, ordinal));
    }
}

void ToolManager::PublishMalformed(AclsanCallbackDomain domain, AclsanCallbackId cbid, const char* reason)
{
    std::ostringstream output;
    output << "[NPU-CHECK-MALFORMED-CALLBACK] domain=" << static_cast<uint32_t>(domain) << " cbid=" << cbid
           << " reason=" << reason;
    (void)server_.Publish(ipc::MessageType::ERROR, output.str());
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
           << " dropped_device_operations=" << stats.droppedDeviceOperations
           << " malformed_callbacks=" << malformedCallbacks_ << " framework_errors=" << frameworkErrors_
           << " dropped_messages=" << server_.DroppedMessages();
    return output.str();
}

} // namespace npu::sanitizer
