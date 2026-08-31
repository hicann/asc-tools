/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef ACLPTI_CALLBACK_H_
#define ACLPTI_CALLBACK_H_

#include "aclpti/aclpti_types.h"

#include "acl/acl_rt.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum aclptiCallbackDomain {
    ACLPTI_CB_DOMAIN_INVALID = 0,
    ACLPTI_CB_DOMAIN_RUNTIME_API = 1,
    ACLPTI_CB_DOMAIN_SIZE,
} aclptiCallbackDomain;

typedef enum aclptiCallbackSite {
    ACLPTI_API_ENTER = 0,
    ACLPTI_API_EXIT = 1,
} aclptiCallbackSite;

typedef uint32_t aclptiCallbackId;

typedef struct aclptiCallbackData {
    aclptiCallbackDomain domain;
    aclptiCallbackId cbid;
    aclptiCallbackSite callbackSite;
    void* functionParams;
    aclError retval;
} aclptiCallbackData;

typedef void (*aclptiCallbackFunc)(
    void* userdata, aclptiCallbackDomain domain, aclptiCallbackId cbid, const aclptiCallbackData* cbData);

ACLPTI_EXPORT aclptiResult aclptiSubscribe(
    aclptiSubscribeHandle* subscriber, aclptiCallbackFunc callback, void* userData, aclptiSubscribeParams* pParams);

ACLPTI_EXPORT aclptiResult
aclptiEnableCallback(bool enable, aclptiSubscribeHandle subscriber, aclptiCallbackDomain domain, aclptiCallbackId cbid);

ACLPTI_EXPORT aclptiResult aclptiSupportedDomains(size_t* domainCount, aclptiCallbackDomain* domains);

#ifdef __cplusplus
}
#endif

#endif // ACLPTI_CALLBACK_H_
