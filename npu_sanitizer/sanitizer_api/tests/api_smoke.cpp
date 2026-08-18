/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aclsan/aclsan_api.h"
#include "aclsan/aclsan_callback.h"
#include "aclsan/aclsan_memory.h"
#include "aclsan/aclsan_patch.h"
#include "aclsan/aclsan_symbolize.h"
#include "aclsan/internal_api.h"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/stat.h>

namespace {

static_assert(
    sizeof(AclsanInitParams) == sizeof(uint32_t) * 2 + sizeof(const AclsanLaunchConfig*),
    "AclsanInitParams should only carry version, size, and launchConfig");
static_assert(ACLSAN_CBID_INVALID == 0, "Callback ID 0 is invalid in every domain");
static_assert(ACLSAN_CBID_RESOURCE_INVALID == 0, "Resource callback ID 0 is invalid");
static_assert(ACLSAN_CBID_RESOURCE_MEMORY_ALLOC == 1, "Resource callback IDs are domain-local");
static_assert(ACLSAN_CBID_MEMORY_INVALID == 0, "Memory callback ID 0 is invalid");
static_assert(ACLSAN_CBID_MEMORY_MEMCPY_BEGIN == 1, "Memory callback IDs are domain-local");
static_assert(ACLSAN_CBID_BINARY_INVALID == 0, "Binary callback ID 0 is invalid");
static_assert(ACLSAN_CBID_BINARY_LOAD_BEGIN == 1, "Binary callback IDs are domain-local");
static_assert(ACLSAN_CBID_PATCH_INVALID == 0, "Patch callback ID 0 is invalid");
static_assert(ACLSAN_CBID_PATCH_BEGIN == 1, "Patch callback IDs are domain-local");
static_assert(ACLSAN_CBID_LAUNCH_INVALID == 0, "Launch callback ID 0 is invalid");
static_assert(ACLSAN_CBID_LAUNCH_BEGIN == 1, "Launch callback IDs are domain-local");
static_assert(ACLSAN_CBID_SYNCHRONIZE_INVALID == 0, "Synchronize callback ID 0 is invalid");
static_assert(ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END == 1, "Synchronize callback IDs are domain-local");
static_assert(ACLSAN_CBID_DEVICE_INSTRUCTION_INVALID == 0, "Device callback ID 0 is invalid");
static_assert(ACLSAN_CBID_DEVICE_MEMORY_ACCESS == 1, "Device callback IDs are domain-local");
static_assert(ACLSAN_CBID_DEVICE_SYNC == 2, "Device callback IDs are domain-local");
static_assert(
    ACLSAN_CBID_DEVICE_INSTRUCTION_MTE2 == ACLSAN_CBID_DEVICE_MEMORY_ACCESS,
    "Pipeline-specific memory aliases map to DEVICE_MEMORY_ACCESS");
static_assert(
    ACLSAN_CBID_DEVICE_INSTRUCTION_SET_WAIT_FLAG == ACLSAN_CBID_DEVICE_SYNC,
    "Pipeline-specific sync aliases map to DEVICE_SYNC");
static_assert(ACLSAN_CBID_REPORT_INVALID == 0, "Report callback ID 0 is invalid");
static_assert(ACLSAN_CBID_REPORT_RECORD == 1, "Report callback IDs are domain-local");
static_assert(ACLSAN_CBID_ERROR_INVALID == 0, "Error callback ID 0 is invalid");
static_assert(ACLSAN_CBID_ERROR_RECORD == 1, "Error callback IDs are domain-local");

struct CallbackCounters {
    int patchBegin = 0;
    int patchEnd = 0;
    int siteMap = 0;
    int mte2 = 0;
    int typedMemoryAccess = 0;
    int resourceAlloc = 0;
    int memcpyEnd = 0;
    int binaryFileBegin = 0;
    int binaryDataBegin = 0;
    int insideCallbackObserved = 0;
};

void EnsureDir(const char* path) { mkdir(path, 0755); }

void Callback(void* userdata, AclsanCallbackDomain domain, uint32_t cbid, const void* cbdata)
{
    auto* counters = static_cast<CallbackCounters*>(userdata);
    assert(aclsanIsInsideCallback() == 1);
    counters->insideCallbackObserved = 1;

    if (domain == ACLSAN_CB_DOMAIN_PATCH && cbid == ACLSAN_CBID_PATCH_BEGIN) {
        ++counters->patchBegin;
    }
    if (domain == ACLSAN_CB_DOMAIN_PATCH && cbid == ACLSAN_CBID_PATCH_SITE_MAP_CREATED) {
        ++counters->siteMap;
    }
    if (domain == ACLSAN_CB_DOMAIN_PATCH && cbid == ACLSAN_CBID_PATCH_END) {
        ++counters->patchEnd;
    }
    if (domain == ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION && cbid == ACLSAN_CBID_DEVICE_MEMORY_ACCESS) {
        const auto* data = static_cast<const AclsanDeviceMemoryAccessData*>(cbdata);
        assert(data != nullptr);
        assert(data->header.version == ACLSAN_API_VERSION);
        assert(data->header.size == sizeof(*data));
        assert(data->memorySpace == ACLSAN_DEVICE_MEMORY_SPACE_GM);
        assert(data->layoutKind == ACLSAN_MEM_LAYOUT_RANGE);
        assert(data->address == 0x1000);
        assert(data->layout.range.bytes == 16);
        ++counters->typedMemoryAccess;
        if (data->header.pipeline == ACLSAN_PATCH_PIPELINE_MTE2) {
            assert(data->header.sourceKind == ACLSAN_DEVICE_SOURCE_MTE2);
            assert(data->accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_READ);
            ++counters->mte2;
        }
    }
    if (domain == ACLSAN_CB_DOMAIN_RESOURCE && cbid == ACLSAN_CBID_RESOURCE_MEMORY_ALLOC) {
        ++counters->resourceAlloc;
        const auto* data = static_cast<const AclsanResourceData*>(cbdata);
        assert(data != nullptr);
        assert(data->resourceId == 7);
        assert(data->bytes == 1024);
    }
    if (domain == ACLSAN_CB_DOMAIN_MEMORY && cbid == ACLSAN_CBID_MEMORY_MEMCPY_END) {
        ++counters->memcpyEnd;
        const auto* data = static_cast<const AclsanMemoryMemcpyData*>(cbdata);
        assert(data != nullptr);
        assert(data->bytes == 32);
        assert(data->kind == ACLSAN_MEMCPY_HOST_TO_DEVICE);
    }
    if (domain == ACLSAN_CB_DOMAIN_BINARY && cbid == ACLSAN_CBID_BINARY_LOAD_BEGIN) {
        const auto* data = static_cast<const AclsanBinaryData*>(cbdata);
        assert(data != nullptr);
        if (data->image.kind == ACLSAN_PATCH_IMAGE_FILE) {
            ++counters->binaryFileBegin;
            assert((data->image.flags & ACLSAN_BINARY_IMAGE_FLAG_PATH_VALID) != 0);
            assert(data->image.path != nullptr);
        } else if (data->image.kind == ACLSAN_PATCH_IMAGE_MEMORY) {
            ++counters->binaryDataBegin;
            assert((data->image.flags & ACLSAN_BINARY_IMAGE_FLAG_DATA_VALID) != 0);
            assert(data->image.imageData != nullptr);
            assert(data->image.imageSize != 0);
        } else {
            assert(false);
        }
    }
}

void* TestPtr(uint64_t value) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(value)); }

} // namespace

int main()
{
    const char* workDir = "/tmp/aclsan_api_smoke";
    const char* cacheDir = "/tmp/aclsan_api_smoke/cache";
    EnsureDir(workDir);
    EnsureDir(cacheDir);

    const std::string originalPath = std::string(workDir) + "/kernel.o";
    {
        std::ofstream original(originalPath, std::ios::binary);
        original << "dummy kernel object\n";
    }

    AclsanLaunchConfig config{};
    config.version = ACLSAN_API_VERSION;
    config.size = sizeof(config);
    std::snprintf(config.toolName, sizeof(config.toolName), "%s", "memcheck");
    std::snprintf(config.workDir, sizeof(config.workDir), "%s", workDir);
    std::snprintf(config.probeCacheDir, sizeof(config.probeCacheDir), "%s", cacheDir);

    AclsanInitParams init{};
    init.version = ACLSAN_API_VERSION;
    init.size = sizeof(init);
    init.launchConfig = &config;
    assert(aclsanInitialize(&init) == ACLSAN_STATUS_SUCCESS);
    assert(aclsanApplyLaunchConfig(&config) == ACLSAN_STATUS_SUCCESS);
    assert(aclsanRegisterBuiltinPatchPipelines() == ACLSAN_STATUS_SUCCESS);

    CallbackCounters counters{};
    AclsanSubscribeDesc subDesc{};
    subDesc.version = ACLSAN_API_VERSION;
    subDesc.size = sizeof(subDesc);
    subDesc.name = "smoke";
    subDesc.callback = Callback;
    subDesc.userdata = &counters;
    AclsanSubscriberHandle subscriber = ACLSAN_INVALID_SUBSCRIBER_HANDLE;
    assert(aclsanSubscribe(&subDesc, &subscriber) == ACLSAN_STATUS_SUCCESS);
    AclsanSubscriberHandle duplicateSubscriber = ACLSAN_INVALID_SUBSCRIBER_HANDLE;
    assert(aclsanSubscribe(&subDesc, &duplicateSubscriber) == ACLSAN_STATUS_ERROR_MAX_LIMIT_REACHED);
    assert(
        aclsanEnableCallback(subscriber, ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_INVALID, 1) ==
        ACLSAN_STATUS_ERROR_INVALID_VALUE);
    assert(
        aclsanEnableCallback(subscriber, ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_BEGIN, 1) == ACLSAN_STATUS_SUCCESS);
    assert(
        aclsanEnableCallback(subscriber, ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_SITE_MAP_CREATED, 1) ==
        ACLSAN_STATUS_SUCCESS);
    assert(aclsanEnableCallback(subscriber, ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_END, 1) == ACLSAN_STATUS_SUCCESS);
    assert(
        aclsanEnableCallback(subscriber, ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_MEMORY_ACCESS, 1) ==
        ACLSAN_STATUS_SUCCESS);
    assert(
        aclsanEnableCallback(subscriber, ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC, 1) ==
        ACLSAN_STATUS_SUCCESS);
    assert(
        aclsanEnableCallback(subscriber, ACLSAN_CB_DOMAIN_MEMORY, ACLSAN_CBID_MEMORY_MEMCPY_END, 1) ==
        ACLSAN_STATUS_SUCCESS);
    assert(
        aclsanEnableCallback(subscriber, ACLSAN_CB_DOMAIN_BINARY, ACLSAN_CBID_BINARY_LOAD_BEGIN, 1) ==
        ACLSAN_STATUS_SUCCESS);

    AclsanRuntimeHookState hookState{};
    assert(aclsanGetRuntimeHookState(&hookState) == ACLSAN_STATUS_SUCCESS);
    assert((hookState.activePlan.patchPipelineMask & (1u << ACLSAN_PATCH_PIPELINE_MTE2)) != 0);
    assert(hookState.activePlan.ruleCount > 0);

    AclsanRuntimeMemoryAllocParams alloc{};
    alloc.version = ACLSAN_API_VERSION;
    alloc.size = sizeof(alloc);
    alloc.ptr = TestPtr(0x7000);
    alloc.bytes = 1024;
    alloc.memorySpace = ACLSAN_MEMORY_SPACE_DEVICE;
    alloc.deviceId = 0;
    alloc.resourceId = 7;
    AclsanRuntimeEvent allocEvent{};
    allocEvent.version = ACLSAN_API_VERSION;
    allocEvent.size = sizeof(allocEvent);
    allocEvent.apiId = ACLSAN_RT_API_ACLRT_MALLOC;
    allocEvent.phase = ACLSAN_RUNTIME_EVENT_EXIT;
    allocEvent.apiName = "aclrtMalloc";
    allocEvent.params = &alloc;
    assert(aclsanOnRuntimeEvent(&allocEvent) == ACLSAN_STATUS_SUCCESS);
    assert(counters.resourceAlloc == 1);

    AclsanRuntimeMemcpyParams memcpy{};
    memcpy.version = ACLSAN_API_VERSION;
    memcpy.size = sizeof(memcpy);
    memcpy.dst = TestPtr(0x8000);
    memcpy.dstMax = 64;
    memcpy.src = TestPtr(0x9000);
    memcpy.bytes = 32;
    memcpy.kind = ACLSAN_MEMCPY_HOST_TO_DEVICE;
    AclsanRuntimeEvent memcpyEvent{};
    memcpyEvent.version = ACLSAN_API_VERSION;
    memcpyEvent.size = sizeof(memcpyEvent);
    memcpyEvent.apiId = ACLSAN_RT_API_ACLRT_MEMCPY;
    memcpyEvent.phase = ACLSAN_RUNTIME_EVENT_EXIT;
    memcpyEvent.apiName = "aclrtMemcpy";
    memcpyEvent.params = &memcpy;
    assert(aclsanOnRuntimeEvent(&memcpyEvent) == ACLSAN_STATUS_SUCCESS);
    assert(counters.memcpyEnd == 1);

    AclsanRuntimeBinaryLoadFromFileParams binaryFile{};
    binaryFile.version = ACLSAN_API_VERSION;
    binaryFile.size = sizeof(binaryFile);
    binaryFile.path = originalPath.c_str();
    binaryFile.imageVersion = "file-v1";
    binaryFile.binaryId = 11;
    AclsanRuntimeEvent binaryFileEvent{};
    binaryFileEvent.version = ACLSAN_API_VERSION;
    binaryFileEvent.size = sizeof(binaryFileEvent);
    binaryFileEvent.apiId = ACLSAN_RT_API_ACLRT_BINARY_LOAD_FROM_FILE;
    binaryFileEvent.phase = ACLSAN_RUNTIME_EVENT_ENTER;
    binaryFileEvent.apiName = "aclrtBinaryLoadFromFile";
    binaryFileEvent.params = &binaryFile;
    assert(aclsanOnRuntimeEvent(&binaryFileEvent) == ACLSAN_STATUS_SUCCESS);
    assert(counters.binaryFileBegin == 1);

    const char runtimeImageData[] = "runtime memory image\n";
    AclsanRuntimeBinaryLoadFromDataParams binaryData{};
    binaryData.version = ACLSAN_API_VERSION;
    binaryData.size = sizeof(binaryData);
    binaryData.imageData = runtimeImageData;
    binaryData.imageSize = sizeof(runtimeImageData);
    binaryData.imageVersion = "memory-v1";
    binaryData.binaryId = 12;
    AclsanRuntimeEvent binaryDataEvent{};
    binaryDataEvent.version = ACLSAN_API_VERSION;
    binaryDataEvent.size = sizeof(binaryDataEvent);
    binaryDataEvent.apiId = ACLSAN_RT_API_ACLRT_BINARY_LOAD_FROM_DATA;
    binaryDataEvent.phase = ACLSAN_RUNTIME_EVENT_ENTER;
    binaryDataEvent.apiName = "aclrtBinaryLoadFromData";
    binaryDataEvent.params = &binaryData;
    assert(aclsanOnRuntimeEvent(&binaryDataEvent) == ACLSAN_STATUS_SUCCESS);
    assert(counters.binaryDataBegin == 1);

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
    assert(counters.patchBegin == 1);
    assert(counters.siteMap == 1);
    assert(counters.patchEnd == 1);

    const char memoryImageData[] = "dummy memory kernel object\n";
    AclsanPatchImageDesc memoryImage{};
    memoryImage.version = ACLSAN_API_VERSION;
    memoryImage.size = sizeof(memoryImage);
    memoryImage.kind = ACLSAN_PATCH_IMAGE_MEMORY;
    memoryImage.imageData = memoryImageData;
    memoryImage.imageSize = sizeof(memoryImageData);

    char patchedMemoryPath[ACLSAN_PATH_MAX] = {};
    AclsanPatchPlanHandle memoryPlan = 0;
    assert(
        aclsanPatchBinaryFromImage(&memoryImage, nullptr, patchedMemoryPath, sizeof(patchedMemoryPath), &memoryPlan) ==
        ACLSAN_STATUS_SUCCESS);
    assert(memoryPlan != 0);
    assert(std::strlen(patchedMemoryPath) > 0);
    assert(counters.patchBegin == 2);
    assert(counters.siteMap == 2);
    assert(counters.patchEnd == 2);

    AclsanPatchSiteInfo site{};
    assert(aclsanGetPatchSiteInfo(1, &site) == ACLSAN_STATUS_SUCCESS);
    assert(site.pipeline == ACLSAN_PATCH_PIPELINE_MTE2);
    assert(site.opName != nullptr);

    AclsanDevicePcQuery query{};
    query.version = ACLSAN_API_VERSION;
    query.size = sizeof(query);
    query.binaryId = site.binaryId;
    query.functionId = site.functionId;
    query.siteId = site.siteId;
    query.pc = site.pc;
    char symbolPayload[512] = {};
    uint64_t symbolPayloadBytes = 0;
    assert(
        aclsanSymbolizeDevicePc(&query, symbolPayload, sizeof(symbolPayload), &symbolPayloadBytes) ==
        ACLSAN_STATUS_SUCCESS);
    assert(symbolPayloadBytes > 0);
    assert(std::strstr(symbolPayload, "MTE2") != nullptr);
    assert(std::strstr(symbolPayload, "0x1030") != nullptr);

    AclsanRawTraceRecord record{};
    record.version = ACLSAN_API_VERSION;
    record.pipeline = ACLSAN_PATCH_PIPELINE_MTE2;
    record.siteId = 1;
    record.pc = 0x1010;
    record.arg0 = 0x1000;
    record.arg1 = 0x2000;
    record.arg2 = 16;
    assert(aclsanIngestRawTraces(&record, 1) == ACLSAN_STATUS_SUCCESS);
    assert(aclsanStreamSynchronize(nullptr) == ACLSAN_STATUS_SUCCESS);
    assert(counters.mte2 == 1);
    assert(counters.typedMemoryAccess == 1);
    assert(counters.insideCallbackObserved == 1);

    void* dev = nullptr;
    assert(aclsanDeviceMalloc(&dev, 16) == ACLSAN_STATUS_SUCCESS);
    const char hostIn[16] = "aclsan-smoke";
    char hostOut[16] = {};
    assert(aclsanMemcpyH2D(dev, hostIn, sizeof(hostIn)) == ACLSAN_STATUS_SUCCESS);
    assert(aclsanMemcpyD2H(hostOut, dev, sizeof(hostOut)) == ACLSAN_STATUS_SUCCESS);
    assert(std::memcmp(hostIn, hostOut, sizeof(hostIn)) == 0);

    AclsanMemoryInfo memInfo{};
    assert(aclsanMemoryGetInfo(dev, &memInfo) == ACLSAN_STATUS_SUCCESS);
    assert(memInfo.bytes == 16);
    assert(aclsanDeviceFree(dev) == ACLSAN_STATUS_SUCCESS);

    assert(aclsanFinalize() == ACLSAN_STATUS_SUCCESS);
    return 0;
}
