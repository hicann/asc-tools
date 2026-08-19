/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_CBDATA_COMMON_H
#define ACLSAN_CBDATA_COMMON_H

#include <stdint.h>

typedef struct AclsanCallbackCommonData {
    uint32_t version;
    uint32_t size;
    const char* apiName;    // 调用的api命名，例如AclsanResourceData可能传回aclrtMalloc
    int result;             // 运行接口返回的结果
    uint64_t correlationId; // TODO: 确认是否有用
    uint64_t timestampNs;   // TODO: 确认是否有用
} AclsanCallbackCommonData;

#endif
