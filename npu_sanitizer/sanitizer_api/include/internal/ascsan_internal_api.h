/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_INTERNAL_API_H
#define ACLSAN_INTERNAL_API_H

#include "internal/ascsan_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal SPI for sanitizer_api.so providers.
 *
 * These entry points are intentionally separated from the public ACLSan headers. They are
 * used by in-repo provider modules such as injection.so, npu_check.so,
 * trace fetch backends, and tests. They are not part of the stable checker API.
 */

typedef struct AclsanRuntimeMemoryAllocParams {
    uint32_t version;
    uint32_t size;
    void* ptr;
    uint64_t bytes;
    uint32_t memorySpace;
    uint32_t deviceId;
    uint64_t resourceId;
} AclsanRuntimeMemoryAllocParams;

typedef struct AclsanRuntimeMemoryFreeParams {
    uint32_t version;
    uint32_t size;
    void* ptr;
    uint64_t bytes;
    uint32_t memorySpace;
    uint32_t deviceId;
    uint64_t resourceId;
} AclsanRuntimeMemoryFreeParams;

typedef struct AclsanRuntimeMemcpyParams {
    uint32_t version;
    uint32_t size;
    void* dst;
    uint64_t dstMax;
    const void* src;
    uint64_t bytes;
    uint32_t kind;
    void* stream;
} AclsanRuntimeMemcpyParams;

typedef struct AclsanRuntimeMemsetParams {
    uint32_t version;
    uint32_t size;
    void* dst;
    uint64_t dstMax;
    int32_t value;
    uint64_t bytes;
    void* stream;
} AclsanRuntimeMemsetParams;

typedef struct AclsanRuntimeBinaryLoadFromFileParams {
    uint32_t version;
    uint32_t size;
    const char* path;
    const char* imageVersion;
    uint64_t binaryId;
} AclsanRuntimeBinaryLoadFromFileParams;

typedef struct AclsanRuntimeBinaryLoadFromDataParams {
    uint32_t version;
    uint32_t size;
    const void* imageData;
    uint64_t imageSize;
    const char* imageVersion;
    uint64_t binaryId;
} AclsanRuntimeBinaryLoadFromDataParams;

const char* aclsanGetVersionString(void);

AclsanStatus aclsanExportLaunchConfigToFd(const AclsanLaunchConfig* config, int fd);
AclsanStatus aclsanImportLaunchConfigFromFd(int fd, AclsanLaunchConfig* config);
AclsanStatus aclsanApplyLaunchConfig(const AclsanLaunchConfig* config);
const AclsanLaunchConfig* aclsanGetLaunchConfig(void);

AclsanStatus aclsanOnRuntimeEvent(const AclsanRuntimeEvent* event);
AclsanStatus aclsanConfigureRuntimeHook(const AclsanRuntimeHookPlan* plan);
AclsanStatus aclsanGetRuntimeHookState(AclsanRuntimeHookState* state);

AclsanStatus aclsanIngestRawTraces(const AclsanRawTraceRecord* records, uint64_t count);
AclsanStatus aclsanReportError(const char* tool, const char* message);
AclsanStatus aclsanFlushReports(void);

#ifdef __cplusplus
}
#endif

#endif
