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
#include "injection/runtime_stub_api.h"
#include "profiling/prof_api.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <string_view>

#include <unistd.h>

namespace {

bool gFailNextHostAllocation = false;

} // namespace

void* operator new(std::size_t size)
{
    if (gFailNextHostAllocation) {
        gFailNextHostAllocation = false;
        throw std::bad_alloc();
    }
    void* pointer = std::malloc(size);
    if (pointer == nullptr) {
        throw std::bad_alloc();
    }
    return pointer;
}

void operator delete(void* pointer) noexcept { std::free(pointer); }

void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }

namespace {

#define CHECK(expression)                                                                 \
    do {                                                                                  \
        if (!(expression)) {                                                              \
            std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expression); \
            return 1;                                                                     \
        }                                                                                 \
    } while (false)

constexpr aclError kMallocFailure = -41;
constexpr aclError kFreeFailure = -42;
constexpr aclError kSyncFailure = -43;
constexpr aclError kMemcpyFailure = -44;

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

std::size_t gMallocCalls = 0;
std::size_t gFreeCalls = 0;
std::size_t gMemcpyCalls = 0;
std::size_t gLaunchCalls = 0;
std::size_t gSyncCalls = 0;
std::size_t gStartCalls = 0;
std::size_t gShutdownCalls = 0;
std::size_t gFailMallocCall = 0;
std::size_t gFailFreeCall = 0;
std::size_t gFailMemcpyCall = 0;
std::size_t gFailSyncCall = 0;

aclError RealMalloc(void** pointer, std::size_t size, aclrtMemMallocPolicy)
{
    ++gMallocCalls;
    if (gMallocCalls == gFailMallocCall) {
        return kMallocFailure;
    }
    *pointer = std::malloc(size);
    return *pointer == nullptr ? ACL_ERROR_BAD_ALLOC : ACL_SUCCESS;
}

aclError RealFree(void* pointer)
{
    ++gFreeCalls;
    if (gFreeCalls == gFailFreeCall) {
        return kFreeFailure;
    }
    std::free(pointer);
    return ACL_SUCCESS;
}

aclError RealMemcpy(
    void* destination, std::size_t destinationSize, const void* source, std::size_t count, aclrtMemcpyKind)
{
    ++gMemcpyCalls;
    if (gMemcpyCalls == gFailMemcpyCall) {
        return kMemcpyFailure;
    }
    if (destination == nullptr || source == nullptr || count > destinationSize) {
        return ACL_ERROR_INVALID_PARAM;
    }
    std::memmove(destination, source, count);
    return ACL_SUCCESS;
}

aclError RealMemset(void* destination, std::size_t destinationSize, std::int32_t value, std::size_t count)
{
    if (destination == nullptr || count > destinationSize) {
        return ACL_ERROR_INVALID_PARAM;
    }
    std::memset(destination, value, count);
    return ACL_SUCCESS;
}

aclError RealLaunch(void*, std::uint32_t, const void*, std::size_t, void*)
{
    ++gLaunchCalls;
    return ACL_SUCCESS;
}

aclError RealSynchronize(void*)
{
    ++gSyncCalls;
    return gSyncCalls == gFailSyncCall ? kSyncFailure : ACL_SUCCESS;
}

aclptiResult FailingShutdownCallback(void*) { return ACLPTI_ERROR_INTERNAL; }

aclptiResult CountingShutdownCallback(void* userData)
{
    if (userData != nullptr) {
        return ACLPTI_ERROR_INVALID_PARAMETER;
    }
    ++gShutdownCalls;
    return ACLPTI_SUCCESS;
}

int Initialize()
{
    CHECK(RuntimeStubSetOriginFunction("aclrtMalloc", &RealMalloc) == ACL_SUCCESS);
    CHECK(RuntimeStubSetOriginFunction("aclrtFree", &RealFree) == ACL_SUCCESS);
    CHECK(RuntimeStubSetOriginFunction("aclrtMemcpy", &RealMemcpy) == ACL_SUCCESS);
    CHECK(RuntimeStubSetOriginFunction("aclrtMemset", &RealMemset) == ACL_SUCCESS);
    CHECK(RuntimeStubSetOriginFunction("aclrtLaunchKernel", &RealLaunch) == ACL_SUCCESS);
    CHECK(RuntimeStubSetOriginFunction("aclrtSynchronizeStream", &RealSynchronize) == ACL_SUCCESS);
    aclptiSubscribeHandle subscriber = nullptr;
    CHECK(aclptiSubscribe(&subscriber, nullptr, nullptr, nullptr) == ACLPTI_SUCCESS);
    CHECK(subscriber != nullptr);
    const char* sections[] = {"PipeUtilization"};
    aclptiRangeProfilerSetConfigParams config{sections, 1};
    CHECK(aclptiRangeProfilerSetConfig(&config) == ACLPTI_SUCCESS);
    return 0;
}

int TestShadowMallocStopsProfiling()
{
    CHECK(aclptiRegisterDataModuleShutdownCallback(&CountingShutdownCallback, nullptr) == ACLPTI_SUCCESS);
    gFailMallocCall = 2;
    void* allocation = nullptr;
    const std::size_t freeCallsBeforeMalloc = gFreeCalls;
    aclError status = ACL_ERROR_INTERNAL_ERROR;
    std::string logText;
    CHECK(CaptureStderr([&] { status = aclrtMalloc(&allocation, 16, ACL_MEM_MALLOC_HUGE_FIRST); }, &logText));
    CHECK(status == ACL_ERROR_PROFILING_FAILURE);
    CHECK(allocation != nullptr);
    CHECK(gMallocCalls == 2);
    CHECK(gFreeCalls == freeCallsBeforeMalloc);
    CHECK(logText.find("[aclpti] error operation=shadow_malloc") != std::string::npos);
    CHECK(logText.find("status=-41") != std::string::npos);
    CHECK(logText.find("rollback_status=") == std::string::npos);
    CHECK(logText.find("domain=") == std::string::npos);
    CHECK(logText.find("consistency=") == std::string::npos);
    CHECK(gShutdownCalls == 1);

    const std::size_t startsBeforeLaunch = gStartCalls;
    CHECK(aclrtLaunchKernel(nullptr, 1, nullptr, 0, nullptr) == ACL_ERROR_PROFILING_FAILURE);
    CHECK(gLaunchCalls == 1);
    CHECK(gStartCalls == startsBeforeLaunch);
    CHECK(gShutdownCalls == 1);

    CHECK(aclrtFree(allocation) == ACL_SUCCESS);
    CHECK(gFreeCalls == freeCallsBeforeMalloc + 1);
    return 0;
}

int TestReplaySynchronizeFailure()
{
    gFailSyncCall = 2;
    const std::size_t memcpyCallsBeforeLaunch = gMemcpyCalls;
    aclError launchStatus = ACL_ERROR_INTERNAL_ERROR;
    std::string logText;
    CHECK(CaptureStderr([&] { launchStatus = aclrtLaunchKernel(nullptr, 1, nullptr, 0, nullptr); }, &logText));

    CHECK(launchStatus == ACL_ERROR_INTERNAL_ERROR);
    CHECK(logText.find("[aclpti] error operation=replay_sync") != std::string::npos);
    CHECK(logText.find("status=-43") != std::string::npos);
    CHECK(logText.find("domain=") == std::string::npos);
    CHECK(gLaunchCalls == 2);
    CHECK(gStartCalls == 1);
    CHECK(gSyncCalls == 2);
    CHECK(gMemcpyCalls == memcpyCallsBeforeLaunch);
    const std::size_t startsBeforeRetry = gStartCalls;
    CHECK(aclrtLaunchKernel(nullptr, 1, nullptr, 0, nullptr) == ACL_ERROR_PROFILING_FAILURE);
    CHECK(gLaunchCalls == 3);
    CHECK(gStartCalls == startsBeforeRetry);
    return 0;
}

int TestInitialSynchronizeFailure()
{
    gFailSyncCall = 1;
    aclError launchStatus = ACL_SUCCESS;
    std::string logText;
    CHECK(CaptureStderr([&] { launchStatus = aclrtLaunchKernel(nullptr, 1, nullptr, 0, nullptr); }, &logText));

    CHECK(launchStatus == ACL_ERROR_INTERNAL_ERROR);
    CHECK(logText.find("[aclpti] error operation=replay_initial_sync") != std::string::npos);
    CHECK(logText.find("status=-43") != std::string::npos);
    CHECK(gLaunchCalls == 1);
    CHECK(gStartCalls == 0);
    CHECK(gSyncCalls == 1);
    return 0;
}

int TestReplayInitialSynchronize()
{
    CHECK(aclrtLaunchKernel(nullptr, 1, nullptr, 0, nullptr) == ACL_SUCCESS);
    CHECK(gLaunchCalls >= 2);
    CHECK(gSyncCalls == gLaunchCalls);
    CHECK(gStartCalls + 1 == gLaunchCalls);
    CHECK(gMemcpyCalls == 0);
    return 0;
}

int TestProfilingFlowFailure()
{
    CHECK(aclptiRegisterDataModuleShutdownCallback(&FailingShutdownCallback, nullptr) == ACLPTI_SUCCESS);
    aclError status = ACL_ERROR_INTERNAL_ERROR;
    std::string logText;
    CHECK(CaptureStderr([&] { status = aclrtLaunchKernel(nullptr, 1, nullptr, 0, nullptr); }, &logText));
    CHECK(status == ACL_ERROR_PROFILING_FAILURE);
    CHECK(logText.find("[aclpti] error operation=data_shutdown") != std::string::npos);
    CHECK(logText.find("status=8") != std::string::npos);
    CHECK(logText.find("domain=") == std::string::npos);
    return 0;
}

int TestShadowFreeRetry()
{
    void* allocation = nullptr;
    CHECK(aclrtMalloc(&allocation, 16, ACL_MEM_MALLOC_HUGE_FIRST) == ACL_SUCCESS);
    CHECK(gMallocCalls == 2);
    gFailFreeCall = 2;
    aclError status = ACL_ERROR_INTERNAL_ERROR;
    bool threwBadAlloc = false;
    std::string logText;
    CHECK(CaptureStderr(
        [&] {
            gFailNextHostAllocation = true;
            try {
                status = aclrtFree(allocation);
            } catch (const std::bad_alloc&) {
                threwBadAlloc = true;
            }
            gFailNextHostAllocation = false;
        },
        &logText));
    CHECK(!threwBadAlloc);
    CHECK(status == ACL_ERROR_PROFILING_FAILURE);
    CHECK(logText.find("[aclpti] error operation=shadow_free") != std::string::npos);
    CHECK(logText.find("status=-42") != std::string::npos);
    CHECK(logText.find("domain=") == std::string::npos);

    void* cleanupAllocation = nullptr;
    CHECK(aclrtMalloc(&cleanupAllocation, 16, ACL_MEM_MALLOC_HUGE_FIRST) == ACL_ERROR_PROFILING_FAILURE);
    CHECK(aclrtFree(cleanupAllocation) == ACL_SUCCESS);
    CHECK(gFreeCalls == 4);
    return 0;
}

int TestNullFreeDoesNotStopProfiling()
{
    CHECK(aclptiRegisterDataModuleShutdownCallback(&CountingShutdownCallback, nullptr) == ACLPTI_SUCCESS);
    CHECK(aclrtFree(nullptr) == ACL_SUCCESS);
    CHECK(gFreeCalls == 1);
    CHECK(gShutdownCalls == 0);

    CHECK(aclrtLaunchKernel(nullptr, 1, nullptr, 0, nullptr) == ACL_SUCCESS);
    CHECK(gStartCalls == 2);
    CHECK(gShutdownCalls == 1);
    return 0;
}

int TestMissingMemcpyMirrorStopsProfiling()
{
    CHECK(aclptiRegisterDataModuleShutdownCallback(&CountingShutdownCallback, nullptr) == ACLPTI_SUCCESS);
    void* allocation = nullptr;
    CHECK(RealMalloc(&allocation, 16, ACL_MEM_MALLOC_HUGE_FIRST) == ACL_SUCCESS);
    std::uint8_t value = 9;
    CHECK(aclrtMemcpy(allocation, 16, &value, sizeof(value), ACL_MEMCPY_HOST_TO_DEVICE) == ACL_ERROR_PROFILING_FAILURE);
    CHECK(*static_cast<std::uint8_t*>(allocation) == value);
    CHECK(gShutdownCalls == 1);

    const std::size_t startsBeforeLaunch = gStartCalls;
    CHECK(aclrtLaunchKernel(nullptr, 1, nullptr, 0, nullptr) == ACL_ERROR_PROFILING_FAILURE);
    CHECK(gStartCalls == startsBeforeLaunch);
    CHECK(gShutdownCalls == 1);
    CHECK(aclrtFree(allocation) == ACL_SUCCESS);
    return 0;
}

int TestMissingMemsetMirrorStopsProfiling()
{
    CHECK(aclptiRegisterDataModuleShutdownCallback(&CountingShutdownCallback, nullptr) == ACLPTI_SUCCESS);
    void* allocation = nullptr;
    CHECK(RealMalloc(&allocation, 16, ACL_MEM_MALLOC_HUGE_FIRST) == ACL_SUCCESS);
    CHECK(aclrtMemset(allocation, 16, 5, 16) == ACL_ERROR_PROFILING_FAILURE);
    CHECK(*static_cast<std::uint8_t*>(allocation) == 5);
    CHECK(gShutdownCalls == 1);

    const std::size_t startsBeforeLaunch = gStartCalls;
    CHECK(aclrtLaunchKernel(nullptr, 1, nullptr, 0, nullptr) == ACL_ERROR_PROFILING_FAILURE);
    CHECK(gStartCalls == startsBeforeLaunch);
    CHECK(gShutdownCalls == 1);
    CHECK(aclrtFree(allocation) == ACL_SUCCESS);
    return 0;
}

int TestMallocMissingFreeStopsProfiling()
{
    CHECK(aclptiRegisterDataModuleShutdownCallback(&CountingShutdownCallback, nullptr) == ACLPTI_SUCCESS);
    CHECK(RuntimeStubClearOrigin("aclrtFree") == ACL_SUCCESS);
    void* allocation = nullptr;
    CHECK(aclrtMalloc(&allocation, 16, ACL_MEM_MALLOC_HUGE_FIRST) == ACL_ERROR_PROFILING_FAILURE);
    CHECK(allocation != nullptr);
    CHECK(gMallocCalls == 1);
    CHECK(gShutdownCalls == 1);

    CHECK(RuntimeStubSetOriginFunction("aclrtFree", &RealFree) == ACL_SUCCESS);
    CHECK(aclrtFree(allocation) == ACL_SUCCESS);
    CHECK(gFreeCalls == 1);
    return 0;
}

int TestShadowMetadataAllocationFailure()
{
    CHECK(aclptiRegisterDataModuleShutdownCallback(&CountingShutdownCallback, nullptr) == ACLPTI_SUCCESS);
    void* allocation = nullptr;
    aclError status = ACL_ERROR_INTERNAL_ERROR;
    bool threwBadAlloc = false;
    gFailNextHostAllocation = true;
    try {
        status = aclrtMalloc(&allocation, 16, ACL_MEM_MALLOC_HUGE_FIRST);
    } catch (const std::bad_alloc&) {
        threwBadAlloc = true;
    }
    gFailNextHostAllocation = false;

    CHECK(!threwBadAlloc);
    CHECK(status == ACL_ERROR_PROFILING_FAILURE);
    CHECK(allocation != nullptr);
    CHECK(gMallocCalls == 2);
    CHECK(gFreeCalls == 1);
    CHECK(gShutdownCalls == 1);
    CHECK(aclrtFree(allocation) == ACL_SUCCESS);
    CHECK(gFreeCalls == 2);
    return 0;
}

int TestShadowMemcpyStopsProfiling()
{
    CHECK(aclptiRegisterDataModuleShutdownCallback(&CountingShutdownCallback, nullptr) == ACLPTI_SUCCESS);

    void* allocation = nullptr;
    CHECK(aclrtMalloc(&allocation, 16, ACL_MEM_MALLOC_HUGE_FIRST) == ACL_SUCCESS);
    std::uint8_t value = 7;
    gFailMemcpyCall = gMemcpyCalls + 2;
    aclError status = ACL_ERROR_INTERNAL_ERROR;
    std::string logText;
    CHECK(CaptureStderr(
        [&] { status = aclrtMemcpy(allocation, 16, &value, sizeof(value), ACL_MEMCPY_HOST_TO_DEVICE); }, &logText));
    CHECK(status == ACL_ERROR_PROFILING_FAILURE);
    CHECK(logText.find("[aclpti] error operation=shadow_memcpy status=-44") != std::string::npos);
    CHECK(gShutdownCalls == 1);

    const std::size_t startsBeforeLaunch = gStartCalls;
    CHECK(aclrtLaunchKernel(nullptr, 1, nullptr, 0, nullptr) == ACL_ERROR_PROFILING_FAILURE);
    CHECK(gLaunchCalls == 1);
    CHECK(gStartCalls == startsBeforeLaunch);
    CHECK(gShutdownCalls == 1);
    CHECK(aclrtFree(allocation) == ACL_SUCCESS);
    return 0;
}

} // namespace

std::int32_t MsprofStart(std::uint32_t, const void*, std::uint32_t)
{
    ++gStartCalls;
    return 0;
}

std::int32_t MsprofStop(std::uint32_t, const void*, std::uint32_t) { return 0; }

std::int32_t MsprofRegisterDataCallback(std::uint32_t, void* callback) { return callback == nullptr ? -1 : 0; }

extern "C" aclError aclrtGetDevice(std::int32_t* deviceId)
{
    if (deviceId == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *deviceId = 0;
    return ACL_SUCCESS;
}

int main(int argc, char** argv)
{
    CHECK(argc == 2);
    CHECK(setenv("NPU_COMPUTE_DEBUG", "1", 1) == 0);
    CHECK(Initialize() == 0);
    const std::string_view scenario(argv[1]);
    if (scenario == "shadow-malloc-stop") {
        return TestShadowMallocStopsProfiling();
    }
    if (scenario == "replay-sync") {
        return TestReplaySynchronizeFailure();
    }
    if (scenario == "replay-initial-sync-failure") {
        return TestInitialSynchronizeFailure();
    }
    if (scenario == "replay-initial-sync") {
        return TestReplayInitialSynchronize();
    }
    if (scenario == "profiling-flow") {
        return TestProfilingFlowFailure();
    }
    if (scenario == "shadow-free") {
        return TestShadowFreeRetry();
    }
    if (scenario == "null-free") {
        return TestNullFreeDoesNotStopProfiling();
    }
    if (scenario == "shadow-memcpy-stop") {
        return TestShadowMemcpyStopsProfiling();
    }
    if (scenario == "missing-memcpy-mirror") {
        return TestMissingMemcpyMirrorStopsProfiling();
    }
    if (scenario == "missing-memset-mirror") {
        return TestMissingMemsetMirrorStopsProfiling();
    }
    if (scenario == "malloc-missing-free") {
        return TestMallocMissingFreeStopsProfiling();
    }
    if (scenario == "shadow-metadata-oom") {
        return TestShadowMetadataAllocationFailure();
    }
    return 1;
}
