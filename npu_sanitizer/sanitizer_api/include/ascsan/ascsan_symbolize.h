/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ASCSAN_SYMBOLIZE_H
#define ASCSAN_SYMBOLIZE_H

#include "ascsan/ascsan_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum AscsanSymbolizeFlags {
    ASCSAN_SYMBOLIZE_FLAG_NONE = 0,
    ASCSAN_SYMBOLIZE_FLAG_FROM_SITE_MAP = 1u << 0u,
    ASCSAN_SYMBOLIZE_FLAG_FROM_DEBUG_INFO = 1u << 1u,
    ASCSAN_SYMBOLIZE_FLAG_INLINE = 1u << 2u,
    ASCSAN_SYMBOLIZE_FLAG_DYNAMIC_CALL = 1u << 3u,
    ASCSAN_SYMBOLIZE_FLAG_FALLBACK = 1u << 4u
} AscsanSymbolizeFlags;

typedef struct AscsanDevicePcQuery {
    uint32_t version;
    uint32_t size;
    uint64_t launchId;
    uint64_t binaryId;
    uint64_t functionId;
    uint32_t siteId;
    uint64_t instrExecId;
    uint64_t serialNo;
    uint64_t pc;
    uint64_t flags;
} AscsanDevicePcQuery;

typedef struct AscsanDeviceStackFrame {
    uint32_t version;
    uint32_t size;
    uint64_t pc;
    uint64_t functionPc;
    uint64_t functionOffset;
    uint32_t sourceLine;
    uint32_t sourceColumn;
    uint32_t flags;
    char functionName[ASCSAN_SYMBOL_NAME_MAX];
    char opName[ASCSAN_SYMBOL_NAME_MAX];
    char sourceFile[ASCSAN_PATH_MAX];
} AscsanDeviceStackFrame;

AscsanStatus ascsanSymbolizeDevicePc(
    const AscsanDevicePcQuery* query, char* payload, uint64_t payloadSize, uint64_t* payloadBytes);

#ifdef __cplusplus
}
#endif

#endif
