/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_CALLBACK_H
#define ACLSAN_CALLBACK_H

#include "aclsan/aclsan_types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AclsanCallbackCommonData {
    uint32_t version;
    uint32_t size;
    const char* apiName;
    int result;
    uint64_t correlationId;
    uint64_t timestampNs;
} AclsanCallbackCommonData;

typedef enum AclsanBinaryImageFlags {
    ACLSAN_BINARY_IMAGE_FLAG_NONE = 0,
    ACLSAN_BINARY_IMAGE_FLAG_PATH_VALID = 1u << 0u,
    ACLSAN_BINARY_IMAGE_FLAG_DATA_VALID = 1u << 1u,
    ACLSAN_BINARY_IMAGE_FLAG_MATERIALIZED_PATH = 1u << 2u
} AclsanBinaryImageFlags;

typedef struct AclsanBinaryImageData {
    uint32_t kind;
    uint32_t flags;
    const char* path;
    const void* imageData;
    uint64_t imageSize;
    const char* imageVersion;
    uint64_t imageHash;
} AclsanBinaryImageData;

typedef struct AclsanResourceData {
    AclsanCallbackCommonData common;
    void* ptr;
    uint64_t bytes;
    uint32_t memorySpace;
    uint32_t deviceId;
    uint64_t resourceId;
} AclsanResourceData;

typedef struct AclsanMemoryMemcpyData {
    AclsanCallbackCommonData common;
    void* dst;
    const void* src;
    uint64_t bytes;
    uint32_t kind;
    void* stream;
} AclsanMemoryMemcpyData;

typedef struct AclsanMemoryMemsetData {
    AclsanCallbackCommonData common;
    void* dst;
    uint64_t bytes;
    int32_t value;
    void* stream;
} AclsanMemoryMemsetData;

typedef struct AclsanBinaryData {
    AclsanCallbackCommonData common;
    uint64_t binaryId;
    AclsanBinaryImageData image;
} AclsanBinaryData;

typedef struct AclsanPatchData {
    AclsanCallbackCommonData common;
    AclsanBinaryImageData original;
    AclsanBinaryImageData patched;
    uint64_t binaryId;
    uint64_t patchPlanId;
    uint32_t pipelineMask;
    uint32_t siteCount;
} AclsanPatchData;

typedef struct AclsanLaunchData {
    AclsanCallbackCommonData common;
    uint64_t launchId;
    void* function;
    void* stream;
    const char* functionName;
} AclsanLaunchData;

typedef struct AclsanSynchronizeData {
    AclsanCallbackCommonData common;
    void* stream;
} AclsanSynchronizeData;

typedef struct AclsanDeviceInstructionData {
    AclsanCallbackCommonData common;
    uint32_t pipeline;
    uint32_t cbid;
    uint32_t siteId;
    uint32_t blockId;
    uint64_t launchId;
    uint64_t binaryId;
    uint64_t functionId;
    uint64_t pc;
    uint64_t rawArgs[ACLSAN_RAW_ARG_MAX];
} AclsanDeviceInstructionData;

typedef enum AclsanDeviceSourceKind {
    ACLSAN_DEVICE_SOURCE_UNKNOWN = 0,
    ACLSAN_DEVICE_SOURCE_MTE2 = 1,
    ACLSAN_DEVICE_SOURCE_MTE3 = 2,
    ACLSAN_DEVICE_SOURCE_FIXPIPE = 3,
    ACLSAN_DEVICE_SOURCE_SET_WAIT_FLAG = 4,
    ACLSAN_DEVICE_SOURCE_GET_RLS_BUF = 5,
    ACLSAN_DEVICE_SOURCE_LD = 6,
    ACLSAN_DEVICE_SOURCE_ST = 7,
    ACLSAN_DEVICE_SOURCE_VECTOR = 8,
    ACLSAN_DEVICE_SOURCE_CUBE = 9,
    ACLSAN_DEVICE_SOURCE_SCALAR = 10
} AclsanDeviceSourceKind;

typedef enum AclsanDeviceEventFlags {
    ACLSAN_DEVICE_EVENT_FLAG_NONE = 0,
    ACLSAN_DEVICE_EVENT_FLAG_EXACT = 1u << 0,
    ACLSAN_DEVICE_EVENT_FLAG_ESTIMATED = 1u << 1,
    ACLSAN_DEVICE_EVENT_FLAG_TRUNCATED = 1u << 2,
    ACLSAN_DEVICE_EVENT_FLAG_PREDICATED = 1u << 3,
    ACLSAN_DEVICE_EVENT_FLAG_DROPPED_PRIOR = 1u << 4
} AclsanDeviceEventFlags;

typedef enum AclsanDeviceMemorySpace {
    ACLSAN_DEVICE_MEMORY_SPACE_UNKNOWN = 0,
    ACLSAN_DEVICE_MEMORY_SPACE_GM = 1,
    ACLSAN_DEVICE_MEMORY_SPACE_UB = 2,
    ACLSAN_DEVICE_MEMORY_SPACE_L1 = 3,
    ACLSAN_DEVICE_MEMORY_SPACE_L0A = 4,
    ACLSAN_DEVICE_MEMORY_SPACE_L0B = 5,
    ACLSAN_DEVICE_MEMORY_SPACE_L0C = 6,
    ACLSAN_DEVICE_MEMORY_SPACE_BT = 7,
    ACLSAN_DEVICE_MEMORY_SPACE_PRIVATE = 8
} AclsanDeviceMemorySpace;

typedef enum AclsanDeviceMemoryAccessMode {
    ACLSAN_DEVICE_MEMORY_ACCESS_READ = 1,
    ACLSAN_DEVICE_MEMORY_ACCESS_WRITE = 2,
    ACLSAN_DEVICE_MEMORY_ACCESS_READ_WRITE = 3
} AclsanDeviceMemoryAccessMode;

typedef enum AclsanMemLayoutKind {
    ACLSAN_MEM_LAYOUT_SCALAR = 1,
    ACLSAN_MEM_LAYOUT_RANGE = 2,
    ACLSAN_MEM_LAYOUT_BLOCK_REPEAT = 3,
    ACLSAN_MEM_LAYOUT_ND_AFFINE = 4
} AclsanMemLayoutKind;

typedef struct AclsanDeviceEventHeader {
    uint32_t version;
    uint32_t size;
    uint64_t launchId;
    uint64_t pc;
    uint32_t siteId;
    uint32_t sourceKind;
    uint64_t instrExecId;
    uint64_t serialNo;
    uint32_t deviceId;
    uint32_t coreId;
    uint32_t blockId;
    uint32_t blockType;
    uint32_t pipeline;
    uint32_t flags;
} AclsanDeviceEventHeader;

typedef struct AclsanMemScalarLayout {
    uint32_t bytes;
    uint32_t reserved;
} AclsanMemScalarLayout;

typedef struct AclsanMemRangeLayout {
    uint64_t bytes;
} AclsanMemRangeLayout;

typedef struct AclsanMemBlockRepeatLayout {
    uint32_t blockNum;
    uint32_t blockSize;
    int64_t blockStride;
    uint32_t repeatTimes;
    uint32_t reserved;
    int64_t repeatStride;
} AclsanMemBlockRepeatLayout;

typedef struct AclsanMemNdAffineLayout {
    uint32_t rank;
    uint32_t elementBytes;
    uint64_t dims[5];
    int64_t strides[5];
} AclsanMemNdAffineLayout;

typedef struct AclsanDeviceMemoryAccessData {
    AclsanDeviceEventHeader header;
    uint64_t address;
    uint32_t memorySpace;
    uint32_t accessMode;
    uint32_t accessIndex;
    uint32_t accessCount;
    uint32_t dataBits;
    uint32_t alignSize;
    uint64_t vectorMask0;
    uint64_t vectorMask1;
    uint64_t predicateMask0;
    uint64_t predicateMask1;
    uint32_t layoutKind;
    uint32_t memoryFlags;
    union {
        AclsanMemScalarLayout scalar;
        AclsanMemRangeLayout range;
        AclsanMemBlockRepeatLayout blockRepeat;
        AclsanMemNdAffineLayout ndAffine;
    } layout;
} AclsanDeviceMemoryAccessData;

typedef struct AclsanReportData {
    AclsanCallbackCommonData common;
    const char* tool;
    const char* message;
} AclsanReportData;

typedef struct AclsanErrorData {
    AclsanCallbackCommonData common;
    const char* tool;
    const char* message;
} AclsanErrorData;

typedef void (*AclsanCallbackFunc)(void* userdata, AclsanCallbackDomain domain, uint32_t cbid, const void* cbdata);

typedef struct AclsanSubscribeDesc {
    uint32_t version;
    uint32_t size;
    const char* name;
    AclsanCallbackFunc callback;
    void* userdata;
    uint64_t flags;
} AclsanSubscribeDesc;

AclsanStatus aclsanSubscribe(const AclsanSubscribeDesc* desc, AclsanSubscriberHandle* subscriber);
AclsanStatus aclsanUnsubscribe(AclsanSubscriberHandle subscriber);
AclsanStatus aclsanEnableCallback(
    AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, uint32_t cbid, int enable);
AclsanStatus aclsanEnableDomain(AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, int enable);
AclsanStatus aclsanGetCallbackState(
    AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, uint32_t cbid, int* enabled);
int aclsanIsInsideCallback(void);

#ifdef __cplusplus
}
#endif

#endif
