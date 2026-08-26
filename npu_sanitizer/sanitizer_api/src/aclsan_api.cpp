/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aclsan/aclsan_api.h"
#include "internal/aclsan_active_probe_plan.h"
#include "internal/aclsan_internal.h"
#include "internal/aclsan_log.h"
#include "npu_compute/injection_hook.h"
#include "device_runtime/device_symbolizer.h"

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <map>
#include <mutex>
#include <new>
#include <set>
#include <shared_mutex>
#include <string>
#include <tuple>
#include <vector>

namespace {

template <size_t BufferBytes>
bool CopyText(const std::string& text, char (&buffer)[BufferBytes])
{
    const size_t copyBytes = std::min(text.size(), BufferBytes - 1);
    std::memcpy(buffer, text.data(), copyBytes);
    buffer[copyBytes] = '\0';
    return copyBytes == text.size();
}

AclsanStatus UnavailableStatus(const std::string& error)
{
    if (error == "invalid_state" || error == "invalid_configuration") {
        return ACLSAN_STATUS_ERROR_INVALID_STATE;
    }
    if (error == "invalid_symbolizer_output") {
        return ACLSAN_STATUS_ERROR_INTERNAL;
    }
    return ACLSAN_STATUS_ERROR_RUNTIME;
}

struct CallbackKey {
    AclsanCallbackDomain domain;
    AclsanCallbackId id;

    bool operator<(const CallbackKey& other) const noexcept
    {
        return std::tie(domain, id) < std::tie(other.domain, other.id);
    }
};

// 记录每组 domain + callback id 需要开启 Hook 的 Runtime API。
const std::map<CallbackKey, std::vector<aclrtApiId>> kCallbackRoutes = {
    {{ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC}, {ACL_RT_API_aclrtMalloc}},
    {{ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_FREE}, {ACL_RT_API_aclrtFree}},
    {{ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_MEMORY_ACCESS},
     {ACL_RT_API_aclrtBinaryLoadFromData, ACL_RT_API_aclrtBinaryGetFunction, ACL_RT_API_aclrtBinaryGetFunctionByEntry,
      ACL_RT_API_aclrtLaunchKernelWithHostArgs, ACL_RT_API_aclrtSynchronizeStream,
      ACL_RT_API_aclrtSynchronizeStreamWithTimeout, ACL_RT_API_aclrtGetFuncBySymbol, ACL_RT_API_aclrtBinaryUnLoad,
      ACL_RT_API_aclrtResetDevice, ACL_RT_API_aclrtMalloc}},
    {{ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_SYNC},
     {ACL_RT_API_aclrtBinaryLoadFromData, ACL_RT_API_aclrtBinaryGetFunction, ACL_RT_API_aclrtBinaryGetFunctionByEntry,
      ACL_RT_API_aclrtLaunchKernelWithHostArgs, ACL_RT_API_aclrtSynchronizeStream,
      ACL_RT_API_aclrtSynchronizeStreamWithTimeout, ACL_RT_API_aclrtGetFuncBySymbol, ACL_RT_API_aclrtBinaryUnLoad,
      ACL_RT_API_aclrtResetDevice}},
    {{ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END},
     {ACL_RT_API_aclrtSynchronizeStream, ACL_RT_API_aclrtSynchronizeStreamWithTimeout}},
};

} // namespace

struct AclsanSubscriber final {
public:
    static AclsanSubscriber& Instance() noexcept;

    AclsanStatus Subscribe(AclsanSubscriberHandle* subscriber, AclsanCallbackFunc callback, void* userdata);
    AclsanStatus EnableCallback(
        uint32_t enable, AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, AclsanCallbackId id);
    AclsanStatus Unsubscribe(AclsanSubscriberHandle subscriber) noexcept;
    AclsanStatus EnableDomain(AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, uint32_t enable);
    AclsanStatus GetCallbackState(
        AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, uint32_t cbid,
        uint32_t* enabled) const noexcept;
    bool IsCallbackEnabled(AclsanCallbackDomain domain, AclsanCallbackId id) const noexcept;
    bool InvokeCallback(AclsanCallbackDomain domain, AclsanCallbackId id, const void* callbackData) noexcept;

private:
    AclsanSubscriber() = default;
    ~AclsanSubscriber() = default;
    AclsanSubscriber(const AclsanSubscriber&) = delete;
    AclsanSubscriber& operator=(const AclsanSubscriber&) = delete;
    AclsanSubscriber(AclsanSubscriber&&) = delete;
    AclsanSubscriber& operator=(AclsanSubscriber&&) = delete;

    static bool IsValidCallbackId(AclsanCallbackDomain domain, AclsanCallbackId callbackId) noexcept;
    static bool IsValidCallbackDomain(AclsanCallbackDomain domain) noexcept;
    static bool HasCallbackRoute(const CallbackKey& key) noexcept;
    static std::set<aclrtApiId> ComputeRequiredHooks(const std::set<CallbackKey>& enabledCallbacks);
    static uint32_t ComputeRequiredProbeGroups(const std::set<CallbackKey>& enabledCallbacks) noexcept;

    bool IsActive(AclsanSubscriberHandle subscriber) const noexcept;
    void LogConfigurationState(const char* operation, const char* stage) const noexcept;
    void Reset(const char* operation) noexcept;
    void ClearAfterException(AclsanStatus failure, const char* operation) noexcept;
    AclsanStatus ApplyCallbackConfiguration(std::set<CallbackKey>& candidateCallbacks, const char* operation);
    AclsanStatus UpdateCallbackConfiguration(const CallbackKey& key, bool shouldEnable);

    AclsanSubscriberHandle activeHandle_ = nullptr;
    AclsanCallbackFunc callback_ = nullptr;
    void* userdata_ = nullptr;
    std::set<CallbackKey> enabledCallbacks_;
    std::set<aclrtApiId> requiredHooks_;
    mutable std::recursive_mutex mutex_;
    std::condition_variable_any callbacksDrained_;
    size_t activeCallbackInvocations_ = 0;
    bool acceptingCallbacks_ = false;
    bool unsubscribeInProgress_ = false;
};

// 获取进程内唯一的AclsanSubscriber对象
AclsanSubscriber& AclsanSubscriber::Instance() noexcept
{
    static AclsanSubscriber subscriber;
    return subscriber;
}

bool AclsanSubscriber::IsValidCallbackId(AclsanCallbackDomain domain, AclsanCallbackId callbackId) noexcept
{
    switch (domain) {
        case ACLSAN_CB_DOMAIN_RESOURCE:
            return callbackId >= ACLSAN_CBID_RESOURCE_MEMORY_ALLOC && callbackId <= ACLSAN_CBID_RESOURCE_MEMORY_FREE;
        case ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION:
            return callbackId >= ACLSAN_CBID_DEVICE_MEMORY_ACCESS && callbackId <= ACLSAN_CBID_DEVICE_SYNC;
        case ACLSAN_CB_DOMAIN_SYNCHRONIZE:
            return callbackId == ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END;
        default:
            return false;
    }
}

bool AclsanSubscriber::IsValidCallbackDomain(AclsanCallbackDomain domain) noexcept
{
    return domain == ACLSAN_CB_DOMAIN_RESOURCE || domain == ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION ||
           domain == ACLSAN_CB_DOMAIN_SYNCHRONIZE;
}

// 根据(callback domain, callback id)查询该 callback 是否已有 Runtime Hook route。
bool AclsanSubscriber::HasCallbackRoute(const CallbackKey& key) noexcept
{
    return kCallbackRoutes.find(key) != kCallbackRoutes.end();
}

// 根据现在订阅的domain + id，确认哪些aclrt函数需要被hook
std::set<aclrtApiId> AclsanSubscriber::ComputeRequiredHooks(const std::set<CallbackKey>& enabledCallbacks)
{
    std::set<aclrtApiId> requiredHooks;
    for (const CallbackKey& key : enabledCallbacks) {
        const auto route = kCallbackRoutes.find(key);
        if (route == kCallbackRoutes.end()) {
            continue;
        }
        for (aclrtApiId apiId : route->second) {
            requiredHooks.insert(apiId);
        }
    }
    return requiredHooks;
}

uint32_t AclsanSubscriber::ComputeRequiredProbeGroups(const std::set<CallbackKey>& enabledCallbacks) noexcept
{
    uint32_t probeGroupMask = 0;
    for (const CallbackKey& key : enabledCallbacks) {
        probeGroupMask |= aclsan::ProbeGroupMaskForCallback(key.domain, key.id);
    }
    return probeGroupMask;
}

bool AclsanSubscriber::IsActive(AclsanSubscriberHandle subscriber) const noexcept
{
    return activeHandle_ != nullptr && subscriber == activeHandle_;
}

void AclsanSubscriber::LogConfigurationState(const char* operation, const char* stage) const noexcept
{
    ASC_SAN_DEBUG(
        "%s: callback state stage=%s enabledCallbacks_=%zu requiredHooks_=%zu", operation, stage,
        enabledCallbacks_.size(), requiredHooks_.size());
    for (const CallbackKey& key : enabledCallbacks_) {
        ASC_SAN_DEBUG(
            "%s: enabledCallbacks_ domain=%u id=%u", operation, static_cast<uint32_t>(key.domain),
            static_cast<uint32_t>(key.id));
    }
    for (aclrtApiId apiId : requiredHooks_) {
        ASC_SAN_DEBUG("%s: requiredHooks_ apiId=%u", operation, static_cast<uint32_t>(apiId));
    }
}

void AclsanSubscriber::Reset(const char* operation) noexcept
{
    LogConfigurationState(operation, "before-reset");
    activeHandle_ = nullptr;
    callback_ = nullptr;
    userdata_ = nullptr;
    enabledCallbacks_.clear();
    requiredHooks_.clear();
    acceptingCallbacks_ = false;
    aclsan::CommitActiveProbePlan(0);
    LogConfigurationState(operation, "after-reset");
}

// 异常情况，还原所有aclrt接口变为原始的aclrt功能
void AclsanSubscriber::ClearAfterException(AclsanStatus failure, const char* operation) noexcept
{
    ASC_SAN_ERROR("%s: failed with result=%u", operation, static_cast<uint32_t>(failure));
    const AclsanStatus restoreResult = aclsan::ApplyRuntimeHooks({});
    if (restoreResult != ACLSAN_STATUS_SUCCESS) {
        ASC_SAN_ERROR("Failed to clear EnableCallback info, result=%u", static_cast<uint32_t>(restoreResult));
    } else {
        ASC_SAN_ERROR("All EnableCallback info is cleared, all Runtime function remains original");
    }
    Reset(operation);
}

AclsanStatus AclsanSubscriber::ApplyCallbackConfiguration(
    std::set<CallbackKey>& candidateCallbacks, const char* operation)
{
    std::set<aclrtApiId> candidateRequiredHooks = ComputeRequiredHooks(candidateCallbacks);
    const uint32_t candidateProbeGroupMask = ComputeRequiredProbeGroups(candidateCallbacks);
    const uint32_t previousProbeGroupMask = aclsan::SnapshotActiveProbePlan();
    aclsan::CommitActiveProbePlan(candidateProbeGroupMask);
    const AclsanStatus result = aclsan::ApplyRuntimeHooks(candidateRequiredHooks);
    if (result != ACLSAN_STATUS_SUCCESS) {
        aclsan::CommitActiveProbePlan(previousProbeGroupMask);
        ASC_SAN_ERROR("%s: ApplyRuntimeHooks failed, result=%u", operation, static_cast<uint32_t>(result));
        const AclsanStatus rollbackResult = aclsan::ApplyRuntimeHooks(requiredHooks_);
        if (rollbackResult != ACLSAN_STATUS_SUCCESS) {
            ASC_SAN_ERROR(
                "%s: failed to restore previous Runtime Hooks, result=%u", operation,
                static_cast<uint32_t>(rollbackResult));
            Reset(operation);
        } else {
            LogConfigurationState(operation, "rollback-restored");
        }
        return result;
    }
    enabledCallbacks_.swap(candidateCallbacks);
    requiredHooks_.swap(candidateRequiredHooks);
    LogConfigurationState(operation, "committed");
    return ACLSAN_STATUS_SUCCESS;
}

// 根据domain + id + enable更新subscriber状态，记录已启用的callback，计算哪些aclrt接口需要被hook
AclsanStatus AclsanSubscriber::UpdateCallbackConfiguration(const CallbackKey& key, bool shouldEnable)
{
    try {
        // 基于当前已启用的 domain + id 构建本次候选状态。
        std::set<CallbackKey> candidateCallbacks = enabledCallbacks_;
        if (shouldEnable) {
            candidateCallbacks.insert(key);
        } else {
            candidateCallbacks.erase(key);
        }

        return ApplyCallbackConfiguration(candidateCallbacks, "aclsanEnableCallback");
    } catch (const std::bad_alloc&) {
        ClearAfterException(ACLSAN_STATUS_ERROR_OUT_OF_MEMORY, "aclsanEnableCallback");
        return ACLSAN_STATUS_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        ClearAfterException(ACLSAN_STATUS_ERROR_INTERNAL, "aclsanEnableCallback");
        return ACLSAN_STATUS_ERROR_INTERNAL;
    }
}

AclsanStatus AclsanSubscriber::Subscribe(
    AclsanSubscriberHandle* subscriber, AclsanCallbackFunc callback, void* userdata)
{
    std::unique_lock<std::shared_mutex> planLock(aclsan::ActiveProbePlanMutex());
    std::lock_guard<std::recursive_mutex> stateLock(mutex_);
    if (subscriber == nullptr || callback == nullptr) {
        ASC_SAN_ERROR("aclsanSubscribe: subscriber / callback is nullptr");
        return ACLSAN_STATUS_ERROR_INVALID_PARAMETER;
    }
    *subscriber = nullptr;

    if (activeHandle_ != nullptr || unsubscribeInProgress_) {
        ASC_SAN_ERROR("aclsanSubscribe: subscriber already exists");
        return ACLSAN_STATUS_ERROR_ALREADY_SUBSCRIBED;
    }
    if (aclsan::IsRuntimeHookStatePoisoned()) {
        ASC_SAN_ERROR("aclsanSubscribe: Runtime Hook state is poisoned");
        return ACLSAN_STATUS_ERROR_INJECTION_FAILED;
    }

    if (acltoolHookInit() != 0) {
        ASC_SAN_ERROR("aclsanSubscribe: acltoolHookInit failed");
        return ACLSAN_STATUS_ERROR_INJECTION_FAILED;
    }

    activeHandle_ = this;
    callback_ = callback;
    userdata_ = userdata;
    acceptingCallbacks_ = true;
    *subscriber = activeHandle_;
    ASC_SAN_DEBUG("aclsanSubscribe succeed");
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus AclsanSubscriber::EnableCallback(
    uint32_t enable, AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, AclsanCallbackId id)
{
    std::unique_lock<std::shared_mutex> planLock(aclsan::ActiveProbePlanMutex());
    std::lock_guard<std::recursive_mutex> stateLock(mutex_);
    ASC_SAN_DEBUG(
        "aclsanEnableCallback start: enable=%u domain=%u id=%u", enable, static_cast<uint32_t>(domain),
        static_cast<uint32_t>(id));
    if (!IsActive(subscriber)) {
        ASC_SAN_ERROR("aclsanEnableCallback: subscriber is not initialized");
        return ACLSAN_STATUS_ERROR_INVALID_PARAMETER;
    }
    if (!IsValidCallbackId(domain, id)) {
        ASC_SAN_ERROR(
            "aclsanEnableCallback: invalid domain + id combination, enable=%u domain=%u id=%u", enable,
            static_cast<uint32_t>(domain), static_cast<uint32_t>(id));
        return ACLSAN_STATUS_ERROR_INVALID_PARAMETER;
    }

    const CallbackKey key{domain, id};
    // 已声明但尚无 producer 的 callback 不应被伪装成可用 route。
    if (!HasCallbackRoute(key)) {
        ASC_SAN_ERROR(
            "aclsanEnableCallback: no supported callback route for domain=%u id=%u", static_cast<uint32_t>(domain),
            static_cast<uint32_t>(id));
        return ACLSAN_STATUS_ERROR_NOT_SUPPORTED;
    }
    return UpdateCallbackConfiguration(key, enable != 0);
}

AclsanStatus AclsanSubscriber::Unsubscribe(AclsanSubscriberHandle subscriber) noexcept
{
    AclsanStatus result = ACLSAN_STATUS_SUCCESS;
    {
        std::unique_lock<std::shared_mutex> planLock(aclsan::ActiveProbePlanMutex());
        std::lock_guard<std::recursive_mutex> stateLock(mutex_);
        if (!IsActive(subscriber)) {
            ASC_SAN_ERROR("aclsanUnsubscribe: subscriber is not initialized");
            return ACLSAN_STATUS_ERROR_INVALID_PARAMETER;
        }

        acceptingCallbacks_ = false;
        unsubscribeInProgress_ = true;
        result = aclsan::ApplyRuntimeHooks({});
        Reset("aclsanUnsubscribe");
    }

    {
        std::unique_lock<std::recursive_mutex> stateLock(mutex_);
        callbacksDrained_.wait(stateLock, [this] { return activeCallbackInvocations_ == 0; });
        unsubscribeInProgress_ = false;
    }
    if (result != ACLSAN_STATUS_SUCCESS) {
        ASC_SAN_ERROR("aclsanUnsubscribe: ApplyRuntimeHooks failed, result=%u", static_cast<uint32_t>(result));
    }
    return result;
}

AclsanStatus AclsanSubscriber::EnableDomain(
    AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, uint32_t enable)
{
    std::unique_lock<std::shared_mutex> planLock(aclsan::ActiveProbePlanMutex());
    std::lock_guard<std::recursive_mutex> stateLock(mutex_);
    if (!IsActive(subscriber)) {
        ASC_SAN_ERROR("aclsanEnableDomain: subscriber is not initialized");
        return ACLSAN_STATUS_ERROR_INVALID_PARAMETER;
    }
    if (!IsValidCallbackDomain(domain)) {
        ASC_SAN_ERROR("aclsanEnableDomain: invalid domain=%u", static_cast<uint32_t>(domain));
        return ACLSAN_STATUS_ERROR_INVALID_PARAMETER;
    }

    try {
        std::set<CallbackKey> candidateCallbacks = enabledCallbacks_;
        bool hasRoute = false;
        for (const auto& route : kCallbackRoutes) {
            if (route.first.domain != domain) {
                continue;
            }
            hasRoute = true;
            if (enable != 0) {
                candidateCallbacks.insert(route.first);
            } else {
                candidateCallbacks.erase(route.first);
            }
        }
        if (!hasRoute) {
            ASC_SAN_ERROR(
                "aclsanEnableDomain: no supported callback route for domain=%u", static_cast<uint32_t>(domain));
            return ACLSAN_STATUS_ERROR_NOT_SUPPORTED;
        }
        return ApplyCallbackConfiguration(candidateCallbacks, "aclsanEnableDomain");
    } catch (const std::bad_alloc&) {
        ASC_SAN_ERROR("aclsanEnableDomain: failed to allocate candidate callback state");
        return ACLSAN_STATUS_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        ASC_SAN_ERROR("aclsanEnableDomain: failed to build candidate callback state");
        return ACLSAN_STATUS_ERROR_INTERNAL;
    }
}

AclsanStatus AclsanSubscriber::GetCallbackState(
    AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, uint32_t cbid, uint32_t* enabled) const noexcept
{
    std::lock_guard<std::recursive_mutex> stateLock(mutex_);
    if (enabled == nullptr) {
        ASC_SAN_ERROR("aclsanGetCallbackState: enabled is nullptr");
        return ACLSAN_STATUS_ERROR_INVALID_PARAMETER;
    }

    if (!IsActive(subscriber)) {
        ASC_SAN_ERROR("aclsanGetCallbackState: subscriber is not initialized");
        return ACLSAN_STATUS_ERROR_INVALID_PARAMETER;
    }

    const AclsanCallbackId id = static_cast<AclsanCallbackId>(cbid);
    if (!IsValidCallbackId(domain, id)) {
        ASC_SAN_ERROR(
            "aclsanGetCallbackState: invalid domain + id combination, domain=%u id=%u", static_cast<uint32_t>(domain),
            cbid);
        return ACLSAN_STATUS_ERROR_INVALID_PARAMETER;
    }
    const CallbackKey key{domain, id};
    if (!HasCallbackRoute(key)) {
        ASC_SAN_ERROR(
            "aclsanGetCallbackState: no supported callback route for domain=%u id=%u", static_cast<uint32_t>(domain),
            cbid);
        return ACLSAN_STATUS_ERROR_NOT_SUPPORTED;
    }
    *enabled = IsCallbackEnabled(domain, id) ? 1 : 0;
    return ACLSAN_STATUS_SUCCESS;
}

bool AclsanSubscriber::IsCallbackEnabled(AclsanCallbackDomain domain, AclsanCallbackId id) const noexcept
{
    std::lock_guard<std::recursive_mutex> stateLock(mutex_);
    return activeHandle_ != nullptr && enabledCallbacks_.count(CallbackKey{domain, id}) != 0;
}

bool AclsanSubscriber::InvokeCallback(
    AclsanCallbackDomain domain, AclsanCallbackId id, const void* callbackData) noexcept
{
    AclsanCallbackFunc callback = nullptr;
    void* userdata = nullptr;
    {
        std::lock_guard<std::recursive_mutex> stateLock(mutex_);
        if (!acceptingCallbacks_ || callback_ == nullptr) {
            ASC_SAN_ERROR("InvokeCallback: callback is nullptr or subscriber is stopping");
            return false;
        }

        // 如果对应domain和id用户没有主动订阅，那么就不传回cbdata
        if (enabledCallbacks_.count(CallbackKey{domain, id}) == 0) {
            ASC_SAN_DEBUG(
                "InvokeCallback: domain=%u id=%u is not enabled. No call for callback func",
                static_cast<uint32_t>(domain), static_cast<uint32_t>(id));
            return true;
        }
        callback = callback_;
        userdata = userdata_;
        ++activeCallbackInvocations_;
    }

    bool success = true;
    try {
        callback(userdata, domain, id, callbackData);
    } catch (...) {
        ASC_SAN_ERROR(
            "InvokeCallback: domain=%u id=%u failed", static_cast<uint32_t>(domain), static_cast<uint32_t>(id));
        success = false;
    }
    {
        std::lock_guard<std::recursive_mutex> stateLock(mutex_);
        if (activeCallbackInvocations_ != 0) {
            --activeCallbackInvocations_;
        }
        if (activeCallbackInvocations_ == 0) {
            callbacksDrained_.notify_all();
        }
    }
    return success;
}

// 创建唯一subscriber，更新callback函数。该步骤还会刷新aclrt的hook表
extern "C" AclsanStatus aclsanSubscribe(AclsanSubscriberHandle* subscriber, AclsanCallbackFunc callback, void* userdata)
{
    return AclsanSubscriber::Instance().Subscribe(subscriber, callback, userdata);
}

// 更新显式callback route，并开启/关闭相关aclrt接口的hook
extern "C" AclsanStatus aclsanEnableCallback(
    uint32_t enable, AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, AclsanCallbackId id)
{
    return AclsanSubscriber::Instance().EnableCallback(enable, subscriber, domain, id);
}

extern "C" AclsanStatus aclsanUnsubscribe(AclsanSubscriberHandle subscriber)
{
    return AclsanSubscriber::Instance().Unsubscribe(subscriber);
}

extern "C" AclsanStatus aclsanEnableDomain(
    AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, uint32_t enable)
{
    return AclsanSubscriber::Instance().EnableDomain(subscriber, domain, enable);
}

extern "C" AclsanStatus aclsanGetCallbackState(
    AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, AclsanCallbackId cbid, uint32_t* enabled)
{
    return AclsanSubscriber::Instance().GetCallbackState(subscriber, domain, cbid, enabled);
}

extern "C" AclsanStatus aclsanGetDeviceCallStack(uint64_t pc, AclsanDeviceCallStack* result)
{
    if (result == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_PARAMETER;
    }
    *result = {};
    result->pc = pc;

    aclsan::device_runtime::CallStackResult resolved;
    const AclsanStatus resolveStatus = aclsan::ResolveActiveDeviceCallStack(pc, &resolved);
    result->binaryId = resolved.binaryId;
    if (resolveStatus != ACLSAN_STATUS_SUCCESS) {
        return resolveStatus;
    }
    if (!resolved.available) {
        return UnavailableStatus(resolved.error);
    }

    const size_t frameCount = std::min(resolved.frames.size(), static_cast<size_t>(ACLSAN_CALL_STACK_MAX_DEPTH));
    result->depth = static_cast<uint32_t>(frameCount);
    bool complete = frameCount == resolved.frames.size();
    for (size_t index = 0; index < frameCount; ++index) {
        const aclsan::device_runtime::CallStackFrame& source = resolved.frames[index];
        AclsanDeviceCallStackFrame& destination = result->frames[index];
        destination.line = source.line;
        destination.column = source.column;
        destination.inlineDepth = source.inlineDepth;
        complete = CopyText(source.functionName, destination.functionName) && complete;
        complete = CopyText(source.fileName, destination.fileName) && complete;
    }
    if (!complete) {
        result->flags |= ACLSAN_CALL_STACK_FLAG_TRUNCATED;
        return ACLSAN_STATUS_ERROR_MAX_LIMIT_REACHED;
    }
    return ACLSAN_STATUS_SUCCESS;
}

namespace aclsan {

bool IsCallbackEnabled(AclsanCallbackDomain domain, AclsanCallbackId id) noexcept
{
    return AclsanSubscriber::Instance().IsCallbackEnabled(domain, id);
}

// 在 libsanitizer_api 内部通过 subscriber 保存的 callback 和 userdata 派发已启用键。
bool InvokeCallback(AclsanCallbackDomain domain, AclsanCallbackId id, const void* callbackData) noexcept
{
    return AclsanSubscriber::Instance().InvokeCallback(domain, id, callbackData);
}

} // namespace aclsan
