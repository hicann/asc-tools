/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aclsan/cann_sanitizer.h"
#include "aclsan/internal_api.h"

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/stat.h>

namespace {

void EnsureDir(const char* path) { mkdir(path, 0755); }

AclsanLaunchConfig MakeConfig(const char* toolName, const char* workDir, const char* cacheDir)
{
    EnsureDir(workDir);
    EnsureDir(cacheDir);

    AclsanLaunchConfig config{};
    config.version = ACLSAN_API_VERSION;
    config.size = sizeof(config);
    std::snprintf(config.toolName, sizeof(config.toolName), "%s", toolName);
    std::snprintf(config.workDir, sizeof(config.workDir), "%s", workDir);
    std::snprintf(config.probeCacheDir, sizeof(config.probeCacheDir), "%s", cacheDir);
    return config;
}

void PatchDummyKernel(const char* workDir)
{
    const std::string originalPath = std::string(workDir) + "/kernel.o";
    {
        std::ofstream original(originalPath, std::ios::binary);
        original << "dummy kernel object\n";
    }

    AclsanPatchImageDesc image{};
    image.version = ACLSAN_API_VERSION;
    image.size = sizeof(image);
    image.kind = ACLSAN_PATCH_IMAGE_FILE;
    image.path = originalPath.c_str();
    char patchedPath[ACLSAN_PATH_MAX] = {};
    AclsanPatchPlanHandle plan = 0;
    assert(
        aclsanPatchBinaryFromImage(&image, nullptr, patchedPath, sizeof(patchedPath), &plan) == ACLSAN_STATUS_SUCCESS);
    assert(plan != 0);
    assert(std::strlen(patchedPath) > 0);
}

void* TestPtr(uint64_t value) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(value)); }

void SendMemcheckHostEvents()
{
    AclsanRuntimeMemoryAllocParams alloc{};
    alloc.version = ACLSAN_API_VERSION;
    alloc.size = sizeof(alloc);
    alloc.ptr = TestPtr(0x700000);
    alloc.bytes = 4096;
    alloc.memorySpace = ACLSAN_MEMORY_SPACE_DEVICE;
    alloc.deviceId = 0;
    alloc.resourceId = 42;

    AclsanRuntimeEvent allocEvent{};
    allocEvent.version = ACLSAN_API_VERSION;
    allocEvent.size = sizeof(allocEvent);
    allocEvent.apiId = ACLSAN_RT_API_ACLRT_MALLOC;
    allocEvent.phase = ACLSAN_RUNTIME_EVENT_EXIT;
    allocEvent.apiName = "aclrtMalloc";
    allocEvent.params = &alloc;
    allocEvent.result = 0;
    allocEvent.correlationId = 1001;
    assert(aclsanOnRuntimeEvent(&allocEvent) == ACLSAN_STATUS_SUCCESS);

    AclsanCannSanitizerStats stats{};
    assert(aclsanCannSanitizerGetStats(&stats) == ACLSAN_STATUS_SUCCESS);
    assert(stats.lastDomain == ACLSAN_CB_DOMAIN_RESOURCE);
    assert(stats.lastCbid == ACLSAN_CBID_RESOURCE_MEMORY_ALLOC);
    assert(stats.lastResourceId == 42);
    assert(stats.lastResourceBytes == 4096);
    assert(stats.lastResourceMemorySpace == ACLSAN_MEMORY_SPACE_DEVICE);
    assert(std::strcmp(stats.lastApiName, "aclrtMalloc") == 0);

    AclsanRuntimeMemcpyParams copy{};
    copy.version = ACLSAN_API_VERSION;
    copy.size = sizeof(copy);
    copy.dst = TestPtr(0x700100);
    copy.dstMax = 128;
    copy.src = TestPtr(0x100100);
    copy.bytes = 64;
    copy.kind = ACLSAN_MEMCPY_HOST_TO_DEVICE;
    copy.stream = TestPtr(0x55);

    AclsanRuntimeEvent memcpyEvent{};
    memcpyEvent.version = ACLSAN_API_VERSION;
    memcpyEvent.size = sizeof(memcpyEvent);
    memcpyEvent.apiId = ACLSAN_RT_API_ACLRT_MEMCPY;
    memcpyEvent.phase = ACLSAN_RUNTIME_EVENT_EXIT;
    memcpyEvent.apiName = "aclrtMemcpy";
    memcpyEvent.params = &copy;
    memcpyEvent.result = 0;
    memcpyEvent.correlationId = 1002;
    assert(aclsanOnRuntimeEvent(&memcpyEvent) == ACLSAN_STATUS_SUCCESS);

    assert(aclsanCannSanitizerGetStats(&stats) == ACLSAN_STATUS_SUCCESS);
    assert(stats.lastDomain == ACLSAN_CB_DOMAIN_MEMORY);
    assert(stats.lastCbid == ACLSAN_CBID_MEMORY_MEMCPY_END);
    assert(stats.lastMemorySrc == static_cast<uint64_t>(static_cast<std::uintptr_t>(0x100100)));
    assert(stats.lastMemoryDst == static_cast<uint64_t>(static_cast<std::uintptr_t>(0x700100)));
    assert(stats.lastMemoryBytes == 64);
    assert(stats.lastMemoryKind == ACLSAN_MEMCPY_HOST_TO_DEVICE);
    assert(std::strcmp(stats.lastApiName, "aclrtMemcpy") == 0);
}

void SendSyncEvent()
{
    AclsanRuntimeEvent syncEvent{};
    syncEvent.version = ACLSAN_API_VERSION;
    syncEvent.size = sizeof(syncEvent);
    syncEvent.apiId = ACLSAN_RT_API_ACLRT_SYNCHRONIZE_STREAM;
    syncEvent.phase = ACLSAN_RUNTIME_EVENT_EXIT;
    syncEvent.apiName = "aclrtSynchronizeStream";
    syncEvent.result = 0;
    syncEvent.correlationId = 2001;
    assert(aclsanOnRuntimeEvent(&syncEvent) == ACLSAN_STATUS_SUCCESS);
}

void RunMemcheckProfile()
{
    const char* workDir = "/tmp/aclsan_cann_sanitizer_smoke_memcheck";
    const char* cacheDir = "/tmp/aclsan_cann_sanitizer_smoke_memcheck/cache";
    AclsanLaunchConfig config = MakeConfig("memcheck", workDir, cacheDir);

    assert(acltoolInitalize(&config) == static_cast<int>(ACLSAN_STATUS_SUCCESS));

    AclsanRuntimeHookState hookState{};
    assert(aclsanGetRuntimeHookState(&hookState) == ACLSAN_STATUS_SUCCESS);
    assert((hookState.activePlan.patchPipelineMask & (1u << ACLSAN_PATCH_PIPELINE_MTE2)) != 0);
    assert((hookState.activePlan.patchPipelineMask & (1u << ACLSAN_PATCH_PIPELINE_MTE3)) != 0);
    assert((hookState.activePlan.patchPipelineMask & (1u << ACLSAN_PATCH_PIPELINE_FIXPIPE)) != 0);

    SendMemcheckHostEvents();
    PatchDummyKernel(workDir);

    AclsanRawTraceRecord records[2]{};
    records[0].version = ACLSAN_API_VERSION;
    records[0].pipeline = ACLSAN_PATCH_PIPELINE_MTE2;
    records[0].siteId = 1;
    records[0].pc = 0x1010;
    records[0].arg0 = 0x1000;
    records[0].arg1 = 0x2000;
    records[0].arg2 = 64;
    records[1].version = ACLSAN_API_VERSION;
    records[1].pipeline = ACLSAN_PATCH_PIPELINE_FIXPIPE;
    records[1].siteId = 3;
    records[1].pc = 0x1050;
    records[1].arg0 = 0x3000;
    records[1].arg1 = 0x4000;
    records[1].arg2 = 128;
    assert(aclsanIngestRawTraces(records, 2) == ACLSAN_STATUS_SUCCESS);

    AclsanCannSanitizerStats stats{};
    assert(aclsanCannSanitizerGetStats(&stats) == ACLSAN_STATUS_SUCCESS);
    assert(std::strcmp(stats.toolName, "memcheck") == 0);
    assert(stats.patchCallbacks >= 3);
    assert(stats.resourceCallbacks >= 1);
    assert(stats.memoryCallbacks >= 1);
    assert(stats.deviceInstructionCallbacks >= 2);
    assert(stats.memoryTransferEvents >= 1);
    assert(stats.fixpipeEvents >= 1);
    assert(stats.lastDomain == ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION);
    assert(stats.lastCbid == ACLSAN_CBID_DEVICE_INSTRUCTION_FIXPIPE);
    assert(stats.lastInstructionPc == 0x1050);
    assert(stats.lastInstructionBytes == 128);
    assert(std::strcmp(stats.lastInstructionOpName, "FIXPIPE") == 0);

    SendSyncEvent();
    assert(aclsanCannSanitizerGetStats(&stats) == ACLSAN_STATUS_SUCCESS);
    assert(stats.checkerEvents >= 6);
    assert(stats.checkerInstructions >= 2);
    assert(stats.checkerWindows >= 1);
    assert(stats.checkerCompletedWindows >= 1);
    assert(stats.checkerReports == 0);

    assert(aclsanCannSanitizerFinalize() == ACLSAN_STATUS_SUCCESS);
    assert(aclsanFinalize() == ACLSAN_STATUS_SUCCESS);
}

void RunSynccheckProfile()
{
    const char* workDir = "/tmp/aclsan_cann_sanitizer_smoke_synccheck";
    const char* cacheDir = "/tmp/aclsan_cann_sanitizer_smoke_synccheck/cache";
    AclsanLaunchConfig config = MakeConfig("synccheck", workDir, cacheDir);

    assert(acltoolInitalize(&config) == static_cast<int>(ACLSAN_STATUS_SUCCESS));

    AclsanRuntimeHookState hookState{};
    assert(aclsanGetRuntimeHookState(&hookState) == ACLSAN_STATUS_SUCCESS);
    assert((hookState.activePlan.patchPipelineMask & (1u << ACLSAN_PATCH_PIPELINE_SET_WAIT_FLAG)) != 0);
    assert((hookState.activePlan.patchPipelineMask & (1u << ACLSAN_PATCH_PIPELINE_GET_RLS_BUF)) != 0);
    assert((hookState.activePlan.patchPipelineMask & (1u << ACLSAN_PATCH_PIPELINE_MTE2)) == 0);
    assert((hookState.activePlan.patchPipelineMask & (1u << ACLSAN_PATCH_PIPELINE_MTE3)) == 0);
    assert((hookState.activePlan.patchPipelineMask & (1u << ACLSAN_PATCH_PIPELINE_FIXPIPE)) == 0);

    PatchDummyKernel(workDir);

    AclsanRawTraceRecord records[2]{};
    records[0].version = ACLSAN_API_VERSION;
    records[0].pipeline = ACLSAN_PATCH_PIPELINE_SET_WAIT_FLAG;
    records[0].siteId = 1;
    records[0].pc = 0x2010;
    records[0].arg0 = 7;
    records[0].arg1 = 1;
    records[1].version = ACLSAN_API_VERSION;
    records[1].pipeline = ACLSAN_PATCH_PIPELINE_GET_RLS_BUF;
    records[1].siteId = 2;
    records[1].pc = 0x2020;
    records[1].arg0 = 0x5000;
    records[1].arg1 = 32;
    records[1].arg2 = 9;
    assert(aclsanIngestRawTraces(records, 2) == ACLSAN_STATUS_SUCCESS);

    AclsanCannSanitizerStats stats{};
    assert(aclsanCannSanitizerGetStats(&stats) == ACLSAN_STATUS_SUCCESS);
    assert(std::strcmp(stats.toolName, "synccheck") == 0);
    assert(stats.patchCallbacks >= 3);
    assert(stats.deviceInstructionCallbacks >= 2);
    assert(stats.syncEvents >= 2);
    assert(stats.memoryTransferEvents == 0);
    assert(stats.fixpipeEvents == 0);
    assert(stats.lastDomain == ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION);
    assert(stats.lastCbid == ACLSAN_CBID_DEVICE_INSTRUCTION_GET_RLS_BUF);
    assert(std::strcmp(stats.lastInstructionOpName, "GET_RLS_BUF") == 0);

    SendSyncEvent();
    assert(aclsanCannSanitizerGetStats(&stats) == ACLSAN_STATUS_SUCCESS);
    assert(stats.checkerEvents >= 4);
    assert(stats.checkerInstructions >= 2);
    assert(stats.checkerWindows >= 1);
    assert(stats.checkerCompletedWindows >= 1);
    assert(stats.checkerReports == 0);

    assert(aclsanCannSanitizerFinalize() == ACLSAN_STATUS_SUCCESS);
    assert(aclsanFinalize() == ACLSAN_STATUS_SUCCESS);
}

} // namespace

int main()
{
    (void)aclsanCannSanitizerFinalize();
    (void)aclsanFinalize();
    RunMemcheckProfile();
    RunSynccheckProfile();
    return 0;
}
