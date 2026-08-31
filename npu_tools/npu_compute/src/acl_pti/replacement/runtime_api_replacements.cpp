/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "runtime_api_replacements.h"

#include "acl_pti/callback/dispatcher.h"
#include "acl_pti/profiling/replay_runtime.h"
#include "common/debug_log.h"
#include "injection/injection_hook.h"

#include "aclpti/aclpti_runtime_api.h"

#include <utility>

namespace npu_compute::aclpti::replacement {
namespace {

void LogOriginalFailure(const char* apiName, aclError status)
{
    npu_compute::detail::DebugLog(
        "aclpti", "error operation=original_call status=%d api=%s", status, apiName == nullptr ? "unknown" : apiName);
}

aclError MissingOriginalFunction(const char* apiName)
{
    npu_compute::detail::DebugLog(
        "aclpti", "error operation=original_lookup status=%d api=%s", ACL_ERROR_INTERNAL_ERROR,
        apiName == nullptr ? "unknown" : apiName);
    return ACL_ERROR_INTERNAL_ERROR;
}

aclError MapProfilingResult(aclptiResult status)
{
    if (status == ACLPTI_SUCCESS) {
        return ACL_SUCCESS;
    }
    return status == ACLPTI_ERROR_RESULT_UNRELIABLE ? ACL_ERROR_INTERNAL_ERROR : ACL_ERROR_PROFILING_FAILURE;
}

template <typename Params, typename Operation>
aclError InvokeRuntimeCallback(aclptiCallbackId cbid, Params& params, Operation&& operation)
{
    callback::GetDispatcher().Dispatch(ACLPTI_CB_DOMAIN_RUNTIME_API, cbid, ACLPTI_API_ENTER, &params, ACL_SUCCESS);
    const aclError result = operation();
    callback::GetDispatcher().Dispatch(ACLPTI_CB_DOMAIN_RUNTIME_API, cbid, ACLPTI_API_EXIT, &params, result);
    return result;
}

template <typename Function, typename Params, typename Operation>
aclError ForwardRuntimeApi(
    aclptiCallbackId cbid, aclrtApiId apiId, const char* apiName, Params& params, Operation&& operation)
{
    return InvokeRuntimeCallback(
        cbid, params, [apiId, apiName, operation = std::forward<Operation>(operation)]() mutable -> aclError {
            const auto function = reinterpret_cast<Function>(acltoolGetOriginalRuntimeApi(apiId));
            if (function == nullptr) {
                return MissingOriginalFunction(apiName);
            }
            const aclError result = operation(function);
            if (result != ACL_SUCCESS) {
                LogOriginalFailure(apiName, result);
            }
            return result;
        });
}

template <typename Function>
bool RegisterRuntimeReplacement(aclrtApiId apiId, std::int32_t (*registerCallback)(Function), Function replacement)
{
    const std::int32_t result = registerCallback(replacement);
    npu_compute::detail::DebugLog(
        "aclpti", "register runtime replacement apiId=%d result=%d", static_cast<int>(apiId), result);
    return result == 0;
}

aclError AclrtLaunchKernelWithHostArgsReplacement(
    aclrtFuncHandle funcHandle, std::uint32_t numBlocks, aclrtStream stream, aclrtLaunchKernelCfg* cfg, void* hostArgs,
    std::size_t argsSize, aclrtPlaceHolderInfo* placeHolderArray, std::size_t placeHolderNum)
{
    aclptiAclrtLaunchKernelWithHostArgsParams params{funcHandle, numBlocks, stream,           cfg,
                                                     hostArgs,   argsSize,  placeHolderArray, placeHolderNum};
    return InvokeRuntimeCallback(ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithHostArgs, params, [&params]() -> aclError {
        const auto launchFunction = reinterpret_cast<aclrtLaunchKernelWithHostArgsFunc>(
            acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtLaunchKernelWithHostArgs));
        if (launchFunction == nullptr) {
            return MissingOriginalFunction("aclrtLaunchKernelWithHostArgs");
        }
        const aclError result = launchFunction(
            params.funcHandle, params.numBlocks, params.stream, params.cfg, params.hostArgs, params.argsSize,
            params.placeHolderArray, params.placeHolderNum);
        if (result != ACL_SUCCESS) {
            LogOriginalFailure("aclrtLaunchKernelWithHostArgs", result);
            return result;
        }
        npu_compute::detail::DebugLog(
            "aclpti", "launch kernel with host args original succeeded blocks=%u argsSize=%zu", params.numBlocks,
            params.argsSize);
        const aclptiResult replayStatus = profiling::GetReplayRuntime().ReplayKernel(
            [launchFunction, &params]() -> aclError {
                return launchFunction(
                    params.funcHandle, params.numBlocks, params.stream, params.cfg, params.hostArgs, params.argsSize,
                    params.placeHolderArray, params.placeHolderNum);
            },
            params.stream);
        npu_compute::detail::DebugLog(
            "aclpti", "launch kernel with host args replay result=%d", static_cast<int>(replayStatus));
        return MapProfilingResult(replayStatus);
    });
}

aclError AclrtLaunchSIMTKernelWithHostArgsReplacement(
    void* func, dim3 gridDim, dim3 blockDim, std::size_t dynUbufSize, aclrtStream stream, aclrtLaunchKernelCfg* cfg,
    void* hostArgs, std::size_t argsSize, aclrtPlaceHolderInfo* placeHolderArray, std::size_t placeHolderNum)
{
    aclptiAclrtLaunchSIMTKernelWithHostArgsParams params{func, gridDim,  blockDim, dynUbufSize,      stream,
                                                         cfg,  hostArgs, argsSize, placeHolderArray, placeHolderNum};
    return InvokeRuntimeCallback(
        ACLPTI_RUNTIME_CBID_aclrtLaunchSIMTKernelWithHostArgs, params, [&params]() -> aclError {
            const auto launchFunction = reinterpret_cast<aclrtLaunchSIMTKernelWithHostArgsFunc>(
                acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtLaunchSIMTKernelWithHostArgs));
            if (launchFunction == nullptr) {
                return MissingOriginalFunction("aclrtLaunchSIMTKernelWithHostArgs");
            }
            const aclError result = launchFunction(
                params.func, params.gridDim, params.blockDim, params.dynUbufSize, params.stream, params.cfg,
                params.hostArgs, params.argsSize, params.placeHolderArray, params.placeHolderNum);
            if (result != ACL_SUCCESS) {
                LogOriginalFailure("aclrtLaunchSIMTKernelWithHostArgs", result);
                return result;
            }
            npu_compute::detail::DebugLog(
                "aclpti", "launch SIMT kernel with host args original succeeded argsSize=%zu", params.argsSize);
            const aclptiResult replayStatus = profiling::GetReplayRuntime().ReplayKernel(
                [launchFunction, &params]() -> aclError {
                    return launchFunction(
                        params.func, params.gridDim, params.blockDim, params.dynUbufSize, params.stream, params.cfg,
                        params.hostArgs, params.argsSize, params.placeHolderArray, params.placeHolderNum);
                },
                params.stream);
            npu_compute::detail::DebugLog(
                "aclpti", "launch SIMT kernel with host args replay result=%d", static_cast<int>(replayStatus));
            return MapProfilingResult(replayStatus);
        });
}

aclError AclrtLaunchKernelWithArgsArrayReplacement(
    void* func, std::uint32_t numBlocks, aclrtStream stream, aclrtLaunchKernelCfg* cfg, void** args)
{
    aclptiAclrtLaunchKernelWithArgsArrayParams params{func, numBlocks, stream, cfg, args};
    return InvokeRuntimeCallback(ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithArgsArray, params, [&params]() -> aclError {
        const auto launchFunction = reinterpret_cast<aclrtLaunchKernelWithArgsArrayFunc>(
            acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtLaunchKernelWithArgsArray));
        if (launchFunction == nullptr) {
            return MissingOriginalFunction("aclrtLaunchKernelWithArgsArray");
        }
        const aclError result = launchFunction(params.func, params.numBlocks, params.stream, params.cfg, params.args);
        if (result != ACL_SUCCESS) {
            LogOriginalFailure("aclrtLaunchKernelWithArgsArray", result);
            return result;
        }
        npu_compute::detail::DebugLog(
            "aclpti", "launch kernel with args array original succeeded blocks=%u", params.numBlocks);
        const aclptiResult replayStatus = profiling::GetReplayRuntime().ReplayKernel(
            [launchFunction, &params]() -> aclError {
                return launchFunction(params.func, params.numBlocks, params.stream, params.cfg, params.args);
            },
            params.stream);
        npu_compute::detail::DebugLog(
            "aclpti", "launch kernel with args array replay result=%d", static_cast<int>(replayStatus));
        return MapProfilingResult(replayStatus);
    });
}

aclError AclrtLaunchSIMTKernelWithArgsArrayReplacement(
    void* func, dim3 gridDim, dim3 blockDim, std::size_t dynUbufSize, aclrtStream stream, aclrtLaunchKernelCfg* cfg,
    void** args)
{
    aclptiAclrtLaunchSIMTKernelWithArgsArrayParams params{func, gridDim, blockDim, dynUbufSize, stream, cfg, args};
    return InvokeRuntimeCallback(
        ACLPTI_RUNTIME_CBID_aclrtLaunchSIMTKernelWithArgsArray, params, [&params]() -> aclError {
            const auto launchFunction = reinterpret_cast<aclrtLaunchSIMTKernelWithArgsArrayFunc>(
                acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtLaunchSIMTKernelWithArgsArray));
            if (launchFunction == nullptr) {
                return MissingOriginalFunction("aclrtLaunchSIMTKernelWithArgsArray");
            }
            const aclError result = launchFunction(
                params.func, params.gridDim, params.blockDim, params.dynUbufSize, params.stream, params.cfg,
                params.args);
            if (result != ACL_SUCCESS) {
                LogOriginalFailure("aclrtLaunchSIMTKernelWithArgsArray", result);
                return result;
            }
            npu_compute::detail::DebugLog("aclpti", "launch SIMT kernel with args array original succeeded");
            const aclptiResult replayStatus = profiling::GetReplayRuntime().ReplayKernel(
                [launchFunction, &params]() -> aclError {
                    return launchFunction(
                        params.func, params.gridDim, params.blockDim, params.dynUbufSize, params.stream, params.cfg,
                        params.args);
                },
                params.stream);
            npu_compute::detail::DebugLog(
                "aclpti", "launch SIMT kernel with args array replay result=%d", static_cast<int>(replayStatus));
            return MapProfilingResult(replayStatus);
        });
}

aclError AclrtMemcpyReplacement(
    void* destination, std::size_t destinationSize, const void* source, std::size_t count, aclrtMemcpyKind kind)
{
    aclptiAclrtMemcpyParams params{destination, destinationSize, source, count, kind};
    return InvokeRuntimeCallback(ACLPTI_RUNTIME_CBID_aclrtMemcpy, params, [&params]() -> aclError {
        const auto function = reinterpret_cast<aclrtMemcpyFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtMemcpy));
        if (function == nullptr) {
            return MissingOriginalFunction("aclrtMemcpy");
        }
        const aclError result = function(params.dst, params.destMax, params.src, params.count, params.kind);
        if (result != ACL_SUCCESS) {
            LogOriginalFailure("aclrtMemcpy", result);
            return result;
        }
        const aclptiResult mirrorStatus = profiling::GetReplayRuntime().MirrorMemcpy(
            params.dst, params.destMax, params.src, params.count, params.kind);
        return MapProfilingResult(mirrorStatus);
    });
}

aclError AclrtBinaryLoadFromDataReplacement(
    const void* data, std::size_t length, const aclrtBinaryLoadOptions* options, aclrtBinHandle* binHandle)
{
    aclptiAclrtBinaryLoadFromDataParams params{data, length, options, binHandle};
    return ForwardRuntimeApi<aclrtBinaryLoadFromDataFunc>(
        ACLPTI_RUNTIME_CBID_aclrtBinaryLoadFromData, ACL_RT_API_aclrtBinaryLoadFromData, "aclrtBinaryLoadFromData",
        params, [&params](aclrtBinaryLoadFromDataFunc function) {
            return function(params.data, params.length, params.options, params.binHandle);
        });
}

aclError AclrtBinaryGetFunctionReplacement(
    const aclrtBinHandle binHandle, const char* kernelName, aclrtFuncHandle* funcHandle)
{
    aclptiAclrtBinaryGetFunctionParams params{binHandle, kernelName, funcHandle};
    return ForwardRuntimeApi<aclrtBinaryGetFunctionFunc>(
        ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunction, ACL_RT_API_aclrtBinaryGetFunction, "aclrtBinaryGetFunction", params,
        [&params](aclrtBinaryGetFunctionFunc function) {
            return function(params.binHandle, params.kernelName, params.funcHandle);
        });
}

aclError AclrtMallocReplacement(void** devPtr, std::size_t size, aclrtMemMallocPolicy policy)
{
    aclptiAclrtMallocParams params{devPtr, size, policy};
    return InvokeRuntimeCallback(ACLPTI_RUNTIME_CBID_aclrtMalloc, params, [&params]() -> aclError {
        const auto mallocFunction =
            reinterpret_cast<aclrtMallocFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtMalloc));
        if (mallocFunction == nullptr) {
            return MissingOriginalFunction("aclrtMalloc");
        }
        const aclError result = mallocFunction(params.devPtr, params.size, params.policy);
        if (result != ACL_SUCCESS) {
            LogOriginalFailure("aclrtMalloc", result);
            return result;
        }
        const aclptiResult mirrorStatus =
            profiling::GetReplayRuntime().MirrorMalloc(params.devPtr, params.size, params.policy);
        return MapProfilingResult(mirrorStatus);
    });
}

aclError AclrtMallocAlign32Replacement(void** devPtr, std::size_t size, aclrtMemMallocPolicy policy)
{
    aclptiAclrtMallocAlign32Params params{devPtr, size, policy};
    return InvokeRuntimeCallback(ACLPTI_RUNTIME_CBID_aclrtMallocAlign32, params, [&params]() -> aclError {
        const auto mallocFunction =
            reinterpret_cast<aclrtMallocAlign32Func>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtMallocAlign32));
        if (mallocFunction == nullptr) {
            return MissingOriginalFunction("aclrtMallocAlign32");
        }
        const aclError result = mallocFunction(params.devPtr, params.size, params.policy);
        if (result != ACL_SUCCESS) {
            LogOriginalFailure("aclrtMallocAlign32", result);
            return result;
        }
        const aclptiResult mirrorStatus =
            profiling::GetReplayRuntime().MirrorMalloc(params.devPtr, params.size, params.policy);
        return MapProfilingResult(mirrorStatus);
    });
}

aclError AclrtMemsetReplacement(void* devPtr, std::size_t maxCount, std::int32_t value, std::size_t count)
{
    aclptiAclrtMemsetParams params{devPtr, maxCount, value, count};
    return InvokeRuntimeCallback(ACLPTI_RUNTIME_CBID_aclrtMemset, params, [&params]() -> aclError {
        const auto function = reinterpret_cast<aclrtMemsetFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtMemset));
        if (function == nullptr) {
            return MissingOriginalFunction("aclrtMemset");
        }
        const aclError result = function(params.devPtr, params.maxCount, params.value, params.count);
        if (result != ACL_SUCCESS) {
            LogOriginalFailure("aclrtMemset", result);
            return result;
        }
        const aclptiResult mirrorStatus =
            profiling::GetReplayRuntime().MirrorMemset(params.devPtr, params.maxCount, params.value, params.count);
        return MapProfilingResult(mirrorStatus);
    });
}

aclError AclrtFreeReplacement(void* devPtr)
{
    aclptiAclrtFreeParams params{devPtr};
    return InvokeRuntimeCallback(ACLPTI_RUNTIME_CBID_aclrtFree, params, [&params]() -> aclError {
        const auto function = reinterpret_cast<aclrtFreeFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtFree));
        if (function == nullptr) {
            return MissingOriginalFunction("aclrtFree");
        }
        const aclError result = function(params.devPtr);
        if (result != ACL_SUCCESS) {
            LogOriginalFailure("aclrtFree", result);
            return result;
        }
        const aclptiResult mirrorStatus = profiling::GetReplayRuntime().MirrorFree(params.devPtr);
        return MapProfilingResult(mirrorStatus);
    });
}

aclError AclrtCreateStreamReplacement(aclrtStream* stream)
{
    aclptiAclrtCreateStreamParams params{stream};
    return ForwardRuntimeApi<aclrtCreateStreamFunc>(
        ACLPTI_RUNTIME_CBID_aclrtCreateStream, ACL_RT_API_aclrtCreateStream, "aclrtCreateStream", params,
        [&params](aclrtCreateStreamFunc function) { return function(params.stream); });
}

aclError AclrtDestroyStreamReplacement(aclrtStream stream)
{
    aclptiAclrtDestroyStreamParams params{stream};
    return ForwardRuntimeApi<aclrtDestroyStreamFunc>(
        ACLPTI_RUNTIME_CBID_aclrtDestroyStream, ACL_RT_API_aclrtDestroyStream, "aclrtDestroyStream", params,
        [&params](aclrtDestroyStreamFunc function) { return function(params.stream); });
}

aclError AclrtSetDeviceReplacement(std::int32_t deviceId)
{
    aclptiAclrtSetDeviceParams params{deviceId};
    return ForwardRuntimeApi<aclrtSetDeviceFunc>(
        ACLPTI_RUNTIME_CBID_aclrtSetDevice, ACL_RT_API_aclrtSetDevice, "aclrtSetDevice", params,
        [&params](aclrtSetDeviceFunc function) { return function(params.deviceId); });
}

aclError AclrtResetDeviceReplacement(std::int32_t deviceId)
{
    aclptiAclrtResetDeviceParams params{deviceId};
    return ForwardRuntimeApi<aclrtResetDeviceFunc>(
        ACLPTI_RUNTIME_CBID_aclrtResetDevice, ACL_RT_API_aclrtResetDevice, "aclrtResetDevice", params,
        [&params](aclrtResetDeviceFunc function) { return function(params.deviceId); });
}

aclError AclrtSynchronizeStreamReplacement(aclrtStream stream)
{
    aclptiAclrtSynchronizeStreamParams params{stream};
    return ForwardRuntimeApi<aclrtSynchronizeStreamFunc>(
        ACLPTI_RUNTIME_CBID_aclrtSynchronizeStream, ACL_RT_API_aclrtSynchronizeStream, "aclrtSynchronizeStream", params,
        [&params](aclrtSynchronizeStreamFunc function) { return function(params.stream); });
}

aclError AclrtBinaryGetFunctionByEntryReplacement(
    aclrtBinHandle binHandle, std::uint64_t funcEntry, aclrtFuncHandle* funcHandle)
{
    aclptiAclrtBinaryGetFunctionByEntryParams params{binHandle, funcEntry, funcHandle};
    return ForwardRuntimeApi<aclrtBinaryGetFunctionByEntryFunc>(
        ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunctionByEntry, ACL_RT_API_aclrtBinaryGetFunctionByEntry,
        "aclrtBinaryGetFunctionByEntry", params, [&params](aclrtBinaryGetFunctionByEntryFunc function) {
            return function(params.binHandle, params.funcEntry, params.funcHandle);
        });
}

aclError AclrtLaunchKernelReplacement(
    aclrtFuncHandle function, std::uint32_t blockCount, const void* argsData, std::size_t argsSize, aclrtStream stream)
{
    aclptiAclrtLaunchKernelParams params{function, blockCount, argsData, argsSize, stream};
    return InvokeRuntimeCallback(ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, params, [&params]() -> aclError {
        const auto launchFunction =
            reinterpret_cast<aclrtLaunchKernelFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtLaunchKernel));
        if (launchFunction == nullptr) {
            return MissingOriginalFunction("aclrtLaunchKernel");
        }
        const aclError result =
            launchFunction(params.funcHandle, params.numBlocks, params.argsData, params.argsSize, params.stream);
        if (result != ACL_SUCCESS) {
            LogOriginalFailure("aclrtLaunchKernel", result);
            return result;
        }
        npu_compute::detail::DebugLog(
            "aclpti", "launch kernel original succeeded blocks=%u argsSize=%zu", params.numBlocks, params.argsSize);
        const aclptiResult replayStatus = profiling::GetReplayRuntime().ReplayKernel(
            [launchFunction, &params]() -> aclError {
                return launchFunction(
                    params.funcHandle, params.numBlocks, params.argsData, params.argsSize, params.stream);
            },
            params.stream);
        npu_compute::detail::DebugLog("aclpti", "launch kernel replay result=%d", static_cast<int>(replayStatus));
        return MapProfilingResult(replayStatus);
    });
}

aclError AclrtGetFuncBySymbolReplacement(const void* symbol, aclrtFuncHandle* funcHandle)
{
    aclptiAclrtGetFuncBySymbolParams params{symbol, funcHandle};
    return ForwardRuntimeApi<aclrtGetFuncBySymbolFunc>(
        ACLPTI_RUNTIME_CBID_aclrtGetFuncBySymbol, ACL_RT_API_aclrtGetFuncBySymbol, "aclrtGetFuncBySymbol", params,
        [&params](aclrtGetFuncBySymbolFunc function) { return function(params.symbol, params.funcHandle); });
}

aclError AclrtBinaryUnLoadReplacement(aclrtBinHandle binHandle)
{
    aclptiAclrtBinaryUnLoadParams params{binHandle};
    return ForwardRuntimeApi<aclrtBinaryUnLoadFunc>(
        ACLPTI_RUNTIME_CBID_aclrtBinaryUnLoad, ACL_RT_API_aclrtBinaryUnLoad, "aclrtBinaryUnLoad", params,
        [&params](aclrtBinaryUnLoadFunc function) { return function(params.binHandle); });
}

} // namespace

bool RegisterRuntimeApiReplacements()
{
    const bool registered =
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtLaunchKernelWithHostArgs, acltoolRegisterAclrtLaunchKernelWithHostArgsCallbacks,
            &AclrtLaunchKernelWithHostArgsReplacement) &&
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtLaunchSIMTKernelWithHostArgs, acltoolRegisterAclrtLaunchSIMTKernelWithHostArgsCallbacks,
            &AclrtLaunchSIMTKernelWithHostArgsReplacement) &&
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtLaunchKernelWithArgsArray, acltoolRegisterAclrtLaunchKernelWithArgsArrayCallbacks,
            &AclrtLaunchKernelWithArgsArrayReplacement) &&
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtLaunchSIMTKernelWithArgsArray, acltoolRegisterAclrtLaunchSIMTKernelWithArgsArrayCallbacks,
            &AclrtLaunchSIMTKernelWithArgsArrayReplacement) &&
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtMemcpy, acltoolRegisterAclrtMemcpyCallbacks, &AclrtMemcpyReplacement) &&
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtBinaryLoadFromData, acltoolRegisterAclrtBinaryLoadFromDataCallbacks,
            &AclrtBinaryLoadFromDataReplacement) &&
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtBinaryGetFunction, acltoolRegisterAclrtBinaryGetFunctionCallbacks,
            &AclrtBinaryGetFunctionReplacement) &&
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtMalloc, acltoolRegisterAclrtMallocCallbacks, &AclrtMallocReplacement) &&
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtMallocAlign32, acltoolRegisterAclrtMallocAlign32Callbacks,
            &AclrtMallocAlign32Replacement) &&
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtMemset, acltoolRegisterAclrtMemsetCallbacks, &AclrtMemsetReplacement) &&
        RegisterRuntimeReplacement(ACL_RT_API_aclrtFree, acltoolRegisterAclrtFreeCallbacks, &AclrtFreeReplacement) &&
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtCreateStream, acltoolRegisterAclrtCreateStreamCallbacks, &AclrtCreateStreamReplacement) &&
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtDestroyStream, acltoolRegisterAclrtDestroyStreamCallbacks,
            &AclrtDestroyStreamReplacement) &&
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtSetDevice, acltoolRegisterAclrtSetDeviceCallbacks, &AclrtSetDeviceReplacement) &&
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtResetDevice, acltoolRegisterAclrtResetDeviceCallbacks, &AclrtResetDeviceReplacement) &&
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtSynchronizeStream, acltoolRegisterAclrtSynchronizeStreamCallbacks,
            &AclrtSynchronizeStreamReplacement) &&
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtBinaryGetFunctionByEntry, acltoolRegisterAclrtBinaryGetFunctionByEntryCallbacks,
            &AclrtBinaryGetFunctionByEntryReplacement) &&
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtLaunchKernel, acltoolRegisterAclrtLaunchKernelCallbacks, &AclrtLaunchKernelReplacement) &&
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtGetFuncBySymbol, acltoolRegisterAclrtGetFuncBySymbolCallbacks,
            &AclrtGetFuncBySymbolReplacement) &&
        RegisterRuntimeReplacement(
            ACL_RT_API_aclrtBinaryUnLoad, acltoolRegisterAclrtBinaryUnLoadCallbacks, &AclrtBinaryUnLoadReplacement);
    if (!registered) {
        npu_compute::detail::DebugLog("aclpti", "runtime replacement registration failed");
        return false;
    }
    npu_compute::detail::DebugLog("aclpti", "runtime replacement registration complete");
    return true;
}

} // namespace npu_compute::aclpti::replacement
