/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_TYPES_H
#define ACLSAN_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include "aclsan/aclsan_api.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ACLSAN_API_VERSION 1u
#define ACLSAN_CONFIG_FD 199
#define ACLSAN_TOOL_NAME_MAX 32
#define ACLSAN_SYMBOL_NAME_MAX 256
#define ACLSAN_PATH_MAX 4096
#define ACLSAN_HOOK_RULE_MAX 64
#define ACLSAN_RAW_ARG_MAX 6

#define ACLSAN_INVALID_SUBSCRIBER_HANDLE ((AclsanSubscriberHandle)0)

typedef uint64_t AclsanBinaryHandle;
typedef uint64_t AclsanPatchPlanHandle;
typedef uint64_t AclsanPatchSiteHandle;
typedef uint64_t AclsanLaunchHandle;
typedef uint64_t AclsanMemoryHandle;

typedef enum AclsanPatchPipeline {
    ACLSAN_PATCH_PIPELINE_INVALID = 0,
    ACLSAN_PATCH_PIPELINE_SET_WAIT_FLAG = 1,
    ACLSAN_PATCH_PIPELINE_GET_RLS_BUF = 2,
    ACLSAN_PATCH_PIPELINE_MTE2 = 3,
    ACLSAN_PATCH_PIPELINE_MTE3 = 4,
    ACLSAN_PATCH_PIPELINE_FIXPIPE = 5
} AclsanPatchPipeline;

typedef enum AclsanDeviceSourceKind {
    ACLSAN_DEVICE_SOURCE_UNKNOWN = 0,
    ACLSAN_DEVICE_SOURCE_MTE2 = 1,
    ACLSAN_DEVICE_SOURCE_MTE3 = 2,
    ACLSAN_DEVICE_SOURCE_FIXPIPE = 3,
    ACLSAN_DEVICE_SOURCE_SET_WAIT_FLAG = 4,
    ACLSAN_DEVICE_SOURCE_GET_RLS_BUF = 5,
    ACLSAN_DEVICE_SOURCE_LD = 6,
    ACLSAN_DEVICE_SOURCE_ST = 7,
    ACLSAN_DEVICE_SOURCE_VECTOR = 8,
    ACLSAN_DEVICE_SOURCE_CUBE = 9,
    ACLSAN_DEVICE_SOURCE_SCALAR = 10
} AclsanDeviceSourceKind;

typedef enum AclsanRuntimeApiId {
    ACLSAN_RT_API_ACL_INIT = 1,
    ACLSAN_RT_API_ACL_FINALIZE = 2,
    ACLSAN_RT_API_ACLRT_MALLOC = 3,
    ACLSAN_RT_API_ACLRT_FREE = 4,
    ACLSAN_RT_API_ACLRT_MALLOC_HOST = 5,
    ACLSAN_RT_API_ACLRT_FREE_HOST = 6,
    ACLSAN_RT_API_ACLRT_MEMCPY = 7,
    ACLSAN_RT_API_ACLRT_MEMSET = 8,
    ACLSAN_RT_API_ACLRT_BINARY_LOAD_FROM_FILE = 9,
    ACLSAN_RT_API_ACLRT_BINARY_LOAD_FROM_DATA = 10,
    ACLSAN_RT_API_ACLRT_BINARY_UNLOAD = 11,
    ACLSAN_RT_API_ACLRT_GET_FUNCTION = 12,
    ACLSAN_RT_API_ACLRT_LAUNCH_KERNEL = 13,
    ACLSAN_RT_API_ACLRT_LAUNCH_KERNEL_WITH_ARGS_ARRAY = 14,
    ACLSAN_RT_API_ACLRT_SYNCHRONIZE_STREAM = 15,
    ACLSAN_RT_API_ACLRT_SYNCHRONIZE_DEVICE = 16
} AclsanRuntimeApiId;

typedef enum AclsanRuntimeEventPhase {
    ACLSAN_RUNTIME_EVENT_ENTER = 1,
    ACLSAN_RUNTIME_EVENT_EXIT = 2
} AclsanRuntimeEventPhase;

typedef enum AclsanHookAction {
    ACLSAN_HOOK_NONE = 0,
    ACLSAN_HOOK_RECORD_PRE = 1u << 0u,
    ACLSAN_HOOK_RECORD_POST = 1u << 1u,
    ACLSAN_HOOK_PATCH_BINARY = 1u << 2u,
    ACLSAN_HOOK_FLUSH_TRACE = 1u << 3u,
    ACLSAN_HOOK_TRACK_RESOURCE = 1u << 4u,
    ACLSAN_HOOK_DISPATCH_CALLBACK = 1u << 5u
} AclsanHookAction;

typedef enum AclsanMemorySpace { ACLSAN_MEMORY_SPACE_DEVICE = 1, ACLSAN_MEMORY_SPACE_HOST = 2 } AclsanMemorySpace;

typedef enum AclsanMemcpyKind {
    ACLSAN_MEMCPY_HOST_TO_HOST = 0,
    ACLSAN_MEMCPY_HOST_TO_DEVICE = 1,
    ACLSAN_MEMCPY_DEVICE_TO_HOST = 2,
    ACLSAN_MEMCPY_DEVICE_TO_DEVICE = 3,
    ACLSAN_MEMCPY_DEFAULT = 4
} AclsanMemcpyKind;

typedef enum AclsanMemoryFlags {
    ACLSAN_MEMORY_FLAG_INTERNAL = 1u << 0u,
    ACLSAN_MEMORY_FLAG_TRACE_BUFFER = 1u << 1u,
    ACLSAN_MEMORY_FLAG_VISIBLE_CALLBACK = 1u << 2u,
    ACLSAN_MEMORY_FLAG_ZERO_INIT = 1u << 3u
} AclsanMemoryFlags;

typedef struct AclsanLaunchConfig {
    uint32_t version;
    uint32_t size;
    uint64_t sessionId;
    uint32_t strict;
    uint32_t keepTemp;
    char toolName[ACLSAN_TOOL_NAME_MAX];
    char logFile[ACLSAN_PATH_MAX];
    char workDir[ACLSAN_PATH_MAX];
    char probeCacheDir[ACLSAN_PATH_MAX];
    char readyPath[ACLSAN_PATH_MAX];
} AclsanLaunchConfig;

typedef struct AclsanSubscribeDesc {
    uint32_t version;
    uint32_t size;
    const char* name;
    AclsanCallbackFunc callback;
    void* userdata;
    uint64_t flags;
} AclsanSubscribeDesc;

typedef struct AclsanRuntimeHookRule {
    uint32_t api;
    uint32_t actions;
} AclsanRuntimeHookRule;

typedef struct AclsanRuntimeHookPlan {
    uint32_t version;
    uint32_t size;
    uint64_t generation;
    uint32_t ruleCount;
    uint32_t patchPipelineMask;
    uint32_t callbackDomainMask;
    uint32_t reserved;
    AclsanRuntimeHookRule rules[ACLSAN_HOOK_RULE_MAX];
} AclsanRuntimeHookPlan;

typedef struct AclsanRuntimeHookState {
    uint32_t version;
    uint32_t size;
    uint64_t generation;
    AclsanRuntimeHookPlan activePlan;
} AclsanRuntimeHookState;

typedef struct AclsanRawTraceRecord {
    uint32_t magic;
    uint16_t version;
    uint16_t pipeline;
    uint32_t siteId;
    uint32_t blockId;
    uint64_t pc;
    uint64_t arg0;
    uint64_t arg1;
    uint64_t arg2;
    uint64_t arg3;
    uint64_t arg4;
    uint64_t arg5;
} AclsanRawTraceRecord;

typedef struct AclsanRuntimeEvent {
    uint32_t version;
    uint32_t size;
    uint32_t apiId;
    uint32_t phase;
    const char* apiName;
    const void* params;
    int result;
    uint64_t correlationId;
} AclsanRuntimeEvent;

#ifdef __cplusplus
}
#endif

#endif
