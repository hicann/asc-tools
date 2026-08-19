/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_CBDATA_RUNTIME_H
#define ACLSAN_CBDATA_RUNTIME_H

#include "aclsan/aclsan_cbdata_common.h"

typedef enum AclsanBinaryImageFlags {
    ACLSAN_BINARY_IMAGE_FLAG_NONE = 0,
    ACLSAN_BINARY_IMAGE_FLAG_PATH_VALID = 1u << 0u,
    ACLSAN_BINARY_IMAGE_FLAG_DATA_VALID = 1u << 1u,
    ACLSAN_BINARY_IMAGE_FLAG_MATERIALIZED_PATH = 1u << 2u,
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

#endif
