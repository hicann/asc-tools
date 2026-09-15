/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "runtime/kernel_metadata_collector.h"
#include <iostream>
#include <limits>

extern "C" __attribute__((noinline)) void MetadataKernelFixture() {}

#define CHECK(condition)                                         \
    do {                                                         \
        if (!(condition)) {                                      \
            std::cerr << __LINE__ << ": " << #condition << '\n'; \
            return 1;                                            \
        }                                                        \
    } while (false)

int main()
{
    for (const bool success : {false, true}) {
        npucompute::KernelMetadataCollector bySymbol;
        aclrtFuncHandle symbolHandle = reinterpret_cast<aclrtFuncHandle>(0x2000);
        aclptiAclrtGetFuncBySymbolParams lookup{reinterpret_cast<const void*>(&MetadataKernelFixture), &symbolHandle};
        aclptiCallbackData callback{};
        callback.functionParams = &lookup;
        callback.callbackSite = ACLPTI_API_ENTER;
        bySymbol.OnCallback(ACLPTI_RUNTIME_CBID_aclrtGetFuncBySymbol, callback);
        lookup.symbol = nullptr;
        callback.callbackSite = ACLPTI_API_EXIT;
        callback.retval = success ? ACL_SUCCESS : 1;
        bySymbol.OnCallback(ACLPTI_RUNTIME_CBID_aclrtGetFuncBySymbol, callback);
        aclptiAclrtLaunchKernelWithArgsArrayParams launchArgs{};
        launchArgs.func = symbolHandle;
        launchArgs.numBlocks = 8;
        callback.functionParams = &launchArgs;
        callback.callbackSite = ACLPTI_API_ENTER;
        bySymbol.OnCallback(ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithArgsArray, callback);
        const auto captured = bySymbol.Snapshot();
        CHECK(success ? captured.name == "MetadataKernelFixture" : !captured.name);
        CHECK(captured.blockDim == 8);
    }
    for (const void* symbol : {static_cast<const void*>(nullptr), reinterpret_cast<const void*>(0x1234)}) {
        npucompute::KernelMetadataCollector unknown;
        aclrtFuncHandle unknownHandle = reinterpret_cast<aclrtFuncHandle>(0x3000);
        aclptiAclrtGetFuncBySymbolParams lookup{symbol, &unknownHandle};
        aclptiCallbackData callback{};
        callback.functionParams = &lookup;
        callback.callbackSite = ACLPTI_API_ENTER;
        unknown.OnCallback(ACLPTI_RUNTIME_CBID_aclrtGetFuncBySymbol, callback);
        callback.callbackSite = ACLPTI_API_EXIT;
        callback.retval = ACL_SUCCESS;
        unknown.OnCallback(ACLPTI_RUNTIME_CBID_aclrtGetFuncBySymbol, callback);
        aclptiAclrtLaunchKernelWithArgsArrayParams args{};
        args.func = unknownHandle;
        callback.functionParams = &args;
        callback.callbackSite = ACLPTI_API_ENTER;
        unknown.OnCallback(ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithArgsArray, callback);
        CHECK(!unknown.Snapshot().name);
    }
    npucompute::KernelMetadataCollector collector;
    aclrtFuncHandle handle = nullptr;
    std::string name = "original kernel";
    aclptiAclrtBinaryGetFunctionParams params{nullptr, name.c_str(), &handle};
    aclptiCallbackData data{};
    data.functionParams = &params;
    data.callbackSite = ACLPTI_API_ENTER;
    collector.OnCallback(ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunction, data);
    name = "changed";
    params.kernelName = nullptr;
    handle = reinterpret_cast<aclrtFuncHandle>(0x1000);
    data.callbackSite = ACLPTI_API_EXIT;
    data.retval = ACL_SUCCESS;
    collector.OnCallback(ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunction, data);
    aclptiAclrtLaunchKernelParams launch{};
    launch.funcHandle = handle;
    launch.numBlocks = 64;
    data.functionParams = &launch;
    data.callbackSite = ACLPTI_API_ENTER;
    collector.OnCallback(ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, data);
    auto metadata = collector.Snapshot();
    CHECK(metadata.name == "original kernel");
    CHECK(metadata.blockDim == 64);
    launch.numBlocks = 128;
    collector.OnCallback(ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, data);
    CHECK(collector.Snapshot().blockDim == 64);

    npucompute::KernelMetadataCollector failed;
    params.kernelName = "failed";
    data.functionParams = &params;
    failed.OnCallback(ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunction, data);
    data.callbackSite = ACLPTI_API_EXIT;
    data.retval = 1;
    failed.OnCallback(ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunction, data);
    data.functionParams = &launch;
    data.callbackSite = ACLPTI_API_ENTER;
    failed.OnCallback(ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, data);
    CHECK(!failed.Snapshot().name);

    for (bool overflow : {false, true}) {
        npucompute::KernelMetadataCollector simt;
        aclptiAclrtLaunchSIMTKernelWithArgsArrayParams simtLaunch{};
        simtLaunch.gridDim.x = overflow ? std::numeric_limits<uint32_t>::max() : 2;
        simtLaunch.gridDim.y = overflow ? std::numeric_limits<uint32_t>::max() : 3;
        simtLaunch.gridDim.z = overflow ? std::numeric_limits<uint32_t>::max() : 4;
        data.functionParams = &simtLaunch;
        simt.OnCallback(ACLPTI_RUNTIME_CBID_aclrtLaunchSIMTKernelWithArgsArray, data);
        CHECK(overflow ? !simt.Snapshot().blockDim : simt.Snapshot().blockDim == 24);
    }
    for (const bool hostArgs : {false, true}) {
        npucompute::KernelMetadataCollector simt;
        params.kernelName = "simt kernel";
        data.functionParams = &params;
        data.callbackSite = ACLPTI_API_ENTER;
        simt.OnCallback(ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunction, data);
        data.callbackSite = ACLPTI_API_EXIT;
        data.retval = ACL_SUCCESS;
        simt.OnCallback(ACLPTI_RUNTIME_CBID_aclrtBinaryGetFunction, data);
        aclptiAclrtLaunchSIMTKernelWithArgsArrayParams arrayLaunch{};
        arrayLaunch.func = handle;
        arrayLaunch.gridDim = {2, 3, 4};
        aclptiAclrtLaunchSIMTKernelWithHostArgsParams hostLaunch{};
        hostLaunch.func = handle;
        hostLaunch.gridDim = {2, 3, 4};
        data.functionParams = hostArgs ? static_cast<void*>(&hostLaunch) : static_cast<void*>(&arrayLaunch);
        data.callbackSite = ACLPTI_API_ENTER;
        simt.OnCallback(
            hostArgs ? ACLPTI_RUNTIME_CBID_aclrtLaunchSIMTKernelWithHostArgs :
                       ACLPTI_RUNTIME_CBID_aclrtLaunchSIMTKernelWithArgsArray,
            data);
        CHECK(simt.Snapshot().name == "simt kernel");
        CHECK(simt.Snapshot().blockDim == 24);
    }
    return 0;
}
