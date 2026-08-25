/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "aclpti/aclpti.h"

#include "npu_compute/injection_hook.h"
#include "npu_compute/runtime_stub_api.h"

#include <array>
#include <cstdio>
#include <cstdlib>

namespace {

#define CHECK(expression)                                                                 \
    do {                                                                                  \
        if (!(expression)) {                                                              \
            std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expression); \
            return 1;                                                                     \
        }                                                                                 \
    } while (false)

struct CallbackEvent {
    aclptiCallbackDomain domain;
    aclptiCallbackId cbid;
    aclptiCallbackSite site;
    aclError retval;
};

struct CallbackState {
    std::array<CallbackEvent, 8> events{};
    std::size_t eventCount = 0;
    bool receivedExpectedUserData = true;
};

CallbackState g_callbackState;
std::size_t g_mallocSize = 0;
std::size_t g_mallocCalls = 0;
std::size_t g_freeCalls = 0;
bool g_failMalloc = false;

aclError OriginalMalloc(void** devPtr, std::size_t size, aclrtMemMallocPolicy)
{
    ++g_mallocCalls;
    g_mallocSize = size;
    if (g_failMalloc) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *devPtr = std::malloc(size);
    return *devPtr == nullptr ? ACL_ERROR_BAD_ALLOC : ACL_SUCCESS;
}

aclError OriginalFree(void* devPtr)
{
    ++g_freeCalls;
    std::free(devPtr);
    return ACL_SUCCESS;
}

void RuntimeCallback(
    void* userData, aclptiCallbackDomain domain, aclptiCallbackId cbid, const aclptiCallbackData* callbackData)
{
    auto* state = static_cast<CallbackState*>(userData);
    if (state != &g_callbackState || callbackData == nullptr || callbackData->domain != domain ||
        callbackData->cbid != cbid) {
        g_callbackState.receivedExpectedUserData = false;
        return;
    }
    if (state->eventCount < state->events.size()) {
        state->events[state->eventCount++] = {domain, cbid, callbackData->callbackSite, callbackData->retval};
    }
    if (callbackData->callbackSite == ACLPTI_API_ENTER && cbid == ACLPTI_RUNTIME_CBID_aclrtMalloc) {
        auto* params = static_cast<aclptiAclrtMallocParams*>(callbackData->functionParams);
        params->size = 64;
    }
}

} // namespace

std::int32_t MsprofStart(uint32_t, const void*, uint32_t) { return 0; }

std::int32_t MsprofStop(uint32_t, const void*, uint32_t) { return 0; }

std::int32_t MsprofRegisterDataCallback(uint32_t, void* function) { return function == nullptr ? -1 : 0; }

static_assert(
    static_cast<aclptiCallbackId>(ACLPTI_RUNTIME_CBID_aclrtLaunchKernelWithHostArgs) ==
    static_cast<aclptiCallbackId>(ACL_RT_API_aclrtLaunchKernelWithHostArgs));
static_assert(
    static_cast<aclptiCallbackId>(ACLPTI_RUNTIME_CBID_aclrtLaunchKernel) ==
    static_cast<aclptiCallbackId>(ACL_RT_API_aclrtLaunchKernel));

int main()
{
    std::size_t domainCount = 0;
    CHECK(aclptiSupportedDomains(&domainCount, nullptr) == ACLPTI_SUCCESS);
    CHECK(domainCount == 1);

    std::array<aclptiCallbackDomain, 1> insufficientDomains{};
    std::size_t insufficientCount = 0;
    CHECK(aclptiSupportedDomains(&insufficientCount, insufficientDomains.data()) == ACLPTI_ERROR_INVALID_PARAMETER);
    CHECK(insufficientCount == 1);

    std::array<aclptiCallbackDomain, 1> domains{};
    CHECK(aclptiSupportedDomains(&domainCount, domains.data()) == ACLPTI_SUCCESS);
    CHECK(domainCount == domains.size());
    CHECK(domains[0] == ACLPTI_CB_DOMAIN_RUNTIME_API);
    CHECK(aclptiSupportedDomains(nullptr, nullptr) == ACLPTI_ERROR_INVALID_PARAMETER);

    CHECK(RuntimeStubSetOriginFunction("aclrtMalloc", &OriginalMalloc) == ACL_SUCCESS);
    CHECK(RuntimeStubSetOriginFunction("aclrtFree", &OriginalFree) == ACL_SUCCESS);

    aclptiSubscribeHandle subscriber = nullptr;
    CHECK(aclptiSubscribe(&subscriber, &RuntimeCallback, &g_callbackState, nullptr) == ACLPTI_SUCCESS);
    CHECK(subscriber != nullptr);
    CHECK(
        aclptiEnableCallback(true, subscriber, ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtMalloc) ==
        ACLPTI_SUCCESS);
    CHECK(
        aclptiEnableCallback(true, subscriber, ACLPTI_CB_DOMAIN_INVALID, ACLPTI_RUNTIME_CBID_aclrtMalloc) ==
        ACLPTI_ERROR_NOT_SUPPORTED);
    CHECK(
        aclptiEnableCallback(true, subscriber, ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_SIZE) ==
        ACLPTI_ERROR_INVALID_PARAMETER);
    CHECK(
        aclptiEnableCallback(true, nullptr, ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtMalloc) ==
        ACLPTI_ERROR_INVALID_SUBSCRIBER);

    void* allocation = nullptr;
    CHECK(aclrtMalloc(&allocation, 8, ACL_MEM_MALLOC_HUGE_FIRST) == ACL_SUCCESS);
    CHECK(allocation != nullptr);
    CHECK(g_callbackState.receivedExpectedUserData);
    CHECK(g_callbackState.eventCount == 2);
    CHECK(g_callbackState.events[0].domain == ACLPTI_CB_DOMAIN_RUNTIME_API);
    CHECK(g_callbackState.events[0].cbid == ACLPTI_RUNTIME_CBID_aclrtMalloc);
    CHECK(g_callbackState.events[0].site == ACLPTI_API_ENTER);
    CHECK(g_callbackState.events[1].site == ACLPTI_API_EXIT);
    CHECK(g_callbackState.events[1].retval == ACL_SUCCESS);
    CHECK(g_mallocCalls == 2);
    CHECK(g_mallocSize == 64);

    CHECK(aclrtFree(allocation) == ACL_SUCCESS);
    CHECK(g_freeCalls == 2);
    CHECK(g_callbackState.eventCount == 2);

    g_failMalloc = true;
    allocation = nullptr;
    CHECK(aclrtMalloc(&allocation, 8, ACL_MEM_MALLOC_HUGE_FIRST) == ACL_ERROR_INVALID_PARAM);
    CHECK(allocation == nullptr);
    CHECK(g_callbackState.eventCount == 4);
    CHECK(g_callbackState.events[2].site == ACLPTI_API_ENTER);
    CHECK(g_callbackState.events[3].site == ACLPTI_API_EXIT);
    CHECK(g_callbackState.events[3].retval == ACL_ERROR_INVALID_PARAM);

    CHECK(
        aclptiEnableCallback(false, subscriber, ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtMalloc) ==
        ACLPTI_SUCCESS);
    g_failMalloc = false;
    allocation = nullptr;
    CHECK(aclrtMalloc(&allocation, 8, ACL_MEM_MALLOC_HUGE_FIRST) == ACL_SUCCESS);
    CHECK(g_callbackState.eventCount == 4);
    CHECK(aclrtFree(allocation) == ACL_SUCCESS);

    aclptiSubscribeHandle activityOnlySubscriber = nullptr;
    CHECK(aclptiSubscribe(&activityOnlySubscriber, nullptr, nullptr, nullptr) == ACLPTI_SUCCESS);
    CHECK(
        aclptiEnableCallback(
            true, activityOnlySubscriber, ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtMalloc) ==
        ACLPTI_ERROR_INVALID_STATE);
    return 0;
}
