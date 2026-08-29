/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "aclpti/aclpti.h"
#include "npu_compute/injection_hook.h"
#include "npu_compute/runtime_stub_api.h"
#include "profiling/prof_api.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

#define CHECK(expression)                                                                 \
    do {                                                                                  \
        if (!(expression)) {                                                              \
            std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expression); \
            return 1;                                                                     \
        }                                                                                 \
    } while (false)

struct ObservedLaunch {
    void* func;
    dim3 gridDim;
    dim3 blockDim;
    std::size_t dynUbufSize;
    aclrtStream stream;
    aclrtLaunchKernelCfg* cfg;
    void* hostArgs;
    std::size_t argsSize;
    aclrtPlaceHolderInfo* placeHolderArray;
    std::size_t placeHolderNum;
};

std::vector<ObservedLaunch> gLaunches;
std::size_t gStartCalls = 0;
std::size_t gStopCalls = 0;
std::size_t gSyncCalls = 0;

aclError RealSIMTLaunch(
    void* func, dim3 gridDim, dim3 blockDim, std::size_t dynUbufSize, aclrtStream stream, aclrtLaunchKernelCfg* cfg,
    void* hostArgs, std::size_t argsSize, aclrtPlaceHolderInfo* placeHolderArray, std::size_t placeHolderNum)
{
    gLaunches.push_back(
        {func, gridDim, blockDim, dynUbufSize, stream, cfg, hostArgs, argsSize, placeHolderArray, placeHolderNum});
    return ACL_SUCCESS;
}

aclError RealSynchronize(aclrtStream)
{
    ++gSyncCalls;
    return ACL_SUCCESS;
}

} // namespace

std::int32_t MsprofStart(uint32_t, const void*, uint32_t)
{
    ++gStartCalls;
    return 0;
}

std::int32_t MsprofStop(uint32_t, const void*, uint32_t)
{
    ++gStopCalls;
    return 0;
}

std::int32_t MsprofRegisterDataCallback(uint32_t, void* callback) { return callback == nullptr ? -1 : 0; }

int main()
{
    CHECK(RuntimeStubSetOriginFunction("aclrtLaunchSIMTKernelWithHostArgs", &RealSIMTLaunch) == ACL_SUCCESS);
    CHECK(RuntimeStubSetOriginFunction("aclrtSynchronizeStream", &RealSynchronize) == ACL_SUCCESS);

    aclptiSubscribeHandle subscriber = nullptr;
    CHECK(aclptiSubscribe(&subscriber, nullptr, nullptr, nullptr) == ACLPTI_SUCCESS);
    CHECK(subscriber != nullptr);
    CHECK(
        reinterpret_cast<aclrtLaunchSIMTKernelWithHostArgsFunc>(
            acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtLaunchSIMTKernelWithHostArgs)) == &RealSIMTLaunch);
    CHECK(aclrtSetDevice(7) == ACL_SUCCESS);

    const char* sections[] = {"PipeUtilization"};
    aclptiRangeProfilerSetConfigParams config{sections, 1};
    CHECK(aclptiRangeProfilerSetConfig(&config) == ACLPTI_SUCCESS);

    int functionToken = 0;
    std::uint64_t hostArgs[] = {11, 12};
    dim3 gridDim{2, 3, 4};
    dim3 blockDim{5, 6, 7};
    aclrtLaunchKernelCfg launchConfig{};
    aclrtPlaceHolderInfo placeHolder{};
    const auto stream = reinterpret_cast<aclrtStream>(0x10);

    CHECK(
        aclrtLaunchSIMTKernelWithHostArgs(
            &functionToken, gridDim, blockDim, 4096, stream, &launchConfig, hostArgs, sizeof(hostArgs), &placeHolder,
            1) == ACL_SUCCESS);
    CHECK(gLaunches.size() == 3);
    CHECK(gStartCalls == 2);
    CHECK(gStopCalls == 2);
    CHECK(gSyncCalls == 3);

    for (const auto& launch : gLaunches) {
        CHECK(launch.func == &functionToken);
        CHECK(launch.gridDim.x == 2 && launch.gridDim.y == 3 && launch.gridDim.z == 4);
        CHECK(launch.blockDim.x == 5 && launch.blockDim.y == 6 && launch.blockDim.z == 7);
        CHECK(launch.dynUbufSize == 4096);
        CHECK(launch.stream == stream);
        CHECK(launch.cfg == &launchConfig);
        CHECK(launch.hostArgs == hostArgs);
        CHECK(launch.argsSize == sizeof(hostArgs));
        CHECK(launch.placeHolderArray == &placeHolder);
        CHECK(launch.placeHolderNum == 1);
    }
    return 0;
}
