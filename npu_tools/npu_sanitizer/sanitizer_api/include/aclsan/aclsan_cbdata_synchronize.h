/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_CBDATA_SYNCHRONIZE_H
#define ACLSAN_CBDATA_SYNCHRONIZE_H

#include "aclsan/aclsan_cbdata_common.h"

typedef enum AclsanTraceCollectionStatus {
    ACLSAN_TRACE_COLLECTION_NOT_REQUIRED = 0,
    ACLSAN_TRACE_COLLECTION_COMPLETE = 1,
    ACLSAN_TRACE_COLLECTION_DEFERRED = 2,
    ACLSAN_TRACE_COLLECTION_FAILED = 3,
} AclsanTraceCollectionStatus;

typedef struct AclsanSynchronizeData {
    AclsanCallbackCommonData common;
    void* stream;                   // 对应aclrtStream
    uint32_t traceCollectionStatus; // 本次同步后的插桩 trace 采集状态。
    uint32_t pendingTraceLaunches;  // DEFERRED 时仍由 sanitizer 持有的 launch 数。
} AclsanSynchronizeData;

#endif
