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
#include "npu_compute/runtime_stub_api.h"
#include "profiling/prof_api.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

#define CHECK(expression)                                                                 \
    do {                                                                                  \
        if (!(expression)) {                                                              \
            std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expression); \
            return 1;                                                                     \
        }                                                                                 \
    } while (false)

constexpr int kOriginalFailure = -31;

bool gFailNextMalloc = false;
bool gFailNextFree = false;
bool gFailNextMemcpy = false;
bool gFailNextMemset = false;
bool gFailNextLaunch = false;
bool gFailNextStart = false;
std::size_t gMallocCalls = 0;
std::size_t gFreeCalls = 0;
std::size_t gMemcpyCalls = 0;
std::size_t gMemsetCalls = 0;
std::size_t gLaunchCalls = 0;
std::size_t gStartCalls = 0;

int RealMalloc(void** pointer, std::size_t size, aclrtMemMallocPolicy)
{
    ++gMallocCalls;
    if (gFailNextMalloc) {
        gFailNextMalloc = false;
        return kOriginalFailure;
    }
    *pointer = std::malloc(size);
    return *pointer == nullptr ? -1 : 0;
}

int RealFree(void* pointer)
{
    ++gFreeCalls;
    if (gFailNextFree) {
        gFailNextFree = false;
        return kOriginalFailure;
    }
    std::free(pointer);
    return 0;
}

int RealMemcpy(void* destination, std::size_t destinationSize, const void* source, std::size_t count, aclrtMemcpyKind)
{
    ++gMemcpyCalls;
    if (gFailNextMemcpy) {
        gFailNextMemcpy = false;
        return kOriginalFailure;
    }
    if (destination == nullptr || source == nullptr || count > destinationSize) {
        return -1;
    }
    std::memmove(destination, source, count);
    return 0;
}

int RealMemset(void* destination, std::size_t destinationSize, int value, std::size_t count)
{
    ++gMemsetCalls;
    if (gFailNextMemset) {
        gFailNextMemset = false;
        return kOriginalFailure;
    }
    if (destination == nullptr || count > destinationSize) {
        return -1;
    }
    std::memset(destination, value, count);
    return 0;
}

int RealLaunch(void*, std::uint32_t, const void*, std::size_t, void*)
{
    ++gLaunchCalls;
    if (gFailNextLaunch) {
        gFailNextLaunch = false;
        return kOriginalFailure;
    }
    return 0;
}

int RealSynchronize(void*) { return 0; }

} // namespace

std::int32_t MsprofStart(std::uint32_t, const void*, std::uint32_t)
{
    ++gStartCalls;
    if (gFailNextStart) {
        gFailNextStart = false;
        return kOriginalFailure;
    }
    return 0;
}

std::int32_t MsprofStop(std::uint32_t, const void*, std::uint32_t) { return 0; }

std::int32_t MsprofRegisterDataCallback(std::uint32_t, void* callback) { return callback == nullptr ? -1 : 0; }

int main()
{
    CHECK(RuntimeStubSetOriginFunction("aclrtMalloc", &RealMalloc) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtFree", &RealFree) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtMemcpy", &RealMemcpy) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtMemset", &RealMemset) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtLaunchKernel", &RealLaunch) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtSynchronizeStream", &RealSynchronize) == 0);

    aclptiSubscribeHandle subscriber = nullptr;
    CHECK(aclptiSubscribe(&subscriber, nullptr, nullptr, nullptr) == ACLPTI_SUCCESS);
    CHECK(subscriber != nullptr);
    const char* sections[] = {"PipeUtilization"};
    aclptiRangeProfilerSetConfigParams config{sections, 1};
    CHECK(aclptiRangeProfilerSetConfig(&config) == ACLPTI_SUCCESS);

    void* failedAllocation = nullptr;
    gFailNextMalloc = true;
    const std::size_t mallocCallsBeforeFailure = gMallocCalls;
    CHECK(aclrtMalloc(&failedAllocation, 8, ACL_MEM_MALLOC_HUGE_FIRST) == kOriginalFailure);
    CHECK(failedAllocation == nullptr);
    CHECK(gMallocCalls == mallocCallsBeforeFailure + 1);

    void* allocation = nullptr;
    CHECK(aclrtMalloc(&allocation, 8, ACL_MEM_MALLOC_HUGE_FIRST) == 0);
    CHECK(allocation != nullptr);
    const std::uint8_t value = 7;

    gFailNextMemcpy = true;
    const std::size_t memcpyCallsBeforeFailure = gMemcpyCalls;
    CHECK(aclrtMemcpy(allocation, 8, &value, 1, ACL_MEMCPY_HOST_TO_DEVICE) == kOriginalFailure);
    CHECK(gMemcpyCalls == memcpyCallsBeforeFailure + 1);

    gFailNextMemset = true;
    const std::size_t memsetCallsBeforeFailure = gMemsetCalls;
    CHECK(aclrtMemset(allocation, 8, 0, 8) == kOriginalFailure);
    CHECK(gMemsetCalls == memsetCallsBeforeFailure + 1);

    gFailNextLaunch = true;
    const std::size_t launchCallsBeforeFailure = gLaunchCalls;
    const std::size_t startsBeforeFailure = gStartCalls;
    CHECK(aclrtLaunchKernel(nullptr, 1, nullptr, 0, nullptr) == kOriginalFailure);
    CHECK(gLaunchCalls == launchCallsBeforeFailure + 1);
    CHECK(gStartCalls == startsBeforeFailure);

    gFailNextStart = true;
    const std::size_t launchesBeforeStartFailure = gLaunchCalls;
    const std::size_t startsBeforeStartFailure = gStartCalls;
    CHECK(aclrtLaunchKernel(nullptr, 1, nullptr, 0, nullptr) == kOriginalFailure);
    CHECK(gLaunchCalls == launchesBeforeStartFailure + 1);
    CHECK(gStartCalls == startsBeforeStartFailure + 1);

    const std::size_t launchesBeforeRetry = gLaunchCalls;
    const std::size_t startsBeforeRetry = gStartCalls;
    CHECK(aclrtLaunchKernel(nullptr, 1, nullptr, 0, nullptr) == static_cast<int>(ACLPTI_ERROR_NOT_INITIALIZED));
    CHECK(gLaunchCalls == launchesBeforeRetry + 1);
    CHECK(gStartCalls == startsBeforeRetry);

    gFailNextFree = true;
    const std::size_t freeCallsBeforeFailure = gFreeCalls;
    CHECK(aclrtFree(allocation) == kOriginalFailure);
    CHECK(gFreeCalls == freeCallsBeforeFailure + 1);
    CHECK(aclrtFree(allocation) == 0);
    CHECK(gFreeCalls == freeCallsBeforeFailure + 3);
    return 0;
}
