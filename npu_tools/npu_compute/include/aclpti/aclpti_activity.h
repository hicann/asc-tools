/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef ACLPTI_ACTIVITY_H_
#define ACLPTI_ACTIVITY_H_

#include "aclpti/aclpti_types.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum aclptiActivityKind {
    ACLPTI_ACTIVITY_KIND_INVALID = 0,
    ACLPTI_ACTIVITY_KIND_FULL = 1,
} aclptiActivityKind;

typedef struct aclptiActivityConfig {
    uint64_t reserved;
} aclptiActivityConfig;

ACLPTI_EXPORT aclptiResult
aclptiActivityEnable(aclptiSubscribeHandle subscriber, aclptiActivityKind kind, aclptiActivityConfig* pActivityConfig);

ACLPTI_EXPORT aclptiResult
aclptiActivityDisable(aclptiSubscribeHandle subscriber, aclptiActivityKind kind, aclptiActivityConfig* pActivityConfig);

#ifdef __cplusplus
}
#endif

#endif // ACLPTI_ACTIVITY_H_
