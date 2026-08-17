/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "registry.h"

namespace npu_compute::aclpti::callback {

bool Registry::RegisterDomain(aclptiCallbackDomain domain, std::initializer_list<aclptiCallbackId> callbackIds)
{
    if (domain == ACLPTI_CB_DOMAIN_INVALID || domain >= ACLPTI_CB_DOMAIN_SIZE || callbackIds.size() == 0) {
        return false;
    }
    auto [iterator, inserted] = callbacks_.emplace(domain, std::unordered_set<aclptiCallbackId>{});
    if (!inserted) {
        return false;
    }
    iterator->second.insert(callbackIds.begin(), callbackIds.end());
    return true;
}

bool Registry::IsSupported(aclptiCallbackDomain domain, aclptiCallbackId cbid) const
{
    const auto domainIterator = callbacks_.find(domain);
    if (domainIterator == callbacks_.end()) {
        return false;
    }
    return domainIterator->second.find(cbid) != domainIterator->second.end();
}

std::vector<aclptiCallbackDomain> Registry::SupportedDomains() const
{
    std::vector<aclptiCallbackDomain> domains;
    domains.reserve(callbacks_.size());
    for (const auto& entry : callbacks_) {
        domains.push_back(entry.first);
    }
    return domains;
}

} // namespace npu_compute::aclpti::callback
