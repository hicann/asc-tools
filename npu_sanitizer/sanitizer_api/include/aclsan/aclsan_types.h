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

typedef enum AclsanStatus {
    ACLSAN_STATUS_SUCCESS = 0,
    ACLSAN_STATUS_ERROR_INVALID_VALUE = 1,
    ACLSAN_STATUS_ERROR_NOT_INITIALIZED = 2,
    ACLSAN_STATUS_ERROR_ALREADY_INITIALIZED = 3,
    ACLSAN_STATUS_ERROR_VERSION_MISMATCH = 4,
    ACLSAN_STATUS_ERROR_NOT_SUPPORTED = 5,
    ACLSAN_STATUS_ERROR_OUT_OF_MEMORY = 6,
    ACLSAN_STATUS_ERROR_IO = 7,
    ACLSAN_STATUS_ERROR_RUNTIME = 8,
    ACLSAN_STATUS_ERROR_PATCH_FAILED = 9,
    ACLSAN_STATUS_ERROR_REENTRANT = 10,
    ACLSAN_STATUS_ERROR_NOT_FOUND = 11,
    ACLSAN_STATUS_ERROR_MAX_LIMIT_REACHED = 12
} AclsanStatus;

typedef struct AclsanSubscriberToken_st* AclsanSubscriberHandle;
#define ACLSAN_INVALID_SUBSCRIBER_HANDLE ((AclsanSubscriberHandle)0)

typedef uint64_t AclsanBinaryHandle;
typedef uint64_t AclsanPatchPlanHandle;
typedef uint64_t AclsanPatchSiteHandle;
typedef uint64_t AclsanLaunchHandle;
typedef uint64_t AclsanMemoryHandle;

typedef enum AclsanCallbackDomain {
    ACLSAN_CB_DOMAIN_RESOURCE = 1,
    ACLSAN_CB_DOMAIN_MEMORY = 2,
    ACLSAN_CB_DOMAIN_BINARY = 3,
    ACLSAN_CB_DOMAIN_PATCH = 4,
    ACLSAN_CB_DOMAIN_LAUNCH = 5,
    ACLSAN_CB_DOMAIN_SYNCHRONIZE = 6,
    ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION = 7,
    ACLSAN_CB_DOMAIN_REPORT = 8,
    ACLSAN_CB_DOMAIN_ERROR = 9
} AclsanCallbackDomain;

typedef enum AclsanCallbackId {
    ACLSAN_CBID_INVALID = 0,

    ACLSAN_CBID_RESOURCE_INVALID = 0,
    ACLSAN_CBID_RESOURCE_MEMORY_ALLOC = 1,
    ACLSAN_CBID_RESOURCE_MEMORY_FREE = 2,
    ACLSAN_CBID_RESOURCE_MODULE_LOAD = 3,
    ACLSAN_CBID_RESOURCE_MODULE_UNLOAD = 4,
    ACLSAN_CBID_RESOURCE_FUNCTION_GET = 5,

    ACLSAN_CBID_MEMORY_INVALID = 0,
    ACLSAN_CBID_MEMORY_MEMCPY_BEGIN = 1,
    ACLSAN_CBID_MEMORY_MEMCPY_END = 2,
    ACLSAN_CBID_MEMORY_MEMSET_BEGIN = 3,
    ACLSAN_CBID_MEMORY_MEMSET_END = 4,

    ACLSAN_CBID_BINARY_INVALID = 0,
    ACLSAN_CBID_BINARY_LOAD_BEGIN = 1,
    ACLSAN_CBID_BINARY_LOAD_END = 2,

    ACLSAN_CBID_PATCH_INVALID = 0,
    ACLSAN_CBID_PATCH_BEGIN = 1,
    ACLSAN_CBID_PATCH_END = 2,
    ACLSAN_CBID_PATCH_SITE_MAP_CREATED = 3,

    ACLSAN_CBID_LAUNCH_INVALID = 0,
    ACLSAN_CBID_LAUNCH_BEGIN = 1,
    ACLSAN_CBID_LAUNCH_END = 2,

    ACLSAN_CBID_SYNCHRONIZE_INVALID = 0,
    ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END = 1,
    ACLSAN_CBID_SYNCHRONIZE_DEVICE_SYNC_END = 2,
    ACLSAN_CBID_SYNCHRONIZE_END = ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END,

    ACLSAN_CBID_DEVICE_INSTRUCTION_INVALID = 0,
    ACLSAN_CBID_DEVICE_MEMORY_ACCESS = 1,
    ACLSAN_CBID_DEVICE_SYNC = 2,
    ACLSAN_CBID_DEVICE_STATE = 3,
    ACLSAN_CBID_DEVICE_CONTROL = 4,
    ACLSAN_CBID_DEVICE_ERROR = 5,

    ACLSAN_CBID_DEVICE_INSTRUCTION_MTE2 = ACLSAN_CBID_DEVICE_MEMORY_ACCESS,
    ACLSAN_CBID_DEVICE_INSTRUCTION_MTE3 = ACLSAN_CBID_DEVICE_MEMORY_ACCESS,
    ACLSAN_CBID_DEVICE_INSTRUCTION_FIXPIPE = ACLSAN_CBID_DEVICE_MEMORY_ACCESS,
    ACLSAN_CBID_DEVICE_INSTRUCTION_SET_WAIT_FLAG = ACLSAN_CBID_DEVICE_SYNC,
    ACLSAN_CBID_DEVICE_INSTRUCTION_GET_RLS_BUF = ACLSAN_CBID_DEVICE_SYNC,

    ACLSAN_CBID_REPORT_INVALID = 0,
    ACLSAN_CBID_REPORT_RECORD = 1,

    ACLSAN_CBID_ERROR_INVALID = 0,
    ACLSAN_CBID_ERROR_RECORD = 1
} AclsanCallbackId;

typedef enum AclsanPatchPipeline {
    ACLSAN_PATCH_PIPELINE_INVALID = 0,
    ACLSAN_PATCH_PIPELINE_SET_WAIT_FLAG = 1,
    ACLSAN_PATCH_PIPELINE_GET_RLS_BUF = 2,
    ACLSAN_PATCH_PIPELINE_MTE2 = 3,
    ACLSAN_PATCH_PIPELINE_MTE3 = 4,
    ACLSAN_PATCH_PIPELINE_FIXPIPE = 5
} AclsanPatchPipeline;

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

typedef struct AclsanRuntimeApiTable {
    uint32_t version;
    uint32_t size;
    int (*mallocDevice)(void** devPtr, size_t bytes, uint32_t policy);
    int (*freeDevice)(void* devPtr);
    int (*mallocHost)(void** hostPtr, size_t bytes);
    int (*freeHost)(void* hostPtr);
    int (*memcpy)(void* dst, size_t dstMax, const void* src, size_t bytes, int kind);
    int (*memset)(void* dst, size_t dstMax, int32_t value, size_t bytes);
    int (*synchronizeStream)(void* stream);
} AclsanRuntimeApiTable;

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
