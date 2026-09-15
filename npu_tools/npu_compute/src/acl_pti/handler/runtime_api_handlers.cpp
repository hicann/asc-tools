/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "runtime_api_handlers.h"

#include "acl_pti/callback/dispatcher.h"
#include "acl_pti/profiling/replay_runtime.h"
#include "common/debug_log.h"
#include "injection/injection_hook.h"

#include "aclpti/aclpti_runtime_api.h"

#include <utility>

namespace aclpti::handler {
namespace {

aclError MissingOriginalFunction(const char* apiName)
{
    npucompute::detail::DebugLog(
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

template <typename Function, typename Params, typename Operation>
aclError InvokeRuntimeCallback(
    aclptiCallbackId cbid, aclrtApiId apiId, const char* apiName, Params& params, Operation&& operation)
{
    callback::GetDispatcher().Dispatch(ACLPTI_CB_DOMAIN_RUNTIME_API, cbid, ACLPTI_API_ENTER, &params, ACL_SUCCESS);
    const auto original = reinterpret_cast<Function>(acltoolGetOriginalRuntimeApi(apiId));
    aclError result;
    if (original == nullptr) {
        result = MissingOriginalFunction(apiName);
    } else {
        result = std::forward<Operation>(operation)(original);
        if (result != ACL_SUCCESS) {
            npucompute::detail::DebugLog("aclpti", "error operation=runtime_call status=%d api=%s", result, apiName);
        }
    }
    if (result != ACL_SUCCESS) {
        // Operation-local resources are released before shutdown and EXIT callbacks.
        (void)profiling::GetReplayRuntime().StopProfiling();
    }
    callback::GetDispatcher().Dispatch(ACLPTI_CB_DOMAIN_RUNTIME_API, cbid, ACLPTI_API_EXIT, &params, result);
    return result;
}

template <typename Function>
bool RegisterRuntimeHandler(aclrtApiId apiId, std::int32_t (*registerCallback)(Function), Function handler)
{
    const std::int32_t result = registerCallback(handler);
    npucompute::detail::DebugLog(
        "aclpti", "register runtime handler apiId=%d result=%d", static_cast<int>(apiId), result);
    return result == 0;
}

// Share the original launch followed by profiling replay across launch APIs.
// The member pointer reads the function after ENTER callbacks have updated params.
template <typename Function, typename Params, typename Launch>
aclError InvokeLaunch(
    aclptiCallbackId cbid, aclrtApiId apiId, const char* apiName, Params& params,
    aclrtFuncHandle Params::*functionMember, Launch launch)
{
    return InvokeRuntimeCallback<Function>(cbid, apiId, apiName, params, [&](Function original) -> aclError {
        const auto function = params.*functionMember;
        const auto invoke = [&](aclrtFuncHandle selectedFunction) { return launch(original, selectedFunction); };
        const aclError result = invoke(function);
        if (result != ACL_SUCCESS) {
            return result;
        }
        const auto status = profiling::GetReplayRuntime().ReplayKernel(function, invoke, params.stream);
        npucompute::detail::DebugLog("aclpti", "launch replay api=%s result=%d", apiName, static_cast<int>(status));
        return MapProfilingResult(status);
    });
}

aclError AclrtLaunchKernelWithHostArgsHandler(
    aclrtFuncHandle funcHandle, std::uint32_t numBlocks, aclrtStream stream, aclrtLaunchKernelCfg* cfg, void* hostArgs,
    std::size_t argsSize, aclrtPlaceHolderInfo* placeHolderArray, std::size_t placeHolderNum)
{
    aclptiAclrtLaunchKernelWithHostArgsParams params{funcHandle, numBlocks, stream,           cfg,
                                                     hostArgs,   argsSize,  placeHolderArray, placeHolderNum};
    return InvokeLaunch<aclrtLaunchKernelWithHostArgsFunc>(
        ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithHostArgs, ACL_RT_API_aclrtLaunchKernelWithHostArgs,
        "aclrtLaunchKernelWithHostArgs", params, &aclptiAclrtLaunchKernelWithHostArgsParams::funcHandle,
        [&params](auto launch, aclrtFuncHandle function) {
            return launch(
                function, params.numBlocks, params.stream, params.cfg, params.hostArgs, params.argsSize,
                params.placeHolderArray, params.placeHolderNum);
        });
}

aclError AclrtLaunchSIMTKernelWithHostArgsHandler(
    void* func, dim3 gridDim, dim3 blockDim, std::size_t dynUbufSize, aclrtStream stream, aclrtLaunchKernelCfg* cfg,
    void* hostArgs, std::size_t argsSize, aclrtPlaceHolderInfo* placeHolderArray, std::size_t placeHolderNum)
{
    aclptiAclrtLaunchSIMTKernelWithHostArgsParams params{func, gridDim,  blockDim, dynUbufSize,      stream,
                                                         cfg,  hostArgs, argsSize, placeHolderArray, placeHolderNum};
    return InvokeLaunch<aclrtLaunchSIMTKernelWithHostArgsFunc>(
        ACLPTI_RUNTIME_CBID_aclrtLaunchSIMTKernelWithHostArgs, ACL_RT_API_aclrtLaunchSIMTKernelWithHostArgs,
        "aclrtLaunchSIMTKernelWithHostArgs", params, &aclptiAclrtLaunchSIMTKernelWithHostArgsParams::func,
        [&params](auto launch, aclrtFuncHandle function) {
            return launch(
                function, params.gridDim, params.blockDim, params.dynUbufSize, params.stream, params.cfg,
                params.hostArgs, params.argsSize, params.placeHolderArray, params.placeHolderNum);
        });
}

aclError AclrtLaunchKernelWithArgsArrayHandler(
    void* func, std::uint32_t numBlocks, aclrtStream stream, aclrtLaunchKernelCfg* cfg, void** args)
{
    aclptiAclrtLaunchKernelWithArgsArrayParams params{func, numBlocks, stream, cfg, args};
    return InvokeLaunch<aclrtLaunchKernelWithArgsArrayFunc>(
        ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithArgsArray, ACL_RT_API_aclrtLaunchKernelWithArgsArray,
        "aclrtLaunchKernelWithArgsArray", params, &aclptiAclrtLaunchKernelWithArgsArrayParams::func,
        [&params](auto launch, aclrtFuncHandle function) {
            return launch(function, params.numBlocks, params.stream, params.cfg, params.args);
        });
}

aclError AclrtLaunchSIMTKernelWithArgsArrayHandler(
    void* func, dim3 gridDim, dim3 blockDim, std::size_t dynUbufSize, aclrtStream stream, aclrtLaunchKernelCfg* cfg,
    void** args)
{
    aclptiAclrtLaunchSIMTKernelWithArgsArrayParams params{func, gridDim, blockDim, dynUbufSize, stream, cfg, args};
    return InvokeLaunch<aclrtLaunchSIMTKernelWithArgsArrayFunc>(
        ACLPTI_RUNTIME_CBID_aclrtLaunchSIMTKernelWithArgsArray, ACL_RT_API_aclrtLaunchSIMTKernelWithArgsArray,
        "aclrtLaunchSIMTKernelWithArgsArray", params, &aclptiAclrtLaunchSIMTKernelWithArgsArrayParams::func,
        [&params](auto launch, aclrtFuncHandle function) {
            return launch(
                function, params.gridDim, params.blockDim, params.dynUbufSize, params.stream, params.cfg, params.args);
        });
}

aclError AclrtMemcpyHandler(
    void* destination, std::size_t destinationSize, const void* source, std::size_t count, aclrtMemcpyKind kind)
{
    aclptiAclrtMemcpyParams params{destination, destinationSize, source, count, kind};
    return InvokeRuntimeCallback<aclrtMemcpyFunc>(
        ACLPTI_RUNTIME_CBID_aclrtMemcpy, ACL_RT_API_aclrtMemcpy, "aclrtMemcpy", params,
        [&params](aclrtMemcpyFunc function) -> aclError {
            const aclError result = function(params.dst, params.destMax, params.src, params.count, params.kind);
            if (result != ACL_SUCCESS) {
                return result;
            }
            const aclptiResult mirrorStatus = profiling::GetReplayRuntime().MirrorMemcpy(
                params.dst, params.destMax, params.src, params.count, params.kind);
            return MapProfilingResult(mirrorStatus);
        });
}

aclError AclrtBinaryLoadFromDataHandler(
    const void* data, std::size_t length, const aclrtBinaryLoadOptions* options, aclrtBinHandle* binHandle)
{
    aclptiAclrtBinaryLoadFromDataParams params{data, length, options, binHandle};
    return InvokeRuntimeCallback<aclrtBinaryLoadFromDataFunc>(
        ACLPTI_RUNTIME_CBID_aclrtBinaryLoadFromData, ACL_RT_API_aclrtBinaryLoadFromData, "aclrtBinaryLoadFromData",
        params, [&params](aclrtBinaryLoadFromDataFunc original) -> aclError {
            const aclError status = original(params.data, params.length, params.options, params.binHandle);
            if (status != ACL_SUCCESS) {
                return status;
            }
            return MapProfilingResult(profiling::GetReplayRuntime().RegisterBinary(
                params.data, params.length, params.options, *params.binHandle));
        });
}

aclError AclrtBinaryGetFunctionHandler(
    const aclrtBinHandle binHandle, const char* kernelName, aclrtFuncHandle* funcHandle)
{
    aclptiAclrtBinaryGetFunctionParams params{binHandle, kernelName, funcHandle};
    return InvokeRuntimeCallback<aclrtBinaryGetFunctionFunc>(
        ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunction, ACL_RT_API_aclrtBinaryGetFunction, "aclrtBinaryGetFunction", params,
        [&params](aclrtBinaryGetFunctionFunc original) -> aclError {
            const aclError status = original(params.binHandle, params.kernelName, params.funcHandle);
            if (status != ACL_SUCCESS) {
                return status;
            }
            return MapProfilingResult(profiling::GetReplayRuntime().RegisterBinaryFunction(
                params.binHandle, params.kernelName, *params.funcHandle));
        });
}

aclError AclrtMallocHandler(void** devPtr, std::size_t size, aclrtMemMallocPolicy policy)
{
    aclptiAclrtMallocParams params{devPtr, size, policy};
    return InvokeRuntimeCallback<aclrtMallocFunc>(
        ACLPTI_RUNTIME_CBID_aclrtMalloc, ACL_RT_API_aclrtMalloc, "aclrtMalloc", params,
        [&params](aclrtMallocFunc mallocFunction) -> aclError {
            const aclError result = mallocFunction(params.devPtr, params.size, params.policy);
            if (result != ACL_SUCCESS) {
                return result;
            }
            const aclptiResult mirrorStatus =
                profiling::GetReplayRuntime().MirrorMalloc(params.devPtr, params.size, params.policy);
            return MapProfilingResult(mirrorStatus);
        });
}

aclError AclrtMallocAlign32Handler(void** devPtr, std::size_t size, aclrtMemMallocPolicy policy)
{
    aclptiAclrtMallocAlign32Params params{devPtr, size, policy};
    return InvokeRuntimeCallback<aclrtMallocAlign32Func>(
        ACLPTI_RUNTIME_CBID_aclrtMallocAlign32, ACL_RT_API_aclrtMallocAlign32, "aclrtMallocAlign32", params,
        [&params](aclrtMallocAlign32Func mallocFunction) -> aclError {
            const aclError result = mallocFunction(params.devPtr, params.size, params.policy);
            if (result != ACL_SUCCESS) {
                return result;
            }
            const aclptiResult mirrorStatus =
                profiling::GetReplayRuntime().MirrorMalloc(params.devPtr, params.size, params.policy);
            return MapProfilingResult(mirrorStatus);
        });
}

aclError AclrtMemsetHandler(void* devPtr, std::size_t maxCount, std::int32_t value, std::size_t count)
{
    aclptiAclrtMemsetParams params{devPtr, maxCount, value, count};
    return InvokeRuntimeCallback<aclrtMemsetFunc>(
        ACLPTI_RUNTIME_CBID_aclrtMemset, ACL_RT_API_aclrtMemset, "aclrtMemset", params,
        [&params](aclrtMemsetFunc function) -> aclError {
            const aclError result = function(params.devPtr, params.maxCount, params.value, params.count);
            if (result != ACL_SUCCESS) {
                return result;
            }
            const aclptiResult mirrorStatus =
                profiling::GetReplayRuntime().MirrorMemset(params.devPtr, params.maxCount, params.value, params.count);
            return MapProfilingResult(mirrorStatus);
        });
}

aclError AclrtFreeHandler(void* devPtr)
{
    aclptiAclrtFreeParams params{devPtr};
    return InvokeRuntimeCallback<aclrtFreeFunc>(
        ACLPTI_RUNTIME_CBID_aclrtFree, ACL_RT_API_aclrtFree, "aclrtFree", params,
        [&params](aclrtFreeFunc function) -> aclError {
            const aclError result = function(params.devPtr);
            if (result != ACL_SUCCESS) {
                return result;
            }
            const aclptiResult mirrorStatus = profiling::GetReplayRuntime().MirrorFree(params.devPtr);
            return MapProfilingResult(mirrorStatus);
        });
}

aclError AclrtCreateStreamHandler(aclrtStream* stream)
{
    aclptiAclrtCreateStreamParams params{stream};
    return InvokeRuntimeCallback<aclrtCreateStreamFunc>(
        ACLPTI_RUNTIME_CBID_aclrtCreateStream, ACL_RT_API_aclrtCreateStream, "aclrtCreateStream", params,
        [&params](aclrtCreateStreamFunc function) { return function(params.stream); });
}

aclError AclrtDestroyStreamHandler(aclrtStream stream)
{
    aclptiAclrtDestroyStreamParams params{stream};
    return InvokeRuntimeCallback<aclrtDestroyStreamFunc>(
        ACLPTI_RUNTIME_CBID_aclrtDestroyStream, ACL_RT_API_aclrtDestroyStream, "aclrtDestroyStream", params,
        [&params](aclrtDestroyStreamFunc function) { return function(params.stream); });
}

aclError AclrtSetDeviceHandler(std::int32_t deviceId)
{
    aclptiAclrtSetDeviceParams params{deviceId};
    return InvokeRuntimeCallback<aclrtSetDeviceFunc>(
        ACLPTI_RUNTIME_CBID_aclrtSetDevice, ACL_RT_API_aclrtSetDevice, "aclrtSetDevice", params,
        [&params](aclrtSetDeviceFunc function) { return function(params.deviceId); });
}

aclError AclrtResetDeviceHandler(std::int32_t deviceId)
{
    aclptiAclrtResetDeviceParams params{deviceId};
    return InvokeRuntimeCallback<aclrtResetDeviceFunc>(
        ACLPTI_RUNTIME_CBID_aclrtResetDevice, ACL_RT_API_aclrtResetDevice, "aclrtResetDevice", params,
        [&params](aclrtResetDeviceFunc function) { return function(params.deviceId); });
}

aclError AclrtSynchronizeStreamHandler(aclrtStream stream)
{
    aclptiAclrtSynchronizeStreamParams params{stream};
    return InvokeRuntimeCallback<aclrtSynchronizeStreamFunc>(
        ACLPTI_RUNTIME_CBID_aclrtSynchronizeStream, ACL_RT_API_aclrtSynchronizeStream, "aclrtSynchronizeStream", params,
        [&params](aclrtSynchronizeStreamFunc function) { return function(params.stream); });
}

aclError AclrtBinaryGetFunctionByEntryHandler(
    aclrtBinHandle binHandle, std::uint64_t funcEntry, aclrtFuncHandle* funcHandle)
{
    aclptiAclrtBinaryGetFunctionByEntryParams params{binHandle, funcEntry, funcHandle};
    return InvokeRuntimeCallback<aclrtBinaryGetFunctionByEntryFunc>(
        ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunctionByEntry, ACL_RT_API_aclrtBinaryGetFunctionByEntry,
        "aclrtBinaryGetFunctionByEntry", params, [&params](aclrtBinaryGetFunctionByEntryFunc original) -> aclError {
            const aclError status = original(params.binHandle, params.funcEntry, params.funcHandle);
            if (status != ACL_SUCCESS) {
                return status;
            }
            return MapProfilingResult(profiling::GetReplayRuntime().RegisterBinaryFunction(
                params.binHandle, params.funcEntry, *params.funcHandle));
        });
}

aclError AclrtLaunchKernelHandler(
    aclrtFuncHandle function, std::uint32_t blockCount, const void* argsData, std::size_t argsSize, aclrtStream stream)
{
    aclptiAclrtLaunchKernelParams params{function, blockCount, argsData, argsSize, stream};
    return InvokeLaunch<aclrtLaunchKernelFunc>(
        ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, ACL_RT_API_aclrtLaunchKernel, "aclrtLaunchKernel", params,
        &aclptiAclrtLaunchKernelParams::funcHandle, [&params](auto launch, aclrtFuncHandle function) {
            return launch(function, params.numBlocks, params.argsData, params.argsSize, params.stream);
        });
}

aclError AclrtGetFuncBySymbolHandler(const void* symbol, aclrtFuncHandle* funcHandle)
{
    aclptiAclrtGetFuncBySymbolParams params{symbol, funcHandle};
    return InvokeRuntimeCallback<aclrtGetFuncBySymbolFunc>(
        ACLPTI_RUNTIME_CBID_aclrtGetFuncBySymbol, ACL_RT_API_aclrtGetFuncBySymbol, "aclrtGetFuncBySymbol", params,
        [&params](aclrtGetFuncBySymbolFunc original) -> aclError {
            const aclError status = original(params.symbol, params.funcHandle);
            if (status != ACL_SUCCESS) {
                return status;
            }
            return MapProfilingResult(profiling::GetReplayRuntime().RegisterSymbolFunction(*params.funcHandle));
        });
}

aclError AclrtBinaryUnLoadHandler(aclrtBinHandle binHandle)
{
    aclptiAclrtBinaryUnLoadParams params{binHandle};
    return InvokeRuntimeCallback<aclrtBinaryUnLoadFunc>(
        ACLPTI_RUNTIME_CBID_aclrtBinaryUnLoad, ACL_RT_API_aclrtBinaryUnLoad, "aclrtBinaryUnLoad", params,
        [&params](aclrtBinaryUnLoadFunc original) -> aclError {
            auto& runtime = profiling::GetReplayRuntime();
            profiling::BinaryRegistry::UnloadContext context;
            const auto prepareStatus = runtime.PrepareBinaryUnload(params.binHandle, context);
            if (prepareStatus != ACLPTI_SUCCESS) {
                return MapProfilingResult(prepareStatus);
            }
            const aclError status = original(params.binHandle);
            if (status != ACL_SUCCESS) {
                return status; // The context releases the lock and retains the record for retry.
            }
            return MapProfilingResult(runtime.CompleteBinaryUnload(context));
        });
}

} // namespace

bool RegisterRuntimeApiHandlers()
{
    const bool registered =
        RegisterRuntimeHandler(
            ACL_RT_API_aclrtLaunchKernelWithHostArgs, acltoolRegisterAclrtLaunchKernelWithHostArgsCallbacks,
            &AclrtLaunchKernelWithHostArgsHandler) &&
        RegisterRuntimeHandler(
            ACL_RT_API_aclrtLaunchSIMTKernelWithHostArgs, acltoolRegisterAclrtLaunchSIMTKernelWithHostArgsCallbacks,
            &AclrtLaunchSIMTKernelWithHostArgsHandler) &&
        RegisterRuntimeHandler(
            ACL_RT_API_aclrtLaunchKernelWithArgsArray, acltoolRegisterAclrtLaunchKernelWithArgsArrayCallbacks,
            &AclrtLaunchKernelWithArgsArrayHandler) &&
        RegisterRuntimeHandler(
            ACL_RT_API_aclrtLaunchSIMTKernelWithArgsArray, acltoolRegisterAclrtLaunchSIMTKernelWithArgsArrayCallbacks,
            &AclrtLaunchSIMTKernelWithArgsArrayHandler) &&
        RegisterRuntimeHandler(ACL_RT_API_aclrtMemcpy, acltoolRegisterAclrtMemcpyCallbacks, &AclrtMemcpyHandler) &&
        RegisterRuntimeHandler(
            ACL_RT_API_aclrtBinaryLoadFromData, acltoolRegisterAclrtBinaryLoadFromDataCallbacks,
            &AclrtBinaryLoadFromDataHandler) &&
        RegisterRuntimeHandler(
            ACL_RT_API_aclrtBinaryGetFunction, acltoolRegisterAclrtBinaryGetFunctionCallbacks,
            &AclrtBinaryGetFunctionHandler) &&
        RegisterRuntimeHandler(ACL_RT_API_aclrtMalloc, acltoolRegisterAclrtMallocCallbacks, &AclrtMallocHandler) &&
        RegisterRuntimeHandler(
            ACL_RT_API_aclrtMallocAlign32, acltoolRegisterAclrtMallocAlign32Callbacks, &AclrtMallocAlign32Handler) &&
        RegisterRuntimeHandler(ACL_RT_API_aclrtMemset, acltoolRegisterAclrtMemsetCallbacks, &AclrtMemsetHandler) &&
        RegisterRuntimeHandler(ACL_RT_API_aclrtFree, acltoolRegisterAclrtFreeCallbacks, &AclrtFreeHandler) &&
        RegisterRuntimeHandler(
            ACL_RT_API_aclrtCreateStream, acltoolRegisterAclrtCreateStreamCallbacks, &AclrtCreateStreamHandler) &&
        RegisterRuntimeHandler(
            ACL_RT_API_aclrtDestroyStream, acltoolRegisterAclrtDestroyStreamCallbacks, &AclrtDestroyStreamHandler) &&
        RegisterRuntimeHandler(
            ACL_RT_API_aclrtSetDevice, acltoolRegisterAclrtSetDeviceCallbacks, &AclrtSetDeviceHandler) &&
        RegisterRuntimeHandler(
            ACL_RT_API_aclrtResetDevice, acltoolRegisterAclrtResetDeviceCallbacks, &AclrtResetDeviceHandler) &&
        RegisterRuntimeHandler(
            ACL_RT_API_aclrtSynchronizeStream, acltoolRegisterAclrtSynchronizeStreamCallbacks,
            &AclrtSynchronizeStreamHandler) &&
        RegisterRuntimeHandler(
            ACL_RT_API_aclrtBinaryGetFunctionByEntry, acltoolRegisterAclrtBinaryGetFunctionByEntryCallbacks,
            &AclrtBinaryGetFunctionByEntryHandler) &&
        RegisterRuntimeHandler(
            ACL_RT_API_aclrtLaunchKernel, acltoolRegisterAclrtLaunchKernelCallbacks, &AclrtLaunchKernelHandler) &&
        RegisterRuntimeHandler(
            ACL_RT_API_aclrtGetFuncBySymbol, acltoolRegisterAclrtGetFuncBySymbolCallbacks,
            &AclrtGetFuncBySymbolHandler) &&
        RegisterRuntimeHandler(
            ACL_RT_API_aclrtBinaryUnLoad, acltoolRegisterAclrtBinaryUnLoadCallbacks, &AclrtBinaryUnLoadHandler);
    if (!registered) {
        npucompute::detail::DebugLog("aclpti", "runtime handler registration failed");
        return false;
    }
    npucompute::detail::DebugLog("aclpti", "runtime handler registration complete");
    return true;
}

} // namespace aclpti::handler
