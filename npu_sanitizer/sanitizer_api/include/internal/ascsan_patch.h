/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_PATCH_H
#define ACLSAN_PATCH_H

#include "aclsan/aclsan_api.h"
#include "internal/ascsan_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum AclsanPatchImageKind {
    ACLSAN_PATCH_IMAGE_BUILTIN = 1,
    ACLSAN_PATCH_IMAGE_FILE = 2,
    ACLSAN_PATCH_IMAGE_MEMORY = 3
} AclsanPatchImageKind;

typedef struct AclsanPatchImageDesc {
    uint32_t version;
    uint32_t size;
    AclsanPatchImageKind kind;
    const char* arch;
    const char* path;
    const void* imageData;
    uint64_t imageSize;
    const char* imageVersion;
    uint64_t flags;
} AclsanPatchImageDesc;

typedef struct AclsanPatchPipelineDesc {
    uint32_t version;
    uint32_t size;
    AclsanPatchPipeline pipeline;
    AclsanCallbackDomain callbackDomain;
    uint32_t callbackId;
    const char* name;
    const char* probeSymbol;
    const char* matcherName;
    uint32_t rawArgCount;
    uint64_t flags;
} AclsanPatchPipelineDesc;

typedef struct AclsanPatchOptions {
    uint32_t version;
    uint32_t size;
    uint32_t strict;
    uint32_t keepTemp;
    const char* workDir;
    const char* cacheDir;
    const char* toolchainRoot;
    uint64_t flags;
} AclsanPatchOptions;

typedef struct AclsanPatchSiteInfo {
    uint32_t version;
    uint32_t size;
    uint32_t siteId;
    AclsanPatchPipeline pipeline;
    uint64_t binaryId;
    uint64_t functionId;
    uint64_t pc;
    const char* functionName;
    const char* opName;
    const char* sourceFile;
    uint32_t sourceLine;
} AclsanPatchSiteInfo;

AclsanStatus aclsanRegisterBuiltinPatchPipelines(void);
AclsanStatus aclsanRegisterPatchImage(const AclsanPatchImageDesc* desc, uint64_t* patchImageId);
AclsanStatus aclsanRegisterPatchPipeline(const AclsanPatchPipelineDesc* desc);
AclsanStatus aclsanSetPatchOptions(const AclsanPatchOptions* options);
AclsanStatus aclsanBuildPatchPlanForBinary(AclsanBinaryHandle binary, AclsanPatchPlanHandle* plan);
AclsanStatus aclsanPatchBinaryFromImage(
    const AclsanPatchImageDesc* image, const AclsanPatchOptions* options, char* patchedPath, uint64_t patchedPathSize,
    AclsanPatchPlanHandle* plan);
AclsanStatus aclsanGetPatchSiteInfo(uint32_t siteId, AclsanPatchSiteInfo* info);
AclsanStatus aclsanSetLaunchUserData(
    AclsanLaunchHandle launch, void* function, void* stream, const void* deviceUserData, uint64_t deviceUserDataSize);

#ifdef __cplusplus
}
#endif

#endif
