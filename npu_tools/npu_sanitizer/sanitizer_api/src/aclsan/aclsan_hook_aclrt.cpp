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
#include "dbi/binary_instrumenter.h"
#include "device_runtime/device_symbolizer.h"
#include "internal/aclsan_active_probe_plan.h"
#include "internal/aclsan_dispatch.h"
#include "internal/aclsan_device_data.h"
#include "plog_sink.h"
#include "internal/aclsan_runtime_hook.h"
#include "internal/aclsan_trace_runtime.h"
#include "injection/injection_hook.h"

#include <array>
#include <cstdint>
#include <dlfcn.h>
#include <new>
#include <set>
#include <shared_mutex>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace aclsan {
namespace {

bool IsHookRequired(const std::set<aclrtApiId>& requiredHooks, aclrtApiId apiId) noexcept
{
    return requiredHooks.find(apiId) != requiredHooks.end();
}

} // namespace

} // namespace aclsan

namespace {

using aclsan::AbortHookFailure;
using aclsan::GetOriginalRuntimeFunction;

// TODO: 中间要加上异常报错 / 中止机制
// 复制业务参数地址，再追加调用方保存的隐藏指针的地址；其存储需保持有效直到 launch 返回。
// 基于originalArgs更新增加参数隐藏trace指针的存储地址，最终aclrtLaunchKernelWithArgsArray使用的是args
// 将原有的args复制一份后，增加一个trace指针
aclError BuildInstrumentedArgsArray(
    const void* function, void*& deviceBuffer, size_t traceArgumentOffset, void* const* originalArgs,
    std::vector<void*>& args)
{
    if (function == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    const auto getCount = GetOriginalRuntimeFunction<aclrtFunctionGetParamCountFunc>(
        ACL_RT_API_aclrtFunctionGetParamCount, "aclrtFunctionGetParamCount");
    const auto getInfo = GetOriginalRuntimeFunction<aclrtFunctionGetParamInfoFunc>(
        ACL_RT_API_aclrtFunctionGetParamInfo, "aclrtFunctionGetParamInfo");
    size_t count = 0;
    ACLSAN_RETURN_IF_ACL_ERROR(getCount(function, &count), "Failed to get kernel parameter count");
    // 因为插桩的kernel已经增加隐藏trace参数，所以不应该为0
    if (count == 0) {
        return ACL_ERROR_FEATURE_UNSUPPORTED;
    }

    // 校验funcHandle对应paramInfo符号预期  参数个数和偏移都得符合预期
    const size_t originalCount = count - 1;
    size_t offset = 0;
    size_t size = 0;
    ACLSAN_RETURN_IF_ACL_ERROR(
        getInfo(function, originalCount, &offset, &size), "Failed to get hidden trace parameter info");
    // 只校验 sanitizer 追加的隐藏 GM 指针，业务参数地址由调用者直接提供。
    if (size != sizeof(void*) || offset != traceArgumentOffset) {
        return ACL_ERROR_FEATURE_UNSUPPORTED;
    }
    if (originalCount != 0 && originalArgs == nullptr) { // 仅有0参数场景才可能originalArgs为nullptr
        return ACL_ERROR_INVALID_PARAM;
    }
    for (size_t index = 0; index < originalCount; ++index) {
        args.push_back(originalArgs[index]); // hostArgs在host内存中的存储地址
    }
    args.push_back(&deviceBuffer); // 插入隐藏指针的存储地址
    return ACL_SUCCESS;
}

thread_local bool g_binaryLoadInProgress = false;

class BinaryLoadGuard {
public:
    BinaryLoadGuard() { g_binaryLoadInProgress = true; }
    ~BinaryLoadGuard() { g_binaryLoadInProgress = false; }

    BinaryLoadGuard(const BinaryLoadGuard&) = delete;
    BinaryLoadGuard& operator=(const BinaryLoadGuard&) = delete;
};

// 获取当前运行的deviceId用于记录
bool GetCurrentDeviceId(uint32_t& deviceId) noexcept
{
    const auto function = GetOriginalRuntimeFunction<aclrtGetDeviceFunc>(ACL_RT_API_aclrtGetDevice, "aclrtGetDevice");
    int32_t currentDeviceId = -1;
    const aclError result = function(&currentDeviceId);
    if (result != ACL_SUCCESS || currentDeviceId < 0) {
        ACL_SAN_ERROR("acl_san: aclrtGetDevice failed: result=%d deviceId=%d", result, currentDeviceId);
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

AclsanLaunchData MakeLaunchData(
    uint64_t launchId, aclrtFuncHandle function, aclrtStream stream, const std::string& functionName,
    uint32_t numBlocks, aclError launchResult, const char* apiName) noexcept
{
    const char* name = functionName.empty() ? nullptr : functionName.c_str();
    return {
        MakeCallbackCommonData(apiName, launchResult, static_cast<uint32_t>(sizeof(AclsanLaunchData))),
        launchId,
        function,
        stream,
        name,
        numBlocks};
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
    const auto original = GetOriginalRuntimeFunction<aclrtMallocFunc>(ACL_RT_API_aclrtMalloc, "aclrtMalloc");
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
    const auto original = GetOriginalRuntimeFunction<aclrtFreeFunc>(ACL_RT_API_aclrtFree, "aclrtFree");
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
    const auto original = GetOriginalRuntimeFunction<aclrtBinaryLoadFromDataFunc>(
        ACL_RT_API_aclrtBinaryLoadFromData, "aclrtBinaryLoadFromData");

    if (g_binaryLoadInProgress) {
        const aclError result = original(data, length, options, binHandle);
        if (result == ACL_SUCCESS && binHandle != nullptr) {
            aclsan::RecordTraceBinaryLoadFromData(*binHandle, false, 0, data, length);
        }
        return result;
    }

    uint32_t probePlan = 0;
    {
        const std::shared_lock<std::shared_mutex> planLock(aclsan::ActiveProbePlanMutex());
        probePlan = aclsan::SnapshotActiveProbePlan();
    }
    if (probePlan == 0) {
        const aclError result = original(data, length, options, binHandle);
        if (result == ACL_SUCCESS && binHandle != nullptr) {
            aclsan::RecordTraceBinaryLoadFromData(*binHandle, false, 0, data, length);
        }
        return result;
    }
    const BinaryLoadGuard guard;
    bool loadedPatched = false;
    InstrumentedBinaryLoadContext loadContext{original, options, binHandle};
    const auto getSocName =
        GetOriginalRuntimeFunction<aclrtGetSocNameFunc>(ACL_RT_API_aclrtGetSocName, "aclrtGetSocName");
    Dl_info runtimeInfo{};
    // TODO: 是否没必要，可以通过ENV ASCEND_HOME_PATH来推断，这样的话动态库可以不用链接dl
    const char* runtimeLibrary =
        dladdr(reinterpret_cast<const void*>(getSocName), &runtimeInfo) != 0 ? runtimeInfo.dli_fname : nullptr;
    const aclsan::RuntimeBinaryInstrumentationResult instrumentation = aclsan::InstrumentRuntimeBinary(
        data, length, probePlan, getSocName(), runtimeLibrary, &LoadInstrumentedBinary, &loadContext);
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
    const auto original = GetOriginalRuntimeFunction<aclrtBinaryGetFunctionFunc>(
        ACL_RT_API_aclrtBinaryGetFunction, "aclrtBinaryGetFunction");
    const aclError result = original(binHandle, kernelName, funcHandle);
    if (result == ACL_SUCCESS && funcHandle != nullptr) {
        aclsan::RecordTraceBinaryFunctionLookup(binHandle, *funcHandle, kernelName);
    }
    return result;
}

// TODO：是否没必要，因为aclrtBinaryGetFunctionByEntry是预留接口
aclError aclrtBinaryGetFunctionByEntryHook(
    aclrtBinHandle binHandle, uint64_t functionEntry, aclrtFuncHandle* funcHandle) noexcept
{
    const auto original = GetOriginalRuntimeFunction<aclrtBinaryGetFunctionByEntryFunc>(
        ACL_RT_API_aclrtBinaryGetFunctionByEntry, "aclrtBinaryGetFunctionByEntry");
    const aclError result = original(binHandle, functionEntry, funcHandle);
    if (result == ACL_SUCCESS && funcHandle != nullptr) {
        aclsan::RecordTraceBinaryFunctionLookup(binHandle, *funcHandle, nullptr);
    }
    return result;
}

// TODO: 待确认现在<<<>>>是否会走这个函数  目前仅将返回的 funcHandle 归属到最近加载的 binary，感觉不合理
aclError aclrtGetFuncBySymbolHook(const void* symbol, aclrtFuncHandle* funcHandle) noexcept
{
    const auto original =
        GetOriginalRuntimeFunction<aclrtGetFuncBySymbolFunc>(ACL_RT_API_aclrtGetFuncBySymbol, "aclrtGetFuncBySymbol");
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
    const auto original = GetOriginalRuntimeFunction<aclrtLaunchKernelWithHostArgsFunc>(
        ACL_RT_API_aclrtLaunchKernelWithHostArgs, "aclrtLaunchKernelWithHostArgs");
    aclsan::PreparedTraceLaunch prepared;
    ACLSAN_RETURN_IF_ACL_ERROR(
        aclsan::PrepareTraceLaunch(
            funcHandle, numBlocks, hostArgs, argsSize, placeHolderArray, placeHolderNum,
            aclsan::TraceArgumentMode::kHostArgs, prepared),
        "Failed to prepare trace for aclrtLaunchKernelWithHostArgs");

    void* launchArguments = prepared.instrumented ? prepared.arguments.data() : hostArgs;
    const size_t launchArgumentBytes = prepared.instrumented ? prepared.arguments.size() : argsSize;
    aclrtPlaceHolderInfo* launchPlaceholders =
        prepared.instrumented && !prepared.placeholders.empty() ? prepared.placeholders.data() : placeHolderArray;
    const size_t launchPlaceholderCount = prepared.instrumented ? prepared.placeholders.size() : placeHolderNum;
    const aclError result = original(
        funcHandle, numBlocks, stream, config, launchArguments, launchArgumentBytes, launchPlaceholders,
        launchPlaceholderCount);
    ACL_SAN_DEBUG(
        "aclrtLaunchKernelWithHostArgs: function=%p blocks=%u stream=%p instrumented=%u result=%d", funcHandle,
        numBlocks, stream, static_cast<unsigned>(prepared.instrumented), result);
    aclsan::CompleteTraceLaunch(std::move(prepared), funcHandle, stream, result);
    std::string functionName;
    (void)aclsan::GetTraceFunctionName(funcHandle, functionName);
    const AclsanLaunchData callbackData = MakeLaunchData(
        prepared.launchId, funcHandle, stream, functionName, numBlocks, result, "aclrtLaunchKernelWithHostArgs");
    aclsan::AclsanCallbackDispatcher::DispatchLaunch(callbackData);
    return result;
}

aclError aclrtLaunchKernelWithArgsArrayHook(
    void* func, uint32_t numBlocks, aclrtStream stream, aclrtLaunchKernelCfg* config, void** args) noexcept
{
    aclsan::PreparedTraceLaunch prepared;
    // 原始参数由 ArgsArray 直接传递，这里只准备隐藏参数及其所属的 trace buffer。
    ACLSAN_RETURN_IF_ACL_ERROR(
        aclsan::PrepareTraceLaunch(
            func, numBlocks, nullptr, 0, nullptr, 0, aclsan::TraceArgumentMode::kArgsArray, prepared),
        "Failed to PrepareTraceLaunch for aclrtLaunchKernelWithArgsArray");

    aclError result = ACL_SUCCESS;
    try {
        std::vector<void*> launchArgs;
        if (prepared.instrumented) {
            result =
                BuildInstrumentedArgsArray(func, prepared.deviceBuffer, prepared.traceArgumentOffset, args, launchArgs);
            if (result != ACL_SUCCESS) {
                ACL_SAN_ERROR(
                    "BuildInstrumentedArgsArray failed in aclrtLaunchKernelWithArgsArrayHook: result=%d", result);
            }
        }
        if (result == ACL_SUCCESS) {
            const auto original = GetOriginalRuntimeFunction<aclrtLaunchKernelWithArgsArrayFunc>(
                ACL_RT_API_aclrtLaunchKernelWithArgsArray, "aclrtLaunchKernelWithArgsArray");
            result = original(func, numBlocks, stream, config, prepared.instrumented ? launchArgs.data() : args);
        }
    } catch (const std::bad_alloc&) {
        result = ACL_ERROR_BAD_ALLOC;
    } catch (...) {
        result = ACL_ERROR_FAILURE;
    }
    aclsan::CompleteTraceLaunch(std::move(prepared), func, stream, result);
    std::string functionName;
    (void)aclsan::GetTraceFunctionName(func, functionName);
    const AclsanLaunchData callbackData = MakeLaunchData(
        prepared.launchId, func, stream, functionName, numBlocks, result, "aclrtLaunchKernelWithArgsArray");
    aclsan::AclsanCallbackDispatcher::DispatchLaunch(callbackData);
    return result;
}

// DONE
aclError aclrtSynchronizeStreamHook(aclrtStream stream) noexcept
{
    const auto original = GetOriginalRuntimeFunction<aclrtSynchronizeStreamFunc>(
        ACL_RT_API_aclrtSynchronizeStream, "aclrtSynchronizeStream");
    const aclError result = original(stream);
    aclsan::CollectTraceStream(stream);
    const AclsanSynchronizeData callbackData = MakeSynchronizeData("aclrtSynchronizeStream", stream, result);
    aclsan::AclsanCallbackDispatcher::DispatchSynchronizeEnd(callbackData);
    return result;
}

// DONE
aclError aclrtSynchronizeStreamWithTimeoutHook(aclrtStream stream, int32_t timeout) noexcept
{
    const auto original = GetOriginalRuntimeFunction<aclrtSynchronizeStreamWithTimeoutFunc>(
        ACL_RT_API_aclrtSynchronizeStreamWithTimeout, "aclrtSynchronizeStreamWithTimeout");
    const aclError result = original(stream, timeout);
    aclsan::CollectTraceStream(stream);
    const AclsanSynchronizeData callbackData = MakeSynchronizeData("aclrtSynchronizeStreamWithTimeout", stream, result);
    aclsan::AclsanCallbackDispatcher::DispatchSynchronizeEnd(callbackData);
    return result;
}

aclError aclrtBinaryUnLoadHook(aclrtBinHandle binHandle) noexcept
{
    const auto original =
        GetOriginalRuntimeFunction<aclrtBinaryUnLoadFunc>(ACL_RT_API_aclrtBinaryUnLoad, "aclrtBinaryUnLoad");
    const aclError result = original(binHandle);
    if (result == ACL_SUCCESS) {
        aclsan::RecordTraceBinaryUnload(binHandle);
    }
    return result;
}

aclError aclrtResetDeviceHook(int32_t deviceId) noexcept
{
    const auto original =
        GetOriginalRuntimeFunction<aclrtResetDeviceFunc>(ACL_RT_API_aclrtResetDevice, "aclrtResetDevice");
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
const std::array<RuntimeHookBinding, 12> g_runtimeHookBindings = {{
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtLaunchKernelWithHostArgs, acltoolRegisterAclrtLaunchKernelWithHostArgsCallbacks,
        aclrtLaunchKernelWithHostArgsHook>("aclrtLaunchKernelWithHostArgs"),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtLaunchKernelWithArgsArray, acltoolRegisterAclrtLaunchKernelWithArgsArrayCallbacks,
        aclrtLaunchKernelWithArgsArrayHook>("aclrtLaunchKernelWithArgsArray"),
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
