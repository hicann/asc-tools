/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "replay_runtime.h"

#include "common/debug_log.h"

#include <new>

namespace npu_compute::aclpti::profiling {

aclptiResult ReplayRuntime::Initialize()
{
    if (initialized_) {
        npu_compute::detail::DebugLog("aclpti", "replay runtime already initialized");
        return ACLPTI_SUCCESS;
    }
    const aclptiResult result = rangeProfiler_.Initialize();
    if (result != ACLPTI_SUCCESS) {
        npu_compute::detail::DebugLog("aclpti", "replay runtime initialization failed");
        return result;
    }
    initialized_ = true;
    npu_compute::detail::DebugLog("aclpti", "replay runtime initialized");
    return ACLPTI_SUCCESS;
}

aclptiResult ReplayRuntime::SetConfig(const aclptiRangeProfilerSetConfigParams* params)
{
    return rangeProfiler_.SetConfig(params);
}

aclptiResult ReplayRuntime::MirrorMalloc(void** devPtr, std::size_t size, aclrtMemMallocPolicy policy)
{
    if (!ProfilingAvailable()) {
        return ACLPTI_ERROR_PROFILING_FAILED;
    }
    return HandleProfilingResult(replayMemory_.MirrorMalloc(devPtr, size, policy));
}

aclptiResult ReplayRuntime::MirrorFree(void* devPtr) { return HandleProfilingResult(replayMemory_.MirrorFree(devPtr)); }

aclptiResult ReplayRuntime::MirrorMemcpy(
    void* destination, std::size_t destinationSize, const void* source, std::size_t count, aclrtMemcpyKind kind)
{
    if (count == 0 || (kind != ACL_MEMCPY_DEVICE_TO_DEVICE && kind != ACL_MEMCPY_HOST_TO_DEVICE)) {
        return ACLPTI_SUCCESS;
    }
    if (!ProfilingAvailable()) {
        return ACLPTI_ERROR_PROFILING_FAILED;
    }
    return HandleProfilingResult(replayMemory_.MirrorMemcpy(destination, destinationSize, source, count, kind));
}

aclptiResult ReplayRuntime::MirrorMemset(void* devPtr, std::size_t maxCount, std::int32_t value, std::size_t count)
{
    if (count == 0) {
        return ACLPTI_SUCCESS;
    }
    if (!ProfilingAvailable()) {
        return ACLPTI_ERROR_PROFILING_FAILED;
    }
    return HandleProfilingResult(replayMemory_.MirrorMemset(devPtr, maxCount, value, count));
}

aclptiResult ReplayRuntime::ReplayKernel(const ReplayLaunchFunction& launchFunction, aclrtStream stream)
{
    if (!ProfilingAvailable()) {
        return ACLPTI_ERROR_PROFILING_FAILED;
    }
    const aclptiResult status = rangeProfiler_.ReplayKernel(replayMemory_, launchFunction, stream);
    const aclptiResult shutdownStatus = StopProfiling();
    if (status == ACLPTI_SUCCESS) {
        return shutdownStatus;
    }
    return status;
}

bool ReplayRuntime::ProfilingAvailable() const { return profilingAvailable_.load(std::memory_order_acquire); }

aclptiResult ReplayRuntime::HandleProfilingResult(aclptiResult status)
{
    if (status != ACLPTI_SUCCESS) {
        (void)StopProfiling();
    }
    return status;
}

aclptiResult ReplayRuntime::StopProfiling()
{
    bool expected = true;
    if (!profilingAvailable_.compare_exchange_strong(
            expected, false, std::memory_order_acq_rel, std::memory_order_acquire)) {
        return ACLPTI_SUCCESS;
    }
    try {
        return rangeProfiler_.Shutdown();
    } catch (const std::bad_alloc&) {
        npu_compute::detail::DebugLog(
            "aclpti", "error operation=profiling_shutdown status=%d", ACLPTI_ERROR_OUT_OF_MEMORY);
        return ACLPTI_ERROR_OUT_OF_MEMORY;
    }
}

ReplayRuntime& GetReplayRuntime()
{
    static ReplayRuntime runtime;
    return runtime;
}

} // namespace npu_compute::aclpti::profiling
