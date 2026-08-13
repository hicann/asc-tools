/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_API_H
#define ACLSAN_API_H

#include "aclsan/aclsan_callback.h"
#include "aclsan/aclsan_memory.h"
#include "aclsan/aclsan_patch.h"
#include "aclsan/aclsan_symbolize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AclsanInitParams {
    uint32_t version;
    uint32_t size;
    const AclsanLaunchConfig* launchConfig;
} AclsanInitParams;

AclsanStatus aclsanInitialize(const AclsanInitParams* params);
AclsanStatus aclsanFinalize(void);

#ifdef __cplusplus
}
#endif

#endif
