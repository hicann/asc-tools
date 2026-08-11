/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ASCSAN_API_H
#define ASCSAN_API_H

#include "ascsan/ascsan_callback.h"
#include "ascsan/ascsan_memory.h"
#include "ascsan/ascsan_patch.h"
#include "ascsan/ascsan_symbolize.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AscsanInitParams {
    uint32_t version;
    uint32_t size;
    const AscsanLaunchConfig* launchConfig;
} AscsanInitParams;

AscsanStatus ascsanInitialize(const AscsanInitParams* params);
AscsanStatus ascsanFinalize(void);

#ifdef __cplusplus
}
#endif

#endif
