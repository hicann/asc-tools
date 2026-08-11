/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ASCSAN_TYPES_H
#define ASCSAN_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ASCSAN_API_VERSION 1u
#define ASCSAN_CONFIG_FD 199
#define ASCSAN_TOOL_NAME_MAX 32
#define ASCSAN_SYMBOL_NAME_MAX 256
#define ASCSAN_PATH_MAX 4096
#define ASCSAN_HOOK_RULE_MAX 64
#define ASCSAN_RAW_ARG_MAX 6

typedef enum AscsanStatus {
    ASCSAN_STATUS_SUCCESS = 0,
    ASCSAN_STATUS_ERROR_INVALID_VALUE = 1,
    ASCSAN_STATUS_ERROR_NOT_INITIALIZED = 2,
    ASCSAN_STATUS_ERROR_ALREADY_INITIALIZED = 3,
    ASCSAN_STATUS_ERROR_VERSION_MISMATCH = 4,
    ASCSAN_STATUS_ERROR_NOT_SUPPORTED = 5,
    ASCSAN_STATUS_ERROR_OUT_OF_MEMORY = 6,
    ASCSAN_STATUS_ERROR_IO = 7,
    ASCSAN_STATUS_ERROR_RUNTIME = 8,
    ASCSAN_STATUS_ERROR_PATCH_FAILED = 9,
    ASCSAN_STATUS_ERROR_REENTRANT = 10,
    ASCSAN_STATUS_ERROR_NOT_FOUND = 11,
    ASCSAN_STATUS_ERROR_MAX_LIMIT_REACHED = 12
} AscsanStatus;

typedef struct AscsanSubscriberToken_st* AscsanSubscriberHandle;
#define ASCSAN_INVALID_SUBSCRIBER_HANDLE ((AscsanSubscriberHandle)0)

typedef uint64_t AscsanBinaryHandle;
typedef uint64_t AscsanPatchPlanHandle;
typedef uint64_t AscsanPatchSiteHandle;
typedef uint64_t AscsanLaunchHandle;
typedef uint64_t AscsanMemoryHandle;

typedef enum AscsanCallbackDomain {
    ASCSAN_CB_DOMAIN_RESOURCE = 1,
    ASCSAN_CB_DOMAIN_MEMORY = 2,
    ASCSAN_CB_DOMAIN_BINARY = 3,
    ASCSAN_CB_DOMAIN_PATCH = 4,
    ASCSAN_CB_DOMAIN_LAUNCH = 5,
    ASCSAN_CB_DOMAIN_SYNCHRONIZE = 6,
    ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION = 7,
    ASCSAN_CB_DOMAIN_REPORT = 8,
    ASCSAN_CB_DOMAIN_ERROR = 9
} AscsanCallbackDomain;

typedef enum AscsanCallbackId {
    ASCSAN_CBID_INVALID = 0,

    ASCSAN_CBID_RESOURCE_INVALID = 0,
    ASCSAN_CBID_RESOURCE_MEMORY_ALLOC = 1,
    ASCSAN_CBID_RESOURCE_MEMORY_FREE = 2,
    ASCSAN_CBID_RESOURCE_MODULE_LOAD = 3,
    ASCSAN_CBID_RESOURCE_MODULE_UNLOAD = 4,
    ASCSAN_CBID_RESOURCE_FUNCTION_GET = 5,

    ASCSAN_CBID_MEMORY_INVALID = 0,
    ASCSAN_CBID_MEMORY_MEMCPY_BEGIN = 1,
    ASCSAN_CBID_MEMORY_MEMCPY_END = 2,
    ASCSAN_CBID_MEMORY_MEMSET_BEGIN = 3,
    ASCSAN_CBID_MEMORY_MEMSET_END = 4,

    ASCSAN_CBID_BINARY_INVALID = 0,
    ASCSAN_CBID_BINARY_LOAD_BEGIN = 1,
    ASCSAN_CBID_BINARY_LOAD_END = 2,

    ASCSAN_CBID_PATCH_INVALID = 0,
    ASCSAN_CBID_PATCH_BEGIN = 1,
    ASCSAN_CBID_PATCH_END = 2,
    ASCSAN_CBID_PATCH_SITE_MAP_CREATED = 3,

    ASCSAN_CBID_LAUNCH_INVALID = 0,
    ASCSAN_CBID_LAUNCH_BEGIN = 1,
    ASCSAN_CBID_LAUNCH_END = 2,

    ASCSAN_CBID_SYNCHRONIZE_INVALID = 0,
    ASCSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END = 1,
    ASCSAN_CBID_SYNCHRONIZE_DEVICE_SYNC_END = 2,
    ASCSAN_CBID_SYNCHRONIZE_END = ASCSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END,

    ASCSAN_CBID_DEVICE_INSTRUCTION_INVALID = 0,
    ASCSAN_CBID_DEVICE_MEMORY_ACCESS = 1,
    ASCSAN_CBID_DEVICE_SYNC = 2,
    ASCSAN_CBID_DEVICE_STATE = 3,
    ASCSAN_CBID_DEVICE_CONTROL = 4,
    ASCSAN_CBID_DEVICE_ERROR = 5,

    ASCSAN_CBID_DEVICE_INSTRUCTION_MTE2 = ASCSAN_CBID_DEVICE_MEMORY_ACCESS,
    ASCSAN_CBID_DEVICE_INSTRUCTION_MTE3 = ASCSAN_CBID_DEVICE_MEMORY_ACCESS,
    ASCSAN_CBID_DEVICE_INSTRUCTION_FIXPIPE = ASCSAN_CBID_DEVICE_MEMORY_ACCESS,
    ASCSAN_CBID_DEVICE_INSTRUCTION_SET_WAIT_FLAG = ASCSAN_CBID_DEVICE_SYNC,
    ASCSAN_CBID_DEVICE_INSTRUCTION_GET_RLS_BUF = ASCSAN_CBID_DEVICE_SYNC,

    ASCSAN_CBID_REPORT_INVALID = 0,
    ASCSAN_CBID_REPORT_RECORD = 1,

    ASCSAN_CBID_ERROR_INVALID = 0,
    ASCSAN_CBID_ERROR_RECORD = 1
} AscsanCallbackId;

typedef enum AscsanPatchPipeline {
    ASCSAN_PATCH_PIPELINE_INVALID = 0,
    ASCSAN_PATCH_PIPELINE_SET_WAIT_FLAG = 1,
    ASCSAN_PATCH_PIPELINE_GET_RLS_BUF = 2,
    ASCSAN_PATCH_PIPELINE_MTE2 = 3,
    ASCSAN_PATCH_PIPELINE_MTE3 = 4,
    ASCSAN_PATCH_PIPELINE_FIXPIPE = 5
} AscsanPatchPipeline;

typedef enum AscsanRuntimeApiId {
    ASCSAN_RT_API_ACL_INIT = 1,
    ASCSAN_RT_API_ACL_FINALIZE = 2,
    ASCSAN_RT_API_ACLRT_MALLOC = 3,
    ASCSAN_RT_API_ACLRT_FREE = 4,
    ASCSAN_RT_API_ACLRT_MALLOC_HOST = 5,
    ASCSAN_RT_API_ACLRT_FREE_HOST = 6,
    ASCSAN_RT_API_ACLRT_MEMCPY = 7,
    ASCSAN_RT_API_ACLRT_MEMSET = 8,
    ASCSAN_RT_API_ACLRT_BINARY_LOAD_FROM_FILE = 9,
    ASCSAN_RT_API_ACLRT_BINARY_LOAD_FROM_DATA = 10,
    ASCSAN_RT_API_ACLRT_BINARY_UNLOAD = 11,
    ASCSAN_RT_API_ACLRT_GET_FUNCTION = 12,
    ASCSAN_RT_API_ACLRT_LAUNCH_KERNEL = 13,
    ASCSAN_RT_API_ACLRT_LAUNCH_KERNEL_WITH_ARGS_ARRAY = 14,
    ASCSAN_RT_API_ACLRT_SYNCHRONIZE_STREAM = 15,
    ASCSAN_RT_API_ACLRT_SYNCHRONIZE_DEVICE = 16
} AscsanRuntimeApiId;

typedef enum AscsanRuntimeEventPhase {
    ASCSAN_RUNTIME_EVENT_ENTER = 1,
    ASCSAN_RUNTIME_EVENT_EXIT = 2
} AscsanRuntimeEventPhase;

typedef enum AscsanHookAction {
    ASCSAN_HOOK_NONE = 0,
    ASCSAN_HOOK_RECORD_PRE = 1u << 0u,
    ASCSAN_HOOK_RECORD_POST = 1u << 1u,
    ASCSAN_HOOK_PATCH_BINARY = 1u << 2u,
    ASCSAN_HOOK_FLUSH_TRACE = 1u << 3u,
    ASCSAN_HOOK_TRACK_RESOURCE = 1u << 4u,
    ASCSAN_HOOK_DISPATCH_CALLBACK = 1u << 5u
} AscsanHookAction;

typedef enum AscsanMemorySpace { ASCSAN_MEMORY_SPACE_DEVICE = 1, ASCSAN_MEMORY_SPACE_HOST = 2 } AscsanMemorySpace;

typedef enum AscsanMemcpyKind {
    ASCSAN_MEMCPY_HOST_TO_HOST = 0,
    ASCSAN_MEMCPY_HOST_TO_DEVICE = 1,
    ASCSAN_MEMCPY_DEVICE_TO_HOST = 2,
    ASCSAN_MEMCPY_DEVICE_TO_DEVICE = 3,
    ASCSAN_MEMCPY_DEFAULT = 4
} AscsanMemcpyKind;

typedef enum AscsanMemoryFlags {
    ASCSAN_MEMORY_FLAG_INTERNAL = 1u << 0u,
    ASCSAN_MEMORY_FLAG_TRACE_BUFFER = 1u << 1u,
    ASCSAN_MEMORY_FLAG_VISIBLE_CALLBACK = 1u << 2u,
    ASCSAN_MEMORY_FLAG_ZERO_INIT = 1u << 3u
} AscsanMemoryFlags;

typedef struct AscsanLaunchConfig {
    uint32_t version;
    uint32_t size;
    uint64_t sessionId;
    uint32_t strict;
    uint32_t keepTemp;
    char toolName[ASCSAN_TOOL_NAME_MAX];
    char logFile[ASCSAN_PATH_MAX];
    char workDir[ASCSAN_PATH_MAX];
    char probeCacheDir[ASCSAN_PATH_MAX];
    char readyPath[ASCSAN_PATH_MAX];
} AscsanLaunchConfig;

typedef struct AscsanRuntimeHookRule {
    uint32_t api;
    uint32_t actions;
} AscsanRuntimeHookRule;

typedef struct AscsanRuntimeHookPlan {
    uint32_t version;
    uint32_t size;
    uint64_t generation;
    uint32_t ruleCount;
    uint32_t patchPipelineMask;
    uint32_t callbackDomainMask;
    uint32_t reserved;
    AscsanRuntimeHookRule rules[ASCSAN_HOOK_RULE_MAX];
} AscsanRuntimeHookPlan;

typedef struct AscsanRuntimeHookState {
    uint32_t version;
    uint32_t size;
    uint64_t generation;
    AscsanRuntimeHookPlan activePlan;
} AscsanRuntimeHookState;

typedef struct AscsanRuntimeApiTable {
    uint32_t version;
    uint32_t size;
    int (*mallocDevice)(void** devPtr, size_t bytes, uint32_t policy);
    int (*freeDevice)(void* devPtr);
    int (*mallocHost)(void** hostPtr, size_t bytes);
    int (*freeHost)(void* hostPtr);
    int (*memcpy)(void* dst, size_t dstMax, const void* src, size_t bytes, int kind);
    int (*memset)(void* dst, size_t dstMax, int32_t value, size_t bytes);
    int (*synchronizeStream)(void* stream);
} AscsanRuntimeApiTable;

typedef struct AscsanRawTraceRecord {
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
} AscsanRawTraceRecord;

typedef struct AscsanRuntimeEvent {
    uint32_t version;
    uint32_t size;
    uint32_t apiId;
    uint32_t phase;
    const char* apiName;
    const void* params;
    int result;
    uint64_t correlationId;
} AscsanRuntimeEvent;

#ifdef __cplusplus
}
#endif

#endif
