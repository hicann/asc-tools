/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "range_profiler.h"

#include "common/debug_log.h"
#include "profiling/prof_api.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace npu_compute::aclpti::profiling {
namespace {

struct SectionDefinition {
    std::string_view name;
    const uint32_t* events;
    std::size_t eventCount;
};

constexpr uint32_t kArithmeticUtilization[] = {768, 789, 790, 808, 809, 810, 1281, 1282, 1283, 1284};
constexpr uint32_t kPipeUtilization[] = {0, 1, 10, 36, 52, 53, 514, 515, 769, 810, 1281, 1794, 1812, 1813};
constexpr uint32_t kResourceConflictRatio[] = {11, 12, 13, 14, 15, 1344, 1366, 1376, 1377, 1378, 1379};
constexpr uint32_t kMemory[] = {512,  513,  514,  515,  516,  518,  1058, 1059, 1391, 1395, 1396, 1397,
                                1398, 1400, 1404, 1407, 1408, 1792, 1794, 1799, 1801, 1804, 1806, 1815};
constexpr uint32_t kMemoryL0[] = {772, 774, 776, 778, 1795, 1797};
constexpr uint32_t kMemoryUb[] = {1058, 1059, 1393, 1394, 1397, 1398, 1407, 1408};
constexpr uint32_t kL2Cache[] = {1060, 1061, 1062, 1063, 1064, 1065, 1066, 1067, 1068, 1069, 1070, 1071};

constexpr uint32_t kMsprofCollectionType = 8;
constexpr uint64_t kDefaultProfSwitch = PROF_AICORE_METRICS_MASK | PROF_TASK_TIME_MASK;

constexpr SectionDefinition kSectionCatalog[] = {
    {"ArithmeticUtilization", kArithmeticUtilization, std::size(kArithmeticUtilization)},
    {"PipeUtilization", kPipeUtilization, std::size(kPipeUtilization)},
    {"ResourceConflictRatio", kResourceConflictRatio, std::size(kResourceConflictRatio)},
    {"Memory", kMemory, std::size(kMemory)},
    {"MemoryL0", kMemoryL0, std::size(kMemoryL0)},
    {"MemoryUB", kMemoryUb, std::size(kMemoryUb)},
    {"L2Cache", kL2Cache, std::size(kL2Cache)},
};

const SectionDefinition* FindSection(std::string_view name)
{
    for (const auto& section : kSectionCatalog) {
        if (section.name == name) {
            return &section;
        }
    }
    return nullptr;
}

} // namespace

bool RangeProfiler::Initialize()
{
    const aclptiResult initializeStatus = dataModule_.Initialize();
    npu_compute::detail::DebugLog(
        "aclpti", "RangeProfiler data module init result=%d", static_cast<std::int32_t>(initializeStatus));
    if (initializeStatus != ACLPTI_SUCCESS) {
        return false;
    }
    MsprofRawDataCallback callback = dataModule_.GetRawDataCallback();
    if (callback == nullptr) {
        npu_compute::detail::DebugLog("aclpti", "RangeProfiler callback registration missing callback");
        return false;
    }
    const int callbackResult = acltoolUploaderInit(callback);
    npu_compute::detail::DebugLog("aclpti", "RangeProfiler callback registration result=%d", callbackResult);
    return callbackResult == 0;
}

aclptiResult RangeProfiler::SetSections(const aclptiRangeProfilerSetConfigParams* params)
{
    if (params == nullptr || params->sections == nullptr || params->numSections == 0 ||
        params->numSections > ACLPTI_MAX_NUM_SECTIONS) {
        return ACLPTI_ERROR_INVALID_PARAMETER;
    }

    try {
        std::vector<uint32_t> events;
        std::unordered_set<uint32_t> seen;
        std::string sectionName;
        for (std::size_t index = 0; index < params->numSections; ++index) {
            const char* name = params->sections[index];
            if (name == nullptr) {
                return ACLPTI_ERROR_INVALID_PARAMETER;
            }
            const std::size_t length = ::strnlen(name, ACLPTI_MAX_SECTION_NAME_LENGTH + 1);
            if (length == 0 || length > ACLPTI_MAX_SECTION_NAME_LENGTH) {
                return ACLPTI_ERROR_INVALID_PARAMETER;
            }

            const SectionDefinition* section = FindSection(std::string_view(name, length));
            if (section == nullptr) {
                return ACLPTI_ERROR_NOT_SUPPORTED;
            }
            if (sectionName.empty()) {
                sectionName.assign(name, length);
            }
            npu_compute::detail::DebugLog("aclpti", "selected section name=%.*s", static_cast<int>(length), name);
            for (std::size_t eventIndex = 0; eventIndex < section->eventCount; ++eventIndex) {
                const uint32_t event = section->events[eventIndex];
                if (seen.insert(event).second) {
                    events.push_back(event);
                }
            }
        }
        pmuEvents_ = std::move(events);
        sectionName_ = std::move(sectionName);
        npu_compute::detail::DebugLog(
            "aclpti", "requested sections=%zu events=%zu", params->numSections, pmuEvents_.size());
        return ACLPTI_SUCCESS;
    } catch (const std::bad_alloc&) {
        return ACLPTI_ERROR_OUT_OF_MEMORY;
    }
}

int RangeProfiler::ReplayKernel(
    const ReplayMemory& replayMemory, aclrtMemcpyFunc memcpyFunction, const ReplayLaunchFunction& launchFunction,
    aclrtSynchronizeStreamFunc synchronizeFunction, aclrtStream stream, std::int32_t deviceId)
{
    if (deviceId < 0) {
        npu_compute::detail::DebugLog("aclpti", "replay rejected because no active device is set");
        return ACLPTI_ERROR_INVALID_STATE;
    }

    const auto finishReplay = [this](int replayStatus) {
        const aclptiResult shutdownStatus = dataModule_.Shutdown();
        npu_compute::detail::DebugLog(
            "aclpti", "RangeProfiler data module shutdown result=%d", static_cast<std::int32_t>(shutdownStatus));
        return replayStatus != 0 ? replayStatus : static_cast<std::int32_t>(shutdownStatus);
    };

    if (memcpyFunction == nullptr || !launchFunction || synchronizeFunction == nullptr) {
        return finishReplay(-1);
    }
    const std::size_t roundCount =
        std::max<std::size_t>(1, (pmuEvents_.size() + COMPUTE_AICORE_METRICS_NUM - 1) / COMPUTE_AICORE_METRICS_NUM);
    npu_compute::detail::DebugLog("aclpti", "replay rounds=%zu events=%zu", roundCount, pmuEvents_.size());

    for (std::size_t round = 0; round < roundCount; ++round) {
        const int restoreResult = replayMemory.Restore(memcpyFunction);
        if (restoreResult != 0) {
            return finishReplay(restoreResult);
        }

        const std::size_t firstEvent = round * COMPUTE_AICORE_METRICS_NUM;
        const std::size_t eventCount = std::min(
            pmuEvents_.size() - std::min(firstEvent, pmuEvents_.size()),
            static_cast<std::size_t>(COMPUTE_AICORE_METRICS_NUM));
        const uint64_t replayId = round;

        MsprofConfigAttr attr{};
        attr.id = PROF_CONFIG_ATTR_AICORE_METRICS;
        std::fill_n(attr.value.aicoreMetrics, COMPUTE_AICORE_METRICS_NUM, MSPROF_INVALID_AICORE_METRIC);
        data::ReplayPrepareInfo prepareInfo{};
        prepareInfo.replayId = replayId;
        prepareInfo.sectionName = sectionName_;
        prepareInfo.pmuEventIds.fill(data::kInvalidPmuEvent);
        for (std::size_t index = 0; index < eventCount; ++index) {
            const uint32_t event = pmuEvents_[firstEvent + index];
            attr.value.aicoreMetrics[index] = event;
            prepareInfo.pmuEventIds[index] = event;
        }
        npu_compute::detail::DebugLog(
            "aclpti", "prepare replay round=%zu section=%s pmuCount=%zu", round, prepareInfo.sectionName.c_str(),
            eventCount);
        for (std::size_t index = 0; index < eventCount; ++index) {
            npu_compute::detail::DebugLog(
                "aclpti", "replay pmu round=%zu slot=%zu value=%u", round, index, prepareInfo.pmuEventIds[index]);
        }

        const aclptiResult prepareStatus = dataModule_.PrepareReplay(prepareInfo);
        if (prepareStatus != ACLPTI_SUCCESS) {
            return finishReplay(static_cast<std::int32_t>(prepareStatus));
        }

        MsprofConfig config{};
        config.profSwitch = kDefaultProfSwitch;
        config.devNums = 1;
        config.devIdList[0] = static_cast<uint32_t>(deviceId);
        config.configInfo.attrs = &attr;
        config.configInfo.numAttrs = 1;

        npu_compute::detail::DebugLog("aclpti", "start profiling replay round=%zu", round);
        const int startResult = MsprofStart(kMsprofCollectionType, &config, sizeof(config));
        if (startResult != 0) {
            dataModule_.RecordReplayStatus({replayId, ACLPTI_ERROR_INTERNAL});
            dataModule_.ReleaseReplay(replayId);
            return finishReplay(startResult);
        }
        npu_compute::detail::DebugLog("aclpti", "launch replay kernel round=%zu", round);
        const int launchResult = launchFunction();
        npu_compute::detail::DebugLog("aclpti", "synchronize replay kernel round=%zu", round);
        const int syncResult = synchronizeFunction(stream);
        npu_compute::detail::DebugLog("aclpti", "stop profiling replay round=%zu", round);
        const int stopResult = MsprofStop(kMsprofCollectionType, &config, sizeof(config));

        const data::ReplayStopInfo stopInfo{replayId, stopResult == 0 ? ACLPTI_SUCCESS : ACLPTI_ERROR_INTERNAL};
        npu_compute::detail::DebugLog("aclpti", "record replay status round=%zu", round);
        const data::ReplayResult replayResult = dataModule_.RecordReplayStatus(stopInfo);
        npu_compute::detail::DebugLog("aclpti", "release replay round=%zu", round);
        const aclptiResult releaseStatus = dataModule_.ReleaseReplay(replayId);

        if (launchResult != 0) {
            return finishReplay(launchResult);
        }
        if (syncResult != 0) {
            return finishReplay(syncResult);
        }
        if (stopResult != 0) {
            return finishReplay(stopResult);
        }
        if (replayResult.status != ACLPTI_SUCCESS) {
            return finishReplay(static_cast<std::int32_t>(replayResult.status));
        }
        if (releaseStatus != ACLPTI_SUCCESS) {
            return finishReplay(static_cast<std::int32_t>(releaseStatus));
        }
        npu_compute::detail::DebugLog("aclpti", "replay round=%zu complete", round);
    }

    return finishReplay(0);
}

} // namespace npu_compute::aclpti::profiling
