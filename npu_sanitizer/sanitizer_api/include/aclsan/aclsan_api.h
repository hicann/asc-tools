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

#include <stdint.h>
#include "aclsan/aclsan_callback.h"

#define ACLSAN_EXPORT __attribute__((visibility("default")))
#define ACLSAN_API_VERSION 1u

extern "C" {

typedef uint32_t AclsanStatus;
typedef enum AclsanStatusValue {
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
    ACLSAN_STATUS_ERROR_MAX_LIMIT_REACHED = 12,
    ACLSAN_STATUS_ERROR_INVALID_PARAMETER = 13,
    ACLSAN_STATUS_ERROR_INVALID_STATE = 14,
    ACLSAN_STATUS_ERROR_ALREADY_SUBSCRIBED = 15,
    ACLSAN_STATUS_ERROR_INJECTION_FAILED = 16,
    ACLSAN_STATUS_ERROR_INTERNAL = 17
} AclsanStatusValue;

typedef enum AclsanCallbackDomain {
    ACLSAN_CB_DOMAIN_INVALID = 0,
    ACLSAN_CB_DOMAIN_RESOURCE = 1,
    ACLSAN_CB_DOMAIN_SYNCHRONIZE = 2,
    ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION = 3,
    ACLSAN_CB_DOMAIN_MEMORY = 4,
    ACLSAN_CB_DOMAIN_BINARY = 5,
    ACLSAN_CB_DOMAIN_PATCH = 6,
    ACLSAN_CB_DOMAIN_LAUNCH = 7,
    ACLSAN_CB_DOMAIN_REPORT = 8,
    ACLSAN_CB_DOMAIN_ERROR = 9,
} AclsanCallbackDomain;

typedef enum AclsanCallbackId {
    ACLSAN_CBID_INVALID = 0,

    ACLSAN_CBID_RESOURCE_INVALID = 0,
    ACLSAN_CBID_RESOURCE_MEMORY_ALLOC = 1, // 需要用到
    ACLSAN_CBID_RESOURCE_MEMORY_FREE = 2,  // 需要用到
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
    ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END = 1, // 需要用到
    ACLSAN_CBID_SYNCHRONIZE_DEVICE_SYNC_END = 2,
    ACLSAN_CBID_SYNCHRONIZE_END = ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END,

    ACLSAN_CBID_DEVICE_INSTRUCTION_INVALID = 0,
    ACLSAN_CBID_DEVICE_MEMORY_ACCESS = 1, // 需要用到
    ACLSAN_CBID_DEVICE_SYNC = 2,          // 需要用到
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
    ACLSAN_CBID_ERROR_RECORD = 1,
} AclsanCallbackId;

typedef struct AclsanSubscriber* AclsanSubscriberHandle;
typedef void (*AclsanCallbackFunc)(
    void* userdata, AclsanCallbackDomain domain, AclsanCallbackId id, const void* cbdata);

// 同时只能有1个subscriber
ACLSAN_EXPORT AclsanStatus
aclsanSubscribe(AclsanSubscriberHandle* subscriber, AclsanCallbackFunc callback, void* userdata);
ACLSAN_EXPORT AclsanStatus aclsanUnsubscribe(AclsanSubscriberHandle subscriber);

// 以下相关函数不能被多个线程并发调用
ACLSAN_EXPORT AclsanStatus aclsanEnableCallback(
    uint32_t enable, AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, AclsanCallbackId cbid);
ACLSAN_EXPORT AclsanStatus
aclsanEnableDomain(AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, uint32_t enable);
ACLSAN_EXPORT AclsanStatus aclsanGetCallbackState(
    AclsanSubscriberHandle subscriber, AclsanCallbackDomain domain, AclsanCallbackId cbid, uint32_t* enabled);
}

#endif
