/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_ACLPTI_CALLBACK_DISPATCHER_H_
#define NPU_COMPUTE_ACLPTI_CALLBACK_DISPATCHER_H_

#include "aclpti/aclpti_callback.h"

#include "registry.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_set>

struct aclptiSubscriber_st {
    bool activityEnabled = false;
    aclptiCallbackFunc callback = nullptr;
    void* userData = nullptr;
    std::unordered_set<std::uint64_t> enabledCallbacks;
    std::mutex callbackMutex;
};

namespace npu_compute::aclpti::callback {

class Dispatcher {
public:
    aclptiSubscribeHandle SubscriberHandle();
    bool IsValidSubscriber(aclptiSubscribeHandle subscriber) const;
    bool RegisterDomain(aclptiCallbackDomain domain, std::initializer_list<aclptiCallbackId> callbackIds);
    void Configure(aclptiCallbackFunc callback, void* userData);
    aclptiResult Enable(
        bool enable, aclptiSubscribeHandle subscriber, aclptiCallbackDomain domain, aclptiCallbackId cbid);
    aclptiResult SupportedDomains(std::size_t* domainCount, aclptiCallbackDomain* domains) const;
    void Dispatch(
        aclptiCallbackDomain domain, aclptiCallbackId cbid, aclptiCallbackSite site, void* functionParams,
        aclError retval);
    void SetActivityEnabled(aclptiSubscribeHandle subscriber, bool enabled);

private:
    static std::uint64_t MakeCallbackKey(aclptiCallbackDomain domain, aclptiCallbackId cbid);

    Registry registry_;
    aclptiSubscriber_st subscriber_;
};

Dispatcher& GetDispatcher();

} // namespace npu_compute::aclpti::callback

#endif // NPU_COMPUTE_ACLPTI_CALLBACK_DISPATCHER_H_
