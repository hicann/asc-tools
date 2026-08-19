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
#include "common/debug_log.h"
#include "npu_compute/injection_hook.h"

#include <utility>

namespace npu_compute::aclpti::replacement {
namespace {

template <typename Function>
Function GetOriginalRuntimeFunction(aclrtApiId id)
{
    return reinterpret_cast<Function>(acltoolGetOriginalRuntimeApi(id));
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
aclError ForwardRuntimeApi(aclptiCallbackId cbid, aclrtApiId apiId, Params& params, Operation&& operation)
{
    return InvokeRuntimeCallback(
        cbid, params, [apiId, operation = std::forward<Operation>(operation)]() mutable -> aclError {
            const auto function = GetOriginalRuntimeFunction<Function>(apiId);
            return function == nullptr ? -1 : operation(function);
        });
}

#define ACLPTI_ASSERT_CBID(apiName)                                     \
    static_assert(                                                      \
        static_cast<aclptiCallbackId>(ACLPTI_RUNTIME_CBID_##apiName) == \
            static_cast<aclptiCallbackId>(ACL_RT_API_##apiName),        \
        "ACLPTI callback IDs must match injection hook runtime API IDs")

ACLPTI_ASSERT_CBID(aclrtLaunchKernelWithHostArgs);
ACLPTI_ASSERT_CBID(aclrtMemcpy);
ACLPTI_ASSERT_CBID(aclrtBinaryLoadFromData);
ACLPTI_ASSERT_CBID(aclrtBinaryGetFunction);
ACLPTI_ASSERT_CBID(aclrtMalloc);
ACLPTI_ASSERT_CBID(aclrtMemset);
ACLPTI_ASSERT_CBID(aclrtFree);
ACLPTI_ASSERT_CBID(aclrtCreateStream);
ACLPTI_ASSERT_CBID(aclrtDestroyStream);
ACLPTI_ASSERT_CBID(aclrtSetDevice);
ACLPTI_ASSERT_CBID(aclrtResetDevice);
ACLPTI_ASSERT_CBID(aclrtSynchronizeStream);
ACLPTI_ASSERT_CBID(aclrtBinaryGetFunctionByEntry);
ACLPTI_ASSERT_CBID(aclrtLaunchKernel);

#undef ACLPTI_ASSERT_CBID

} // namespace

RuntimeApiReplacements& RuntimeApiReplacements::Instance() { return GetRuntimeApiReplacements(); }

RuntimeApiReplacements& GetRuntimeApiReplacements()
{
    static RuntimeApiReplacements replacements;
    return replacements;
}

bool RuntimeApiReplacements::RegisterCallbacks(callback::Dispatcher& dispatcher)
{
    return dispatcher.RegisterDomain(
        ACLPTI_CB_DOMAIN_RUNTIME_API,
        {ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithHostArgs, ACLPTI_RUNTIME_CBID_aclrtMemcpy,
         ACLPTI_RUNTIME_CBID_aclrtBinaryLoadFromData, ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunction,
         ACLPTI_RUNTIME_CBID_aclrtMalloc, ACLPTI_RUNTIME_CBID_aclrtMemset, ACLPTI_RUNTIME_CBID_aclrtFree,
         ACLPTI_RUNTIME_CBID_aclrtCreateStream, ACLPTI_RUNTIME_CBID_aclrtDestroyStream,
         ACLPTI_RUNTIME_CBID_aclrtSetDevice, ACLPTI_RUNTIME_CBID_aclrtResetDevice,
         ACLPTI_RUNTIME_CBID_aclrtSynchronizeStream, ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunctionByEntry,
         ACLPTI_RUNTIME_CBID_aclrtLaunchKernel});
}

bool RuntimeApiReplacements::Initialize(profiling::ReplayMemory& replayMemory, profiling::RangeProfiler& rangeProfiler)
{
    if (initialized_) {
        return true;
    }
    replayMemory_ = &replayMemory;
    rangeProfiler_ = &rangeProfiler;
    if (!RegisterReplacements()) {
        return false;
    }
    if (!rangeProfiler_->Initialize()) {
        return false;
    }
    initialized_ = true;
    return true;
}

bool RuntimeApiReplacements::RegisterReplacements()
{
    if (acltoolRegisterAclrtLaunchKernelWithHostArgsCallbacks(
            &RuntimeApiReplacements::AclrtLaunchKernelWithHostArgsReplacement) != 0 ||
        acltoolRegisterAclrtMemcpyCallbacks(&RuntimeApiReplacements::AclrtMemcpyReplacement) != 0 ||
        acltoolRegisterAclrtBinaryLoadFromDataCallbacks(&RuntimeApiReplacements::AclrtBinaryLoadFromDataReplacement) !=
            0 ||
        acltoolRegisterAclrtBinaryGetFunctionCallbacks(&RuntimeApiReplacements::AclrtBinaryGetFunctionReplacement) !=
            0 ||
        acltoolRegisterAclrtMallocCallbacks(&RuntimeApiReplacements::AclrtMallocReplacement) != 0 ||
        acltoolRegisterAclrtMemsetCallbacks(&RuntimeApiReplacements::AclrtMemsetReplacement) != 0 ||
        acltoolRegisterAclrtFreeCallbacks(&RuntimeApiReplacements::AclrtFreeReplacement) != 0 ||
        acltoolRegisterAclrtCreateStreamCallbacks(&RuntimeApiReplacements::AclrtCreateStreamReplacement) != 0 ||
        acltoolRegisterAclrtDestroyStreamCallbacks(&RuntimeApiReplacements::AclrtDestroyStreamReplacement) != 0 ||
        acltoolRegisterAclrtSetDeviceCallbacks(&RuntimeApiReplacements::AclrtSetDeviceReplacement) != 0 ||
        acltoolRegisterAclrtResetDeviceCallbacks(&RuntimeApiReplacements::AclrtResetDeviceReplacement) != 0 ||
        acltoolRegisterAclrtSynchronizeStreamCallbacks(&RuntimeApiReplacements::AclrtSynchronizeStreamReplacement) !=
            0 ||
        acltoolRegisterAclrtBinaryGetFunctionByEntryCallbacks(
            &RuntimeApiReplacements::AclrtBinaryGetFunctionByEntryReplacement) != 0 ||
        acltoolRegisterAclrtLaunchKernelCallbacks(&RuntimeApiReplacements::AclrtLaunchKernelReplacement) != 0) {
        return false;
    }
    npu_compute::detail::DebugLog("aclpti", "runtime replacement registration complete");
    return true;
}

aclError RuntimeApiReplacements::AclrtLaunchKernelWithHostArgsReplacement(
    aclrtFuncHandle funcHandle, std::uint32_t numBlocks, aclrtStream stream, aclrtLaunchKernelCfg* cfg, void* hostArgs,
    std::size_t argsSize, aclrtPlaceHolderInfo* placeHolderArray, std::size_t placeHolderNum)
{
    aclptiAclrtLaunchKernelWithHostArgsParams params{funcHandle, numBlocks, stream,           cfg,
                                                     hostArgs,   argsSize,  placeHolderArray, placeHolderNum};
    return InvokeRuntimeCallback(ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithHostArgs, params, [&params]() -> aclError {
        const auto launchFunction =
            GetOriginalRuntimeFunction<aclrtLaunchKernelWithHostArgsFunc>(ACL_RT_API_aclrtLaunchKernelWithHostArgs);
        if (launchFunction == nullptr) {
            return -1;
        }
        const aclError result = launchFunction(
            params.funcHandle, params.numBlocks, params.stream, params.cfg, params.hostArgs, params.argsSize,
            params.placeHolderArray, params.placeHolderNum);
        if (result != ACL_SUCCESS) {
            return result;
        }
        RuntimeApiReplacements& replacements = RuntimeApiReplacements::Instance();
        const auto synchronizeFunction =
            GetOriginalRuntimeFunction<aclrtSynchronizeStreamFunc>(ACL_RT_API_aclrtSynchronizeStream);
        return replacements.rangeProfiler_->ReplayKernel(
            *replacements.replayMemory_, GetOriginalRuntimeFunction<aclrtMemcpyFunc>(ACL_RT_API_aclrtMemcpy),
            [launchFunction, &params]() -> aclError {
                return launchFunction(
                    params.funcHandle, params.numBlocks, params.stream, params.cfg, params.hostArgs, params.argsSize,
                    params.placeHolderArray, params.placeHolderNum);
            },
            synchronizeFunction, params.stream, replacements.currentDeviceId_.load());
    });
}

aclError RuntimeApiReplacements::AclrtMemcpyReplacement(
    void* destination, std::size_t destinationSize, const void* source, std::size_t count, aclrtMemcpyKind kind)
{
    aclptiAclrtMemcpyParams params{destination, destinationSize, source, count, kind};
    return InvokeRuntimeCallback(ACLPTI_RUNTIME_CBID_aclrtMemcpy, params, [&params]() -> aclError {
        const auto function = GetOriginalRuntimeFunction<aclrtMemcpyFunc>(ACL_RT_API_aclrtMemcpy);
        if (function == nullptr) {
            return -1;
        }
        const aclError result = function(params.dst, params.destMax, params.src, params.count, params.kind);
        if (result != ACL_SUCCESS) {
            return result;
        }
        return RuntimeApiReplacements::Instance().replayMemory_->MirrorMemcpy(
            function, params.dst, params.destMax, params.src, params.count, params.kind);
    });
}

aclError RuntimeApiReplacements::AclrtBinaryLoadFromDataReplacement(
    const void* data, std::size_t length, const aclrtBinaryLoadOptions* options, aclrtBinHandle* binHandle)
{
    aclptiAclrtBinaryLoadFromDataParams params{data, length, options, binHandle};
    return ForwardRuntimeApi<aclrtBinaryLoadFromDataFunc>(
        ACLPTI_RUNTIME_CBID_aclrtBinaryLoadFromData, ACL_RT_API_aclrtBinaryLoadFromData, params,
        [&params](aclrtBinaryLoadFromDataFunc function) {
            return function(params.data, params.length, params.options, params.binHandle);
        });
}

aclError RuntimeApiReplacements::AclrtBinaryGetFunctionReplacement(
    const aclrtBinHandle binHandle, const char* kernelName, aclrtFuncHandle* funcHandle)
{
    aclptiAclrtBinaryGetFunctionParams params{binHandle, kernelName, funcHandle};
    return ForwardRuntimeApi<aclrtBinaryGetFunctionFunc>(
        ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunction, ACL_RT_API_aclrtBinaryGetFunction, params,
        [&params](aclrtBinaryGetFunctionFunc function) {
            return function(params.binHandle, params.kernelName, params.funcHandle);
        });
}

aclError RuntimeApiReplacements::AclrtMallocReplacement(void** devPtr, std::size_t size, aclrtMemMallocPolicy policy)
{
    aclptiAclrtMallocParams params{devPtr, size, policy};
    return InvokeRuntimeCallback(ACLPTI_RUNTIME_CBID_aclrtMalloc, params, [&params]() -> aclError {
        const auto mallocFunction = GetOriginalRuntimeFunction<aclrtMallocFunc>(ACL_RT_API_aclrtMalloc);
        const auto freeFunction = GetOriginalRuntimeFunction<aclrtFreeFunc>(ACL_RT_API_aclrtFree);
        if (mallocFunction == nullptr || freeFunction == nullptr) {
            return -1;
        }
        const aclError result = mallocFunction(params.devPtr, params.size, params.policy);
        if (result != ACL_SUCCESS) {
            return result;
        }
        return RuntimeApiReplacements::Instance().replayMemory_->MirrorMalloc(
            mallocFunction, freeFunction, params.devPtr, params.size, params.policy);
    });
}

aclError RuntimeApiReplacements::AclrtMemsetReplacement(
    void* devPtr, std::size_t maxCount, std::int32_t value, std::size_t count)
{
    aclptiAclrtMemsetParams params{devPtr, maxCount, value, count};
    return InvokeRuntimeCallback(ACLPTI_RUNTIME_CBID_aclrtMemset, params, [&params]() -> aclError {
        const auto function = GetOriginalRuntimeFunction<aclrtMemsetFunc>(ACL_RT_API_aclrtMemset);
        if (function == nullptr) {
            return -1;
        }
        const aclError result = function(params.devPtr, params.maxCount, params.value, params.count);
        if (result != ACL_SUCCESS) {
            return result;
        }
        return RuntimeApiReplacements::Instance().replayMemory_->MirrorMemset(
            function, params.devPtr, params.maxCount, params.value, params.count);
    });
}

aclError RuntimeApiReplacements::AclrtFreeReplacement(void* devPtr)
{
    aclptiAclrtFreeParams params{devPtr};
    return InvokeRuntimeCallback(ACLPTI_RUNTIME_CBID_aclrtFree, params, [&params]() -> aclError {
        const auto function = GetOriginalRuntimeFunction<aclrtFreeFunc>(ACL_RT_API_aclrtFree);
        if (function == nullptr) {
            return -1;
        }
        const aclError result = function(params.devPtr);
        if (result != ACL_SUCCESS) {
            return result;
        }
        return RuntimeApiReplacements::Instance().replayMemory_->MirrorFree(function, params.devPtr);
    });
}

aclError RuntimeApiReplacements::AclrtCreateStreamReplacement(aclrtStream* stream)
{
    aclptiAclrtCreateStreamParams params{stream};
    return ForwardRuntimeApi<aclrtCreateStreamFunc>(
        ACLPTI_RUNTIME_CBID_aclrtCreateStream, ACL_RT_API_aclrtCreateStream, params,
        [&params](aclrtCreateStreamFunc function) { return function(params.stream); });
}

aclError RuntimeApiReplacements::AclrtDestroyStreamReplacement(aclrtStream stream)
{
    aclptiAclrtDestroyStreamParams params{stream};
    return ForwardRuntimeApi<aclrtDestroyStreamFunc>(
        ACLPTI_RUNTIME_CBID_aclrtDestroyStream, ACL_RT_API_aclrtDestroyStream, params,
        [&params](aclrtDestroyStreamFunc function) { return function(params.stream); });
}

aclError RuntimeApiReplacements::AclrtSetDeviceReplacement(std::int32_t deviceId)
{
    aclptiAclrtSetDeviceParams params{deviceId};
    return InvokeRuntimeCallback(ACLPTI_RUNTIME_CBID_aclrtSetDevice, params, [&params]() -> aclError {
        const auto function = GetOriginalRuntimeFunction<aclrtSetDeviceFunc>(ACL_RT_API_aclrtSetDevice);
        if (function == nullptr) {
            return -1;
        }
        const aclError result = function(params.deviceId);
        if (result == ACL_SUCCESS && params.deviceId >= 0) {
            RuntimeApiReplacements::Instance().currentDeviceId_.store(params.deviceId);
        }
        return result;
    });
}

aclError RuntimeApiReplacements::AclrtResetDeviceReplacement(std::int32_t deviceId)
{
    aclptiAclrtResetDeviceParams params{deviceId};
    return InvokeRuntimeCallback(ACLPTI_RUNTIME_CBID_aclrtResetDevice, params, [&params]() -> aclError {
        const auto function = GetOriginalRuntimeFunction<aclrtResetDeviceFunc>(ACL_RT_API_aclrtResetDevice);
        if (function == nullptr) {
            return -1;
        }
        const aclError result = function(params.deviceId);
        if (result == ACL_SUCCESS) {
            std::int32_t expectedDeviceId = params.deviceId;
            RuntimeApiReplacements::Instance().currentDeviceId_.compare_exchange_strong(expectedDeviceId, -1);
        }
        return result;
    });
}

aclError RuntimeApiReplacements::AclrtSynchronizeStreamReplacement(aclrtStream stream)
{
    aclptiAclrtSynchronizeStreamParams params{stream};
    return ForwardRuntimeApi<aclrtSynchronizeStreamFunc>(
        ACLPTI_RUNTIME_CBID_aclrtSynchronizeStream, ACL_RT_API_aclrtSynchronizeStream, params,
        [&params](aclrtSynchronizeStreamFunc function) { return function(params.stream); });
}

aclError RuntimeApiReplacements::AclrtBinaryGetFunctionByEntryReplacement(
    aclrtBinHandle binHandle, std::uint64_t funcEntry, aclrtFuncHandle* funcHandle)
{
    aclptiAclrtBinaryGetFunctionByEntryParams params{binHandle, funcEntry, funcHandle};
    return ForwardRuntimeApi<aclrtBinaryGetFunctionByEntryFunc>(
        ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunctionByEntry, ACL_RT_API_aclrtBinaryGetFunctionByEntry, params,
        [&params](aclrtBinaryGetFunctionByEntryFunc function) {
            return function(params.binHandle, params.funcEntry, params.funcHandle);
        });
}

aclError RuntimeApiReplacements::AclrtLaunchKernelReplacement(
    aclrtFuncHandle function, std::uint32_t blockCount, const void* argsData, std::size_t argsSize, aclrtStream stream)
{
    aclptiAclrtLaunchKernelParams params{function, blockCount, argsData, argsSize, stream};
    return InvokeRuntimeCallback(ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, params, [&params]() -> aclError {
        const auto launchFunction = GetOriginalRuntimeFunction<aclrtLaunchKernelFunc>(ACL_RT_API_aclrtLaunchKernel);
        if (launchFunction == nullptr) {
            return -1;
        }
        const aclError result =
            launchFunction(params.funcHandle, params.numBlocks, params.argsData, params.argsSize, params.stream);
        if (result != ACL_SUCCESS) {
            return result;
        }
        RuntimeApiReplacements& replacements = RuntimeApiReplacements::Instance();
        const auto synchronizeFunction =
            GetOriginalRuntimeFunction<aclrtSynchronizeStreamFunc>(ACL_RT_API_aclrtSynchronizeStream);
        return replacements.rangeProfiler_->ReplayKernel(
            *replacements.replayMemory_, GetOriginalRuntimeFunction<aclrtMemcpyFunc>(ACL_RT_API_aclrtMemcpy),
            [launchFunction, &params]() -> aclError {
                return launchFunction(
                    params.funcHandle, params.numBlocks, params.argsData, params.argsSize, params.stream);
            },
            synchronizeFunction, params.stream, replacements.currentDeviceId_.load());
    });
}

} // namespace npu_compute::aclpti::replacement
