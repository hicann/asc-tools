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

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

int Check(bool condition, const char* expression, int line)
{
    if (condition) {
        return 0;
    }
    std::fprintf(stderr, "check failed at line %d: %s\n", line, expression);
    return 1;
}

#define CHECK(expression)                                      \
    do {                                                       \
        if (Check((expression), #expression, __LINE__) != 0) { \
            return 1;                                          \
        }                                                      \
    } while (false)

template <typename Function>
bool CaptureStderr(Function function, std::string* output)
{
    FILE* capture = std::tmpfile();
    if (capture == nullptr) {
        return false;
    }

    const int savedStderr = dup(STDERR_FILENO);
    if (savedStderr < 0) {
        std::fclose(capture);
        return false;
    }

    bool success = std::fflush(stderr) == 0 && dup2(fileno(capture), STDERR_FILENO) >= 0;
    if (success) {
        function();
        success = std::fflush(stderr) == 0;
    }
    success = dup2(savedStderr, STDERR_FILENO) >= 0 && success;
    close(savedStderr);

    if (success) {
        std::rewind(capture);
        char buffer[256];
        std::size_t count = 0;
        while ((count = std::fread(buffer, 1, sizeof(buffer), capture)) != 0) {
            output->append(buffer, count);
        }
        success = std::ferror(capture) == 0;
    }

    success = std::fclose(capture) == 0 && success;
    return success;
}

struct KernelArgs {
    uint8_t* value;
};

struct ObservedConfig {
    std::uint32_t devNums;
    std::uint32_t deviceId;
    std::uint64_t profSwitch;
    std::uint32_t primaryAttr;
    std::uint32_t instrMode;
    std::uint32_t blockMode;
    std::array<std::uint32_t, COMPUTE_AICORE_METRICS_NUM> pmuEvents;
};

std::size_t g_malloc_calls = 0;
std::size_t g_free_calls = 0;
std::size_t g_memcpy_calls = 0;
std::size_t g_launch_calls = 0;
std::size_t g_launch_with_host_args_calls = 0;
std::size_t g_sync_calls = 0;
std::size_t g_set_device_calls = 0;
std::size_t g_reset_device_calls = 0;
std::size_t g_get_device_calls = 0;
std::int32_t g_current_device = -1;
std::size_t g_soc_name_calls = 0;
std::size_t g_start_calls = 0;
std::size_t g_stop_calls = 0;
uint32_t g_start_data_type = 0;
uint32_t g_stop_data_type = 0;
uint32_t g_callback_type = 0;
std::vector<uint8_t> g_kernel_inputs;
std::vector<ObservedConfig> g_configs;
const void* g_expected_args_data = nullptr;
const void* g_started_config = nullptr;
MsprofRawDataCallback g_registered_callback = nullptr;

int RealMalloc(void** pointer, std::size_t size, aclrtMemMallocPolicy)
{
    ++g_malloc_calls;
    *pointer = std::malloc(size);
    return *pointer == nullptr ? -1 : 0;
}

int RealFree(void* pointer)
{
    ++g_free_calls;
    std::free(pointer);
    return 0;
}

int RealMemcpy(void* destination, std::size_t destination_size, const void* source, std::size_t count, aclrtMemcpyKind)
{
    ++g_memcpy_calls;
    if (destination == nullptr || source == nullptr || count > destination_size) {
        return -1;
    }
    std::memmove(destination, source, count);
    return 0;
}

int RealMemset(void* destination, std::size_t destination_size, int value, std::size_t count)
{
    if (destination == nullptr || count > destination_size) {
        return -1;
    }
    std::memset(destination, value, count);
    return 0;
}

int RealLaunch(void*, uint32_t, const void* args_data, std::size_t args_size, void*)
{
    ++g_launch_calls;
    if (args_data == nullptr || args_data != g_expected_args_data || args_size != sizeof(KernelArgs)) {
        return -1;
    }
    const auto* args = static_cast<const KernelArgs*>(args_data);
    g_kernel_inputs.push_back(*args->value);
    ++*args->value;
    return 0;
}

int RealLaunchWithHostArgs(
    aclrtFuncHandle, uint32_t, aclrtStream, aclrtLaunchKernelCfg*, void*, std::size_t, aclrtPlaceHolderInfo*,
    std::size_t)
{
    ++g_launch_with_host_args_calls;
    return 0;
}

int RealSynchronize(void*)
{
    ++g_sync_calls;
    return 0;
}

int RealSetDevice(std::int32_t deviceId)
{
    ++g_set_device_calls;
    if (deviceId < 0) {
        return -1;
    }
    g_current_device = deviceId;
    return 0;
}

int RealResetDevice(std::int32_t deviceId)
{
    ++g_reset_device_calls;
    if (deviceId < 0 || deviceId != g_current_device) {
        return -1;
    }
    g_current_device = -1;
    return 0;
}

int ProfilerStart(uint32_t, const void* config, uint32_t length)
{
    ++g_start_calls;
    if (config == nullptr || length != sizeof(MsprofConfig)) {
        return -1;
    }
    const auto* msprofConfig = static_cast<const MsprofConfig*>(config);
    if (msprofConfig->configInfo.attrs == nullptr || msprofConfig->configInfo.numAttrs != 2 ||
        msprofConfig->configInfo.attrs[1].id != PROF_CONFIG_ATTR_TASK_BLOCK) {
        return -1;
    }
    ObservedConfig observed{
        msprofConfig->devNums,
        msprofConfig->devIdList[0],
        msprofConfig->profSwitch,
        msprofConfig->configInfo.attrs[0].id,
        0,
        msprofConfig->configInfo.attrs[1].value.taskBlockMode,
        {},
    };
    if (observed.primaryAttr == PROF_CONFIG_ATTR_AICORE_METRICS) {
        std::memcpy(
            observed.pmuEvents.data(), msprofConfig->configInfo.attrs[0].value.aicoreMetrics,
            sizeof(observed.pmuEvents));
    } else if (observed.primaryAttr == PROF_CONFIG_ATTR_INSTR) {
        observed.instrMode = msprofConfig->configInfo.attrs[0].value.instrMode;
    } else {
        return -1;
    }
    g_configs.push_back(observed);
    g_started_config = config;
    return 0;
}

int ProfilerStop(uint32_t, const void* config, uint32_t length)
{
    ++g_stop_calls;
    if (config == nullptr || config != g_started_config || length != sizeof(MsprofConfig)) {
        return -1;
    }
    g_started_config = nullptr;
    return 0;
}

int RegisterRawData(uint32_t, MsprofRawDataCallback callback)
{
    g_registered_callback = callback;
    return callback == nullptr ? -1 : 0;
}

std::array<uint32_t, COMPUTE_AICORE_METRICS_NUM> ExpectedPmus(std::initializer_list<uint32_t> events)
{
    std::array<uint32_t, COMPUTE_AICORE_METRICS_NUM> result{};
    result.fill(MSPROF_INVALID_AICORE_METRIC);
    std::copy(events.begin(), events.end(), result.begin());
    return result;
}

} // namespace

std::int32_t MsprofStart(uint32_t data_type, const void* config, uint32_t length)
{
    g_start_data_type = data_type;
    return ProfilerStart(data_type, config, length);
}

std::int32_t MsprofStop(uint32_t data_type, const void* config, uint32_t length)
{
    g_stop_data_type = data_type;
    return ProfilerStop(data_type, config, length);
}

std::int32_t MsprofRegisterDataCallback(uint32_t type, void* function)
{
    g_callback_type = type;
    return RegisterRawData(type, reinterpret_cast<MsprofRawDataCallback>(function));
}

extern "C" aclError aclrtGetDevice(std::int32_t* deviceId)
{
    ++g_get_device_calls;
    if (deviceId == nullptr || g_current_device < 0) {
        return ACL_ERROR_RT_CONTEXT_NULL;
    }
    *deviceId = g_current_device;
    return ACL_SUCCESS;
}

int main(int argc, char** argv)
{
    const bool useShrinkBlock = argc == 2 && std::strcmp(argv[1], "shrink") == 0;
    const bool useDebugLog = argc == 2 && std::strcmp(argv[1], "debug") == 0;
    CHECK(argc == 1 || useShrinkBlock || useDebugLog);
    if (useDebugLog) {
        CHECK(setenv("NPU_COMPUTE_DEBUG", "1", 1) == 0);
    } else {
        CHECK(unsetenv("NPU_COMPUTE_DEBUG") == 0);
    }
    const aclptiBlockResultMode blockResult = useShrinkBlock ? ACLPTI_BLOCK_RESULT_SHRINK : ACLPTI_BLOCK_RESULT_ALL;
    const std::uint32_t expectedBlockMode = useShrinkBlock ? PROF_COMPUTE_BLOCK_SHRINK : PROF_COMPUTE_ALL_BLOCK;
    CHECK(RuntimeStubSetOriginFunction("aclrtMalloc", &RealMalloc) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtMallocAlign32", &RealMalloc) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtFree", &RealFree) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtMemcpy", &RealMemcpy) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtMemset", &RealMemset) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtLaunchKernel", &RealLaunch) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtLaunchKernelWithHostArgs", &RealLaunchWithHostArgs) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtSetDevice", &RealSetDevice) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtResetDevice", &RealResetDevice) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtSynchronizeStream", &RealSynchronize) == 0);

    aclptiSubscribeHandle subscriber = nullptr;
    CHECK(aclptiSubscribe(&subscriber, nullptr, nullptr, nullptr) == ACLPTI_SUCCESS);
    CHECK(subscriber != nullptr);
    CHECK(g_registered_callback != nullptr);
    CHECK(g_callback_type == 0);
    CHECK(reinterpret_cast<aclrtMallocFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtMalloc)) == &RealMalloc);
    CHECK(
        reinterpret_cast<aclrtMallocAlign32Func>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtMallocAlign32)) ==
        &RealMalloc);
    CHECK(reinterpret_cast<aclrtFreeFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtFree)) == &RealFree);
    CHECK(reinterpret_cast<aclrtMemcpyFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtMemcpy)) == &RealMemcpy);
    CHECK(reinterpret_cast<aclrtMemsetFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtMemset)) == &RealMemset);
    CHECK(
        reinterpret_cast<aclrtLaunchKernelFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtLaunchKernel)) ==
        &RealLaunch);
    CHECK(
        reinterpret_cast<aclrtLaunchKernelWithHostArgsFunc>(
            acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtLaunchKernelWithHostArgs)) == &RealLaunchWithHostArgs);
    CHECK(
        reinterpret_cast<aclrtSynchronizeStreamFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtSynchronizeStream)) ==
        &RealSynchronize);
    CHECK(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtSetDevice) != nullptr);
    CHECK(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtLaunchKernelWithHostArgs) != nullptr);

    aclptiSubscribeHandle repeated_subscriber = nullptr;
    CHECK(aclptiSubscribe(&repeated_subscriber, nullptr, nullptr, nullptr) == ACLPTI_SUCCESS);
    CHECK(repeated_subscriber == subscriber);

    CHECK(aclptiActivityEnable(subscriber, ACLPTI_ACTIVITY_KIND_FULL, nullptr) == ACLPTI_SUCCESS);
    CHECK(aclptiActivityEnable(subscriber, ACLPTI_ACTIVITY_KIND_FULL, nullptr) == ACLPTI_SUCCESS);
    CHECK(aclptiActivityEnable(subscriber, ACLPTI_ACTIVITY_KIND_INVALID, nullptr) == ACLPTI_ERROR_INVALID_PARAMETER);
    aclptiActivityConfig invalid_activity_config{1};
    CHECK(
        aclptiActivityEnable(subscriber, ACLPTI_ACTIVITY_KIND_FULL, &invalid_activity_config) ==
        ACLPTI_ERROR_INVALID_PARAMETER);
    CHECK(aclptiActivityDisable(subscriber, ACLPTI_ACTIVITY_KIND_FULL, nullptr) == ACLPTI_SUCCESS);
    CHECK(aclptiActivityDisable(subscriber, ACLPTI_ACTIVITY_KIND_FULL, nullptr) == ACLPTI_SUCCESS);
    const char* sections[] = {"PipeUtilization", "ResourceConflictRatio"};
    aclptiRangeProfilerSetConfigParams config{sections, std::size(sections), blockResult, true, true};
    CHECK(aclptiRangeProfilerSetConfig(&config) == ACLPTI_SUCCESS);
    aclptiRangeProfilerSetConfigParams invalidConfig{nullptr, 0, ACLPTI_BLOCK_RESULT_ALL, false, false};
    CHECK(aclptiRangeProfilerSetConfig(&invalidConfig) == ACLPTI_ERROR_INVALID_PARAMETER);

    void* alignedAllocation = nullptr;
    CHECK(aclrtMallocAlign32(&alignedAllocation, 1, ACL_MEM_MALLOC_HUGE_FIRST) == 0);
    uint8_t alignedValue = 3;
    CHECK(aclrtMemcpy(alignedAllocation, 1, &alignedValue, 1, ACL_MEMCPY_HOST_TO_DEVICE) == 0);
    CHECK(aclrtFree(alignedAllocation) == 0);

    void* allocation = nullptr;
    CHECK(aclrtMalloc(&allocation, 1, ACL_MEM_MALLOC_HUGE_FIRST) == 0);
    uint8_t initial_value = 5;
    CHECK(aclrtMemcpy(allocation, 1, &initial_value, 1, ACL_MEMCPY_HOST_TO_DEVICE) == 0);

    KernelArgs args{static_cast<uint8_t*>(allocation)};
    g_expected_args_data = &args;

    CHECK(aclrtSetDevice(7) == 0);
    CHECK(g_set_device_calls == 1);
    std::string profilingLog;
    aclError firstLaunchStatus = ACL_ERROR_RT_FAILURE;
    CHECK(CaptureStderr(
        [&] { firstLaunchStatus = aclrtLaunchKernel(nullptr, 1, &args, sizeof(args), nullptr); }, &profilingLog));
    CHECK(firstLaunchStatus == ACL_SUCCESS);
    CHECK(g_start_calls == 5);
    CHECK(g_stop_calls == 5);
    CHECK(g_start_data_type == 8);
    CHECK(g_stop_data_type == 8);
    CHECK(g_launch_calls == 6);
    CHECK(g_sync_calls == 6);
    CHECK(g_get_device_calls == 1);
    CHECK(g_soc_name_calls == 0);
    CHECK((g_kernel_inputs == std::vector<std::uint8_t>{5, 5, 5, 5, 5, 5}));
    CHECK(*static_cast<std::uint8_t*>(allocation) == 6);

    CHECK(g_configs.size() == 5);
    const auto expected_first = ExpectedPmus({0, 1, 10, 36, 52, 53, 514, 515, 810, 1281});
    const auto expected_second = ExpectedPmus({1794, 1812, 1813, 11, 12, 13, 14, 15, 1344, 1366});
    const auto expected_third = ExpectedPmus({1376, 1377, 1378, 1379});
    CHECK(g_configs[0].pmuEvents == expected_first);
    CHECK(g_configs[0].profSwitch == (PROF_TASK_TIME_MASK | PROF_AICORE_METRICS_MASK));
    CHECK(g_configs[0].primaryAttr == PROF_CONFIG_ATTR_AICORE_METRICS);
    CHECK(g_configs[0].blockMode == expectedBlockMode);
    CHECK(g_configs[0].devNums == 1);
    CHECK(g_configs[0].deviceId == 7);
    CHECK(g_configs[1].pmuEvents == expected_second);
    CHECK(g_configs[1].profSwitch == (PROF_TASK_TIME_MASK | PROF_AICORE_METRICS_MASK));
    CHECK(g_configs[1].primaryAttr == PROF_CONFIG_ATTR_AICORE_METRICS);
    CHECK(g_configs[1].blockMode == expectedBlockMode);
    CHECK(g_configs[1].devNums == 1);
    CHECK(g_configs[1].deviceId == 7);
    CHECK(g_configs[2].pmuEvents == expected_third);
    CHECK(g_configs[2].profSwitch == (PROF_TASK_TIME_MASK | PROF_AICORE_METRICS_MASK));
    CHECK(g_configs[2].primaryAttr == PROF_CONFIG_ATTR_AICORE_METRICS);
    CHECK(g_configs[2].blockMode == expectedBlockMode);
    CHECK(g_configs[2].devNums == 1);
    CHECK(g_configs[2].deviceId == 7);
    CHECK(g_configs[3].profSwitch == (PROF_TASK_TIME_MASK | PROF_INSTR_MASK));
    CHECK(g_configs[3].primaryAttr == PROF_CONFIG_ATTR_INSTR);
    CHECK(g_configs[3].instrMode == PROF_COMPUTE_BIU_PERF);
    CHECK(g_configs[3].blockMode == expectedBlockMode);
    CHECK(g_configs[4].profSwitch == (PROF_TASK_TIME_MASK | PROF_INSTR_MASK));
    CHECK(g_configs[4].primaryAttr == PROF_CONFIG_ATTR_INSTR);
    CHECK(g_configs[4].instrMode == PROF_COMPUTE_PC_SAMPLING);
    CHECK(g_configs[4].blockMode == expectedBlockMode);

    if (useDebugLog) {
        CHECK(profilingLog.find("[aclpti] msprof config round=0 collectionType=8") != std::string::npos);
        CHECK(profilingLog.find("profSwitch=0x804") != std::string::npos);
        CHECK(profilingLog.find("devNums=1") != std::string::npos);
        CHECK(profilingLog.find("devId[0]=7") != std::string::npos);
        CHECK(profilingLog.find("dumpPath=\"\" dumpPathLength=0") != std::string::npos);
        CHECK(profilingLog.find("sampleConfig=\"\" sampleConfigLength=0") != std::string::npos);
        CHECK(profilingLog.find("configInfo.attrs=") != std::string::npos);
        CHECK(profilingLog.find("configInfo.numAttrs=2") != std::string::npos);
        CHECK(profilingLog.find("attr[0] id=0 aicoreMetrics[0]=0") != std::string::npos);
        CHECK(profilingLog.find("aicoreMetrics[9]=1281") != std::string::npos);
        CHECK(profilingLog.find("attr[1] id=2 taskBlockMode=1") != std::string::npos);
        CHECK(profilingLog.find("attr[0] id=1 instrMode=1") != std::string::npos);
        CHECK(profilingLog.find("attr[0] id=1 instrMode=2") != std::string::npos);
    } else {
        CHECK(profilingLog.find("msprof config round=") == std::string::npos);
    }

    const std::size_t starts_after_first_launch = g_start_calls;
    const std::size_t syncs_after_first_launch = g_sync_calls;
    initial_value = 10;
    CHECK(aclrtMemcpy(allocation, 1, &initial_value, 1, ACL_MEMCPY_HOST_TO_DEVICE) == ACL_ERROR_PROFILING_FAILURE);
    CHECK(aclrtLaunchKernel(nullptr, 1, &args, sizeof(args), nullptr) == ACL_ERROR_PROFILING_FAILURE);
    CHECK(g_start_calls == starts_after_first_launch);
    CHECK(g_sync_calls == syncs_after_first_launch);
    CHECK(g_launch_calls == 7);
    CHECK(g_get_device_calls == 1);
    CHECK(g_kernel_inputs.back() == 10);
    CHECK(*static_cast<std::uint8_t*>(allocation) == 11);

    const std::size_t starts_before_host_args = g_start_calls;
    const std::size_t syncs_before_host_args = g_sync_calls;
    CHECK(
        aclrtLaunchKernelWithHostArgs(nullptr, 1, nullptr, nullptr, nullptr, 0, nullptr, 0) ==
        ACL_ERROR_PROFILING_FAILURE);
    CHECK(g_launch_with_host_args_calls == 1);
    CHECK(g_get_device_calls == 1);
    CHECK(g_start_calls == starts_before_host_args);
    CHECK(g_sync_calls == syncs_before_host_args);

    CHECK(aclrtResetDevice(7) == 0);
    CHECK(g_reset_device_calls == 1);
    CHECK(aclrtLaunchKernel(nullptr, 1, &args, sizeof(args), nullptr) == ACL_ERROR_PROFILING_FAILURE);
    CHECK(g_get_device_calls == 1);
    CHECK(g_start_calls == starts_before_host_args);
    CHECK(g_sync_calls == syncs_before_host_args);
    CHECK(aclrtFree(allocation) == 0);
    CHECK(g_malloc_calls == 4);
    CHECK(g_free_calls == 4);
    return 0;
}
