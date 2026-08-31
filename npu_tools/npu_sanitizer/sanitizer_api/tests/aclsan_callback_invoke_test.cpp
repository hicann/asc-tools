/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aclsan/aclsan_api.h"
#include "internal/aclsan_dispatch.h"
#include "internal/aclsan_device_call_stack.h"
#include "internal/aclsan_runtime_hook.h"

#include <cassert>
#include <cstdint>
#include <set>

namespace {

uint32_t g_callbackCalls = 0;

void CaptureCallback(void*, AclsanCallbackDomain, AclsanCallbackId, const void*) { ++g_callbackCalls; }

} // namespace

extern "C" int32_t acltoolHookInit(void) { return 0; }

namespace aclsan {

void ApplyRuntimeHooks(const std::set<aclrtApiId>&) noexcept {}

AclsanStatus ResolveActiveDeviceCallStack(uint64_t, device_runtime::CallStackResult*) noexcept
{
    return ACLSAN_STATUS_ERROR_NOT_FOUND;
}

} // namespace aclsan

int main()
{
    AclsanSubscriberHandle subscriber = nullptr;
    assert(aclsanSubscribe(&subscriber, CaptureCallback, nullptr) == ACLSAN_STATUS_SUCCESS);

    uint32_t enabled = 1;
    assert(
        aclsanGetCallbackState(subscriber, ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC, &enabled) ==
        ACLSAN_STATUS_SUCCESS);
    assert(enabled == 0);

    assert(!aclsan::InvokeCallback(ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC, nullptr));
    assert(g_callbackCalls == 0);

    const AclsanResourceData callbackData{};
    assert(aclsan::InvokeCallback(ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC, &callbackData));
    assert(g_callbackCalls == 0);

    assert(
        aclsanEnableCallback(1, subscriber, ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC) ==
        ACLSAN_STATUS_SUCCESS);
    assert(
        aclsanGetCallbackState(subscriber, ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC, &enabled) ==
        ACLSAN_STATUS_SUCCESS);
    assert(enabled == 1);
    assert(aclsan::InvokeCallback(ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC, &callbackData));
    assert(g_callbackCalls == 1);

    assert(
        aclsanEnableCallback(0, subscriber, ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC) ==
        ACLSAN_STATUS_SUCCESS);
    assert(
        aclsanGetCallbackState(subscriber, ACLSAN_CB_DOMAIN_RESOURCE, ACLSAN_CBID_RESOURCE_MEMORY_ALLOC, &enabled) ==
        ACLSAN_STATUS_SUCCESS);
    assert(enabled == 0);

    assert(aclsanUnsubscribe(subscriber) == ACLSAN_STATUS_SUCCESS);
    return 0;
}
