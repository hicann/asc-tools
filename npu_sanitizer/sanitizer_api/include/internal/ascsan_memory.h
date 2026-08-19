/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_MEMORY_H
#define ACLSAN_MEMORY_H

#include "internal/ascsan_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AclsanMemoryAllocDesc {
    uint32_t version;
    uint32_t size;
    AclsanMemorySpace space;
    uint32_t deviceId;
    uint64_t bytes;
    uint64_t alignment;
    uint64_t flags;
    void* stream;
} AclsanMemoryAllocDesc;

typedef struct AclsanMemoryInfo {
    uint32_t version;
    uint32_t size;
    void* ptr;
    uint64_t bytes;
    AclsanMemorySpace space;
    uint32_t deviceId;
    uint64_t memoryId;
    uint64_t flags;
} AclsanMemoryInfo;

AclsanStatus aclsanMemoryAlloc(const AclsanMemoryAllocDesc* desc, void** ptr, AclsanMemoryHandle* memory);
AclsanStatus aclsanMemoryFree(void* ptr);
AclsanStatus aclsanMemoryMemcpy(void* dst, uint64_t dstMax, const void* src, uint64_t bytes, AclsanMemcpyKind kind);
AclsanStatus aclsanMemoryMemcpyAsync(
    void* dst, uint64_t dstMax, const void* src, uint64_t bytes, AclsanMemcpyKind kind, void* stream);
AclsanStatus aclsanMemoryMemset(void* dst, uint64_t dstMax, int32_t value, uint64_t bytes);
AclsanStatus aclsanMemoryMemsetAsync(void* dst, uint64_t dstMax, int32_t value, uint64_t bytes, void* stream);
AclsanStatus aclsanMemorySynchronizeStream(void* stream);
AclsanStatus aclsanMemoryGetInfo(const void* ptr, AclsanMemoryInfo* info);

AclsanStatus aclsanDeviceMalloc(void** devPtr, uint64_t bytes);
AclsanStatus aclsanDeviceFree(void* devPtr);
AclsanStatus aclsanMemcpyD2H(void* dstHost, const void* srcDevice, uint64_t bytes);
AclsanStatus aclsanMemcpyH2D(void* dstDevice, const void* srcHost, uint64_t bytes);
AclsanStatus aclsanStreamSynchronize(void* stream);

#ifdef __cplusplus
}
#endif

#endif
