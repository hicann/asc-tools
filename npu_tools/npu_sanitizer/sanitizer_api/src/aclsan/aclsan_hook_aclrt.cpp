/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aclsan/aclsan_api.h"
#include "binary_instrumenter.h"
#include "device_runtime/device_symbolizer.h"
#include "internal/aclsan_active_probe_plan.h"
#include "internal/aclsan_dispatch.h"
#include "internal/aclsan_device_data.h"
#include "internal/aclsan_log.h"
#include "internal/aclsan_runtime_hook.h"
#include "internal/aclsan_trace_runtime.h"
#include "injection/injection_hook.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <set>
#include <shared_mutex>
#include <utility>
#include <variant>

namespace aclsan {
namespace {

bool IsHookRequired(const std::set<aclrtApiId>& requiredHooks, aclrtApiId apiId) noexcept
{
    return requiredHooks.find(apiId) != requiredHooks.end();
}

} // namespace

} // namespace aclsan

namespace {

thread_local bool g_binaryLoadInProgress = false;

class BinaryLoadGuard {
public:
    BinaryLoadGuard() { g_binaryLoadInProgress = true; }
    ~BinaryLoadGuard() { g_binaryLoadInProgress = false; }

    BinaryLoadGuard(const BinaryLoadGuard&) = delete;
    BinaryLoadGuard& operator=(const BinaryLoadGuard&) = delete;
};

template <aclrtApiId ApiId>
struct RuntimeFunctionTraits;

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtLaunchKernelWithHostArgs> {
    using Type = aclrtLaunchKernelWithHostArgsFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtMemcpy> {
    using Type = aclrtMemcpyFunc;
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

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtGetDevice> {
    using Type = aclrtGetDeviceFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtBinaryGetGlobal> {
    using Type = aclrtBinaryGetGlobalFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtGetFunctionAttribute> {
    using Type = aclrtGetFunctionAttributeFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtGetSocName> {
    using Type = aclrtGetSocNameFunc;
};

[[noreturn]] void AbortHookFailure(const char* hookName, const char* stage, const char* reason) noexcept
{
    ASC_SAN_ERROR("[FATAL] npucheck internal failure: hook=%s stage=%s reason=%s", hookName, stage, reason);
    std::abort();
}

template <aclrtApiId ApiId>
typename RuntimeFunctionTraits<ApiId>::Type GetOriginalRuntimeFunction(const char* hookName) noexcept
{
    using Function = typename RuntimeFunctionTraits<ApiId>::Type;
    const auto function = reinterpret_cast<Function>(acltoolGetOriginalRuntimeApi(ApiId));
    // 如果获取不到原始aclrt函数指针，直接异常中止
    if (function == nullptr) {
        AbortHookFailure(hookName, "call_original_aclrt", "acltoolGetOriginalRuntimeApi returned nullptr");
    }
    return function;
}

// 获取当前运行的deviceId用于记录
bool GetCurrentDeviceId(uint32_t& deviceId) noexcept
{
    const auto function = GetOriginalRuntimeFunction<ACL_RT_API_aclrtGetDevice>("aclrtGetDevice");
    int32_t currentDeviceId = -1;
    const aclError result = function(&currentDeviceId);
    if (result != ACL_SUCCESS || currentDeviceId < 0) {
        ASC_SAN_ERROR("acl_san: aclrtGetDevice failed: result=%d deviceId=%d", result, currentDeviceId);
        return false;
    }
    deviceId = static_cast<uint32_t>(currentDeviceId);
    return true;
}

// ==============================================
// =============     创建cbdata     =============
// ==============================================
AclsanCallbackCommonData MakeCallbackCommonData(const char* apiName, int result, uint32_t size) noexcept
{
    return {ACLSAN_API_VERSION, size, apiName, result, 0};
}

// TODO: resourceId目前感觉用不到，可能得改成0
AclsanResourceData MakeDeviceResourceData(
    const char* apiName, int result, void* deviceAddress, uint64_t bytes, uint32_t deviceId) noexcept
{
    return {
        MakeCallbackCommonData(apiName, result, static_cast<uint32_t>(sizeof(AclsanResourceData))),
        deviceAddress,
        bytes,
        ACLSAN_MEMORY_SPACE_DEVICE,
        deviceId,
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(deviceAddress))};
}

AclsanSynchronizeData MakeSynchronizeData(const char* apiName, aclrtStream stream, int result) noexcept
{
    return {MakeCallbackCommonData(apiName, result, static_cast<uint32_t>(sizeof(AclsanSynchronizeData))), stream};
}

// ==============================================
// =========    aclrt接口的hook逻辑    ===========
// ==============================================

// 基本逻辑: 如果获取原始aclrt函数指针失败，那么直接abort。否则把cbdata传回去

struct InstrumentedBinaryLoadContext {
    aclrtBinaryLoadFromDataFunc original = nullptr;
    const aclrtBinaryLoadOptions* options = nullptr;
    aclrtBinHandle* binHandle = nullptr;
};

int32_t LoadInstrumentedBinary(const void* data, size_t length, void* userdata)
{
    auto& context = *static_cast<InstrumentedBinaryLoadContext*>(userdata);
    return context.original(data, length, context.options, context.binHandle);
}

// DONE
aclError aclrtMallocHook(void** deviceAddress, std::size_t size, aclrtMemMallocPolicy policy) noexcept
{
    uint32_t deviceId;
    const bool hasDeviceId = GetCurrentDeviceId(deviceId);
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtMalloc>("aclrtMalloc");
    const aclError result = original(deviceAddress, size, policy);
    if (!hasDeviceId) {
        return result;
    }
    void* allocatedAddress = nullptr;
    if (result == ACL_SUCCESS && deviceAddress != nullptr) {
        allocatedAddress = *deviceAddress;
    }
    const AclsanResourceData callbackData =
        MakeDeviceResourceData("aclrtMalloc", result, allocatedAddress, static_cast<uint64_t>(size), deviceId);
    aclsan::AclsanCallbackDispatcher::DispatchResource(ACLSAN_CBID_RESOURCE_MEMORY_ALLOC, callbackData);
    return result;
}

// DONE
aclError aclrtFreeHook(void* deviceAddress) noexcept
{
    uint32_t deviceId;
    const bool hasDeviceId = GetCurrentDeviceId(deviceId);
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtFree>("aclrtFree");
    const aclError result = original(deviceAddress);
    if (!hasDeviceId) {
        return result;
    }
    const AclsanResourceData callbackData = MakeDeviceResourceData("aclrtFree", result, deviceAddress, 0, deviceId);
    aclsan::AclsanCallbackDispatcher::DispatchResource(ACLSAN_CBID_RESOURCE_MEMORY_FREE, callbackData);
    return result;
}

aclError aclrtBinaryLoadFromDataHook(
    const void* data, size_t length, const aclrtBinaryLoadOptions* options, aclrtBinHandle* binHandle) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtBinaryLoadFromData>("aclrtBinaryLoadFromData");

    if (g_binaryLoadInProgress) {
        const aclError result = original(data, length, options, binHandle);
        if (result == ACL_SUCCESS && binHandle != nullptr) {
            aclsan::RecordTraceBinaryLoadFromData(*binHandle, false, 0, data, length);
        }
        return result;
    }

    const std::shared_lock<std::shared_mutex> planLock(aclsan::ActiveProbePlanMutex());
    const BinaryLoadGuard guard;
    bool loadedPatched = false;
    InstrumentedBinaryLoadContext loadContext{original, options, binHandle};
    const aclsan::RuntimeBinaryInstrumentationResult instrumentation = aclsan::InstrumentRuntimeBinary(
        data, length, aclsan::SnapshotActiveProbePlan(), &LoadInstrumentedBinary, &loadContext);
    aclError result = ACL_ERROR_FAILURE;
    if (instrumentation.status == aclsan::BinaryInstrumentationStatus::Instrumented) {
        result = instrumentation.consumerStatus;
        loadedPatched = result == ACL_SUCCESS;
    } else if (instrumentation.status == aclsan::BinaryInstrumentationStatus::Failed) {
        result = instrumentation.strict != 0 ? ACL_ERROR_FAILURE : original(data, length, options, binHandle);
    } else {
        result = original(data, length, options, binHandle);
    }
    if (result == ACL_SUCCESS && binHandle != nullptr) {
        aclsan::RecordTraceBinaryLoadFromData(
            *binHandle, loadedPatched, instrumentation.traceArgumentOffset, data, length);
    }
    return result;
}

aclError aclrtBinaryGetFunctionHook(
    const aclrtBinHandle binHandle, const char* kernelName, aclrtFuncHandle* funcHandle) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtBinaryGetFunction>("aclrtBinaryGetFunction");
    const aclError result = original(binHandle, kernelName, funcHandle);
    if (result == ACL_SUCCESS && funcHandle != nullptr) {
        aclsan::RecordTraceBinaryFunctionLookup(binHandle, *funcHandle);
    }
    return result;
}

aclError aclrtBinaryGetFunctionByEntryHook(
    aclrtBinHandle binHandle, uint64_t functionEntry, aclrtFuncHandle* funcHandle) noexcept
{
    const auto original =
        GetOriginalRuntimeFunction<ACL_RT_API_aclrtBinaryGetFunctionByEntry>("aclrtBinaryGetFunctionByEntry");
    const aclError result = original(binHandle, functionEntry, funcHandle);
    if (result == ACL_SUCCESS && funcHandle != nullptr) {
        aclsan::RecordTraceBinaryFunctionLookup(binHandle, *funcHandle);
    }
    return result;
}

aclError aclrtGetFuncBySymbolHook(const void* symbol, aclrtFuncHandle* funcHandle) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtGetFuncBySymbol>("aclrtGetFuncBySymbol");
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
    const auto original =
        GetOriginalRuntimeFunction<ACL_RT_API_aclrtLaunchKernelWithHostArgs>("aclrtLaunchKernelWithHostArgs");
    aclsan::PreparedTraceLaunch prepared;
    const aclError prepareResult = aclsan::PrepareTraceLaunch(
        funcHandle, numBlocks, hostArgs, argsSize, placeHolderArray, placeHolderNum, prepared);
    if (prepareResult != ACL_SUCCESS) {
        return prepareResult;
    }

    void* launchArguments = prepared.instrumented ? prepared.arguments.data() : hostArgs;
    const size_t launchArgumentBytes = prepared.instrumented ? prepared.arguments.size() : argsSize;
    aclrtPlaceHolderInfo* launchPlaceholders =
        prepared.instrumented && !prepared.placeholders.empty() ? prepared.placeholders.data() : placeHolderArray;
    const size_t launchPlaceholderCount = prepared.instrumented ? prepared.placeholders.size() : placeHolderNum;
    const aclError result = original(
        funcHandle, numBlocks, stream, config, launchArguments, launchArgumentBytes, launchPlaceholders,
        launchPlaceholderCount);
    aclsan::CompleteTraceLaunch(std::move(prepared), funcHandle, stream, result);
    return result;
}

// DONE
aclError aclrtSynchronizeStreamHook(aclrtStream stream) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtSynchronizeStream>("aclrtSynchronizeStream");
    const aclError result = original(stream);
    aclsan::CollectTraceStream(stream);
    const AclsanSynchronizeData callbackData = MakeSynchronizeData("aclrtSynchronizeStream", stream, result);
    aclsan::AclsanCallbackDispatcher::DispatchSynchronizeEnd(callbackData);
    return result;
}

// DONE
aclError aclrtSynchronizeStreamWithTimeoutHook(aclrtStream stream, int32_t timeout) noexcept
{
    const auto original =
        GetOriginalRuntimeFunction<ACL_RT_API_aclrtSynchronizeStreamWithTimeout>("aclrtSynchronizeStreamWithTimeout");
    const aclError result = original(stream, timeout);
    aclsan::CollectTraceStream(stream);
    const AclsanSynchronizeData callbackData = MakeSynchronizeData("aclrtSynchronizeStreamWithTimeout", stream, result);
    aclsan::AclsanCallbackDispatcher::DispatchSynchronizeEnd(callbackData);
    return result;
}

aclError aclrtBinaryUnLoadHook(aclrtBinHandle binHandle) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtBinaryUnLoad>("aclrtBinaryUnLoad");
    const aclError result = original(binHandle);
    if (result == ACL_SUCCESS) {
        aclsan::RecordTraceBinaryUnload(binHandle);
    }
    return result;
}

aclError aclrtResetDeviceHook(int32_t deviceId) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtResetDevice>("aclrtResetDevice");
    const aclError result = original(deviceId);
    if (result == ACL_SUCCESS) {
        aclsan::ResetTraceRuntimeState();
    }
    return result;
}

using ConfigureHook = int32_t (*)(bool enable) noexcept;

struct RuntimeHookBinding {
    aclrtApiId apiId;
    const char* hookName;
    ConfigureHook configure;
};

// 如果enable，那么注册hook; 反之清除hook
template <aclrtApiId ApiId, auto Register, auto Hook>
int32_t ConfigureRuntimeHook(bool enable) noexcept
{
    return enable ? Register(Hook) : acltoolClearCallback(ApiId);
}

template <aclrtApiId ApiId, auto Register, auto Hook>
constexpr RuntimeHookBinding MakeRuntimeHookBinding(const char* hookName) noexcept
{
    return {ApiId, hookName, ConfigureRuntimeHook<ApiId, Register, Hook>};
}

// aclrtApiId + acl_tool_inject提供的注册aclrt的函数 + 我们实现的hook函数
const std::array<RuntimeHookBinding, 11> g_runtimeHookBindings = {{
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtLaunchKernelWithHostArgs, acltoolRegisterAclrtLaunchKernelWithHostArgsCallbacks,
        aclrtLaunchKernelWithHostArgsHook>("aclrtLaunchKernelWithHostArgs"),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtBinaryLoadFromData, acltoolRegisterAclrtBinaryLoadFromDataCallbacks,
        aclrtBinaryLoadFromDataHook>("aclrtBinaryLoadFromData"),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtBinaryGetFunction, acltoolRegisterAclrtBinaryGetFunctionCallbacks, aclrtBinaryGetFunctionHook>(
        "aclrtBinaryGetFunction"),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtBinaryGetFunctionByEntry, acltoolRegisterAclrtBinaryGetFunctionByEntryCallbacks,
        aclrtBinaryGetFunctionByEntryHook>("aclrtBinaryGetFunctionByEntry"),
    MakeRuntimeHookBinding<ACL_RT_API_aclrtMalloc, acltoolRegisterAclrtMallocCallbacks, aclrtMallocHook>("aclrtMalloc"),
    MakeRuntimeHookBinding<ACL_RT_API_aclrtFree, acltoolRegisterAclrtFreeCallbacks, aclrtFreeHook>("aclrtFree"),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtSynchronizeStream, acltoolRegisterAclrtSynchronizeStreamCallbacks, aclrtSynchronizeStreamHook>(
        "aclrtSynchronizeStream"),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtSynchronizeStreamWithTimeout, acltoolRegisterAclrtSynchronizeStreamWithTimeoutCallbacks,
        aclrtSynchronizeStreamWithTimeoutHook>("aclrtSynchronizeStreamWithTimeout"),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtGetFuncBySymbol, acltoolRegisterAclrtGetFuncBySymbolCallbacks, aclrtGetFuncBySymbolHook>(
        "aclrtGetFuncBySymbol"),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtBinaryUnLoad, acltoolRegisterAclrtBinaryUnLoadCallbacks, aclrtBinaryUnLoadHook>(
        "aclrtBinaryUnLoad"),
    MakeRuntimeHookBinding<ACL_RT_API_aclrtResetDevice, acltoolRegisterAclrtResetDeviceCallbacks, aclrtResetDeviceHook>(
        "aclrtResetDevice"),
}};

} // namespace

namespace aclsan {

AclsanStatus ResolveActiveDeviceCallStack(uint64_t pc, device_runtime::CallStackResult* result) noexcept
{
    ACLSAN_CHECK_NULLPTR("ResolveActiveDeviceCallStack", result);
    *result = ResolveTraceDeviceCallStack(pc);
    return ACLSAN_STATUS_SUCCESS;
}

// 针对所有hook相关的aclrt函数，不在requiredHooks中的统一清除hook，反之注册hook
void ApplyRuntimeHooks(const std::set<aclrtApiId>& requiredHooks) noexcept
{
    for (const RuntimeHookBinding& binding : g_runtimeHookBindings) {
        const bool enable = aclsan::IsHookRequired(requiredHooks, binding.apiId);
        if (binding.configure(enable) != 0) {
            AbortHookFailure(
                binding.hookName, enable ? "register_runtime_hook" : "clear_runtime_hook",
                "Runtime hook configuration returned nonzero");
        }
    }
}

} // namespace aclsan
