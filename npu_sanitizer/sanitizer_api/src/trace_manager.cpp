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

namespace ascsan {
namespace {

template <typename T>
AscsanStatus ValidateRuntimeParams(const AscsanRuntimeEvent& event, const T** params)
{
    if (event.params == nullptr || params == nullptr) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    const auto* typed = static_cast<const T*>(event.params);
    if (typed->version != ASCSAN_API_VERSION || typed->size < sizeof(T)) {
        return ASCSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    *params = typed;
    return ASCSAN_STATUS_SUCCESS;
}

template <typename T>
void InitCallbackData(T* data, const AscsanRuntimeEvent& event)
{
    data->common.version = ASCSAN_API_VERSION;
    data->common.size = sizeof(*data);
    data->common.apiName = event.apiName;
    data->common.result = event.result;
    data->common.correlationId = event.correlationId;
}

template <typename T>
void InitCallbackData(T* data, const char* apiName)
{
    data->common.version = ASCSAN_API_VERSION;
    data->common.size = sizeof(*data);
    data->common.apiName = apiName;
}

AscsanPatchPipeline FirstEnabledPipeline(uint32_t mask)
{
    const AscsanPatchPipeline pipelines[] = {
        ASCSAN_PATCH_PIPELINE_SET_WAIT_FLAG, ASCSAN_PATCH_PIPELINE_GET_RLS_BUF, ASCSAN_PATCH_PIPELINE_MTE2,
        ASCSAN_PATCH_PIPELINE_MTE3,          ASCSAN_PATCH_PIPELINE_FIXPIPE,
    };
    for (AscsanPatchPipeline pipeline : pipelines) {
        if ((mask & PipelineMask(pipeline)) != 0) {
            return pipeline;
        }
    }
    return ASCSAN_PATCH_PIPELINE_INVALID;
}

AscsanStatus FillResourceFromMalloc(const AscsanRuntimeEvent& event, AscsanResourceData* resource)
{
    const AscsanRuntimeMemoryAllocParams* params = nullptr;
    AscsanStatus status = ValidateRuntimeParams(event, &params);
    if (status != ASCSAN_STATUS_SUCCESS) {
        return status;
    }
    InitCallbackData(resource, event);
    resource->ptr = params->ptr;
    resource->bytes = params->bytes;
    resource->memorySpace = params->memorySpace;
    resource->deviceId = params->deviceId;
    resource->resourceId = params->resourceId;
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus FillResourceFromFree(const AscsanRuntimeEvent& event, AscsanResourceData* resource)
{
    const AscsanRuntimeMemoryFreeParams* params = nullptr;
    AscsanStatus status = ValidateRuntimeParams(event, &params);
    if (status != ASCSAN_STATUS_SUCCESS) {
        return status;
    }
    InitCallbackData(resource, event);
    resource->ptr = params->ptr;
    resource->bytes = params->bytes;
    resource->memorySpace = params->memorySpace;
    resource->deviceId = params->deviceId;
    resource->resourceId = params->resourceId;
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus FillMemoryFromMemcpy(const AscsanRuntimeEvent& event, AscsanMemoryMemcpyData* memory)
{
    const AscsanRuntimeMemcpyParams* params = nullptr;
    AscsanStatus status = ValidateRuntimeParams(event, &params);
    if (status != ASCSAN_STATUS_SUCCESS) {
        return status;
    }
    InitCallbackData(memory, event);
    memory->dst = params->dst;
    memory->src = params->src;
    memory->bytes = params->bytes;
    memory->kind = params->kind;
    memory->stream = params->stream;
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus FillMemoryFromMemset(const AscsanRuntimeEvent& event, AscsanMemoryMemsetData* memory)
{
    const AscsanRuntimeMemsetParams* params = nullptr;
    AscsanStatus status = ValidateRuntimeParams(event, &params);
    if (status != ASCSAN_STATUS_SUCCESS) {
        return status;
    }
    InitCallbackData(memory, event);
    memory->dst = params->dst;
    memory->bytes = params->bytes;
    memory->value = params->value;
    memory->stream = params->stream;
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus FillBinaryFromFile(const AscsanRuntimeEvent& event, AscsanBinaryData* binary)
{
    InitCallbackData(binary, event);
    binary->image.kind = ASCSAN_PATCH_IMAGE_FILE;
    if (event.params == nullptr) {
        return ASCSAN_STATUS_SUCCESS;
    }

    const auto* params = static_cast<const AscsanRuntimeBinaryLoadFromFileParams*>(event.params);
    if (params->version != ASCSAN_API_VERSION || params->size < sizeof(AscsanRuntimeBinaryLoadFromFileParams)) {
        return ASCSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    binary->binaryId = params->binaryId;
    binary->image.path = params->path;
    binary->image.imageVersion = params->imageVersion;
    if (params->path != nullptr && params->path[0] != '\0') {
        binary->image.flags |= ASCSAN_BINARY_IMAGE_FLAG_PATH_VALID;
    }
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus FillBinaryFromData(const AscsanRuntimeEvent& event, AscsanBinaryData* binary)
{
    InitCallbackData(binary, event);
    binary->image.kind = ASCSAN_PATCH_IMAGE_MEMORY;
    if (event.params == nullptr) {
        return ASCSAN_STATUS_SUCCESS;
    }

    const auto* params = static_cast<const AscsanRuntimeBinaryLoadFromDataParams*>(event.params);
    if (params->version != ASCSAN_API_VERSION || params->size < sizeof(AscsanRuntimeBinaryLoadFromDataParams)) {
        return ASCSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    binary->binaryId = params->binaryId;
    binary->image.imageData = params->imageData;
    binary->image.imageSize = params->imageSize;
    binary->image.imageVersion = params->imageVersion;
    if (params->imageData != nullptr && params->imageSize != 0) {
        binary->image.flags |= ASCSAN_BINARY_IMAGE_FLAG_DATA_VALID;
    }
    return ASCSAN_STATUS_SUCCESS;
}

} // namespace

std::vector<AscsanRawTraceRecord> ApiCore::BuildSyntheticRecordsForSync() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const AscsanPatchPipeline pipeline = FirstEnabledPipeline(activeHookPlan_.patchPipelineMask);
    if (pipeline == ASCSAN_PATCH_PIPELINE_INVALID) {
        return {};
    }

    AscsanRawTraceRecord record{};
    record.magic = 0x41534353u; // "ASCS"
    record.version = ASCSAN_API_VERSION;
    record.pipeline = static_cast<uint16_t>(pipeline);
    record.siteId = 1;
    record.blockId = 0;
    record.pc = 0x1000 + static_cast<uint64_t>(pipeline) * 0x10;
    record.arg0 = 0x100000;
    record.arg1 = 0x200000;
    record.arg2 = 64;
    record.arg3 = 1;
    record.arg4 = 0;
    record.arg5 = 0;
    return {record};
}

AscsanStatus ApiCore::IngestRawTraces(const AscsanRawTraceRecord* records, uint64_t count)
{
    if (records == nullptr && count != 0) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    for (uint64_t i = 0; i < count; ++i) {
        const auto& record = records[i];
        const uint32_t cbid = PipelineToCbid(static_cast<AscsanPatchPipeline>(record.pipeline));
        if (cbid == 0) {
            continue;
        }

        AscsanDeviceInstructionData instruction{};
        InitCallbackData(&instruction, "ascsanIngestRawTraces");
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

        Dispatch(ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION, cbid, &instruction);
    }
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::OnRuntimeEvent(const AscsanRuntimeEvent* event)
{
    if (event == nullptr) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (event->version != ASCSAN_API_VERSION || event->size < sizeof(AscsanRuntimeEvent)) {
        return ASCSAN_STATUS_ERROR_VERSION_MISMATCH;
    }

    if ((event->apiId == ASCSAN_RT_API_ACLRT_MALLOC || event->apiId == ASCSAN_RT_API_ACLRT_MALLOC_HOST) &&
        event->phase == ASCSAN_RUNTIME_EVENT_EXIT && event->result == 0) {
        AscsanResourceData resource{};
        AscsanStatus status = FillResourceFromMalloc(*event, &resource);
        if (status != ASCSAN_STATUS_SUCCESS) {
            return status;
        }
        Dispatch(ASCSAN_CB_DOMAIN_RESOURCE, ASCSAN_CBID_RESOURCE_MEMORY_ALLOC, &resource);
        return ASCSAN_STATUS_SUCCESS;
    }

    if ((event->apiId == ASCSAN_RT_API_ACLRT_FREE || event->apiId == ASCSAN_RT_API_ACLRT_FREE_HOST) &&
        event->phase == ASCSAN_RUNTIME_EVENT_ENTER) {
        AscsanResourceData resource{};
        AscsanStatus status = FillResourceFromFree(*event, &resource);
        if (status != ASCSAN_STATUS_SUCCESS) {
            return status;
        }
        Dispatch(ASCSAN_CB_DOMAIN_RESOURCE, ASCSAN_CBID_RESOURCE_MEMORY_FREE, &resource);
        return ASCSAN_STATUS_SUCCESS;
    }

    if (event->apiId == ASCSAN_RT_API_ACLRT_MEMCPY) {
        AscsanMemoryMemcpyData memory{};
        AscsanStatus status = FillMemoryFromMemcpy(*event, &memory);
        if (status != ASCSAN_STATUS_SUCCESS) {
            return status;
        }
        const uint32_t cbid = event->phase == ASCSAN_RUNTIME_EVENT_ENTER ? ASCSAN_CBID_MEMORY_MEMCPY_BEGIN :
                                                                           ASCSAN_CBID_MEMORY_MEMCPY_END;
        Dispatch(ASCSAN_CB_DOMAIN_MEMORY, cbid, &memory);
        return ASCSAN_STATUS_SUCCESS;
    }

    if (event->apiId == ASCSAN_RT_API_ACLRT_MEMSET) {
        AscsanMemoryMemsetData memory{};
        AscsanStatus status = FillMemoryFromMemset(*event, &memory);
        if (status != ASCSAN_STATUS_SUCCESS) {
            return status;
        }
        const uint32_t cbid = event->phase == ASCSAN_RUNTIME_EVENT_ENTER ? ASCSAN_CBID_MEMORY_MEMSET_BEGIN :
                                                                           ASCSAN_CBID_MEMORY_MEMSET_END;
        Dispatch(ASCSAN_CB_DOMAIN_MEMORY, cbid, &memory);
        return ASCSAN_STATUS_SUCCESS;
    }

    if (event->apiId == ASCSAN_RT_API_ACLRT_BINARY_LOAD_FROM_FILE ||
        event->apiId == ASCSAN_RT_API_ACLRT_BINARY_LOAD_FROM_DATA) {
        AscsanBinaryData binary{};
        AscsanStatus status = event->apiId == ASCSAN_RT_API_ACLRT_BINARY_LOAD_FROM_FILE ?
                                  FillBinaryFromFile(*event, &binary) :
                                  FillBinaryFromData(*event, &binary);
        if (status != ASCSAN_STATUS_SUCCESS) {
            return status;
        }
        if (event->phase == ASCSAN_RUNTIME_EVENT_ENTER) {
            Dispatch(ASCSAN_CB_DOMAIN_BINARY, ASCSAN_CBID_BINARY_LOAD_BEGIN, &binary);
        } else {
            Dispatch(ASCSAN_CB_DOMAIN_BINARY, ASCSAN_CBID_BINARY_LOAD_END, &binary);
        }
        return ASCSAN_STATUS_SUCCESS;
    }

    if (event->apiId == ASCSAN_RT_API_ACLRT_LAUNCH_KERNEL ||
        event->apiId == ASCSAN_RT_API_ACLRT_LAUNCH_KERNEL_WITH_ARGS_ARRAY) {
        AscsanLaunchData launch{};
        InitCallbackData(&launch, *event);
        const uint32_t cbid =
            event->phase == ASCSAN_RUNTIME_EVENT_ENTER ? ASCSAN_CBID_LAUNCH_BEGIN : ASCSAN_CBID_LAUNCH_END;
        Dispatch(ASCSAN_CB_DOMAIN_LAUNCH, cbid, &launch);
        return ASCSAN_STATUS_SUCCESS;
    }

    if (event->apiId == ASCSAN_RT_API_ACLRT_SYNCHRONIZE_STREAM && event->phase == ASCSAN_RUNTIME_EVENT_EXIT) {
        AscsanSynchronizeData sync{};
        InitCallbackData(&sync, *event);
        Dispatch(ASCSAN_CB_DOMAIN_SYNCHRONIZE, ASCSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END, &sync);
        auto records = BuildSyntheticRecordsForSync();
        if (!records.empty()) {
            return IngestRawTraces(records.data(), records.size());
        }
    }
    if (event->apiId == ASCSAN_RT_API_ACLRT_SYNCHRONIZE_DEVICE && event->phase == ASCSAN_RUNTIME_EVENT_EXIT) {
        AscsanSynchronizeData sync{};
        InitCallbackData(&sync, *event);
        Dispatch(ASCSAN_CB_DOMAIN_SYNCHRONIZE, ASCSAN_CBID_SYNCHRONIZE_DEVICE_SYNC_END, &sync);
        auto records = BuildSyntheticRecordsForSync();
        if (!records.empty()) {
            return IngestRawTraces(records.data(), records.size());
        }
    }
    return ASCSAN_STATUS_SUCCESS;
}

} // namespace ascsan
