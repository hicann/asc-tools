/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_ACLPTI_DATA_MODULE_H_
#define NPU_COMPUTE_ACLPTI_DATA_MODULE_H_

#include "aclpti/aclpti_data.h"
#include "npu_compute/common.h"
#include "profiling/prof_api.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace npu_compute::aclpti::data {

inline constexpr std::size_t kMaxPmuSlots = 10;
inline constexpr uint32_t kInvalidPmuEvent = 0xffffffffU;
using PmuSlots = std::array<uint32_t, kMaxPmuSlots>;

enum class ReplayKind { Pmu, Pipeline, PcSampling };

struct ReplayPrepareInfo {
    uint64_t replayId;
    PmuSlots pmuEventIds;
    ReplayKind kind = ReplayKind::Pmu;
};

struct ReplayStopInfo {
    uint64_t replayId;
    aclptiResult stopStatus;
};

struct CallbackStats {
    uint64_t copiedRecordCount;
    uint64_t copiedBytes;
    uint64_t receivedBytes;
    uint64_t offsetMismatchCount;
    uint64_t firstExpectedOffset;
    uint64_t firstActualOffset;
    uint32_t lastChunkCount;
};

struct ReplayResult {
    uint64_t replayId;
    aclptiResult status;
    CallbackStats callbackStats;
};

class ReplayControl {
public:
    virtual ~ReplayControl() = default;
    virtual aclptiResult Initialize() = 0;
    virtual MsprofRawDataCallback GetRawDataCallback() = 0;
    virtual aclptiResult PrepareReplay(const ReplayPrepareInfo& info) = 0;
    virtual ReplayResult RecordReplayStatus(const ReplayStopInfo& info) = 0;
    virtual aclptiResult ReleaseReplay(uint64_t replayId) = 0;
    virtual aclptiResult Shutdown() = 0;
};

class NPU_COMPUTE_LOCAL Module final : public ReplayControl {
public:
    Module();
    explicit Module(aclptiProfilingDataCallback callback);
    ~Module() override;

    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;

    aclptiResult Initialize() override;
    MsprofRawDataCallback GetRawDataCallback() override;
    aclptiResult PrepareReplay(const ReplayPrepareInfo& info) override;
    ReplayResult RecordReplayStatus(const ReplayStopInfo& info) override;
    aclptiResult ReleaseReplay(uint64_t replayId) override;
    aclptiResult Shutdown() override;

    /// Releases an active replay, drains queued data, and shuts down the module.
    aclptiResult ForceShutdown();

private:
    class NPU_COMPUTE_LOCAL Impl;
    std::unique_ptr<Impl> impl_;
};

aclptiResult RegisterProfilingDataCallback(aclptiProfilingDataCallback callback);
aclptiResult RegisterShutdownCallback(aclptiDataModuleShutdownCallback callback, void* userData);

} // namespace npu_compute::aclpti::data

#endif // NPU_COMPUTE_ACLPTI_DATA_MODULE_H_
