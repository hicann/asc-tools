/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ASCSAN_PATCH_H
#define ASCSAN_PATCH_H

#include "ascsan/ascsan_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum AscsanPatchImageKind {
    ASCSAN_PATCH_IMAGE_BUILTIN = 1,
    ASCSAN_PATCH_IMAGE_FILE = 2,
    ASCSAN_PATCH_IMAGE_MEMORY = 3
} AscsanPatchImageKind;

typedef struct AscsanPatchImageDesc {
    uint32_t version;
    uint32_t size;
    AscsanPatchImageKind kind;
    const char* arch;
    const char* path;
    const void* imageData;
    uint64_t imageSize;
    const char* imageVersion;
    uint64_t flags;
} AscsanPatchImageDesc;

typedef struct AscsanPatchPipelineDesc {
    uint32_t version;
    uint32_t size;
    AscsanPatchPipeline pipeline;
    AscsanCallbackDomain callbackDomain;
    uint32_t callbackId;
    const char* name;
    const char* probeSymbol;
    const char* matcherName;
    uint32_t rawArgCount;
    uint64_t flags;
} AscsanPatchPipelineDesc;

typedef struct AscsanPatchOptions {
    uint32_t version;
    uint32_t size;
    uint32_t strict;
    uint32_t keepTemp;
    const char* workDir;
    const char* cacheDir;
    const char* toolchainRoot;
    uint64_t flags;
} AscsanPatchOptions;

typedef struct AscsanPatchSiteInfo {
    uint32_t version;
    uint32_t size;
    uint32_t siteId;
    AscsanPatchPipeline pipeline;
    uint64_t binaryId;
    uint64_t functionId;
    uint64_t pc;
    const char* functionName;
    const char* opName;
    const char* sourceFile;
    uint32_t sourceLine;
} AscsanPatchSiteInfo;

AscsanStatus ascsanRegisterBuiltinPatchPipelines(void);
AscsanStatus ascsanRegisterPatchImage(const AscsanPatchImageDesc* desc, uint64_t* patchImageId);
AscsanStatus ascsanRegisterPatchPipeline(const AscsanPatchPipelineDesc* desc);
AscsanStatus ascsanSetPatchOptions(const AscsanPatchOptions* options);
AscsanStatus ascsanBuildPatchPlanForBinary(AscsanBinaryHandle binary, AscsanPatchPlanHandle* plan);
AscsanStatus ascsanPatchBinaryFromImage(
    const AscsanPatchImageDesc* image, const AscsanPatchOptions* options, char* patchedPath, uint64_t patchedPathSize,
    AscsanPatchPlanHandle* plan);
AscsanStatus ascsanGetPatchSiteInfo(uint32_t siteId, AscsanPatchSiteInfo* info);
AscsanStatus ascsanSetLaunchUserData(
    AscsanLaunchHandle launch, void* function, void* stream, const void* deviceUserData, uint64_t deviceUserDataSize);

#ifdef __cplusplus
}
#endif

#endif
