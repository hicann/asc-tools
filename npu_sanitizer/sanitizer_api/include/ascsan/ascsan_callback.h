/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ASCSAN_CALLBACK_H
#define ASCSAN_CALLBACK_H

#include "ascsan/ascsan_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AscsanCallbackCommonData {
    uint32_t version;
    uint32_t size;
    const char* apiName;
    int result;
    uint64_t correlationId;
    uint64_t timestampNs;
} AscsanCallbackCommonData;

typedef enum AscsanBinaryImageFlags {
    ASCSAN_BINARY_IMAGE_FLAG_NONE = 0,
    ASCSAN_BINARY_IMAGE_FLAG_PATH_VALID = 1u << 0u,
    ASCSAN_BINARY_IMAGE_FLAG_DATA_VALID = 1u << 1u,
    ASCSAN_BINARY_IMAGE_FLAG_MATERIALIZED_PATH = 1u << 2u
} AscsanBinaryImageFlags;

typedef struct AscsanBinaryImageData {
    uint32_t kind;
    uint32_t flags;
    const char* path;
    const void* imageData;
    uint64_t imageSize;
    const char* imageVersion;
    uint64_t imageHash;
} AscsanBinaryImageData;

typedef struct AscsanResourceData {
    AscsanCallbackCommonData common;
    void* ptr;
    uint64_t bytes;
    uint32_t memorySpace;
    uint32_t deviceId;
    uint64_t resourceId;
} AscsanResourceData;

typedef struct AscsanMemoryMemcpyData {
    AscsanCallbackCommonData common;
    void* dst;
    const void* src;
    uint64_t bytes;
    uint32_t kind;
    void* stream;
} AscsanMemoryMemcpyData;

typedef struct AscsanMemoryMemsetData {
    AscsanCallbackCommonData common;
    void* dst;
    uint64_t bytes;
    int32_t value;
    void* stream;
} AscsanMemoryMemsetData;

typedef struct AscsanBinaryData {
    AscsanCallbackCommonData common;
    uint64_t binaryId;
    AscsanBinaryImageData image;
} AscsanBinaryData;

typedef struct AscsanPatchData {
    AscsanCallbackCommonData common;
    AscsanBinaryImageData original;
    AscsanBinaryImageData patched;
    uint64_t binaryId;
    uint64_t patchPlanId;
    uint32_t pipelineMask;
    uint32_t siteCount;
} AscsanPatchData;

typedef struct AscsanLaunchData {
    AscsanCallbackCommonData common;
    uint64_t launchId;
    void* function;
    void* stream;
    const char* functionName;
} AscsanLaunchData;

typedef struct AscsanSynchronizeData {
    AscsanCallbackCommonData common;
    void* stream;
} AscsanSynchronizeData;

typedef struct AscsanDeviceInstructionData {
    AscsanCallbackCommonData common;
    uint32_t pipeline;
    uint32_t cbid;
    uint32_t siteId;
    uint32_t blockId;
    uint64_t launchId;
    uint64_t binaryId;
    uint64_t functionId;
    uint64_t pc;
    uint64_t rawArgs[ASCSAN_RAW_ARG_MAX];
} AscsanDeviceInstructionData;

typedef struct AscsanReportData {
    AscsanCallbackCommonData common;
    const char* tool;
    const char* message;
} AscsanReportData;

typedef struct AscsanErrorData {
    AscsanCallbackCommonData common;
    const char* tool;
    const char* message;
} AscsanErrorData;

typedef void (*AscsanCallbackFunc)(void* userdata, AscsanCallbackDomain domain, uint32_t cbid, const void* cbdata);

typedef struct AscsanSubscribeDesc {
    uint32_t version;
    uint32_t size;
    const char* name;
    AscsanCallbackFunc callback;
    void* userdata;
    uint64_t flags;
} AscsanSubscribeDesc;

AscsanStatus ascsanSubscribe(const AscsanSubscribeDesc* desc, AscsanSubscriberHandle* subscriber);
AscsanStatus ascsanUnsubscribe(AscsanSubscriberHandle subscriber);
AscsanStatus ascsanEnableCallback(
    AscsanSubscriberHandle subscriber, AscsanCallbackDomain domain, uint32_t cbid, int enable);
AscsanStatus ascsanEnableDomain(AscsanSubscriberHandle subscriber, AscsanCallbackDomain domain, int enable);
AscsanStatus ascsanGetCallbackState(
    AscsanSubscriberHandle subscriber, AscsanCallbackDomain domain, uint32_t cbid, int* enabled);
int ascsanIsInsideCallback(void);

#ifdef __cplusplus
}
#endif

#endif
