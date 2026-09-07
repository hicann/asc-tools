/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "npu_compute/acl_pti_callback_stub.h"

#include <cstdio>
#include <vector>

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
    void* functionParams;
};

struct CallbackState {
    std::vector<CallbackEvent> events;
};

void CaptureCallback(
    void* userData, aclptiCallbackDomain domain, aclptiCallbackId cbid, const aclptiCallbackData* callbackData)
{
    auto* state = static_cast<CallbackState*>(userData);
    if (state == nullptr || callbackData == nullptr) {
        return;
    }
    state->events.push_back(
        {domain, cbid, callbackData->callbackSite, callbackData->retval, callbackData->functionParams});
}

} // namespace

int main()
{
    npu_compute::test::ResetAclPtiCallbackStub();

    CallbackState state;
    aclptiSubscribeHandle subscriber = nullptr;
    CHECK(aclptiSubscribe(&subscriber, &CaptureCallback, &state, nullptr) == ACLPTI_SUCCESS);
    CHECK(subscriber != nullptr);

    int functionParams = 7;
    CHECK(!npu_compute::test::InvokeAclPtiCallback(
        ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtMalloc, ACLPTI_API_ENTER, ACL_SUCCESS, &functionParams));
    CHECK(state.events.empty());

    CHECK(
        aclptiEnableCallback(true, subscriber, ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtMalloc) ==
        ACLPTI_SUCCESS);
    CHECK(npu_compute::test::AclPtiEnableCount() == 1);

    const std::vector<npu_compute::test::AclPtiEnableCall> enableCalls = npu_compute::test::CapturedAclPtiEnableCalls();
    CHECK(enableCalls.size() == 1);
    CHECK(enableCalls[0].sequence > npu_compute::test::AclPtiSubscribeSequence());
    CHECK(enableCalls[0].enable);
    CHECK(enableCalls[0].subscriber == subscriber);
    CHECK(enableCalls[0].domain == ACLPTI_CB_DOMAIN_RUNTIME_API);
    CHECK(enableCalls[0].cbid == ACLPTI_RUNTIME_CBID_aclrtMalloc);

    CHECK(npu_compute::test::InvokeAclPtiCallback(
        ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtMalloc, ACLPTI_API_ENTER, ACL_SUCCESS, &functionParams));
    CHECK(npu_compute::test::InvokeAclPtiCallback(
        ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtMalloc, ACLPTI_API_EXIT, ACL_ERROR_INVALID_PARAM,
        &functionParams));
    CHECK(npu_compute::test::InvokeAclPtiCallback(
        ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtMalloc, ACLPTI_API_EXIT, ACL_SUCCESS, &functionParams));
    CHECK(state.events.size() == 3);
    CHECK(state.events[0].domain == ACLPTI_CB_DOMAIN_RUNTIME_API);
    CHECK(state.events[0].cbid == ACLPTI_RUNTIME_CBID_aclrtMalloc);
    CHECK(state.events[0].site == ACLPTI_API_ENTER);
    CHECK(state.events[0].functionParams == &functionParams);
    CHECK(state.events[1].site == ACLPTI_API_EXIT);
    CHECK(state.events[1].retval == ACL_ERROR_INVALID_PARAM);
    CHECK(state.events[2].site == ACLPTI_API_EXIT);
    CHECK(state.events[2].retval == ACL_SUCCESS);

    CHECK(!npu_compute::test::InvokeAclPtiCallback(
        ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtSetDevice, ACLPTI_API_EXIT, ACL_SUCCESS, nullptr));
    CHECK(state.events.size() == 3);

    CHECK(
        aclptiEnableCallback(false, subscriber, ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtMalloc) ==
        ACLPTI_SUCCESS);
    CHECK(!npu_compute::test::InvokeAclPtiCallback(
        ACLPTI_CB_DOMAIN_RUNTIME_API, ACLPTI_RUNTIME_CBID_aclrtMalloc, ACLPTI_API_EXIT, ACL_SUCCESS, nullptr));
    CHECK(state.events.size() == 3);

    return 0;
}
