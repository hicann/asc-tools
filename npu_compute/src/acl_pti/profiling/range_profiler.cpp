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
constexpr uint32_t kPipeUtilization[] = {0, 1, 10, 36, 52, 53, 514, 515, 810, 1281, 1794, 1812, 1813};
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

aclptiResult RangeProfiler::Initialize()
{
    const aclptiResult initializeStatus = dataModule_.Initialize();
    npu_compute::detail::DebugLog(
        "aclpti", "RangeProfiler data module init result=%d", static_cast<std::int32_t>(initializeStatus));
    if (initializeStatus != ACLPTI_SUCCESS) {
        npu_compute::detail::DebugLog("aclpti", "RangeProfiler data module initialization failed");
        return initializeStatus;
    }
    MsprofRawDataCallback callback = dataModule_.GetRawDataCallback();
    if (callback == nullptr) {
        npu_compute::detail::DebugLog("aclpti", "RangeProfiler callback registration missing callback");
        Shutdown();
        return ACLPTI_ERROR_INITIALIZATION_FAILED;
    }
    const int callbackResult = acltoolUploaderInit(callback);
    npu_compute::detail::DebugLog("aclpti", "RangeProfiler callback registration result=%d", callbackResult);
    if (callbackResult == 0) {
        npu_compute::detail::DebugLog("aclpti", "RangeProfiler initialized");
        return ACLPTI_SUCCESS;
    }
    Shutdown();
    return ACLPTI_ERROR_INITIALIZATION_FAILED;
}

aclptiResult RangeProfiler::Shutdown()
{
    const aclptiResult status = dataModule_.ForceShutdown();
    npu_compute::detail::DebugLog(
        "aclpti", "RangeProfiler data module shutdown result=%d", static_cast<std::int32_t>(status));
    if (status != ACLPTI_SUCCESS) {
        npu_compute::detail::DebugLog("aclpti", "error operation=data_shutdown status=%d", static_cast<int>(status));
    }
    return status;
}

aclptiResult RangeProfiler::SetSections(const aclptiRangeProfilerSetConfigParams* params)
{
    if (params == nullptr || params->sections == nullptr || params->numSections == 0 ||
        params->numSections > ACLPTI_MAX_NUM_SECTIONS) {
        npu_compute::detail::DebugLog("aclpti", "section configuration rejected: invalid parameters");
        return ACLPTI_ERROR_INVALID_PARAMETER;
    }

    try {
        std::vector<uint32_t> events;
        std::unordered_set<uint32_t> seen;
        std::string sectionName;
        for (std::size_t index = 0; index < params->numSections; ++index) {
            const char* name = params->sections[index];
            if (name == nullptr) {
                npu_compute::detail::DebugLog(
                    "aclpti", "section configuration rejected: null section index=%zu", index);
                return ACLPTI_ERROR_INVALID_PARAMETER;
            }
            const std::size_t length = ::strnlen(name, ACLPTI_MAX_SECTION_NAME_LENGTH + 1);
            if (length == 0 || length > ACLPTI_MAX_SECTION_NAME_LENGTH) {
                npu_compute::detail::DebugLog(
                    "aclpti", "section configuration rejected: invalid section index=%zu", index);
                return ACLPTI_ERROR_INVALID_PARAMETER;
            }

            const SectionDefinition* section = FindSection(std::string_view(name, length));
            if (section == nullptr) {
                npu_compute::detail::DebugLog(
                    "aclpti", "section configuration rejected: unsupported section=%.*s", static_cast<int>(length),
                    name);
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
        npu_compute::detail::DebugLog("aclpti", "section configuration failed: out of memory");
        return ACLPTI_ERROR_OUT_OF_MEMORY;
    }
}

aclptiResult RangeProfiler::PrepareReplayEnvironment(
    const ReplayLaunchFunction& launchFunction, aclrtStream stream, std::int32_t* deviceId,
    aclrtSynchronizeStreamFunc* synchronizeFunction) const
{
    const aclError deviceResult = aclrtGetDevice(deviceId);
    npu_compute::detail::DebugLog("aclpti", "get current device result=%d device=%d", deviceResult, *deviceId);
    if (deviceResult != ACL_SUCCESS) {
        npu_compute::detail::DebugLog("aclpti", "error operation=replay_get_device status=%d", deviceResult);
        return ACLPTI_ERROR_PROFILING_FAILED;
    }

    *synchronizeFunction =
        reinterpret_cast<aclrtSynchronizeStreamFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtSynchronizeStream));
    if (!launchFunction || *synchronizeFunction == nullptr) {
        npu_compute::detail::DebugLog(
            "aclpti", "error operation=original_lookup status=%d launch_available=%d synchronize_available=%d",
            ACLPTI_ERROR_PROFILING_FAILED, static_cast<int>(static_cast<bool>(launchFunction)),
            static_cast<int>(*synchronizeFunction != nullptr));
        return ACLPTI_ERROR_PROFILING_FAILED;
    }
    npu_compute::detail::DebugLog("aclpti", "synchronize stream before replay");
    const int initialSyncResult = (*synchronizeFunction)(stream);
    npu_compute::detail::DebugLog("aclpti", "initial replay synchronization result=%d", initialSyncResult);
    if (initialSyncResult != ACL_SUCCESS) {
        npu_compute::detail::DebugLog("aclpti", "error operation=replay_initial_sync status=%d", initialSyncResult);
        return ACLPTI_ERROR_PROFILING_FAILED;
    }

    return ACLPTI_SUCCESS;
}

std::size_t RangeProfiler::ConfigureProfilingRound(
    std::size_t round, std::int32_t deviceId, MsprofConfigAttr* attr, MsprofConfig* config,
    data::ReplayPrepareInfo* prepareInfo) const
{
    const std::size_t firstEvent = round * COMPUTE_AICORE_METRICS_NUM;
    const std::size_t eventCount = std::min(
        pmuEvents_.size() - std::min(firstEvent, pmuEvents_.size()),
        static_cast<std::size_t>(COMPUTE_AICORE_METRICS_NUM));

    *attr = {};
    attr->id = PROF_CONFIG_ATTR_AICORE_METRICS;
    std::fill_n(attr->value.aicoreMetrics, COMPUTE_AICORE_METRICS_NUM, MSPROF_INVALID_AICORE_METRIC);
    *prepareInfo = {};
    prepareInfo->replayId = round;
    prepareInfo->sectionName = sectionName_;
    prepareInfo->pmuEventIds.fill(data::kInvalidPmuEvent);
    for (std::size_t index = 0; index < eventCount; ++index) {
        const uint32_t event = pmuEvents_[firstEvent + index];
        attr->value.aicoreMetrics[index] = event;
        prepareInfo->pmuEventIds[index] = event;
    }

    *config = {};
    config->profSwitch = kDefaultProfSwitch;
    config->devNums = 1;
    config->devIdList[0] = static_cast<std::uint32_t>(deviceId);
    config->configInfo.attrs = attr;
    config->configInfo.numAttrs = 1;

    npu_compute::detail::DebugLog(
        "aclpti", "prepare replay round=%zu section=%s pmuCount=%zu", round, prepareInfo->sectionName.c_str(),
        eventCount);
    for (std::size_t index = 0; index < eventCount; ++index) {
        npu_compute::detail::DebugLog(
            "aclpti", "replay pmu round=%zu slot=%zu value=%u", round, index, prepareInfo->pmuEventIds[index]);
    }
    return eventCount;
}

aclptiResult RangeProfiler::ResolveProfilingRoundStatus(
    std::size_t round, aclError launchStatus, aclError synchronizeStatus, std::int32_t stopStatus,
    const data::ReplayResult& replayResult, aclptiResult releaseStatus) const
{
    if (launchStatus != ACL_SUCCESS) {
        npu_compute::detail::DebugLog(
            "aclpti", "error operation=replay_launch status=%d replay=%llu round=%zu", launchStatus,
            static_cast<unsigned long long>(round), round);
        return ACLPTI_ERROR_RESULT_UNRELIABLE;
    }
    if (synchronizeStatus != ACL_SUCCESS) {
        npu_compute::detail::DebugLog(
            "aclpti", "error operation=replay_sync status=%d replay=%llu round=%zu", synchronizeStatus,
            static_cast<unsigned long long>(round), round);
        return ACLPTI_ERROR_RESULT_UNRELIABLE;
    }
    if (stopStatus != 0) {
        npu_compute::detail::DebugLog(
            "aclpti", "error operation=prof_stop status=%d replay=%llu round=%zu", stopStatus,
            static_cast<unsigned long long>(round), round);
        return ACLPTI_ERROR_PROFILING_FAILED;
    }
    if (replayResult.status != ACLPTI_SUCCESS) {
        npu_compute::detail::DebugLog(
            "aclpti", "error operation=replay_record status=%d replay=%llu round=%zu",
            static_cast<int>(replayResult.status), static_cast<unsigned long long>(round), round);
        return ACLPTI_ERROR_PROFILING_FAILED;
    }
    if (releaseStatus != ACLPTI_SUCCESS) {
        npu_compute::detail::DebugLog(
            "aclpti", "error operation=replay_release status=%d replay=%llu round=%zu", static_cast<int>(releaseStatus),
            static_cast<unsigned long long>(round), round);
        return ACLPTI_ERROR_PROFILING_FAILED;
    }
    return ACLPTI_SUCCESS;
}

aclptiResult RangeProfiler::StartProfilingRound(
    std::size_t round, std::int32_t deviceId, MsprofConfigAttr* attr, MsprofConfig* config)
{
    data::ReplayPrepareInfo prepareInfo{};
    const std::size_t eventCount = ConfigureProfilingRound(round, deviceId, attr, config, &prepareInfo);

    const aclptiResult prepareStatus = dataModule_.PrepareReplay(prepareInfo);
    if (prepareStatus != ACLPTI_SUCCESS) {
        npu_compute::detail::DebugLog(
            "aclpti", "error operation=replay_prepare status=%d replay=%llu round=%zu", static_cast<int>(prepareStatus),
            static_cast<unsigned long long>(round), round);
        return ACLPTI_ERROR_RESULT_UNRELIABLE;
    }

    npu_compute::detail::DebugLog(
        "aclpti", "start profiling replay round=%zu device=%d pmuCount=%zu", round, deviceId, eventCount);
    const int startResult = MsprofStart(kMsprofCollectionType, config, sizeof(*config));
    if (startResult != 0) {
        dataModule_.RecordReplayStatus({round, ACLPTI_ERROR_INTERNAL});
        dataModule_.ReleaseReplay(round);
        npu_compute::detail::DebugLog(
            "aclpti", "error operation=prof_start status=%d replay=%llu round=%zu", startResult,
            static_cast<unsigned long long>(round), round);
        return ACLPTI_ERROR_RESULT_UNRELIABLE;
    }
    return ACLPTI_SUCCESS;
}

aclptiResult RangeProfiler::FinishProfilingRound(
    std::size_t round, const MsprofConfig& config, aclError launchStatus,
    aclrtSynchronizeStreamFunc synchronizeFunction, aclrtStream stream)
{
    npu_compute::detail::DebugLog("aclpti", "synchronize replay kernel round=%zu", round);
    const aclError syncResult = synchronizeFunction(stream);
    npu_compute::detail::DebugLog("aclpti", "synchronize replay kernel result round=%zu result=%d", round, syncResult);
    npu_compute::detail::DebugLog("aclpti", "stop profiling replay round=%zu", round);
    const int stopResult = MsprofStop(kMsprofCollectionType, &config, sizeof(config));
    npu_compute::detail::DebugLog("aclpti", "stop profiling replay result round=%zu result=%d", round, stopResult);

    const data::ReplayStopInfo stopInfo{round, stopResult == 0 ? ACLPTI_SUCCESS : ACLPTI_ERROR_INTERNAL};
    npu_compute::detail::DebugLog("aclpti", "record replay status round=%zu", round);
    const data::ReplayResult replayResult = dataModule_.RecordReplayStatus(stopInfo);
    npu_compute::detail::DebugLog(
        "aclpti", "record replay status result round=%zu result=%d copiedRecords=%llu copiedBytes=%llu", round,
        static_cast<int>(replayResult.status),
        static_cast<unsigned long long>(replayResult.callbackStats.copiedRecordCount),
        static_cast<unsigned long long>(replayResult.callbackStats.copiedBytes));
    npu_compute::detail::DebugLog("aclpti", "release replay round=%zu", round);
    const aclptiResult releaseStatus = dataModule_.ReleaseReplay(round);
    npu_compute::detail::DebugLog(
        "aclpti", "release replay result round=%zu result=%d", round, static_cast<int>(releaseStatus));

    const aclptiResult status =
        ResolveProfilingRoundStatus(round, launchStatus, syncResult, stopResult, replayResult, releaseStatus);
    if (status == ACLPTI_SUCCESS) {
        npu_compute::detail::DebugLog("aclpti", "replay round=%zu complete", round);
    }
    return status;
}

aclptiResult RangeProfiler::ReplayKernel(
    const ReplayMemory& replayMemory, const ReplayLaunchFunction& launchFunction, aclrtStream stream)
{
    try {
        std::int32_t deviceId = -1;
        aclrtSynchronizeStreamFunc synchronizeFunction = nullptr;
        aclptiResult status = PrepareReplayEnvironment(launchFunction, stream, &deviceId, &synchronizeFunction);
        if (status != ACLPTI_SUCCESS) {
            return status;
        }

        const std::size_t roundCount =
            std::max<std::size_t>(1, (pmuEvents_.size() + COMPUTE_AICORE_METRICS_NUM - 1) / COMPUTE_AICORE_METRICS_NUM);
        npu_compute::detail::DebugLog("aclpti", "replay rounds=%zu events=%zu", roundCount, pmuEvents_.size());

        for (std::size_t round = 0; round < roundCount; ++round) {
            status = replayMemory.Restore();
            if (status != ACLPTI_SUCCESS) {
                npu_compute::detail::DebugLog(
                    "aclpti", "error operation=replay_restore_result status=%d round=%zu", static_cast<int>(status),
                    round);
                return status;
            }
            MsprofConfigAttr attr{};
            MsprofConfig config{};
            status = StartProfilingRound(round, deviceId, &attr, &config);
            if (status != ACLPTI_SUCCESS) {
                return status;
            }

            npu_compute::detail::DebugLog("aclpti", "launch replay kernel round=%zu", round);
            const aclError launchStatus = launchFunction();
            npu_compute::detail::DebugLog(
                "aclpti", "launch replay kernel result round=%zu result=%d", round, launchStatus);
            status = FinishProfilingRound(round, config, launchStatus, synchronizeFunction, stream);
            if (status != ACLPTI_SUCCESS) {
                return status;
            }
        }

        return ACLPTI_SUCCESS;
    } catch (const std::bad_alloc&) {
        npu_compute::detail::DebugLog(
            "aclpti", "error operation=replay_metadata_alloc status=%d", ACLPTI_ERROR_OUT_OF_MEMORY);
        return ACLPTI_ERROR_RESULT_UNRELIABLE;
    }
}

} // namespace npu_compute::aclpti::profiling
