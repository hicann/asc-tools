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
#include <string>

#include <unistd.h>

namespace {

#define CHECK(expression)                                                                 \
    do {                                                                                  \
        if (!(expression)) {                                                              \
            std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expression); \
            return 1;                                                                     \
        }                                                                                 \
    } while (false)

constexpr int kOriginalFailure = -31;

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
std::size_t gShutdownCalls = 0;
bool gFailShutdown = false;
aclError gExitStatus = ACL_SUCCESS;
std::size_t gShutdownAtExit = 0;
aclptiResult CountShutdown(void*)
{
    ++gShutdownCalls;
    return gFailShutdown ? ACLPTI_ERROR_PROFILING_FAILED : ACLPTI_SUCCESS;
}
void RuntimeCallback(void*, aclptiCallbackDomain, aclptiCallbackId, const aclptiCallbackData* data)
{
    if (data->callbackSite == ACLPTI_API_EXIT) {
        gExitStatus = data->retval;
        gShutdownAtExit = gShutdownCalls;
    }
}
std::size_t gGetDeviceCalls = 0;
std::uint8_t* gKernelValue = nullptr;

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

int RealLaunch(void*, uint32_t, const void*, std::size_t, void*)
{
    ++gLaunchCalls;
    if (gFailNextLaunch) {
        gFailNextLaunch = false;
        return kOriginalFailure;
    }
    if (gKernelValue != nullptr) {
        ++*gKernelValue;
    }
    return 0;
}

int RealSynchronize(void*) { return 0; }

int gBinaryTokens[16];
int gBinaryLoads = 0;
int gBinaryQueries = 0;
int gBinaryUnloads = 0;
int gFailBinaryLoad = -1;
void* gFailBinaryQuery = nullptr;
void* gFailBinaryUnload = nullptr;
aclError RealBinaryLoad(const void*, std::size_t, const aclrtBinaryLoadOptions*, aclrtBinHandle* result)
{
    const int index = gBinaryLoads++;
    if (index == gFailBinaryLoad)
        return kOriginalFailure;
    *result = &gBinaryTokens[index];
    return ACL_SUCCESS;
}
aclError RealBinaryFunction(aclrtBinHandle binary, const char*, aclrtFuncHandle* result)
{
    ++gBinaryQueries;
    if (binary == gFailBinaryQuery)
        return kOriginalFailure;
    *result = binary;
    return ACL_SUCCESS;
}
aclError RealBinaryEntry(aclrtBinHandle binary, std::uint64_t, aclrtFuncHandle* result)
{
    return RealBinaryFunction(binary, "kernel", result);
}
aclError RealBinaryUnload(aclrtBinHandle binary)
{
    if (binary == gFailBinaryUnload)
        return kOriginalFailure;
    ++gBinaryUnloads;
    return ACL_SUCCESS;
}

} // namespace

std::int32_t MsprofStart(uint32_t, const void*, uint32_t)
{
    ++gStartCalls;
    if (gFailNextStart) {
        gFailNextStart = false;
        return kOriginalFailure;
    }
    return 0;
}

std::int32_t MsprofStop(uint32_t, const void*, uint32_t) { return 0; }

std::int32_t MsprofRegisterDataCallback(uint32_t, void* callback) { return callback == nullptr ? -1 : 0; }

extern "C" aclError aclrtGetDevice(std::int32_t* deviceId)
{
    ++gGetDeviceCalls;
    if (deviceId == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *deviceId = 0;
    return ACL_SUCCESS;
}

int main(int argc, char** argv)
{
    CHECK(RuntimeStubSetOriginFunction("aclrtMalloc", &RealMalloc) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtFree", &RealFree) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtMemcpy", &RealMemcpy) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtMemset", &RealMemset) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtLaunchKernel", &RealLaunch) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtSynchronizeStream", &RealSynchronize) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtBinaryLoadFromData", &RealBinaryLoad) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtBinaryGetFunction", &RealBinaryFunction) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtBinaryGetFunctionByEntry", &RealBinaryEntry) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtBinaryUnLoad", &RealBinaryUnload) == 0);

    aclptiSubscribeHandle subscriber = nullptr;
    CHECK(aclptiSubscribe(&subscriber, &RuntimeCallback, nullptr, nullptr) == ACLPTI_SUCCESS);
    CHECK(subscriber != nullptr);
    for (aclptiCallbackId cbid = 0; cbid < ACLPTI_RUNTIME_CBID_SIZE; ++cbid) {
        CHECK(aclptiEnableCallback(true, subscriber, ACLPTI_CB_DOMAIN_RUNTIME_API, cbid) == ACLPTI_SUCCESS);
    }
    CHECK(aclrtSetDevice(0) == ACL_SUCCESS);
    const char* sections[] = {"PipeUtilization"};
    aclptiRangeProfilerSetConfigParams config{sections, 1};
    CHECK(aclptiRangeProfilerSetConfig(&config) == ACLPTI_SUCCESS);

    CHECK(aclptiRegisterDataModuleShutdownCallback(&CountShutdown, nullptr) == ACLPTI_SUCCESS);
    if (argc > 1) {
        const std::string scenario = argv[1];
        if (scenario.rfind("binary-", 0) == 0) {
            const char image[] = "ELF";
            aclrtBinHandle binary = nullptr;
            aclrtFuncHandle function = nullptr;
            if (scenario == "binary-disabled") {
                CHECK(aclrtBinaryLoadFromData(image, sizeof(image), nullptr, &binary) == ACL_SUCCESS);
                CHECK(gBinaryLoads == 1);
                CHECK(aclrtBinaryGetFunction(binary, "kernel", &function) == ACL_SUCCESS);
                CHECK(aclrtBinaryGetFunctionByEntry(binary, 42, &function) == ACL_SUCCESS);
                CHECK(gBinaryQueries == 2 && gShutdownCalls == 0);
                CHECK(aclrtBinaryUnLoad(binary) == ACL_SUCCESS);
                return 0;
            }
            config.collectPipeline = true;
            CHECK(aclptiRangeProfilerSetConfig(&config) == ACLPTI_SUCCESS);
            aclError status;
            if (scenario == "binary-load" || scenario == "binary-companion-load") {
                gFailBinaryLoad = scenario == "binary-load" ? 0 : 1;
                status = aclrtBinaryLoadFromData(image, sizeof(image), nullptr, &binary);
            } else {
                CHECK(aclrtBinaryLoadFromData(image, sizeof(image), nullptr, &binary) == ACL_SUCCESS);
                CHECK(gBinaryLoads == 2);
                if (scenario == "binary-unload" || scenario == "binary-companion-unload") {
                    gFailBinaryUnload = scenario == "binary-unload" ? binary : &gBinaryTokens[1];
                    status = aclrtBinaryUnLoad(binary);
                } else {
                    gFailBinaryQuery = scenario.find("companion") != std::string::npos ? &gBinaryTokens[1] : binary;
                    status = scenario.find("entry") != std::string::npos ?
                                 aclrtBinaryGetFunctionByEntry(binary, 42, &function) :
                                 aclrtBinaryGetFunction(binary, "kernel", &function);
                }
            }
            const bool auxiliary = scenario == "binary-companion-load" || scenario == "binary-companion-name" ||
                                   scenario == "binary-companion-entry" || scenario == "binary-companion-unload";
            CHECK(status == (auxiliary ? ACL_ERROR_PROFILING_FAILURE : kOriginalFailure));
            if (auxiliary)
                CHECK(binary == &gBinaryTokens[0]);
            if (scenario == "binary-companion-name" || scenario == "binary-companion-entry") {
                CHECK(function == binary); // Preserve the application's successful original lookup.
            }
            CHECK(gExitStatus == status && gShutdownAtExit == 1);
            CHECK(gShutdownCalls == 1);
            gFailBinaryUnload = nullptr;
            gFailBinaryQuery = nullptr;
            if (binary != nullptr)
                CHECK(aclrtBinaryUnLoad(binary) == ACL_SUCCESS);
            const int previousLoads = gBinaryLoads;
            CHECK(aclrtBinaryLoadFromData(image, sizeof(image), nullptr, &binary) == ACL_SUCCESS);
            CHECK(gBinaryLoads == previousLoads + 1);
            const int previousQueries = gBinaryQueries;
            CHECK(aclrtBinaryGetFunction(binary, "kernel", &function) == ACL_SUCCESS);
            CHECK(aclrtBinaryGetFunctionByEntry(binary, 42, &function) == ACL_SUCCESS);
            CHECK(gBinaryQueries == previousQueries + 2);
            CHECK(aclrtBinaryUnLoad(binary) == ACL_SUCCESS);
            CHECK(aclrtLaunchKernel(nullptr, 1, nullptr, 0, nullptr) == ACL_ERROR_PROFILING_FAILURE);
            CHECK(gLaunchCalls == 1 && gStartCalls == 0 && gShutdownCalls == 1);
            return 0;
        }
        void* pointer = nullptr;
        CHECK(aclrtMalloc(&pointer, 8, ACL_MEM_MALLOC_HUGE_FIRST) == ACL_SUCCESS);
        aclError status = ACL_SUCCESS;
        if (scenario == "malloc" || scenario == "shutdown-failure") {
            gFailShutdown = scenario == "shutdown-failure";
            gFailNextMalloc = true;
            void* failed = nullptr;
            status = aclrtMalloc(&failed, 8, ACL_MEM_MALLOC_HUGE_FIRST);
        } else if (scenario == "free") {
            gFailNextFree = true;
            status = aclrtFree(pointer);
        } else if (scenario == "memcpy") {
            gFailNextMemcpy = true;
            status = aclrtMemcpy(pointer, 8, pointer, 8, ACL_MEMCPY_HOST_TO_DEVICE);
        } else if (scenario == "memset") {
            gFailNextMemset = true;
            status = aclrtMemset(pointer, 8, 0, 8);
        } else if (scenario == "launch") {
            gFailNextLaunch = true;
            status = aclrtLaunchKernel(nullptr, 1, nullptr, 0, nullptr);
        } else if (scenario == "missing") {
            CHECK(RuntimeStubClearOrigin("aclrtSynchronizeStream") == ACL_SUCCESS);
            CHECK(setenv("NPU_COMPUTE_DEBUG", "1", 1) == 0);
            std::string log;
            CHECK(CaptureStderr([&] { status = aclrtSynchronizeStream(nullptr); }, &log));
            CHECK(log.find("operation=original_lookup") != std::string::npos);
            CHECK(log.find("operation=runtime_call") == std::string::npos);
            CHECK(unsetenv("NPU_COMPUTE_DEBUG") == 0);
        } else {
            return 1;
        }
        CHECK(status == (scenario == "missing" ? ACL_ERROR_INTERNAL_ERROR : kOriginalFailure));
        CHECK(gExitStatus == status && gShutdownAtExit == 1);
        CHECK(gShutdownCalls == 1);
        CHECK(gStartCalls == 0);
        const auto launches = gLaunchCalls;
        CHECK(aclrtLaunchKernel(nullptr, 1, nullptr, 0, nullptr) == ACL_ERROR_PROFILING_FAILURE);
        CHECK(gLaunchCalls == launches + 1 && gStartCalls == 0 && gShutdownCalls == 1);
        CHECK(aclrtFree(pointer) == ACL_SUCCESS);
        return 0;
    }

    void* allocation = nullptr;
    CHECK(aclrtMalloc(&allocation, 8, ACL_MEM_MALLOC_HUGE_FIRST) == 0);
    CHECK(allocation != nullptr);
    const std::uint8_t value = 7;
    const std::uint8_t initialValue = 3;
    CHECK(aclrtMemcpy(allocation, 8, &initialValue, sizeof(initialValue), ACL_MEMCPY_HOST_TO_DEVICE) == ACL_SUCCESS);

    gFailNextStart = true;
    gKernelValue = static_cast<std::uint8_t*>(allocation);
    CHECK(setenv("NPU_COMPUTE_DEBUG", "1", 1) == 0);
    const std::size_t launchesBeforeStartFailure = gLaunchCalls;
    const std::size_t startsBeforeStartFailure = gStartCalls;
    aclError profilingStatus = ACL_ERROR_INTERNAL_ERROR;
    std::string profilingLog;
    CHECK(CaptureStderr([&] { profilingStatus = aclrtLaunchKernel(nullptr, 1, nullptr, 0, nullptr); }, &profilingLog));
    CHECK(profilingStatus == ACL_ERROR_INTERNAL_ERROR);
    CHECK(*gKernelValue == initialValue);
    CHECK(gLaunchCalls == launchesBeforeStartFailure + 1);
    CHECK(gStartCalls == startsBeforeStartFailure + 1);
    CHECK(gGetDeviceCalls == 1);
    CHECK(profilingLog.find("[aclpti] error operation=prof_start") != std::string::npos);
    CHECK(profilingLog.find("operation=runtime_call") != std::string::npos);
    CHECK(profilingLog.find("api=aclrtLaunchKernel") != std::string::npos);
    CHECK(profilingLog.find("operation=original_call") == std::string::npos);
    CHECK(profilingLog.find("status=-31") != std::string::npos);
    CHECK(profilingLog.find("replay=0") != std::string::npos);
    CHECK(profilingLog.find("round=0") != std::string::npos);
    CHECK(profilingLog.find("domain=") == std::string::npos);
    CHECK(unsetenv("NPU_COMPUTE_DEBUG") == 0);

    const std::size_t launchesBeforeRetry = gLaunchCalls;
    const std::size_t startsBeforeRetry = gStartCalls;
    CHECK(aclrtLaunchKernel(nullptr, 1, nullptr, 0, nullptr) == ACL_ERROR_PROFILING_FAILURE);
    CHECK(gLaunchCalls == launchesBeforeRetry + 1);
    CHECK(gStartCalls == startsBeforeRetry);
    CHECK(gGetDeviceCalls == 1);
    CHECK(*gKernelValue == initialValue + 1);
    gKernelValue = nullptr;

    gFailNextFree = true;
    const std::size_t freeCallsBeforeFailure = gFreeCalls;
    CHECK(aclrtFree(allocation) == kOriginalFailure);
    CHECK(gFreeCalls == freeCallsBeforeFailure + 1);
    CHECK(aclrtFree(allocation) == 0);
    CHECK(gFreeCalls == freeCallsBeforeFailure + 3);
    return 0;
}
