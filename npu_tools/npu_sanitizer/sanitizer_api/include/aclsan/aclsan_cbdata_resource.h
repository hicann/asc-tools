/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_CBDATA_RESOURCE_H
#define ACLSAN_CBDATA_RESOURCE_H

#include "aclsan/aclsan_cbdata_common.h"

typedef enum AclsanResourceMemorySpace {
    ACLSAN_MEMORY_SPACE_DEVICE = 1,
    ACLSAN_MEMORY_SPACE_HOST = 2
} AclsanResourceMemorySpace;

typedef struct AclsanResourceData {
    AclsanCallbackCommonData common;
    void* ptr;            // malloc / free相关的指针
    uint64_t bytes;       // 分配的内存大小
    uint32_t memorySpace; // 区分是device / host 用AclsanResourceMemorySpace
    uint32_t deviceId;
    uint64_t resourceId; // TODO: 确认是否有用
} AclsanResourceData;

#endif
