/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
/**
 * @file range_profiler.h
 * @brief Configures PMU sections and coordinates profiled kernel replay rounds.
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

struct MsprofConfig;
struct MsprofConfigAttr;

namespace npu_compute::aclpti::profiling {

using ReplayLaunchFunction = std::function<aclError()>;

class RangeProfiler {
public:
    /// Initializes raw profiling data collection and uploader callbacks.
    aclptiResult Initialize();

    /// Resolves requested profiling sections into the PMU events to collect.
    aclptiResult SetSections(const aclptiRangeProfilerSetConfigParams* params);

    /// Replays a kernel and distinguishes profiling-only failures from unreliable device results.
    aclptiResult ReplayKernel(
        const ReplayMemory& replayMemory, const ReplayLaunchFunction& launchFunction, aclrtStream stream);

    /// Drains raw profiling data and stops the data module.
    aclptiResult Shutdown();

private:
    /// Resolves replay dependencies and synchronizes the stream before profiling starts.
    aclptiResult PrepareReplayEnvironment(
        const ReplayLaunchFunction& launchFunction, aclrtStream stream, std::int32_t* deviceId,
        aclrtSynchronizeStreamFunc* synchronizeFunction) const;

    /// Builds the PMU, Msprof, and data-module configuration and returns the valid PMU event count.
    std::size_t ConfigureProfilingRound(
        std::size_t round, std::int32_t deviceId, MsprofConfigAttr* attr, MsprofConfig* config,
        data::ReplayPrepareInfo* prepareInfo) const;

    /// Prepares raw-data collection and starts Msprof for one profiling round.
    aclptiResult StartProfilingRound(
        std::size_t round, std::int32_t deviceId, MsprofConfigAttr* attr, MsprofConfig* config);

    /// Synchronizes the replayed kernel, stops Msprof, records its data, and releases the round.
    aclptiResult FinishProfilingRound(
        std::size_t round, const MsprofConfig& config, aclError launchStatus,
        aclrtSynchronizeStreamFunc synchronizeFunction, aclrtStream stream);

    /// Maps launch, synchronization, collection, recording, and release results to an ACL PTI status.
    aclptiResult ResolveProfilingRoundStatus(
        std::size_t round, aclError launchStatus, aclError synchronizeStatus, std::int32_t stopStatus,
        const data::ReplayResult& replayResult, aclptiResult releaseStatus) const;

    std::vector<uint32_t> pmuEvents_;
    std::string sectionName_ = "default";
    data::Module dataModule_;
};

} // namespace npu_compute::aclpti::profiling

#endif // NPU_COMPUTE_ACLPTI_PROFILING_RANGE_PROFILER_H_
