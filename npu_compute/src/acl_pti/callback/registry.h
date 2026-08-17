/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_ACLPTI_CALLBACK_REGISTRY_H_
#define NPU_COMPUTE_ACLPTI_CALLBACK_REGISTRY_H_

#include "aclpti/aclpti_callback.h"

#include <initializer_list>
#include <map>
#include <unordered_set>
#include <vector>

namespace npu_compute::aclpti::callback {

class Registry {
public:
    bool RegisterDomain(aclptiCallbackDomain domain, std::initializer_list<aclptiCallbackId> callbackIds);
    bool IsSupported(aclptiCallbackDomain domain, aclptiCallbackId cbid) const;
    std::vector<aclptiCallbackDomain> SupportedDomains() const;

private:
    std::map<aclptiCallbackDomain, std::unordered_set<aclptiCallbackId>> callbacks_;
};

} // namespace npu_compute::aclpti::callback

#endif // NPU_COMPUTE_ACLPTI_CALLBACK_REGISTRY_H_
