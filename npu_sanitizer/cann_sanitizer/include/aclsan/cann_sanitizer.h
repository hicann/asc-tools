/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_CANN_SANITIZER_H
#define ACLSAN_CANN_SANITIZER_H

#include "aclsan/aclsan_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ACLSAN_CANN_EVENT_NAME_MAX 128

typedef struct AclsanCannSanitizerStats {
    uint32_t version;
    uint32_t size;
    char toolName[ACLSAN_TOOL_NAME_MAX];
    char lastApiName[ACLSAN_CANN_EVENT_NAME_MAX];
    char lastInstructionOpName[ACLSAN_CANN_EVENT_NAME_MAX];
    uint64_t callbacks;
    uint64_t resourceCallbacks;
    uint64_t memoryCallbacks;
    uint64_t binaryCallbacks;
    uint64_t patchCallbacks;
    uint64_t launchCallbacks;
    uint64_t syncCallbacks;
    uint64_t deviceInstructionCallbacks;
    uint64_t reportCallbacks;
    uint64_t errorCallbacks;
    uint64_t memoryTransferEvents;
    uint64_t syncEvents;
    uint64_t fixpipeEvents;
    uint64_t checkerEvents;
    uint64_t checkerInstructions;
    uint64_t checkerWindows;
    uint64_t checkerCompletedWindows;
    uint64_t checkerReports;
    uint32_t lastDomain;
    uint32_t lastCbid;
    int lastResult;
    uint64_t lastCorrelationId;
    uint64_t lastResourceId;
    uint64_t lastResourceBytes;
    uint32_t lastResourceMemorySpace;
    uint32_t lastResourceDeviceId;
    uint64_t lastMemoryBytes;
    uint64_t lastMemorySrc;
    uint64_t lastMemoryDst;
    uint32_t lastMemoryKind;
    uint64_t lastPatchBinaryId;
    uint64_t lastPatchPlanId;
    uint32_t lastPatchPipelineMask;
    uint32_t lastInstructionSiteId;
    uint32_t lastInstructionPipeline;
    uint64_t lastInstructionPc;
    uint64_t lastInstructionSrc;
    uint64_t lastInstructionDst;
    uint64_t lastInstructionBytes;
} AclsanCannSanitizerStats;

AclsanStatus aclsanCannSanitizerInitialize(const AclsanLaunchConfig* config);
AclsanStatus aclsanCannSanitizerFinalize(void);
AclsanStatus aclsanCannSanitizerGetStats(AclsanCannSanitizerStats* stats);

/*
 * Injection entry points. P0 accepts a nullable initInfo and imports
 * AclsanLaunchConfig from ACLSAN_CONFIG_FD when no explicit config is passed.
 */
int acltoolInitalize(const void* initInfo);
void CannComputeInit(void);

#ifdef __cplusplus
}
#endif

#endif
