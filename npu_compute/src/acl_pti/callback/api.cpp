/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "aclpti/aclpti_callback.h"

#include "acl_pti/manager.h"
#include "dispatcher.h"
#include "common/debug_log.h"

extern "C" ACLPTI_EXPORT aclptiResult aclptiSubscribe(
    aclptiSubscribeHandle* subscriber, aclptiCallbackFunc callback, void* userData, aclptiSubscribeParams* pParams)
{
    if (subscriber == nullptr || (pParams != nullptr && pParams->reserved != 0) ||
        (callback == nullptr && userData != nullptr)) {
        return ACLPTI_ERROR_INVALID_PARAMETER;
    }

    *subscriber = nullptr;
    const aclptiResult result = npu_compute::aclpti::GetManager().Initialize();
    if (result == ACLPTI_SUCCESS) {
        auto& dispatcher = npu_compute::aclpti::callback::GetDispatcher();
        dispatcher.Configure(callback, userData);
        *subscriber = dispatcher.SubscriberHandle();
    }
    npu_compute::detail::DebugLog(
        "aclpti", "subscribe result=%d handle=%p", static_cast<int>(result), static_cast<void*>(*subscriber));
    return result;
}

extern "C" ACLPTI_EXPORT aclptiResult
aclptiEnableCallback(bool enable, aclptiSubscribeHandle subscriber, aclptiCallbackDomain domain, aclptiCallbackId cbid)
{
    return npu_compute::aclpti::callback::GetDispatcher().Enable(enable, subscriber, domain, cbid);
}

extern "C" ACLPTI_EXPORT aclptiResult aclptiSupportedDomains(std::size_t* domainCount, aclptiCallbackDomain* domains)
{
    const aclptiResult result = npu_compute::aclpti::GetManager().EnsureCallbackDomainsRegistered();
    if (result != ACLPTI_SUCCESS) {
        return result;
    }
    return npu_compute::aclpti::callback::GetDispatcher().SupportedDomains(domainCount, domains);
}
