/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#pragma once

#include "aclpti/aclpti.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace npu_compute::test {

struct AclPtiEnableCall {
    std::size_t sequence;
    bool enable;
    aclptiSubscribeHandle subscriber;
    aclptiCallbackDomain domain;
    aclptiCallbackId cbid;
    aclptiResult result;
};

void ResetAclPtiCallbackStub();
void SetAclPtiSubscribeResult(aclptiResult result);
void SetAclPtiEnableResult(aclptiCallbackId cbid, aclptiResult result);
void SetAclPtiRangeConfigResult(aclptiResult result);

std::size_t AclPtiSubscribeCount();
std::size_t AclPtiSubscribeSequence();
std::size_t AclPtiEnableCount();
std::size_t AclPtiRangeConfigCount();
std::size_t AclPtiRangeConfigSequence();
aclptiCallbackFunc CapturedAclPtiCallback();
void* CapturedAclPtiUserData();
aclptiSubscribeHandle CapturedAclPtiSubscriber();
std::vector<AclPtiEnableCall> CapturedAclPtiEnableCalls();
std::vector<std::string> CapturedAclPtiSections();
bool InvokeAclPtiCallback(
    aclptiCallbackDomain domain, aclptiCallbackId cbid, aclptiCallbackSite site, aclError retval, void* functionParams);
bool InvokeAclPtiRuntimeReady();

} // namespace npu_compute::test

extern "C" int AclPtiCallbackStubEmitRuntimeEvent(std::uint32_t cbid, std::uint32_t site, std::int32_t retval);
