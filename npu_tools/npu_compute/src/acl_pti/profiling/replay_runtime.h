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
 * @file replay_runtime.h
 * @brief Owns replay memory and range-profiling state used by runtime API replacements.
 */
#ifndef NPU_COMPUTE_ACLPTI_PROFILING_REPLAY_RUNTIME_H_
#define NPU_COMPUTE_ACLPTI_PROFILING_REPLAY_RUNTIME_H_

#include "range_profiler.h"
#include "replay_memory.h"

#include <atomic>

namespace npu_compute::aclpti::profiling {

class ReplayRuntime {
public:
    /// Initializes the range profiler once while preserving its failure result.
    aclptiResult Initialize();

    /// Applies the requested profiling section configuration.
    aclptiResult SetSections(const aclptiRangeProfilerSetConfigParams* params);

    /// Allocates replay shadow memory for a successful device allocation.
    aclptiResult MirrorMalloc(void** devPtr, std::size_t size, aclrtMemMallocPolicy policy);

    /// Releases replay shadow memory associated with a device allocation.
    aclptiResult MirrorFree(void* devPtr);

    /// Mirrors a supported device memory copy for replay.
    aclptiResult MirrorMemcpy(
        void* destination, std::size_t destinationSize, const void* source, std::size_t count, aclrtMemcpyKind kind);

    /// Mirrors a device memset for replay.
    aclptiResult MirrorMemset(void* devPtr, std::size_t maxCount, std::int32_t value, std::size_t count);

    /// Restores replay memory and profiles each replay round for a kernel launch.
    aclptiResult ReplayKernel(const ReplayLaunchFunction& launchFunction, aclrtStream stream);

private:
    /// Reports whether new profiling work may be started.
    bool ProfilingAvailable() const;

    /// Stops profiling after a failed optional operation and preserves its root status.
    aclptiResult HandleProfilingResult(aclptiResult status);

    /// Stops profiling and shuts down its data module once.
    aclptiResult StopProfiling();

    bool initialized_ = false;
    std::atomic<bool> profilingAvailable_{true};
    ReplayMemory replayMemory_;
    RangeProfiler rangeProfiler_;
};

/// Returns the process-wide replay runtime.
ReplayRuntime& GetReplayRuntime();

} // namespace npu_compute::aclpti::profiling

#endif // NPU_COMPUTE_ACLPTI_PROFILING_REPLAY_RUNTIME_H_
