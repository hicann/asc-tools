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
#include "acl_hook.h"
#include "internal/aclsan_active_probe_plan.h"
#include "internal/aclsan_dispatch_cb.h"
#include "internal/aclsan_internal.h"
#include "internal/aclsan_log.h"
#include "internal/aclsan_trace_runtime.h"
#include "npu_compute/injection_hook.h"
#include "device_runtime/device_symbolizer.h"

#include <array>
#include <cstdint>
#include <shared_mutex>
#include <set>
#include <utility>

namespace aclsan {
namespace {

bool IsHookRequired(const std::set<aclrtApiId>& requiredHooks, aclrtApiId apiId) noexcept
{
    return requiredHooks.find(apiId) != requiredHooks.end();
}

} // namespace

} // namespace aclsan

namespace {

bool g_hookStateValid = true;

template <aclrtApiId ApiId>
struct RuntimeFunctionTraits;

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtLaunchKernelWithHostArgs> {
    using Type = aclrtLaunchKernelWithHostArgsFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtBinaryLoadFromData> {
    using Type = aclrtBinaryLoadFromDataFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtBinaryGetFunction> {
    using Type = aclrtBinaryGetFunctionFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtMalloc> {
    using Type = aclrtMallocFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtFree> {
    using Type = aclrtFreeFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtResetDevice> {
    using Type = aclrtResetDeviceFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtSynchronizeStream> {
    using Type = aclrtSynchronizeStreamFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtSynchronizeStreamWithTimeout> {
    using Type = aclrtSynchronizeStreamWithTimeoutFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtBinaryGetFunctionByEntry> {
    using Type = aclrtBinaryGetFunctionByEntryFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtGetFuncBySymbol> {
    using Type = aclrtGetFuncBySymbolFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtBinaryUnLoad> {
    using Type = aclrtBinaryUnLoadFunc;
};

template <aclrtApiId ApiId>
typename RuntimeFunctionTraits<ApiId>::Type GetOriginalRuntimeFunction() noexcept
{
    using Function = typename RuntimeFunctionTraits<ApiId>::Type;
    const auto function = reinterpret_cast<Function>(acltoolGetOriginalRuntimeApi(ApiId));
    if (function == nullptr) {
        ASC_SAN_ERROR("acltoolGetOriginalRuntimeApi apiId=%u returns nullptr", static_cast<uint32_t>(ApiId));
    }
    return function;
}

AclsanCallbackCommonData MakeCallbackCommonData(const char* apiName, int result, uint32_t size) noexcept
{
    return {ACLSAN_API_VERSION, size, apiName, result, 0, 0};
}

AclsanResourceData MakeDeviceResourceData(const char* apiName, int result, void* deviceAddress, uint64_t bytes) noexcept
{
    return {
        MakeCallbackCommonData(apiName, result, static_cast<uint32_t>(sizeof(AclsanResourceData))),
        deviceAddress,
        bytes,
        ACLSAN_MEMORY_SPACE_DEVICE,
        0,
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(deviceAddress))};
}

AclsanSynchronizeData MakeSynchronizeData(const char* apiName, aclrtStream stream, int result) noexcept
{
    return {MakeCallbackCommonData(apiName, result, static_cast<uint32_t>(sizeof(AclsanSynchronizeData))), stream};
}

aclError aclrtMallocHook(void** deviceAddress, std::size_t size, aclrtMemMallocPolicy policy) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtMalloc>();
    if (original == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    const aclError result = original(deviceAddress, size, policy);
    if (result == ACL_SUCCESS && deviceAddress != nullptr) {
        const AclsanResourceData callbackData =
            MakeDeviceResourceData("aclrtMalloc", result, *deviceAddress, static_cast<uint64_t>(size));
        aclsan::AclsanCallbackDispatcher::DispatchResource(ACLSAN_CBID_RESOURCE_MEMORY_ALLOC, callbackData);
    }
    return result;
}

aclError aclrtFreeHook(void* deviceAddress) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtFree>();
    if (original == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    const aclError result = original(deviceAddress);
    if (result == ACL_SUCCESS && deviceAddress != nullptr) {
        const AclsanResourceData callbackData = MakeDeviceResourceData("aclrtFree", result, deviceAddress, 0);
        aclsan::AclsanCallbackDispatcher::DispatchResource(ACLSAN_CBID_RESOURCE_MEMORY_FREE, callbackData);
    }
    return result;
}

aclError aclrtBinaryLoadFromDataHook(
    const void* data, size_t length, const aclrtBinaryLoadOptions* options, aclrtBinHandle* binHandle) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtBinaryLoadFromData>();
    if (original == nullptr) {
        return ACL_ERROR_UNINITIALIZE;
    }

    std::shared_lock<std::shared_mutex> planLock;
    if (!aclsan::IsBinaryLoadHookReentrant()) {
        planLock = std::shared_lock<std::shared_mutex>(aclsan::ActiveProbePlanMutex());
    }
    bool loadedPatched = false;
    const aclError result = aclsan::HandleBinaryLoadFromDataWithDefaultConfig(
        data, length, options, binHandle, original, aclsan::SnapshotActiveProbePlan(), &loadedPatched);
    if (result == ACL_SUCCESS && binHandle != nullptr) {
        aclsan::RecordTraceBinaryLoadFromData(*binHandle, loadedPatched, data, length);
    }
    return result;
}

aclError aclrtBinaryGetFunctionHook(
    const aclrtBinHandle binary, const char* kernelName, aclrtFuncHandle* function) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtBinaryGetFunction>();
    if (original == nullptr) {
        return ACL_ERROR_UNINITIALIZE;
    }
    const aclError result = original(binary, kernelName, function);
    if (result == ACL_SUCCESS && function != nullptr) {
        aclsan::RecordTraceBinaryFunctionLookup(binary, *function);
    }
    return result;
}

aclError aclrtBinaryGetFunctionByEntryHook(
    aclrtBinHandle binary, uint64_t functionEntry, aclrtFuncHandle* function) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtBinaryGetFunctionByEntry>();
    if (original == nullptr) {
        return ACL_ERROR_UNINITIALIZE;
    }
    const aclError result = original(binary, functionEntry, function);
    if (result == ACL_SUCCESS && function != nullptr) {
        aclsan::RecordTraceBinaryFunctionLookup(binary, *function);
    }
    return result;
}

aclError aclrtGetFuncBySymbolHook(const void* symbol, aclrtFuncHandle* funcHandle) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtGetFuncBySymbol>();
    if (original == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    ASC_SAN_DEBUG("[HOOK aclrtGetFuncBySymbol] symbol=%p", symbol);
    const aclError result = original(symbol, funcHandle);
    if (result == ACL_SUCCESS && funcHandle != nullptr) {
        aclsan::RecordTraceFunctionLookup(*funcHandle);
    }
    return result;
}

aclError aclrtLaunchKernelWithHostArgsHook(
    aclrtFuncHandle funcHandle, uint32_t numBlocks, aclrtStream stream, aclrtLaunchKernelCfg* config, void* hostArgs,
    size_t argsSize, aclrtPlaceHolderInfo* placeHolderArray, size_t placeHolderNum) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtLaunchKernelWithHostArgs>();
    if (original == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    aclsan::PreparedTraceLaunch prepared;
    const aclError prepareStatus = aclsan::PrepareTraceLaunch(
        funcHandle, numBlocks, hostArgs, argsSize, placeHolderArray, placeHolderNum, prepared);
    if (prepareStatus != ACL_SUCCESS) {
        return prepareStatus;
    }
    void* launchArgs = prepared.instrumented ? prepared.arguments.data() : hostArgs;
    const size_t launchArgsSize = prepared.instrumented ? prepared.arguments.size() : argsSize;
    aclrtPlaceHolderInfo* launchPlaceholders =
        prepared.instrumented && !prepared.placeholders.empty() ? prepared.placeholders.data() : placeHolderArray;
    const size_t launchPlaceholderCount = prepared.instrumented ? prepared.placeholders.size() : placeHolderNum;

    const aclError result = original(
        funcHandle, numBlocks, stream, config, launchArgs, launchArgsSize, launchPlaceholders, launchPlaceholderCount);
    aclsan::CompleteTraceLaunch(std::move(prepared), funcHandle, stream, result);
    return result;
}

aclError aclrtSynchronizeStreamHook(aclrtStream stream) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtSynchronizeStream>();
    if (original == nullptr) {
        const AclsanSynchronizeData callbackData =
            MakeSynchronizeData("aclrtSynchronizeStream", stream, ACL_ERROR_RT_INTERNAL_ERROR);
        aclsan::AclsanCallbackDispatcher::DispatchSynchronizeEnd(callbackData);
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }

    const aclError result = original(stream);
    if (result == ACL_SUCCESS) {
        aclsan::CollectTraceStream(stream);
    }
    const AclsanSynchronizeData callbackData = MakeSynchronizeData("aclrtSynchronizeStream", stream, result);
    aclsan::AclsanCallbackDispatcher::DispatchSynchronizeEnd(callbackData);
    return result;
}

aclError aclrtSynchronizeStreamWithTimeoutHook(aclrtStream stream, int32_t timeout) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtSynchronizeStreamWithTimeout>();
    if (original == nullptr) {
        const AclsanSynchronizeData callbackData =
            MakeSynchronizeData("aclrtSynchronizeStreamWithTimeout", stream, ACL_ERROR_RT_INTERNAL_ERROR);
        aclsan::AclsanCallbackDispatcher::DispatchSynchronizeEnd(callbackData);
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }

    const aclError result = original(stream, timeout);
    if (result == ACL_SUCCESS) {
        aclsan::CollectTraceStream(stream);
    }
    const AclsanSynchronizeData callbackData = MakeSynchronizeData("aclrtSynchronizeStreamWithTimeout", stream, result);
    aclsan::AclsanCallbackDispatcher::DispatchSynchronizeEnd(callbackData);
    return result;
}

aclError aclrtBinaryUnLoadHook(aclrtBinHandle binHandle) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtBinaryUnLoad>();
    if (original == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    ASC_SAN_DEBUG("[HOOK aclrtBinaryUnLoad] handle=%p", binHandle);
    const aclError result = original(binHandle);
    if (result == ACL_SUCCESS) {
        aclsan::RecordTraceBinaryUnload(binHandle);
    }
    return result;
}

aclError aclrtResetDeviceHook(int32_t deviceId) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtResetDevice>();
    if (original == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    ASC_SAN_DEBUG("[HOOK aclrtResetDevice] device=%d", deviceId);
    const aclError result = original(deviceId);
    if (result == ACL_SUCCESS) {
        aclsan::ResetTraceRuntimeState();
    }
    return result;
}

using ConfigureHook = int32_t (*)(bool enable) noexcept;

struct RuntimeHookBinding {
    aclrtApiId apiId;
    ConfigureHook configure;
};

template <aclrtApiId ApiId, auto Register, auto Hook>
int32_t ConfigureRuntimeHook(bool enable) noexcept
{
    return enable ? Register(Hook) : acltoolClearCallback(ApiId);
}

template <aclrtApiId ApiId, auto Register, auto Hook>
constexpr RuntimeHookBinding MakeRuntimeHookBinding() noexcept
{
    return {ApiId, ConfigureRuntimeHook<ApiId, Register, Hook>};
}

const std::array<RuntimeHookBinding, 11> kRuntimeHookBindings = {{
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtLaunchKernelWithHostArgs, acltoolRegisterAclrtLaunchKernelWithHostArgsCallbacks,
        aclrtLaunchKernelWithHostArgsHook>(),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtBinaryLoadFromData, acltoolRegisterAclrtBinaryLoadFromDataCallbacks,
        aclrtBinaryLoadFromDataHook>(),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtBinaryGetFunction, acltoolRegisterAclrtBinaryGetFunctionCallbacks,
        aclrtBinaryGetFunctionHook>(),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtBinaryGetFunctionByEntry, acltoolRegisterAclrtBinaryGetFunctionByEntryCallbacks,
        aclrtBinaryGetFunctionByEntryHook>(),
    MakeRuntimeHookBinding<ACL_RT_API_aclrtMalloc, acltoolRegisterAclrtMallocCallbacks, aclrtMallocHook>(),
    MakeRuntimeHookBinding<ACL_RT_API_aclrtFree, acltoolRegisterAclrtFreeCallbacks, aclrtFreeHook>(),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtSynchronizeStream, acltoolRegisterAclrtSynchronizeStreamCallbacks,
        aclrtSynchronizeStreamHook>(),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtSynchronizeStreamWithTimeout, acltoolRegisterAclrtSynchronizeStreamWithTimeoutCallbacks,
        aclrtSynchronizeStreamWithTimeoutHook>(),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtGetFuncBySymbol, acltoolRegisterAclrtGetFuncBySymbolCallbacks, aclrtGetFuncBySymbolHook>(),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtBinaryUnLoad, acltoolRegisterAclrtBinaryUnLoadCallbacks, aclrtBinaryUnLoadHook>(),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtResetDevice, acltoolRegisterAclrtResetDeviceCallbacks, aclrtResetDeviceHook>(),
}};

bool ApplyRuntimeHookConfiguration(const std::set<aclrtApiId>& requiredHooks) noexcept
{
    bool success = true;
    for (const RuntimeHookBinding& binding : kRuntimeHookBindings) {
        const bool enable = aclsan::IsHookRequired(requiredHooks, binding.apiId);
        if (binding.configure(enable) != 0) {
            success = false;
        }
    }
    return success;
}

} // namespace

namespace aclsan {

AclsanStatus ResolveActiveDeviceCallStack(uint64_t pc, device_runtime::CallStackResult* result) noexcept
{
    if (result == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_PARAMETER;
    }
    *result = ResolveTraceDeviceCallStack(pc);
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApplyRuntimeHooks(const std::set<aclrtApiId>& requiredHooks) noexcept
{
    if (!g_hookStateValid) {
        ASC_SAN_ERROR("acl_san: custom hook state is poisoned; restart the process before retrying");
        return ACLSAN_STATUS_ERROR_INJECTION_FAILED;
    }

    if (!ApplyRuntimeHookConfiguration(requiredHooks)) {
        if (!ApplyRuntimeHookConfiguration({})) {
            g_hookStateValid = false;
            ASC_SAN_ERROR("acl_san: custom configuration failed and clearing all custom slots failed");
        } else {
            ASC_SAN_ERROR("acl_san: custom configuration failed; all custom slots cleared");
        }
        return ACLSAN_STATUS_ERROR_INJECTION_FAILED;
    }
    return ACLSAN_STATUS_SUCCESS;
}

bool IsRuntimeHookStatePoisoned() noexcept { return !g_hookStateValid; }

} // namespace aclsan
