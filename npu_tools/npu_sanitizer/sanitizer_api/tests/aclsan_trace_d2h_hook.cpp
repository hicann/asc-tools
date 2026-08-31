/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aclsan/aclsan_api.h"
#include "aclsan/aclsan_cbdata_device.h"
#include "injection/injection_hook.h"
#include "injection/runtime_stub_api.h"
#include "trace_buffer_abi.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" void aclsanTestMarkInstrumentedFunction(aclrtFuncHandle function, uint32_t traceArgumentOffset);

namespace {

#define CHECK(expression)                                                                 \
    do {                                                                                  \
        if (!(expression)) {                                                              \
            std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expression); \
            return 1;                                                                     \
        }                                                                                 \
    } while (false)

struct CapturedRecord {
    uint64_t launchId;
    uint64_t instrExecId;
    uint32_t deviceId;
    uint32_t phyCoreId;
    uint32_t blockId;
    uint32_t blockType;
    uint64_t pc;
    uint32_t srcPipe;
    uint32_t dstPipe;
    uint64_t objectId;
};

struct CapturedMemoryAccess {
    uint64_t address;
    uint64_t bytes;
    uint32_t memorySpace;
    uint32_t accessMode;
    uint32_t layoutKind;
    uint32_t rank;
    uint64_t firstCount;
    uint64_t secondCount;
};

std::vector<CapturedRecord> g_records;
std::vector<CapturedMemoryAccess> g_memoryAccesses;
size_t g_mallocCalls = 0;
size_t g_freeCalls = 0;
size_t g_d2hCalls = 0;
size_t g_deviceInfoCalls = 0;
bool g_failLaunch = false;
bool g_failSync = false;
bool g_failDeviceInfo = false;
bool g_failVectorDeviceInfo = false;
bool g_writeRecord = true;
bool g_writeMemoryRecord = false;
int64_t g_cubeCoreCount = 36;
int64_t g_vectorCoreCount = 72;
bool g_writeMultiMemoryRecords = false;
uint32_t g_expectedHiddenOffset = 16;
size_t g_expectedLaunchArgumentBytes = 24;
uint32_t g_expectedPlaceholderDataOffset = 0;
uint32_t g_expectedPaddingBegin = 0;
bool g_checkPadding = false;

aclError OriginalMalloc(void** pointer, size_t bytes, aclrtMemMallocPolicy)
{
    ++g_mallocCalls;
    *pointer = std::malloc(bytes);
    return *pointer == nullptr ? ACL_ERROR_BAD_ALLOC : ACL_SUCCESS;
}

aclError OriginalFree(void* pointer)
{
    ++g_freeCalls;
    std::free(pointer);
    return ACL_SUCCESS;
}

aclError OriginalMemcpy(void* dst, size_t dstMax, const void* src, size_t bytes, aclrtMemcpyKind kind)
{
    if (dst == nullptr || src == nullptr || bytes > dstMax) {
        return ACL_ERROR_INVALID_PARAM;
    }
    if (kind == ACL_MEMCPY_DEVICE_TO_HOST) {
        ++g_d2hCalls;
    }
    std::memcpy(dst, src, bytes);
    return ACL_SUCCESS;
}

aclError OriginalLaunch(
    aclrtFuncHandle, uint32_t blocks, aclrtStream, aclrtLaunchKernelCfg*, void* hostArgs, size_t argsSize,
    aclrtPlaceHolderInfo* placeholders, size_t placeholderCount)
{
    if (g_failLaunch) {
        return ACL_ERROR_FAILURE;
    }
    if (hostArgs == nullptr || argsSize != g_expectedLaunchArgumentBytes || blocks != 2) {
        return ACL_ERROR_INVALID_PARAM;
    }
    if (g_expectedPlaceholderDataOffset != 0 && (placeholderCount != 1 || placeholders == nullptr ||
                                                 placeholders[0].dataOffset != g_expectedPlaceholderDataOffset)) {
        return ACL_ERROR_INVALID_PARAM;
    }

    void* deviceBuffer = nullptr;
    if (g_checkPadding) {
        for (uint32_t offset = g_expectedPaddingBegin; offset < g_expectedHiddenOffset; ++offset) {
            if (static_cast<const uint8_t*>(hostArgs)[offset] != 0) {
                return ACL_ERROR_INVALID_PARAM;
            }
        }
    }
    std::memcpy(&deviceBuffer, static_cast<const uint8_t*>(hostArgs) + g_expectedHiddenOffset, sizeof(deviceBuffer));
    if (deviceBuffer == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    if (!g_writeRecord) {
        return ACL_SUCCESS;
    }

    auto* bytes = static_cast<uint8_t*>(deviceBuffer);
    auto* header = reinterpret_cast<aclsan::AclsanTraceBufferHeader*>(bytes);
    size_t sliceBytes = 0;
    if (header->magic != aclsan::ASCSAN_TRACE_BUFFER_MAGIC || header->blockCount != blocks ||
        !aclsan::TraceSliceBytes(header->recordsPerCore, &sliceBytes)) {
        return ACL_ERROR_INVALID_PARAM;
    }
    if (header->physicalCoreCount != 108U) {
        return ACL_ERROR_INVALID_PARAM;
    }
    constexpr uint32_t phyCoreId = 18U;
    const uint32_t sliceIndex = phyCoreId;
    auto* slice = reinterpret_cast<aclsan::AclsanTraceSliceHeader*>(
        bytes + sizeof(*header) + static_cast<size_t>(sliceIndex) * sliceBytes);
    auto* record = reinterpret_cast<aclsan::AclsanRawTraceRecord*>(reinterpret_cast<uint8_t*>(slice) + sizeof(*slice));
    record->pc = 0x1234;
    if (g_writeMultiMemoryRecords) {
        record->args[0] = 2;
        record->instrId = 399;
        record->category = aclsan::DeviceInstructionCategory::RegisterState;
        record->pipeline = ACLSAN_DEVICE_PIPE_SCALAR;
        record->blockId = 1;

        auto* multi = record + 1;
        multi->pc = 0x1238;
        multi->args[1] = 0x130000000000ULL;
        multi->args[2] = (64ULL << 4U) | (4ULL << 48U);
        multi->args[3] = 8ULL | (512ULL << 21U);
        multi->instrId = 78;
        multi->siteId = 39;
        multi->category = aclsan::DeviceInstructionCategory::MemoryAccess;
        multi->pipeline = ACLSAN_DEVICE_PIPE_MTE2;
        multi->blockId = 1;
        slice->phyCoreId = phyCoreId;
        slice->recordCount = 2;
        return ACL_SUCCESS;
    }
    if (g_writeMemoryRecord) {
        record->args[0] = 0;
        record->args[1] = 0x120000025000ULL;
        record->args[2] = (1ULL << 4) | (8256ULL << 25);
        record->args[3] = 0;
        record->args[4] = 0;
        record->instrId = 86;
        record->siteId = 38;
        record->category = aclsan::DeviceInstructionCategory::MemoryAccess;
        record->pipeline = ACLSAN_DEVICE_PIPE_MTE2;
    } else {
        record->args[0] = 2;
        record->args[1] = 3;
        record->args[2] = 7;
        record->args[3] = 0;
        record->args[4] = 0;
        record->instrId = 440;
        record->siteId = 37;
        record->category = aclsan::DeviceInstructionCategory::Synchronization;
        record->pipeline = ACLSAN_DEVICE_PIPE_SCALAR;
    }
    record->blockId = 1;
    record->reserved = 0;
    slice->phyCoreId = phyCoreId;
    slice->recordCount = 1;
    if (!g_writeMemoryRecord) {
        auto* second = record + 1;
        second->pc = 0x2234;
        second->args[0] = 2;
        second->args[1] = 3;
        second->args[2] = 8;
        second->args[3] = 0;
        second->args[4] = 0;
        second->instrId = 440;
        second->siteId = 38;
        second->category = aclsan::DeviceInstructionCategory::Synchronization;
        second->pipeline = ACLSAN_DEVICE_PIPE_SCALAR;
        second->blockId = 0;
        second->reserved = 0;
        slice->recordCount = 2;
    }
    return ACL_SUCCESS;
}

aclError OriginalSync(aclrtStream) { return g_failSync ? ACL_ERROR_FAILURE : ACL_SUCCESS; }

const char* OriginalGetSocName() { return "Ascend950PR_9599"; }

aclError OriginalGetDevice(int32_t* deviceId)
{
    if (deviceId == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *deviceId = 3;
    return ACL_SUCCESS;
}

aclError OriginalGetDeviceInfo(uint32_t deviceId, aclrtDevAttr attr, int64_t* value)
{
    if (g_failDeviceInfo || deviceId != 3U || value == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    ++g_deviceInfoCalls;
    if (attr == ACL_DEV_ATTR_CUBE_CORE_NUM) {
        *value = g_cubeCoreCount;
        return ACL_SUCCESS;
    }
    if (attr == ACL_DEV_ATTR_VECTOR_CORE_NUM) {
        if (g_failVectorDeviceInfo) {
            return ACL_ERROR_INVALID_PARAM;
        }
        *value = g_vectorCoreCount;
        return ACL_SUCCESS;
    }
    return ACL_ERROR_INVALID_PARAM;
}

void Callback(void*, AclsanCallbackDomain domain, AclsanCallbackId id, const void* data)
{
    if (domain != ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION || data == nullptr) {
        return;
    }
    if (id == ACLSAN_CBID_DEVICE_MEMORY_ACCESS) {
        const auto* access = static_cast<const AclsanDeviceMemoryAccessData*>(data);
        uint32_t rank = 0;
        uint64_t firstCount = 0;
        uint64_t secondCount = 0;
        uint64_t bytes = 0;
        if (access->layoutKind == ACLSAN_MEM_LAYOUT_ND_AFFINE) {
            rank = access->layout.ndAffine.rank;
            firstCount = access->layout.ndAffine.dims[0];
            secondCount = access->layout.ndAffine.dims[1];
            bytes = access->layout.ndAffine.elementBytes;
        } else if (access->layoutKind == ACLSAN_MEM_LAYOUT_BLOCK_REPEAT) {
            bytes = access->layout.blockRepeat.blockSize;
        } else if (access->layoutKind == ACLSAN_MEM_LAYOUT_RANGE) {
            bytes = access->layout.range.bytes;
        }
        g_memoryAccesses.push_back(
            {access->address, bytes, access->memorySpace, access->accessMode, access->layoutKind, rank, firstCount,
             secondCount});
        return;
    }
    if (id == ACLSAN_CBID_DEVICE_SYNC) {
        const auto* sync = static_cast<const AclsanDeviceSyncData*>(data);
        g_records.push_back(
            {sync->header.launchId, sync->header.instrExecId, sync->header.deviceId, sync->header.phyCoreId,
             sync->header.blockId, sync->header.blockType, sync->header.pc, sync->srcPipe, sync->dstPipe,
             sync->objectId});
    }
}

} // namespace

int main()
{
    setenv("NPU_CHECK_TRACE_RECORDS_PER_BLOCK", "2", 1);
    CHECK(RuntimeStubSetOriginFunction("aclrtMalloc", &OriginalMalloc) == ACL_SUCCESS);
    CHECK(RuntimeStubSetOriginFunction("aclrtFree", &OriginalFree) == ACL_SUCCESS);
    CHECK(RuntimeStubSetOriginFunction("aclrtMemcpy", &OriginalMemcpy) == ACL_SUCCESS);
    CHECK(RuntimeStubSetOriginFunction("aclrtLaunchKernelWithHostArgs", &OriginalLaunch) == ACL_SUCCESS);
    CHECK(RuntimeStubSetOriginFunction("aclrtSynchronizeStream", &OriginalSync) == ACL_SUCCESS);
    CHECK(RuntimeStubSetOriginFunction("aclrtGetSocName", &OriginalGetSocName) == ACL_SUCCESS);
    CHECK(RuntimeStubSetOriginFunction("aclrtGetDevice", &OriginalGetDevice) == ACL_SUCCESS);
    CHECK(RuntimeStubSetOriginFunction("aclrtGetDeviceInfo", &OriginalGetDeviceInfo) == ACL_SUCCESS);
    CHECK(acltoolHookInit() == ACL_SUCCESS);

    AclsanSubscriberHandle subscriber = nullptr;
    CHECK(aclsanSubscribe(&subscriber, &Callback, nullptr) == ACLSAN_STATUS_SUCCESS);
    CHECK(
        aclsanEnableCallback(1, subscriber, ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_SYNC) ==
        ACLSAN_STATUS_SUCCESS);
    CHECK(
        aclsanEnableCallback(1, subscriber, ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_MEMORY_ACCESS) ==
        ACLSAN_STATUS_SUCCESS);

    int functionStorage = 0;
    int ordinaryFunctionStorage = 0;
    int streamStorage1 = 0;
    int streamStorage2 = 0;
    const auto function = reinterpret_cast<aclrtFuncHandle>(&functionStorage);
    const auto ordinaryFunction = reinterpret_cast<aclrtFuncHandle>(&ordinaryFunctionStorage);
    const auto stream1 = reinterpret_cast<aclrtStream>(&streamStorage1);
    const auto stream2 = reinterpret_cast<aclrtStream>(&streamStorage2);
    aclsanTestMarkInstrumentedFunction(function, 16);

    uint64_t arguments[2] = {1, 2};
    CHECK(
        aclrtLaunchKernelWithHostArgs(function, 2, stream1, nullptr, arguments, sizeof(arguments), nullptr, 0) ==
        ACL_SUCCESS);
    CHECK(g_records.empty());
    CHECK(g_d2hCalls == 0);
    CHECK(g_deviceInfoCalls == 2);

    CHECK(
        aclrtLaunchKernelWithHostArgs(function, 2, stream2, nullptr, arguments, sizeof(arguments), nullptr, 0) ==
        ACL_SUCCESS);
    CHECK(aclrtSynchronizeStream(stream1) == ACL_SUCCESS);
    CHECK(g_records.size() == 2);
    CHECK(g_records[0].launchId == 1);
    CHECK(g_records[0].instrExecId == 1);
    CHECK(g_records[0].deviceId == 3);
    CHECK(g_records[0].phyCoreId == 18);
    CHECK(g_records[0].blockId == 1);
    CHECK(g_records[0].blockType == ACLSAN_DEVICE_BLOCK_TYPE_AICORE_VECTOR);
    CHECK(g_records[0].pc == 0x1234);
    CHECK(g_records[0].srcPipe == 2);
    CHECK(g_records[0].dstPipe == 3);
    CHECK(g_records[0].objectId == 7);
    CHECK(g_records[1].launchId == 1);
    CHECK(g_records[1].instrExecId == 1);
    CHECK(g_records[1].deviceId == 3);
    CHECK(g_records[1].phyCoreId == 18);
    CHECK(g_records[1].blockId == 0);
    CHECK(g_records[1].blockType == ACLSAN_DEVICE_BLOCK_TYPE_AICORE_VECTOR);
    CHECK(g_records[1].pc == 0x2234);
    CHECK(g_records[1].objectId == 8);
    CHECK(aclrtSynchronizeStream(stream2) == ACL_SUCCESS);
    CHECK(g_records.size() == 4);

    g_writeMemoryRecord = true;
    CHECK(
        aclrtLaunchKernelWithHostArgs(function, 2, stream1, nullptr, arguments, sizeof(arguments), nullptr, 0) ==
        ACL_SUCCESS);
    CHECK(aclrtSynchronizeStream(stream1) == ACL_SUCCESS);
    CHECK(g_memoryAccesses.size() == 1);
    CHECK(g_memoryAccesses[0].address == 0x120000025000ULL);
    CHECK(g_memoryAccesses[0].bytes == 8256);
    CHECK(g_memoryAccesses[0].memorySpace == ACLSAN_DEVICE_MEMORY_SPACE_GM);
    CHECK(g_memoryAccesses[0].accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_READ);
    g_writeMemoryRecord = false;

    g_writeMultiMemoryRecords = true;
    CHECK(
        aclrtLaunchKernelWithHostArgs(function, 2, stream1, nullptr, arguments, sizeof(arguments), nullptr, 0) ==
        ACL_SUCCESS);
    CHECK(aclrtSynchronizeStream(stream1) == ACL_SUCCESS);
    CHECK(g_memoryAccesses.size() == 2);
    CHECK(g_memoryAccesses[1].address == 0x130000000000ULL);
    CHECK(g_memoryAccesses[1].memorySpace == ACLSAN_DEVICE_MEMORY_SPACE_GM);
    CHECK(g_memoryAccesses[1].accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_READ);
    CHECK(g_memoryAccesses[1].layoutKind == ACLSAN_MEM_LAYOUT_ND_AFFINE);
    CHECK(g_memoryAccesses[1].rank == 2);
    CHECK(g_memoryAccesses[1].firstCount == 4);
    CHECK(g_memoryAccesses[1].secondCount == 2);
    g_writeMultiMemoryRecords = false;

    g_failSync = true;
    CHECK(
        aclrtLaunchKernelWithHostArgs(function, 2, stream1, nullptr, arguments, sizeof(arguments), nullptr, 0) ==
        ACL_SUCCESS);
    CHECK(aclrtSynchronizeStream(stream1) == ACL_ERROR_FAILURE);
    CHECK(g_records.size() == 6);
    g_failSync = false;
    CHECK(aclrtSynchronizeStream(stream1) == ACL_SUCCESS);
    CHECK(g_records.size() == 6);

    const size_t freesBeforeFailedLaunch = g_freeCalls;
    g_failLaunch = true;
    CHECK(
        aclrtLaunchKernelWithHostArgs(function, 2, stream1, nullptr, arguments, sizeof(arguments), nullptr, 0) ==
        ACL_ERROR_FAILURE);
    CHECK(g_freeCalls == freesBeforeFailedLaunch + 1);
    g_failLaunch = false;

    const size_t mallocCallsBeforeInvalidTopology = g_mallocCalls;
    g_failDeviceInfo = true;
    CHECK(
        aclrtLaunchKernelWithHostArgs(function, 2, stream1, nullptr, arguments, sizeof(arguments), nullptr, 0) ==
        ACL_ERROR_RT_INTERNAL_ERROR);
    g_failDeviceInfo = false;
    g_failVectorDeviceInfo = true;
    CHECK(
        aclrtLaunchKernelWithHostArgs(function, 2, stream1, nullptr, arguments, sizeof(arguments), nullptr, 0) ==
        ACL_ERROR_RT_INTERNAL_ERROR);
    g_failVectorDeviceInfo = false;
    g_cubeCoreCount = 0;
    CHECK(
        aclrtLaunchKernelWithHostArgs(function, 2, stream1, nullptr, arguments, sizeof(arguments), nullptr, 0) ==
        ACL_ERROR_RT_INTERNAL_ERROR);
    g_cubeCoreCount = 36;
    g_vectorCoreCount = -1;
    CHECK(
        aclrtLaunchKernelWithHostArgs(function, 2, stream1, nullptr, arguments, sizeof(arguments), nullptr, 0) ==
        ACL_ERROR_RT_INTERNAL_ERROR);
    g_vectorCoreCount = 71;
    CHECK(
        aclrtLaunchKernelWithHostArgs(function, 2, stream1, nullptr, arguments, sizeof(arguments), nullptr, 0) ==
        ACL_ERROR_RT_INTERNAL_ERROR);
    g_cubeCoreCount = 1431655766;
    g_vectorCoreCount = 2863311532;
    CHECK(
        aclrtLaunchKernelWithHostArgs(function, 2, stream1, nullptr, arguments, sizeof(arguments), nullptr, 0) ==
        ACL_ERROR_RT_INTERNAL_ERROR);
    g_cubeCoreCount = 36;
    g_vectorCoreCount = 72;
    CHECK(g_mallocCalls == mallocCallsBeforeInvalidTopology);

    aclsanTestMarkInstrumentedFunction(function, 8);
    g_expectedHiddenOffset = 8;
    g_expectedPlaceholderDataOffset = 16;
    aclrtPlaceHolderInfo placeholder{0, 8};
    CHECK(
        aclrtLaunchKernelWithHostArgs(function, 2, stream1, nullptr, arguments, sizeof(arguments), &placeholder, 1) ==
        ACL_SUCCESS);
    CHECK(placeholder.dataOffset == 8);
    CHECK(aclrtSynchronizeStream(stream1) == ACL_SUCCESS);

    int oneArgumentFunctionStorage = 0;
    int zeroArgumentFunctionStorage = 0;
    int threeArgumentFunctionStorage = 0;
    const auto oneArgumentFunction = reinterpret_cast<aclrtFuncHandle>(&oneArgumentFunctionStorage);
    const auto zeroArgumentFunction = reinterpret_cast<aclrtFuncHandle>(&zeroArgumentFunctionStorage);
    const auto threeArgumentFunction = reinterpret_cast<aclrtFuncHandle>(&threeArgumentFunctionStorage);
    aclsanTestMarkInstrumentedFunction(oneArgumentFunction, 24);
    aclsanTestMarkInstrumentedFunction(zeroArgumentFunction, 24);
    aclsanTestMarkInstrumentedFunction(threeArgumentFunction, 24);
    g_writeRecord = false;
    g_expectedHiddenOffset = 24;
    g_expectedLaunchArgumentBytes = 32;
    g_expectedPlaceholderDataOffset = 0;
    g_checkPadding = true;
    uint64_t oneArgument = 0x1234;
    g_expectedPaddingBegin = 8;
    CHECK(
        aclrtLaunchKernelWithHostArgs(
            oneArgumentFunction, 2, stream1, nullptr, &oneArgument, sizeof(oneArgument), nullptr, 0) == ACL_SUCCESS);
    g_expectedPaddingBegin = 0;
    CHECK(
        aclrtLaunchKernelWithHostArgs(zeroArgumentFunction, 2, stream1, nullptr, nullptr, 0, nullptr, 0) ==
        ACL_SUCCESS);
    uint64_t threeArguments[3] = {1, 2, 3};
    g_expectedPaddingBegin = 24;
    CHECK(
        aclrtLaunchKernelWithHostArgs(
            threeArgumentFunction, 2, stream1, nullptr, threeArguments, sizeof(threeArguments), nullptr, 0) ==
        ACL_SUCCESS);
    CHECK(aclrtSynchronizeStream(stream1) == ACL_SUCCESS);
    g_checkPadding = false;

    void* ordinaryArgs = arguments;
    CHECK(
        aclrtLaunchKernelWithHostArgs(
            ordinaryFunction, 2, stream1, nullptr, ordinaryArgs, sizeof(arguments), &placeholder, 1) ==
        ACL_ERROR_INVALID_PARAM);
    CHECK(g_mallocCalls == g_freeCalls);

    CHECK(aclsanUnsubscribe(subscriber) == ACLSAN_STATUS_SUCCESS);
    unsetenv("NPU_CHECK_TRACE_RECORDS_PER_BLOCK");
    return 0;
}
