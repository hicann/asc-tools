/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_ACLPTI_PROFILING_RANGE_PROFILER_H_
#define NPU_COMPUTE_ACLPTI_PROFILING_RANGE_PROFILER_H_

#include "aclpti/aclpti_range_profiler.h"
#include "acl_pti/data/module.h"
#include "npu_compute/injection_hook.h"
#include "replay_memory.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace npu_compute::aclpti::profiling {

using ReplayLaunchFunction = std::function<aclError()>;

class RangeProfiler {
public:
    bool Initialize();
    aclptiResult SetSections(const aclptiRangeProfilerSetConfigParams* params);
    int ReplayKernel(
        const ReplayMemory& replayMemory, aclrtMemcpyFunc memcpyFunction, const ReplayLaunchFunction& launchFunction,
        aclrtSynchronizeStreamFunc synchronizeFunction, aclrtStream stream, std::int32_t deviceId);

private:
    std::vector<std::uint32_t> pmuEvents_;
    std::string sectionName_ = "default";
    data::Module dataModule_;
};

} // namespace npu_compute::aclpti::profiling

#endif // NPU_COMPUTE_ACLPTI_PROFILING_RANGE_PROFILER_H_
