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
#include "injection/injection_hook.h"
#include "injection/runtime_stub_api.h"
#include "profiling/prof_api.h"

#include <cstdint>
#include <cstdio>
#include <string_view>
#include <vector>

namespace {

#define CHECK(expression)                                                                 \
    do {                                                                                  \
        if (!(expression)) {                                                              \
            std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expression); \
            return 1;                                                                     \
        }                                                                                 \
    } while (false)

struct NormalLaunch {
    void* func;
    uint32_t numBlocks;
    aclrtStream stream;
    aclrtLaunchKernelCfg* cfg;
    void** args;
};

struct SimtLaunch {
    void* func;
    dim3 gridDim;
    dim3 blockDim;
    std::size_t dynUbufSize;
    aclrtStream stream;
    aclrtLaunchKernelCfg* cfg;
    void** args;
};

std::vector<NormalLaunch> gNormalLaunches;
std::vector<SimtLaunch> gSimtLaunches;
std::size_t gStartCalls = 0;
std::size_t gStopCalls = 0;
std::size_t gSyncCalls = 0;

aclError RealNormalLaunch(void* func, uint32_t numBlocks, aclrtStream stream, aclrtLaunchKernelCfg* cfg, void** args)
{
    gNormalLaunches.push_back({func, numBlocks, stream, cfg, args});
    return ACL_SUCCESS;
}

aclError RealSimtLaunch(
    void* func, dim3 gridDim, dim3 blockDim, std::size_t dynUbufSize, aclrtStream stream, aclrtLaunchKernelCfg* cfg,
    void** args)
{
    gSimtLaunches.push_back({func, gridDim, blockDim, dynUbufSize, stream, cfg, args});
    return ACL_SUCCESS;
}

aclError RealSynchronize(aclrtStream)
{
    ++gSyncCalls;
    return ACL_SUCCESS;
}

int CheckNormalReplay()
{
    int functionToken = 0;
    std::uint32_t firstArg = 11;
    std::uint64_t secondArg = 12;
    void* args[] = {&firstArg, &secondArg};
    aclrtLaunchKernelCfg cfg{};
    const auto stream = reinterpret_cast<aclrtStream>(0x10);

    CHECK(aclrtLaunchKernelWithArgsArray(&functionToken, 8, stream, &cfg, args) == ACL_SUCCESS);
    CHECK(gNormalLaunches.size() == 3);
    for (const auto& launch : gNormalLaunches) {
        CHECK(launch.func == &functionToken);
        CHECK(launch.numBlocks == 8);
        CHECK(launch.stream == stream);
        CHECK(launch.cfg == &cfg);
        CHECK(launch.args == args);
    }
    return 0;
}

int CheckSimtReplay()
{
    int functionToken = 0;
    std::uint32_t firstArg = 11;
    std::uint64_t secondArg = 12;
    void* args[] = {&firstArg, &secondArg};
    dim3 gridDim{2, 3, 4};
    dim3 blockDim{5, 6, 7};
    aclrtLaunchKernelCfg cfg{};
    const auto stream = reinterpret_cast<aclrtStream>(0x20);

    CHECK(
        aclrtLaunchSIMTKernelWithArgsArray(&functionToken, gridDim, blockDim, 4096, stream, &cfg, args) == ACL_SUCCESS);
    CHECK(gSimtLaunches.size() == 3);
    for (const auto& launch : gSimtLaunches) {
        CHECK(launch.func == &functionToken);
        CHECK(launch.gridDim.x == 2 && launch.gridDim.y == 3 && launch.gridDim.z == 4);
        CHECK(launch.blockDim.x == 5 && launch.blockDim.y == 6 && launch.blockDim.z == 7);
        CHECK(launch.dynUbufSize == 4096);
        CHECK(launch.stream == stream);
        CHECK(launch.cfg == &cfg);
        CHECK(launch.args == args);
    }
    return 0;
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

int main(int argc, char** argv)
{
    CHECK(argc == 2);
    CHECK(RuntimeStubSetOriginFunction("aclrtLaunchKernelWithArgsArray", &RealNormalLaunch) == ACL_SUCCESS);
    CHECK(RuntimeStubSetOriginFunction("aclrtLaunchSIMTKernelWithArgsArray", &RealSimtLaunch) == ACL_SUCCESS);
    CHECK(RuntimeStubSetOriginFunction("aclrtSynchronizeStream", &RealSynchronize) == ACL_SUCCESS);

    aclptiSubscribeHandle subscriber = nullptr;
    CHECK(aclptiSubscribe(&subscriber, nullptr, nullptr, nullptr) == ACLPTI_SUCCESS);
    CHECK(subscriber != nullptr);
    CHECK(
        reinterpret_cast<aclrtLaunchKernelWithArgsArrayFunc>(
            acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtLaunchKernelWithArgsArray)) == &RealNormalLaunch);
    CHECK(
        reinterpret_cast<aclrtLaunchSIMTKernelWithArgsArrayFunc>(
            acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtLaunchSIMTKernelWithArgsArray)) == &RealSimtLaunch);
    CHECK(aclrtSetDevice(7) == ACL_SUCCESS);

    const char* sections[] = {"PipeUtilization"};
    aclptiRangeProfilerSetConfigParams config{sections, 1};
    CHECK(aclptiRangeProfilerSetConfig(&config) == ACLPTI_SUCCESS);

    const std::string_view mode = argv[1];
    CHECK((mode == "normal" ? CheckNormalReplay() : mode == "simt" ? CheckSimtReplay() : 1) == 0);
    CHECK(gStartCalls == 2);
    CHECK(gStopCalls == 2);
    CHECK(gSyncCalls == 3);
    return 0;
}
