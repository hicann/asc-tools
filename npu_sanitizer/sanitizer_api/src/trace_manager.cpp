/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "api_core.h"

namespace aclsan {
namespace {

template <typename T>
AclsanStatus ValidateRuntimeParams(const AclsanRuntimeEvent& event, const T*& params)
{
    if (event.params == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    const auto* typed = static_cast<const T*>(event.params);
    if (typed->version != ACLSAN_API_VERSION || typed->size < sizeof(T)) {
        return ACLSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    params = typed;
    return ACLSAN_STATUS_SUCCESS;
}

template <typename T>
void InitCallbackData(T& data, const AclsanRuntimeEvent& event)
{
    data.common.version = ACLSAN_API_VERSION;
    data.common.size = sizeof(data);
    data.common.apiName = event.apiName;
    data.common.result = event.result;
    data.common.correlationId = event.correlationId;
}

template <typename T>
void InitCallbackData(T& data, const char* apiName)
{
    data.common.version = ACLSAN_API_VERSION;
    data.common.size = sizeof(data);
    data.common.apiName = apiName;
}

bool FillDeviceMemoryAccess(const AclsanRawTraceRecord& record, AclsanDeviceMemoryAccessData& memory)
{
    uint32_t sourceKind = ACLSAN_DEVICE_SOURCE_UNKNOWN;
    uint32_t accessMode = 0;
    uint64_t address = 0;
    switch (static_cast<AclsanPatchPipeline>(record.pipeline)) {
        case ACLSAN_PATCH_PIPELINE_MTE2:
            sourceKind = ACLSAN_DEVICE_SOURCE_MTE2;
            accessMode = ACLSAN_DEVICE_MEMORY_ACCESS_READ;
            address = record.arg0;
            break;
        case ACLSAN_PATCH_PIPELINE_MTE3:
            sourceKind = ACLSAN_DEVICE_SOURCE_MTE3;
            accessMode = ACLSAN_DEVICE_MEMORY_ACCESS_WRITE;
            address = record.arg1;
            break;
        case ACLSAN_PATCH_PIPELINE_FIXPIPE:
            sourceKind = ACLSAN_DEVICE_SOURCE_FIXPIPE;
            accessMode = ACLSAN_DEVICE_MEMORY_ACCESS_WRITE;
            address = record.arg1;
            break;
        default:
            return false;
    }

    memory = {};
    memory.header.version = ACLSAN_API_VERSION;
    memory.header.size = sizeof(memory);
    memory.header.pc = record.pc;
    memory.header.siteId = record.siteId;
    memory.header.sourceKind = sourceKind;
    memory.header.blockId = record.blockId;
    memory.header.pipeline = record.pipeline;
    memory.address = address;
    memory.memorySpace = ACLSAN_DEVICE_MEMORY_SPACE_GM;
    memory.accessMode = accessMode;
    memory.accessIndex = 0;
    memory.accessCount = 1;
    memory.layoutKind = ACLSAN_MEM_LAYOUT_RANGE;
    memory.layout.range.bytes = record.arg2;
    return true;
}

AclsanStatus FillResourceFromMalloc(const AclsanRuntimeEvent& event, AclsanResourceData& resource)
{
    const AclsanRuntimeMemoryAllocParams* params = nullptr;
    AclsanStatus status = ValidateRuntimeParams(event, params);
    if (status != ACLSAN_STATUS_SUCCESS) {
        return status;
    }
    InitCallbackData(resource, event);
    resource.ptr = params->ptr;
    resource.bytes = params->bytes;
    resource.memorySpace = params->memorySpace;
    resource.deviceId = params->deviceId;
    resource.resourceId = params->resourceId;
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus FillResourceFromFree(const AclsanRuntimeEvent& event, AclsanResourceData& resource)
{
    const AclsanRuntimeMemoryFreeParams* params = nullptr;
    AclsanStatus status = ValidateRuntimeParams(event, params);
    if (status != ACLSAN_STATUS_SUCCESS) {
        return status;
    }
    InitCallbackData(resource, event);
    resource.ptr = params->ptr;
    resource.bytes = params->bytes;
    resource.memorySpace = params->memorySpace;
    resource.deviceId = params->deviceId;
    resource.resourceId = params->resourceId;
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus FillMemoryFromMemcpy(const AclsanRuntimeEvent& event, AclsanMemoryMemcpyData& memory)
{
    const AclsanRuntimeMemcpyParams* params = nullptr;
    AclsanStatus status = ValidateRuntimeParams(event, params);
    if (status != ACLSAN_STATUS_SUCCESS) {
        return status;
    }
    InitCallbackData(memory, event);
    memory.dst = params->dst;
    memory.src = params->src;
    memory.bytes = params->bytes;
    memory.kind = params->kind;
    memory.stream = params->stream;
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus FillMemoryFromMemset(const AclsanRuntimeEvent& event, AclsanMemoryMemsetData& memory)
{
    const AclsanRuntimeMemsetParams* params = nullptr;
    AclsanStatus status = ValidateRuntimeParams(event, params);
    if (status != ACLSAN_STATUS_SUCCESS) {
        return status;
    }
    InitCallbackData(memory, event);
    memory.dst = params->dst;
    memory.bytes = params->bytes;
    memory.value = params->value;
    memory.stream = params->stream;
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus FillBinaryFromFile(const AclsanRuntimeEvent& event, AclsanBinaryData& binary)
{
    InitCallbackData(binary, event);
    binary.image.kind = ACLSAN_PATCH_IMAGE_FILE;
    if (event.params == nullptr) {
        return ACLSAN_STATUS_SUCCESS;
    }

    const auto* params = static_cast<const AclsanRuntimeBinaryLoadFromFileParams*>(event.params);
    if (params->version != ACLSAN_API_VERSION || params->size < sizeof(AclsanRuntimeBinaryLoadFromFileParams)) {
        return ACLSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    binary.binaryId = params->binaryId;
    binary.image.path = params->path;
    binary.image.imageVersion = params->imageVersion;
    if (params->path != nullptr && params->path[0] != '\0') {
        binary.image.flags |= ACLSAN_BINARY_IMAGE_FLAG_PATH_VALID;
    }
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus FillBinaryFromData(const AclsanRuntimeEvent& event, AclsanBinaryData& binary)
{
    InitCallbackData(binary, event);
    binary.image.kind = ACLSAN_PATCH_IMAGE_MEMORY;
    if (event.params == nullptr) {
        return ACLSAN_STATUS_SUCCESS;
    }

    const auto* params = static_cast<const AclsanRuntimeBinaryLoadFromDataParams*>(event.params);
    if (params->version != ACLSAN_API_VERSION || params->size < sizeof(AclsanRuntimeBinaryLoadFromDataParams)) {
        return ACLSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    binary.binaryId = params->binaryId;
    binary.image.imageData = params->imageData;
    binary.image.imageSize = params->imageSize;
    binary.image.imageVersion = params->imageVersion;
    if (params->imageData != nullptr && params->imageSize != 0) {
        binary.image.flags |= ACLSAN_BINARY_IMAGE_FLAG_DATA_VALID;
    }
    return ACLSAN_STATUS_SUCCESS;
}

} // namespace

AclsanStatus ApiCore::IngestRawTraces(const AclsanRawTraceRecord* records, uint64_t count)
{
    if (records == nullptr && count != 0) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    for (uint64_t i = 0; i < count; ++i) {
        const auto& record = records[i];
        const uint32_t cbid = PipelineToCbid(static_cast<AclsanPatchPipeline>(record.pipeline));
        if (cbid == 0) {
            continue;
        }

        if (cbid == ACLSAN_CBID_DEVICE_MEMORY_ACCESS) {
            AclsanDeviceMemoryAccessData memory{};
            if (FillDeviceMemoryAccess(record, memory)) {
                Dispatch(ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, cbid, &memory);
            }
            continue;
        }

        AclsanDeviceInstructionData instruction{};
        InitCallbackData(instruction, "aclsanIngestRawTraces");
        instruction.pipeline = record.pipeline;
        instruction.cbid = cbid;
        instruction.siteId = record.siteId;
        instruction.blockId = record.blockId;
        instruction.launchId = 0;
        instruction.binaryId = 0;
        instruction.functionId = 0;
        instruction.pc = record.pc;
        instruction.rawArgs[0] = record.arg0;
        instruction.rawArgs[1] = record.arg1;
        instruction.rawArgs[2] = record.arg2;
        instruction.rawArgs[3] = record.arg3;
        instruction.rawArgs[4] = record.arg4;
        instruction.rawArgs[5] = record.arg5;

        Dispatch(ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, cbid, &instruction);
    }
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::OnRuntimeEvent(const AclsanRuntimeEvent* event)
{
    if (event == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (event->version != ACLSAN_API_VERSION || event->size < sizeof(AclsanRuntimeEvent)) {
        return ACLSAN_STATUS_ERROR_VERSION_MISMATCH;
    }

    if ((event->apiId == ACLSAN_RT_API_ACLRT_MALLOC || event->apiId == ACLSAN_RT_API_ACLRT_MALLOC_HOST) &&
        event->phase == ACLSAN_RUNTIME_EVENT_EXIT && event->result == 0) {
        AclsanResourceData resource{};
        AclsanStatus status = FillResourceFromMalloc(*event, resource);
        if (status != ACLSAN_STATUS_SUCCESS) {
            return status;
        }
        Dispatch(ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC, &resource);
        return ACLSAN_STATUS_SUCCESS;
    }

    if ((event->apiId == ACLSAN_RT_API_ACLRT_FREE || event->apiId == ACLSAN_RT_API_ACLRT_FREE_HOST) &&
        event->phase == ACLSAN_RUNTIME_EVENT_ENTER) {
        AclsanResourceData resource{};
        AclsanStatus status = FillResourceFromFree(*event, resource);
        if (status != ACLSAN_STATUS_SUCCESS) {
            return status;
        }
        Dispatch(ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_FREE, &resource);
        return ACLSAN_STATUS_SUCCESS;
    }

    if (event->apiId == ACLSAN_RT_API_ACLRT_MEMCPY) {
        AclsanMemoryMemcpyData memory{};
        AclsanStatus status = FillMemoryFromMemcpy(*event, memory);
        if (status != ACLSAN_STATUS_SUCCESS) {
            return status;
        }
        const uint32_t cbid = event->phase == ACLSAN_RUNTIME_EVENT_ENTER ? ACLSAN_CBID_MEMORY_MEMCPY_BEGIN :
                                                                           ACLSAN_CBID_MEMORY_MEMCPY_END;
        Dispatch(ACLSAN_CB_DOMAIN_MEMORY, cbid, &memory);
        return ACLSAN_STATUS_SUCCESS;
    }

    if (event->apiId == ACLSAN_RT_API_ACLRT_MEMSET) {
        AclsanMemoryMemsetData memory{};
        AclsanStatus status = FillMemoryFromMemset(*event, memory);
        if (status != ACLSAN_STATUS_SUCCESS) {
            return status;
        }
        const uint32_t cbid = event->phase == ACLSAN_RUNTIME_EVENT_ENTER ? ACLSAN_CBID_MEMORY_MEMSET_BEGIN :
                                                                           ACLSAN_CBID_MEMORY_MEMSET_END;
        Dispatch(ACLSAN_CB_DOMAIN_MEMORY, cbid, &memory);
        return ACLSAN_STATUS_SUCCESS;
    }

    if (event->apiId == ACLSAN_RT_API_ACLRT_BINARY_LOAD_FROM_FILE ||
        event->apiId == ACLSAN_RT_API_ACLRT_BINARY_LOAD_FROM_DATA) {
        AclsanBinaryData binary{};
        AclsanStatus status = event->apiId == ACLSAN_RT_API_ACLRT_BINARY_LOAD_FROM_FILE ?
                                  FillBinaryFromFile(*event, binary) :
                                  FillBinaryFromData(*event, binary);
        if (status != ACLSAN_STATUS_SUCCESS) {
            return status;
        }
        if (event->phase == ACLSAN_RUNTIME_EVENT_ENTER) {
            Dispatch(ACLSAN_CB_DOMAIN_BINARY, ACLSAN_CBID_BINARY_LOAD_BEGIN, &binary);
        } else {
            Dispatch(ACLSAN_CB_DOMAIN_BINARY, ACLSAN_CBID_BINARY_LOAD_END, &binary);
        }
        return ACLSAN_STATUS_SUCCESS;
    }

    if (event->apiId == ACLSAN_RT_API_ACLRT_LAUNCH_KERNEL ||
        event->apiId == ACLSAN_RT_API_ACLRT_LAUNCH_KERNEL_WITH_ARGS_ARRAY) {
        AclsanLaunchData launch{};
        InitCallbackData(launch, *event);
        const uint32_t cbid =
            event->phase == ACLSAN_RUNTIME_EVENT_ENTER ? ACLSAN_CBID_LAUNCH_BEGIN : ACLSAN_CBID_LAUNCH_END;
        Dispatch(ACLSAN_CB_DOMAIN_LAUNCH, cbid, &launch);
        return ACLSAN_STATUS_SUCCESS;
    }

    if (event->apiId == ACLSAN_RT_API_ACLRT_SYNCHRONIZE_STREAM && event->phase == ACLSAN_RUNTIME_EVENT_EXIT) {
        AclsanSynchronizeData sync{};
        InitCallbackData(sync, *event);
        Dispatch(ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END, &sync);
        return ACLSAN_STATUS_SUCCESS;
    }
    if (event->apiId == ACLSAN_RT_API_ACLRT_SYNCHRONIZE_DEVICE && event->phase == ACLSAN_RUNTIME_EVENT_EXIT) {
        AclsanSynchronizeData sync{};
        InitCallbackData(sync, *event);
        Dispatch(ACLSAN_CB_DOMAIN_SYNCHRONIZE, ACLSAN_CBID_SYNCHRONIZE_DEVICE_SYNC_END, &sync);
        return ACLSAN_STATUS_SUCCESS;
    }
    return ACLSAN_STATUS_SUCCESS;
}

} // namespace aclsan
