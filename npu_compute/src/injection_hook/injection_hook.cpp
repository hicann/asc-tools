/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "npu_compute/injection_hook.h"

#include "common/debug_log.h"
#include "profiling/prof_api.h"

#include <map>
#include <mutex>
#include <pthread.h>
#include <shared_mutex>
#include <string>

namespace {

struct aclApiTable {
    void* active[ACL_RT_API_MAX];
    void* orig[ACL_RT_API_MAX];
    pthread_rwlock_t lock;
};

constexpr uint32_t kMsprofDataCallbackType = 0U;

const std::map<std::string, aclrtApiId> aclrtApiNameMap = {
    {"aclrtLaunchKernelWithHostArgs", ACL_RT_API_aclrtLaunchKernelWithHostArgs},
    {"aclrtMemcpy", ACL_RT_API_aclrtMemcpy},
    {"aclrtBinaryLoadFromData", ACL_RT_API_aclrtBinaryLoadFromData},
    {"aclrtBinaryGetFunction", ACL_RT_API_aclrtBinaryGetFunction},
    {"aclrtMalloc", ACL_RT_API_aclrtMalloc},
    {"aclrtMemset", ACL_RT_API_aclrtMemset},
    {"aclrtFree", ACL_RT_API_aclrtFree},
    {"aclrtCreateStream", ACL_RT_API_aclrtCreateStream},
    {"aclrtDestroyStream", ACL_RT_API_aclrtDestroyStream},
    {"aclrtSetDevice", ACL_RT_API_aclrtSetDevice},
    {"aclrtResetDevice", ACL_RT_API_aclrtResetDevice},
    {"aclrtSynchronizeStream", ACL_RT_API_aclrtSynchronizeStream},
    {"aclrtBinaryGetFunctionByEntry", ACL_RT_API_aclrtBinaryGetFunctionByEntry},
    {"aclrtLaunchKernel", ACL_RT_API_aclrtLaunchKernel}};

struct CallbackTable {
    aclrtApiFunc callbacks[ACL_RT_API_MAX];
    mutable std::shared_mutex lock;
};

aclApiTable g_aclApiTable = {{}, {}, PTHREAD_RWLOCK_INITIALIZER};
CallbackTable g_callbackTable{};
std::mutex g_initMutex;
bool g_hookInstalled = false;

template <typename T>
void* FunctionToAddress(T func)
{
    return reinterpret_cast<void*>(func);
}

template <typename T>
T AddressToFunction(void* address)
{
    return reinterpret_cast<T>(address);
}

template <typename T>
aclrtApiFunc ToGenericFunction(T func)
{
    return AddressToFunction<aclrtApiFunc>(FunctionToAddress(func));
}

bool IsValidApiId(aclrtApiId id)
{
    const auto value = static_cast<int32_t>(id);
    return value >= 0 && value < static_cast<int32_t>(ACL_RT_API_MAX);
}

const char* FindRuntimeApiName(aclrtApiId id);

bool SaveRuntimeEntry(aclrtApiId id, void* origin, void* active)
{
    if (!IsValidApiId(id)) {
        return false;
    }
    const size_t index = static_cast<size_t>(id);
    pthread_rwlock_wrlock(&g_aclApiTable.lock);
    g_aclApiTable.orig[index] = origin;
    g_aclApiTable.active[index] = active;
    pthread_rwlock_unlock(&g_aclApiTable.lock);
    return true;
}

template <typename Function>
Function GetRegisteredCallback(aclrtApiId id)
{
    if (!IsValidApiId(id)) {
        return nullptr;
    }
    const size_t index = static_cast<size_t>(id);
    std::shared_lock<std::shared_mutex> lock(g_callbackTable.lock);
    const aclrtApiFunc callback = g_callbackTable.callbacks[index];
    return AddressToFunction<Function>(FunctionToAddress(callback));
}

template <typename Function>
Function GetOriginalFromTable(aclrtApiId id)
{
    if (!IsValidApiId(id)) {
        return nullptr;
    }
    const size_t index = static_cast<size_t>(id);
    pthread_rwlock_rdlock(&g_aclApiTable.lock);
    void* origin = g_aclApiTable.orig[index];
    pthread_rwlock_unlock(&g_aclApiTable.lock);
    return AddressToFunction<Function>(origin);
}

const char* FindRuntimeApiName(aclrtApiId id)
{
    for (const auto& entry : aclrtApiNameMap) {
        if (entry.second == id) {
            return entry.first.c_str();
        }
    }
    return nullptr;
}

int32_t RegisterCallback(aclrtApiId id, aclrtApiFunc callback)
{
    const char* name = FindRuntimeApiName(id);
    if (!IsValidApiId(id)) {
        npu_compute::detail::DebugLog(
            "tool_injection", "register callback failed: name=%s id=%d", name == nullptr ? "unknown" : name,
            static_cast<int>(id));
        return ACL_ERROR_INVALID_PARAM;
    }
    const size_t index = static_cast<size_t>(id);
    std::unique_lock<std::shared_mutex> lock(g_callbackTable.lock);
    const bool replaced = g_callbackTable.callbacks[index] != nullptr;
    g_callbackTable.callbacks[index] = callback;
    npu_compute::detail::DebugLog(
        "tool_injection", "%s callback: name=%s id=%d callback=%p",
        callback == nullptr ? "clear" : (replaced ? "replace" : "register"), name == nullptr ? "unknown" : name,
        static_cast<int>(id), FunctionToAddress(callback));
    return ACL_SUCCESS;
}

template <typename Function>
Function GetDispatchTarget(aclrtApiId id)
{
    const char* name = FindRuntimeApiName(id);
    Function target = GetRegisteredCallback<Function>(id);
    const char* source = "callback";
    if (target == nullptr) {
        target = GetOriginalFromTable<Function>(id);
        source = "origin";
    }
    npu_compute::detail::DebugLog(
        "tool_injection", "dispatch runtime api: name=%s id=%d source=%s target=%p", name == nullptr ? "unknown" : name,
        static_cast<int>(id), source, FunctionToAddress(target));
    return target;
}

void LogMissingCallback(const char* name, aclrtApiId id)
{
    npu_compute::detail::DebugLog(
        "tool_injection", "hook call failed: callback and origin are unavailable name=%s id=%d",
        name == nullptr ? "unknown" : name, static_cast<int>(id));
}

void LogHookResult(const char* name, aclrtApiId id, aclError result)
{
    npu_compute::detail::DebugLog(
        "tool_injection", "hook return: name=%s id=%d result=%d", name == nullptr ? "unknown" : name,
        static_cast<int>(id), static_cast<int>(result));
}

} // namespace

extern "C" aclError aclrtSetDeviceHook(int32_t deviceId)
{
    constexpr aclrtApiId id = ACL_RT_API_aclrtSetDevice;
    const auto callback = GetDispatchTarget<aclrtSetDeviceFunc>(id);
    if (callback == nullptr) {
        LogMissingCallback("aclrtSetDevice", id);
        return ACL_ERROR_UNINITIALIZE;
    }
    const aclError result = callback(deviceId);
    LogHookResult("aclrtSetDevice", id, result);
    return result;
}

extern "C" aclError aclrtResetDeviceHook(int32_t deviceId)
{
    constexpr aclrtApiId id = ACL_RT_API_aclrtResetDevice;
    const auto callback = GetDispatchTarget<aclrtResetDeviceFunc>(id);
    if (callback == nullptr) {
        LogMissingCallback("aclrtResetDevice", id);
        return ACL_ERROR_UNINITIALIZE;
    }
    const aclError result = callback(deviceId);
    LogHookResult("aclrtResetDevice", id, result);
    return result;
}

extern "C" aclError aclrtCreateStreamHook(aclrtStream* stream)
{
    constexpr aclrtApiId id = ACL_RT_API_aclrtCreateStream;
    const auto callback = GetDispatchTarget<aclrtCreateStreamFunc>(id);
    if (callback == nullptr) {
        LogMissingCallback("aclrtCreateStream", id);
        return ACL_ERROR_UNINITIALIZE;
    }
    const aclError result = callback(stream);
    LogHookResult("aclrtCreateStream", id, result);
    return result;
}

extern "C" aclError aclrtDestroyStreamHook(aclrtStream stream)
{
    constexpr aclrtApiId id = ACL_RT_API_aclrtDestroyStream;
    const auto callback = GetDispatchTarget<aclrtDestroyStreamFunc>(id);
    if (callback == nullptr) {
        LogMissingCallback("aclrtDestroyStream", id);
        return ACL_ERROR_UNINITIALIZE;
    }
    const aclError result = callback(stream);
    LogHookResult("aclrtDestroyStream", id, result);
    return result;
}

extern "C" aclError aclrtMallocHook(void** devPtr, size_t size, aclrtMemMallocPolicy policy)
{
    constexpr aclrtApiId id = ACL_RT_API_aclrtMalloc;
    const auto callback = GetDispatchTarget<aclrtMallocFunc>(id);
    if (callback == nullptr) {
        LogMissingCallback("aclrtMalloc", id);
        return ACL_ERROR_UNINITIALIZE;
    }
    const aclError result = callback(devPtr, size, policy);
    LogHookResult("aclrtMalloc", id, result);
    return result;
}

extern "C" aclError aclrtFreeHook(void* devPtr)
{
    constexpr aclrtApiId id = ACL_RT_API_aclrtFree;
    const auto callback = GetDispatchTarget<aclrtFreeFunc>(id);
    if (callback == nullptr) {
        LogMissingCallback("aclrtFree", id);
        return ACL_ERROR_UNINITIALIZE;
    }
    const aclError result = callback(devPtr);
    LogHookResult("aclrtFree", id, result);
    return result;
}

extern "C" aclError aclrtMemcpyHook(void* dst, size_t destMax, const void* src, size_t count, aclrtMemcpyKind kind)
{
    constexpr aclrtApiId id = ACL_RT_API_aclrtMemcpy;
    const auto callback = GetDispatchTarget<aclrtMemcpyFunc>(id);
    if (callback == nullptr) {
        LogMissingCallback("aclrtMemcpy", id);
        return ACL_ERROR_UNINITIALIZE;
    }
    const aclError result = callback(dst, destMax, src, count, kind);
    LogHookResult("aclrtMemcpy", id, result);
    return result;
}

extern "C" aclError aclrtMemsetHook(void* devPtr, size_t maxCount, int32_t value, size_t count)
{
    constexpr aclrtApiId id = ACL_RT_API_aclrtMemset;
    const auto callback = GetDispatchTarget<aclrtMemsetFunc>(id);
    if (callback == nullptr) {
        LogMissingCallback("aclrtMemset", id);
        return ACL_ERROR_UNINITIALIZE;
    }
    const aclError result = callback(devPtr, maxCount, value, count);
    LogHookResult("aclrtMemset", id, result);
    return result;
}

extern "C" aclError aclrtSynchronizeStreamHook(aclrtStream stream)
{
    constexpr aclrtApiId id = ACL_RT_API_aclrtSynchronizeStream;
    const auto callback = GetDispatchTarget<aclrtSynchronizeStreamFunc>(id);
    if (callback == nullptr) {
        LogMissingCallback("aclrtSynchronizeStream", id);
        return ACL_ERROR_UNINITIALIZE;
    }
    const aclError result = callback(stream);
    LogHookResult("aclrtSynchronizeStream", id, result);
    return result;
}

extern "C" aclError aclrtBinaryLoadFromDataHook(
    const void* binaryData, size_t length, const aclrtBinaryLoadOptions* options, aclrtBinHandle* binHandle)
{
    constexpr aclrtApiId id = ACL_RT_API_aclrtBinaryLoadFromData;
    const auto callback = GetDispatchTarget<aclrtBinaryLoadFromDataFunc>(id);
    if (callback == nullptr) {
        LogMissingCallback("aclrtBinaryLoadFromData", id);
        return ACL_ERROR_UNINITIALIZE;
    }
    const aclError result = callback(binaryData, length, options, binHandle);
    LogHookResult("aclrtBinaryLoadFromData", id, result);
    return result;
}

extern "C" aclError aclrtBinaryGetFunctionHook(
    const aclrtBinHandle binHandle, const char* kernelName, aclrtFuncHandle* funcHandle)
{
    constexpr aclrtApiId id = ACL_RT_API_aclrtBinaryGetFunction;
    const auto callback = GetDispatchTarget<aclrtBinaryGetFunctionFunc>(id);
    if (callback == nullptr) {
        LogMissingCallback("aclrtBinaryGetFunction", id);
        return ACL_ERROR_UNINITIALIZE;
    }
    const aclError result = callback(binHandle, kernelName, funcHandle);
    LogHookResult("aclrtBinaryGetFunction", id, result);
    return result;
}

extern "C" aclError aclrtBinaryGetFunctionByEntryHook(
    aclrtBinHandle binHandle, uint64_t funcEntry, aclrtFuncHandle* funcHandle)
{
    constexpr aclrtApiId id = ACL_RT_API_aclrtBinaryGetFunctionByEntry;
    const auto callback = GetDispatchTarget<aclrtBinaryGetFunctionByEntryFunc>(id);
    if (callback == nullptr) {
        LogMissingCallback("aclrtBinaryGetFunctionByEntry", id);
        return ACL_ERROR_UNINITIALIZE;
    }
    const aclError result = callback(binHandle, funcEntry, funcHandle);
    LogHookResult("aclrtBinaryGetFunctionByEntry", id, result);
    return result;
}

extern "C" aclError aclrtLaunchKernelWithHostArgsHook(
    aclrtFuncHandle funcHandle, uint32_t numBlocks, aclrtStream stream, aclrtLaunchKernelCfg* cfg, void* hostArgs,
    size_t argsSize, aclrtPlaceHolderInfo* placeHolderArray, size_t placeHolderNum)
{
    constexpr aclrtApiId id = ACL_RT_API_aclrtLaunchKernelWithHostArgs;
    const auto callback = GetDispatchTarget<aclrtLaunchKernelWithHostArgsFunc>(id);
    if (callback == nullptr) {
        LogMissingCallback("aclrtLaunchKernelWithHostArgs", id);
        return ACL_ERROR_UNINITIALIZE;
    }
    const aclError result =
        callback(funcHandle, numBlocks, stream, cfg, hostArgs, argsSize, placeHolderArray, placeHolderNum);
    LogHookResult("aclrtLaunchKernelWithHostArgs", id, result);
    return result;
}

extern "C" aclError aclrtLaunchKernelHook(
    aclrtFuncHandle funcHandle, uint32_t numBlocks, const void* argsData, size_t argsSize, aclrtStream stream)
{
    constexpr aclrtApiId id = ACL_RT_API_aclrtLaunchKernel;
    const auto callback = GetDispatchTarget<aclrtLaunchKernelFunc>(id);
    if (callback == nullptr) {
        LogMissingCallback("aclrtLaunchKernel", id);
        return ACL_ERROR_UNINITIALIZE;
    }
    const aclError result = callback(funcHandle, numBlocks, argsData, argsSize, stream);
    LogHookResult("aclrtLaunchKernel", id, result);
    return result;
}

namespace {

const aclrtApiFunc kRuntimeHooks[ACL_RT_API_MAX] = {
    ToGenericFunction(&aclrtLaunchKernelWithHostArgsHook),
    ToGenericFunction(&aclrtMemcpyHook),
    ToGenericFunction(&aclrtBinaryLoadFromDataHook),
    ToGenericFunction(&aclrtBinaryGetFunctionHook),
    ToGenericFunction(&aclrtMallocHook),
    ToGenericFunction(&aclrtMemsetHook),
    ToGenericFunction(&aclrtFreeHook),
    ToGenericFunction(&aclrtCreateStreamHook),
    ToGenericFunction(&aclrtDestroyStreamHook),
    ToGenericFunction(&aclrtSetDeviceHook),
    ToGenericFunction(&aclrtResetDeviceHook),
    ToGenericFunction(&aclrtSynchronizeStreamHook),
    ToGenericFunction(&aclrtBinaryGetFunctionByEntryHook),
    ToGenericFunction(&aclrtLaunchKernelHook)};

} // namespace

extern "C" NPU_COMPUTE_EXPORT int32_t acltoolUploaderInit(MsprofRawDataCallback uploader)
{
    if (uploader == nullptr) {
        npu_compute::detail::DebugLog("tool_injection", "uploader registration failed: null uploader");
        return ACL_ERROR_INVALID_PARAM;
    }
    const int32_t result = MsprofRegisterDataCallback(kMsprofDataCallbackType, FunctionToAddress(uploader));
    npu_compute::detail::DebugLog(
        "tool_injection", "uploader registration: type=%u uploader=%p result=%d", kMsprofDataCallbackType,
        FunctionToAddress(uploader), result);
    return result;
}

extern "C" NPU_COMPUTE_EXPORT int32_t acltoolHookInit(void)
{
    std::lock_guard<std::mutex> initLock(g_initMutex);
    npu_compute::detail::DebugLog("tool_injection", "hook initialization started");
    if (g_hookInstalled) {
        npu_compute::detail::DebugLog("tool_injection", "hook initialization skipped: already installed");
        return ACL_SUCCESS;
    }

    for (int32_t value = 0; value < static_cast<int32_t>(ACL_RT_API_MAX); ++value) {
        const auto id = static_cast<aclrtApiId>(value);
        const size_t index = static_cast<size_t>(id);
        const char* name = FindRuntimeApiName(id);
        aclrtApiFunc hook = kRuntimeHooks[index];
        if (name == nullptr || hook == nullptr) {
            npu_compute::detail::DebugLog(
                "tool_injection", "hook initialization failed: invalid entry id=%d name=%s hook=%p",
                static_cast<int>(id), name == nullptr ? "unknown" : name, FunctionToAddress(hook));
            return ACL_ERROR_INTERNAL_ERROR;
        }
        aclrtApiFunc origin = nullptr;
        aclrtApiFunc current = nullptr;
        aclError ret = aclrtApiInjectionGetFunc(name, &origin, &current);
        npu_compute::detail::DebugLog(
            "tool_injection", "get runtime entry: name=%s id=%d result=%d origin=%p current=%p", name,
            static_cast<int>(id), static_cast<int>(ret), FunctionToAddress(origin), FunctionToAddress(current));
        if (ret != ACL_SUCCESS) {
            npu_compute::detail::DebugLog(
                "tool_injection", "hook initialization failed: get runtime entry name=%s result=%d", name,
                static_cast<int>(ret));
            return ret;
        }
        if (origin == nullptr || current == nullptr) {
            npu_compute::detail::DebugLog(
                "tool_injection", "hook initialization failed: empty runtime entry name=%s", name);
            return ACL_ERROR_UNINITIALIZE;
        }
        if (!SaveRuntimeEntry(id, FunctionToAddress(origin), FunctionToAddress(current))) {
            npu_compute::detail::DebugLog(
                "tool_injection", "hook initialization failed: save origin entry name=%s", name);
            return ACL_ERROR_INTERNAL_ERROR;
        }

        ret = aclrtApiInjectionSetFunc(name, hook);
        npu_compute::detail::DebugLog(
            "tool_injection", "set runtime hook: name=%s id=%d hook=%p result=%d", name, static_cast<int>(id),
            FunctionToAddress(hook), static_cast<int>(ret));
        if (ret != ACL_SUCCESS) {
            npu_compute::detail::DebugLog(
                "tool_injection", "hook initialization failed: set runtime hook name=%s result=%d", name,
                static_cast<int>(ret));
            return ret;
        }
        if (!SaveRuntimeEntry(id, FunctionToAddress(origin), FunctionToAddress(hook))) {
            npu_compute::detail::DebugLog(
                "tool_injection", "hook initialization failed: save active entry name=%s", name);
            return ACL_ERROR_INTERNAL_ERROR;
        }
    }

    g_hookInstalled = true;
    npu_compute::detail::DebugLog(
        "tool_injection", "hook initialization completed: installed=%d", static_cast<int>(ACL_RT_API_MAX));
    return ACL_SUCCESS;
}

extern "C" NPU_COMPUTE_EXPORT int32_t acltoolClearCallback(aclrtApiId id) { return RegisterCallback(id, nullptr); }

extern "C" NPU_COMPUTE_EXPORT void* acltoolGetOriginalRuntimeApi(aclrtApiId id)
{
    if (!IsValidApiId(id)) {
        npu_compute::detail::DebugLog(
            "tool_injection", "get original runtime api failed: invalid id=%d", static_cast<int>(id));
        return nullptr;
    }

    const char* name = FindRuntimeApiName(id);
    if (name == nullptr) {
        npu_compute::detail::DebugLog(
            "tool_injection", "get original runtime api failed: unknown id=%d", static_cast<int>(id));
        return nullptr;
    }

    aclrtApiFunc origin = nullptr;
    const aclError result = aclrtApiInjectionGetFunc(name, &origin, nullptr);
    npu_compute::detail::DebugLog(
        "tool_injection", "get original runtime api: name=%s id=%d result=%d origin=%p", name, static_cast<int>(id),
        static_cast<int>(result), FunctionToAddress(origin));
    if (result != ACL_SUCCESS) {
        return nullptr;
    }
    return FunctionToAddress(origin);
}

extern "C" NPU_COMPUTE_EXPORT int32_t acltoolRegisterAclrtSetDeviceCallbacks(aclrtSetDeviceFunc callback)
{
    return RegisterCallback(ACL_RT_API_aclrtSetDevice, ToGenericFunction(callback));
}

extern "C" NPU_COMPUTE_EXPORT int32_t acltoolRegisterAclrtResetDeviceCallbacks(aclrtResetDeviceFunc callback)
{
    return RegisterCallback(ACL_RT_API_aclrtResetDevice, ToGenericFunction(callback));
}

extern "C" NPU_COMPUTE_EXPORT int32_t acltoolRegisterAclrtCreateStreamCallbacks(aclrtCreateStreamFunc callback)
{
    return RegisterCallback(ACL_RT_API_aclrtCreateStream, ToGenericFunction(callback));
}

extern "C" NPU_COMPUTE_EXPORT int32_t acltoolRegisterAclrtDestroyStreamCallbacks(aclrtDestroyStreamFunc callback)
{
    return RegisterCallback(ACL_RT_API_aclrtDestroyStream, ToGenericFunction(callback));
}

extern "C" NPU_COMPUTE_EXPORT int32_t acltoolRegisterAclrtMallocCallbacks(aclrtMallocFunc callback)
{
    return RegisterCallback(ACL_RT_API_aclrtMalloc, ToGenericFunction(callback));
}

extern "C" NPU_COMPUTE_EXPORT int32_t acltoolRegisterAclrtFreeCallbacks(aclrtFreeFunc callback)
{
    return RegisterCallback(ACL_RT_API_aclrtFree, ToGenericFunction(callback));
}

extern "C" NPU_COMPUTE_EXPORT int32_t acltoolRegisterAclrtMemcpyCallbacks(aclrtMemcpyFunc callback)
{
    return RegisterCallback(ACL_RT_API_aclrtMemcpy, ToGenericFunction(callback));
}

extern "C" NPU_COMPUTE_EXPORT int32_t acltoolRegisterAclrtMemsetCallbacks(aclrtMemsetFunc callback)
{
    return RegisterCallback(ACL_RT_API_aclrtMemset, ToGenericFunction(callback));
}

extern "C" NPU_COMPUTE_EXPORT int32_t
acltoolRegisterAclrtSynchronizeStreamCallbacks(aclrtSynchronizeStreamFunc callback)
{
    return RegisterCallback(ACL_RT_API_aclrtSynchronizeStream, ToGenericFunction(callback));
}

extern "C" NPU_COMPUTE_EXPORT int32_t
acltoolRegisterAclrtBinaryLoadFromDataCallbacks(aclrtBinaryLoadFromDataFunc callback)
{
    return RegisterCallback(ACL_RT_API_aclrtBinaryLoadFromData, ToGenericFunction(callback));
}

extern "C" NPU_COMPUTE_EXPORT int32_t
acltoolRegisterAclrtBinaryGetFunctionCallbacks(aclrtBinaryGetFunctionFunc callback)
{
    return RegisterCallback(ACL_RT_API_aclrtBinaryGetFunction, ToGenericFunction(callback));
}

extern "C" NPU_COMPUTE_EXPORT int32_t
acltoolRegisterAclrtBinaryGetFunctionByEntryCallbacks(aclrtBinaryGetFunctionByEntryFunc callback)
{
    return RegisterCallback(ACL_RT_API_aclrtBinaryGetFunctionByEntry, ToGenericFunction(callback));
}

extern "C" NPU_COMPUTE_EXPORT int32_t
acltoolRegisterAclrtLaunchKernelWithHostArgsCallbacks(aclrtLaunchKernelWithHostArgsFunc callback)
{
    return RegisterCallback(ACL_RT_API_aclrtLaunchKernelWithHostArgs, ToGenericFunction(callback));
}

extern "C" NPU_COMPUTE_EXPORT int32_t acltoolRegisterAclrtLaunchKernelCallbacks(aclrtLaunchKernelFunc callback)
{
    return RegisterCallback(ACL_RT_API_aclrtLaunchKernel, ToGenericFunction(callback));
}
