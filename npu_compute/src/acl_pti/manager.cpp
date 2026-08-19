/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "manager.h"

#include "acl_pti/callback/dispatcher.h"
#include "acl_pti/replacement/runtime_api_replacements.h"
#include "common/debug_log.h"
#include "npu_compute/injection_hook.h"

namespace npu_compute::aclpti {

aclptiResult Manager::Initialize()
{
    if (initialized_) {
        npu_compute::detail::DebugLog("aclpti", "dependencies already initialized");
        return ACLPTI_SUCCESS;
    }

    npu_compute::detail::DebugLog("aclpti", "initialize dependencies");
    const int hookResult = acltoolHookInit();
    npu_compute::detail::DebugLog("aclpti", "hook install result=%d", hookResult);
    if (hookResult != 0) {
        return ACLPTI_ERROR_INITIALIZATION_FAILED;
    }

    const aclptiResult callbackStatus = EnsureCallbackDomainsRegistered();
    if (callbackStatus != ACLPTI_SUCCESS) {
        return callbackStatus;
    }
    auto& replacements = replacement::GetRuntimeApiReplacements();
    if (!replacements.Initialize(replayMemory_, rangeProfiler_)) {
        return ACLPTI_ERROR_INITIALIZATION_FAILED;
    }
    initialized_ = true;
    return ACLPTI_SUCCESS;
}

aclptiResult Manager::EnsureCallbackDomainsRegistered()
{
    if (callbacksRegistered_) {
        return ACLPTI_SUCCESS;
    }
    if (!replacement::GetRuntimeApiReplacements().RegisterCallbacks(callback::GetDispatcher())) {
        return ACLPTI_ERROR_INITIALIZATION_FAILED;
    }
    callbacksRegistered_ = true;
    return ACLPTI_SUCCESS;
}

aclptiResult Manager::SetSections(const aclptiRangeProfilerSetConfigParams* params)
{
    return rangeProfiler_.SetSections(params);
}

Manager& GetManager()
{
    static Manager manager;
    return manager;
}

} // namespace npu_compute::aclpti
