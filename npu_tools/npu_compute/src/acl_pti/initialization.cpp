/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "initialization.h"

#include "acl_pti/profiling/replay_runtime.h"
#include "acl_pti/replacement/runtime_api_replacements.h"
#include "common/debug_log.h"
#include "injection/injection_hook.h"

#include <mutex>

namespace npu_compute::aclpti::initialization {

aclptiResult InitializeDependencies()
{
    static std::mutex initializationMutex;
    static bool initialized = false;

    std::lock_guard<std::mutex> lock(initializationMutex);
    if (initialized) {
        npu_compute::detail::DebugLog("aclpti", "dependencies already initialized");
        return ACLPTI_SUCCESS;
    }

    npu_compute::detail::DebugLog("aclpti", "initialize dependencies");
    const int hookResult = acltoolHookInit();
    npu_compute::detail::DebugLog("aclpti", "hook install result=%d", hookResult);
    if (hookResult != 0) {
        return ACLPTI_ERROR_INITIALIZATION_FAILED;
    }

    const aclptiResult replayResult = profiling::GetReplayRuntime().Initialize();
    if (replayResult != ACLPTI_SUCCESS) {
        npu_compute::detail::DebugLog("aclpti", "replay runtime initialization failed");
        return replayResult;
    }
    if (!replacement::RegisterRuntimeApiReplacements()) {
        npu_compute::detail::DebugLog("aclpti", "runtime replacement registration failed");
        return ACLPTI_ERROR_INITIALIZATION_FAILED;
    }
    initialized = true;
    npu_compute::detail::DebugLog("aclpti", "dependencies initialized");
    return ACLPTI_SUCCESS;
}

} // namespace npu_compute::aclpti::initialization
