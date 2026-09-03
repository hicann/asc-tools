/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "module.h"

#include "bounded_queue.h"
#include "common/debug_log.h"
#include "raw_data_decoder.h"

#include <algorithm>
#include <atomic>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <condition_variable>
#include <map>
#include <limits>
#include <mutex>
#include <new>
#include <optional>
#include <stdexcept>
#include <thread>
#include <unordered_set>
#include <utility>
#include <variant>

namespace npu_compute::aclpti::data {
using detail::BoundedQueue;
using detail::DecodedRecord;
using detail::DecodeRawRecord;
using detail::PmuRecord128;
using detail::TaskLog32;

namespace {

constexpr std::size_t kQueueCapacity = 4096;
constexpr std::size_t kPmuRecordSize = 128;
constexpr std::size_t kLogRecordSize = 32;
constexpr int kLogDataType = 7;

std::mutex gCallbackMutex;
aclptiProfilingDataCallback gCallback;
std::mutex gShutdownCallbackMutex;
aclptiDataModuleShutdownCallback gShutdownCallback = nullptr;
void* gShutdownCallbackUserData = nullptr;

enum class ModuleState { Created, Running, Stopping, Stopped };
enum class ReplayState { Accepting, Closed, Released };

struct ReplaySession {
    explicit ReplaySession(ReplayPrepareInfo replayInfo) : info(std::move(replayInfo)) {}

    ReplayPrepareInfo info;
    std::mutex mutex;
    ReplayState state = ReplayState::Accepting;
    CallbackStats stats{0, 0, 0, 0, 0, 0, 0};
    uint64_t nextRecordIndex = 0;
    std::size_t nextOpaqueOffset = 0;
    bool opaqueLastSeen = false;
    uint64_t completedItemCount = 0;
    std::condition_variable completed;
    std::atomic<uint64_t> failedRecordCount{0};
    aclptiResult resultStatus = ACLPTI_SUCCESS;
};

struct RawRecord {
    std::shared_ptr<ReplaySession> session;
    std::array<std::byte, 128> bytes{};
    std::size_t size = 0;
    uint64_t recordIndex = 0;
};

struct ReplayEnd {
    std::shared_ptr<ReplaySession> session;
};

struct DecodedRecordItem {
    std::shared_ptr<ReplaySession> session;
    DecodedRecord record;
};

struct OpaqueChunkItem {
    std::shared_ptr<ReplaySession> session;
    RawDataType type;
    aclptiRawDataChunk chunk;
};

using RawItem = std::variant<RawRecord, OpaqueChunkItem, ReplayEnd>;
using DecodedItem = std::variant<DecodedRecordItem, OpaqueChunkItem, ReplayEnd>;

void MarkCompleted(const std::shared_ptr<ReplaySession>& session)
{
    std::lock_guard<std::mutex> lock(session->mutex);
    ++session->completedItemCount;
    session->completed.notify_all();
}

const char* ModuleStateName(ModuleState state)
{
    switch (state) {
        case ModuleState::Created:
            return "Created";
        case ModuleState::Running:
            return "Running";
        case ModuleState::Stopping:
            return "Stopping";
        case ModuleState::Stopped:
            return "Stopped";
    }
    return "unknown";
}

const char* ReplayStateName(ReplayState state)
{
    switch (state) {
        case ReplayState::Accepting:
            return "Accepting";
        case ReplayState::Closed:
            return "Closed";
        case ReplayState::Released:
            return "Released";
    }
    return "unknown";
}

const char* RawDataTypeName(RawDataType type)
{
    switch (type) {
        case API_DATA_TYPE:
            return "api";
        case EVENT_DATA_TYPE:
            return "event";
        case RUNTIMETRACK_DATA_TYPE:
            return "runtime-track";
        case PMU_DATA_TYPE:
            return "pmu";
        case DEFAULT_DATA_TYPE:
            return "default";
        case LOG_DATA_TYPE:
            return "log";
        case BIU_PERF_DATA_TYPE:
            return "biu-perf";
        case PC_SAMPLING_DATA_TYPE:
            return "pc-sampling";
    }
    return "unknown";
}

RawDataType PrimaryRawDataType(ReplayKind kind)
{
    switch (kind) {
        case ReplayKind::Pmu:
            return PMU_DATA_TYPE;
        case ReplayKind::Pipeline:
            return BIU_PERF_DATA_TYPE;
        case ReplayKind::PcSampling:
            return PC_SAMPLING_DATA_TYPE;
    }
    return DEFAULT_DATA_TYPE;
}

std::size_t CountConfiguredPmuEvents(const PmuSlots& events)
{
    std::size_t count = 0;
    for (const uint32_t event : events) {
        if (event == kInvalidPmuEvent) {
            break;
        }
        ++count;
    }
    return count;
}

void LogCallbackStats(const char* action, uint64_t replayId, const CallbackStats& stats, aclptiResult status)
{
    npu_compute::detail::DebugLog(
        "aclpti-data", "%s: replay=%llu status=%d copiedRecords=%llu copiedBytes=%llu receivedBytes=%llu",
        action == nullptr ? "callback stats" : action, static_cast<unsigned long long>(replayId),
        static_cast<int>(status), static_cast<unsigned long long>(stats.copiedRecordCount),
        static_cast<unsigned long long>(stats.copiedBytes), static_cast<unsigned long long>(stats.receivedBytes));
}

uint32_t LittleEndianWord(const char* data, std::size_t size, std::size_t offset)
{
    if (data == nullptr || offset + sizeof(uint32_t) > size) {
        return 0;
    }
    uint32_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value |= static_cast<uint32_t>(static_cast<unsigned char>(data[offset + index])) << (index * 8U);
    }
    return value;
}

void LogRawPayloadLine(uint64_t replayId, const MsprofRawData& rawData, std::size_t offset)
{
    constexpr std::size_t kBytesPerLine = 16;
    char hex[kBytesPerLine * 3 + 1]{};
    char ascii[kBytesPerLine + 1]{};
    std::size_t hexOffset = 0;
    for (std::size_t index = 0; index < kBytesPerLine; ++index) {
        const std::size_t byteOffset = offset + index;
        if (byteOffset < rawData.chunkSize) {
            const auto byte = static_cast<unsigned char>(rawData.chunk[byteOffset]);
            const int written = std::snprintf(hex + hexOffset, sizeof(hex) - hexOffset, "%02x ", byte);
            if (written > 0) {
                hexOffset += static_cast<std::size_t>(written);
            }
            ascii[index] = byte >= 0x20U && byte <= 0x7eU ? static_cast<char>(byte) : '.';
        } else {
            const int written = std::snprintf(hex + hexOffset, sizeof(hex) - hexOffset, "   ");
            if (written > 0) {
                hexOffset += static_cast<std::size_t>(written);
            }
            ascii[index] = ' ';
        }
    }
    npu_compute::detail::DebugLog(
        "aclpti-data", "[DEBUG-rawdata] payload replay=%llu offset=%zu absoluteOffset=%zu bytes=%s ascii=%s",
        static_cast<unsigned long long>(replayId), offset, rawData.offset + offset, hex, ascii);
}

void LogDecodedCandidate(
    uint64_t replayId, const ReplayPrepareInfo& info, const char* name, const char* data, std::size_t chunkSize,
    std::size_t offset, std::size_t size)
{
    const uint32_t word0 = LittleEndianWord(data, chunkSize, offset);
    const uint32_t word1 = LittleEndianWord(data, chunkSize, offset + sizeof(uint32_t));
    const uint32_t word2 = LittleEndianWord(data, chunkSize, offset + sizeof(uint32_t) * 2);
    const uint32_t word3 = LittleEndianWord(data, chunkSize, offset + sizeof(uint32_t) * 3);
    const uint32_t function = word0 & 0x3fU;
    const auto taskId = static_cast<uint16_t>(word1 >> 16U);
    const auto streamId = static_cast<uint16_t>(word1);
    const auto decoded =
        DecodeRawRecord(reinterpret_cast<const std::byte*>(data + offset), size, offset / size, info.pmuEventIds);
    npu_compute::detail::DebugLog(
        "aclpti-data",
        "[DEBUG-rawdata] candidate replay=%llu kind=%s offset=%zu size=%zu status=%d word0=0x%08x magic=0x%04x "
        "function=0x%02x word1=0x%08x task=%u stream=%u word2=0x%08x word3=0x%08x",
        static_cast<unsigned long long>(replayId), name == nullptr ? "unknown" : name, offset, size,
        static_cast<int>(decoded.Status()), word0, word0 >> 16U, function, word1, taskId, streamId, word2, word3);
    if (!decoded.Ok()) {
        return;
    }
    const auto& payload = decoded.Value().payload;
    if (const auto* log = std::get_if<TaskLog32>(&payload)) {
        npu_compute::detail::DebugLog(
            "aclpti-data",
            "[DEBUG-rawdata] candidate matched task-log replay=%llu offset=%zu func=0x%02x task=%u stream=%u "
            "counter=%llu block=%u subBlock=%u coreType=%d coreTypeId=%u",
            static_cast<unsigned long long>(replayId), offset, log->funcType, log->taskId, log->rtStreamId,
            static_cast<unsigned long long>(log->systemCounter), log->blockId, log->subBlockId,
            static_cast<int>(log->coreType), log->coreTypeId);
        return;
    }
    const auto& pmu = std::get<PmuRecord128>(payload);
    npu_compute::detail::DebugLog(
        "aclpti-data",
        "[DEBUG-rawdata] candidate matched pmu replay=%llu offset=%zu task=%u stream=%u totalCycles=%llu "
        "startCounter=%llu endCounter=%llu overflow=%d coreType=%d coreId=%u block=%u subBlock=%u values=%zu",
        static_cast<unsigned long long>(replayId), offset, pmu.taskId, pmu.rtStreamId,
        static_cast<unsigned long long>(pmu.totalCycles), static_cast<unsigned long long>(pmu.taskStartSystemCounter),
        static_cast<unsigned long long>(pmu.taskEndSystemCounter), pmu.overflow ? 1 : 0, static_cast<int>(pmu.coreType),
        pmu.coreId, pmu.blockId, pmu.subBlockId, pmu.pmuValues.size());
}

void LogRawDataDiagnostics(const ReplaySession& session, const MsprofRawData& rawData)
{
    if (!npu_compute::detail::DebugEnabled()) {
        return;
    }

    const uint64_t replayId = session.info.replayId;
    npu_compute::detail::DebugLog(
        "aclpti-data",
        "[DEBUG-rawdata] struct replay=%llu raw=%p isLastChunk=%d offset=%zu chunkModule=%d deviceId=%d type=%d(%s) "
        "chunkSize=%zu chunkCapacity=%zu chunk=%p",
        static_cast<unsigned long long>(replayId), static_cast<const void*>(&rawData), rawData.isLastChunk ? 1 : 0,
        rawData.offset, rawData.chunkModule, rawData.deviceId, static_cast<int>(rawData.type),
        RawDataTypeName(rawData.type), rawData.chunkSize, sizeof(rawData.chunk),
        static_cast<const void*>(rawData.chunk));
    for (std::size_t offset = 0; offset < rawData.chunkSize; offset += 16U) {
        LogRawPayloadLine(replayId, rawData, offset);
    }
    if (rawData.chunkSize % kLogRecordSize == 0) {
        for (std::size_t offset = 0; offset < rawData.chunkSize; offset += kLogRecordSize) {
            LogDecodedCandidate(
                replayId, session.info, "task-log-32", rawData.chunk, rawData.chunkSize, offset, kLogRecordSize);
        }
    } else {
        npu_compute::detail::DebugLog(
            "aclpti-data", "[DEBUG-rawdata] candidate skip replay=%llu kind=task-log-32 chunkSize=%zu",
            static_cast<unsigned long long>(replayId), rawData.chunkSize);
    }
    if (rawData.chunkSize % kPmuRecordSize == 0) {
        for (std::size_t offset = 0; offset < rawData.chunkSize; offset += kPmuRecordSize) {
            LogDecodedCandidate(
                replayId, session.info, "pmu-128", rawData.chunk, rawData.chunkSize, offset, kPmuRecordSize);
        }
    } else {
        npu_compute::detail::DebugLog(
            "aclpti-data", "[DEBUG-rawdata] candidate skip replay=%llu kind=pmu-128 chunkSize=%zu",
            static_cast<unsigned long long>(replayId), rawData.chunkSize);
    }
}

struct PmuValueAccumulator {
    long double sum = 0.0L;
    uint64_t count = 0;
};

struct CorePmuAccumulator {
    aclptiCoreType coreType = ACLPTI_CORE_TYPE_AIC;
    uint8_t coreId = 0;
    uint64_t sampleCount = 0;
    bool overflow = false;
    long double totalCyclesSum = 0.0L;
    std::map<uint32_t, PmuValueAccumulator> values;
    std::vector<aclptiPmuDataRow::SystemCounter> systemCounters;

    void Add(const PmuRecord128& record)
    {
        ++sampleCount;
        overflow = overflow || record.overflow;
        totalCyclesSum += static_cast<long double>(record.totalCycles);
        for (const auto& [eventId, value] : record.pmuValues) {
            auto& accumulator = values[eventId];
            accumulator.sum += static_cast<long double>(value);
            ++accumulator.count;
        }
        systemCounters.push_back(
            aclptiPmuDataRow::SystemCounter{record.taskStartSystemCounter, record.taskEndSystemCounter});
    }

    aclptiPmuDataRow::CoreData Snapshot() const
    {
        aclptiPmuDataRow::CoreData result{};
        result.coreType = coreType;
        result.coreId = coreId;
        result.sampleCount = sampleCount;
        result.totalCycles =
            sampleCount == 0 ? 0.0 : static_cast<double>(totalCyclesSum / static_cast<long double>(sampleCount));
        result.overflow = overflow;
        result.systemCounters = systemCounters;
        for (const auto& [eventId, accumulator] : values) {
            if (accumulator.count == 0) {
                continue;
            }
            result.values.emplace(
                eventId, static_cast<double>(accumulator.sum / static_cast<long double>(accumulator.count)));
            result.valueCounts.emplace(eventId, accumulator.count);
        }
        return result;
    }
};

struct PmuAccumulator {
    bool initialized = false;
    aclptiCoreType coreType = ACLPTI_CORE_TYPE_AIC;
    uint8_t coreId = 0;
    std::map<std::pair<aclptiCoreType, uint8_t>, uint64_t> coreInfoCounts;
    std::map<std::pair<aclptiCoreType, uint8_t>, CorePmuAccumulator> coreData;
    bool overflow = false;
    long double totalCyclesSum = 0.0L;
    uint64_t totalCyclesCount = 0;
    std::map<uint32_t, PmuValueAccumulator> values;
    std::vector<aclptiPmuDataRow::SystemCounter> systemCounters;

    void Add(const PmuRecord128& record)
    {
        if (!initialized) {
            initialized = true;
            coreType = record.coreType;
            coreId = record.coreId;
        }
        ++coreInfoCounts[{record.coreType, record.coreId}];
        auto& core = coreData[{record.coreType, record.coreId}];
        core.coreType = record.coreType;
        core.coreId = record.coreId;
        core.Add(record);
        overflow = overflow || record.overflow;
        totalCyclesSum += static_cast<long double>(record.totalCycles);
        ++totalCyclesCount;
        for (const auto& [eventId, value] : record.pmuValues) {
            auto& accumulator = values[eventId];
            accumulator.sum += static_cast<long double>(value);
            ++accumulator.count;
        }
        systemCounters.push_back(
            aclptiPmuDataRow::SystemCounter{record.taskStartSystemCounter, record.taskEndSystemCounter});
    }

    aclptiPmuDataRow Snapshot(aclptiBlockKey key) const
    {
        aclptiPmuDataRow row{};
        row.blockId = key.blockId;
        row.subBlockId = key.subBlockId;
        row.coreType = coreType;
        row.coreId = coreId;
        for (const auto& [core, count] : coreInfoCounts) {
            row.coreInfos.push_back(aclptiPmuDataRow::CoreInfo{core.first, core.second, count});
        }
        for (const auto& [core, accumulator] : coreData) {
            row.coreData.push_back(accumulator.Snapshot());
        }
        row.totalCycles = totalCyclesCount == 0 ?
                              0.0 :
                              static_cast<double>(totalCyclesSum / static_cast<long double>(totalCyclesCount));
        row.overflow = overflow;
        row.systemCounters = systemCounters;
        for (const auto& [eventId, accumulator] : values) {
            if (accumulator.count != 0) {
                row.values.emplace(
                    eventId, static_cast<double>(accumulator.sum / static_cast<long double>(accumulator.count)));
            }
        }
        return row;
    }
};

struct AggregateState {
    std::map<uint16_t, std::vector<aclptiTaskLogRow>> taskLogs;
    std::map<aclptiBlockKey, std::vector<aclptiTaskLogRow>> blockLogs;
    std::map<aclptiBlockKey, PmuAccumulator> pmuLogs;
    std::vector<aclptiRawDataChunk> pipelineData;
    std::vector<aclptiRawDataChunk> pcSamplingData;

    void Add(const DecodedRecordItem& item)
    {
        if (const auto* log = std::get_if<TaskLog32>(&item.record.payload)) {
            const aclptiTaskLogRow row{
                log->funcType, log->taskId,     log->rtStreamId, log->systemCounter,
                log->blockId,  log->subBlockId, log->coreType,   log->coreTypeId,
            };
            if (log->funcType == 0x00U || log->funcType == 0x01U) {
                taskLogs[log->taskId].push_back(row);
            } else {
                blockLogs[aclptiBlockKey{log->blockId, log->subBlockId}].push_back(row);
            }
            return;
        }

        const auto& record = std::get<PmuRecord128>(item.record.payload);
        pmuLogs[aclptiBlockKey{record.blockId, record.subBlockId, record.coreType, record.coreId}].Add(record);
    }

    void Add(OpaqueChunkItem item)
    {
#if defined(NPU_COMPUTE_ENABLE_TEST_CONTROLS)
        const char* forceAllocationFailure = std::getenv("NPU_COMPUTE_TEST_OPAQUE_AGGREGATE_OOM");
        if (forceAllocationFailure != nullptr && forceAllocationFailure[0] != '\0') {
            throw std::bad_alloc();
        }
#endif
        if (item.type == BIU_PERF_DATA_TYPE) {
            pipelineData.push_back(std::move(item.chunk));
        } else {
            pcSamplingData.push_back(std::move(item.chunk));
        }
    }

    aclptiProfilingDataResult::ErrorStats ErrorStats(const std::vector<std::shared_ptr<ReplaySession>>& sessions) const
    {
        aclptiProfilingDataResult::ErrorStats result;
        for (const auto& session : sessions) {
            std::lock_guard<std::mutex> lock(session->mutex);
            const uint64_t failedRecordCount = session->failedRecordCount.load();
            result.failedRecordCount += failedRecordCount;
            if (failedRecordCount != 0) {
                result.failedRecordCountByReplay[session->info.replayId] = failedRecordCount;
            }
        }
        return result;
    }

    aclptiResult ResultStatus(const std::vector<std::shared_ptr<ReplaySession>>& sessions) const
    {
        aclptiResult result = ACLPTI_SUCCESS;
        for (const auto& session : sessions) {
            std::lock_guard<std::mutex> lock(session->mutex);
            if (session->resultStatus == ACLPTI_ERROR_RESULT_UNRELIABLE) {
                return session->resultStatus;
            }
            if (result == ACLPTI_SUCCESS && session->resultStatus != ACLPTI_SUCCESS) {
                result = session->resultStatus;
            }
        }
        return result;
    }

    aclptiProfilingDataResult Snapshot(const std::vector<std::shared_ptr<ReplaySession>>& sessions) const
    {
        aclptiProfilingDataResult result;
        result.errorStats = ErrorStats(sessions);
        result.status = ResultStatus(sessions);
        for (const auto& [taskId, logs] : taskLogs) {
            result.taskLogs.emplace(taskId, logs);
        }
        for (const auto& [key, logs] : blockLogs) {
            result.blockLogs.emplace(key, logs);
        }
        for (const auto& [key, aggregate] : pmuLogs) {
            result.pmuLogs.emplace(key, aggregate.Snapshot(key));
        }
        result.pipelineData = pipelineData;
        result.pcSamplingData = pcSamplingData;
        if (result.status == ACLPTI_SUCCESS && result.errorStats.failedRecordCount != 0) {
            result.status = ACLPTI_ERROR_PROFILING_FAILED;
        }
        return result;
    }
};

bool IsValidReplayInfo(const ReplayPrepareInfo& info)
{
    switch (info.kind) {
        case ReplayKind::Pmu:
        case ReplayKind::Pipeline:
        case ReplayKind::PcSampling:
            break;
        default:
            return false;
    }

    bool unusedSlotSeen = false;
    for (const uint32_t eventId : info.pmuEventIds) {
        if (eventId == kInvalidPmuEvent) {
            unusedSlotSeen = true;
            continue;
        }
        if (unusedSlotSeen) {
            return false;
        }
    }
    return true;
}

bool HasPublishableResult(const aclptiProfilingDataResult& result, std::size_t sessionCount)
{
    return sessionCount != 0 || result.status != ACLPTI_SUCCESS || !result.taskLogs.empty() ||
           !result.blockLogs.empty() || !result.pmuLogs.empty() || !result.pipelineData.empty() ||
           !result.pcSamplingData.empty() || result.errorStats.failedRecordCount != 0;
}

} // namespace

class Module::Impl {
public:
    Impl() = default;

    explicit Impl(aclptiProfilingDataCallback callback) : callback_(std::move(callback))
    {
        if (!callback_) {
            throw std::invalid_argument("PTI data callback is required");
        }
    }

    ~Impl() { (void)ForceShutdown(); }

    aclptiResult Initialize()
    {
#if defined(NPU_COMPUTE_ENABLE_TEST_CONTROLS)
        const char* initializeFailure = std::getenv("NPU_COMPUTE_TEST_PTI_INITIALIZE_FAILURE");
        if (initializeFailure != nullptr && initializeFailure[0] != '\0') {
            npu_compute::detail::DebugLog("aclpti-data", "initialize forced to fail by test control");
            return ACLPTI_ERROR_INTERNAL;
        }
#endif
        std::lock_guard<std::mutex> routerLock(routerMutex_);
        std::lock_guard<std::mutex> lock(mutex_);
        npu_compute::detail::DebugLog(
            "aclpti-data", "initialize requested: state=%s router=%p active=%d callback=%d", ModuleStateName(state_),
            static_cast<void*>(router_), active_ ? 1 : 0, callback_ ? 1 : 0);
        if (state_ == ModuleState::Running) {
            npu_compute::detail::DebugLog("aclpti-data", "initialize skipped: already running");
            return ACLPTI_SUCCESS;
        }
        if (state_ != ModuleState::Created) {
            npu_compute::detail::DebugLog("aclpti-data", "initialize rejected: state=%s", ModuleStateName(state_));
            return ACLPTI_ERROR_INVALID_STATE;
        }
        if (router_ != nullptr) {
            npu_compute::detail::DebugLog(
                "aclpti-data", "initialize rejected: another router is active router=%p", static_cast<void*>(router_));
            return ACLPTI_ERROR_REPLAY_ACTIVE;
        }
        if (!callback_) {
            std::lock_guard<std::mutex> callbackLock(gCallbackMutex);
            callback_ = gCallback;
        }
        if (!callback_) {
            // ACLPTI can be used without an NPU Compute result consumer. Keep
            // collection and replay lifecycle functional while dropping the
            // final aggregate in that standalone mode.
            npu_compute::detail::DebugLog("aclpti-data", "initialize using default drop-result callback");
            callback_ = [](std::shared_ptr<const aclptiProfilingDataResult>) { return ACLPTI_SUCCESS; };
        }
        router_ = this;
        state_ = ModuleState::Running;
        try {
            assembler_ = std::thread(&AssembleThread, static_cast<void*>(this));
            decoder_ = std::thread(&DecodeThread, static_cast<void*>(this));
        } catch (...) {
            npu_compute::detail::DebugLog("aclpti-data", "initialize failed: worker thread creation failed");
            router_ = nullptr;
            state_ = ModuleState::Stopped;
            rawQueue_.Close();
            decodedQueue_.Close();
            if (decoder_.joinable()) {
                decoder_.join();
            }
            if (assembler_.joinable()) {
                assembler_.join();
            }
            return ACLPTI_ERROR_INTERNAL;
        }
        npu_compute::detail::DebugLog("aclpti-data", "initialize complete: decoder=1 assembler=1");
        return ACLPTI_SUCCESS;
    }

    MsprofRawDataCallback GetRawDataCallback()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        MsprofRawDataCallback callback = state_ == ModuleState::Running ? &RawDataThunk : nullptr;
        npu_compute::detail::DebugLog(
            "aclpti-data", "get raw data callback: state=%s callback=%p", ModuleStateName(state_),
            reinterpret_cast<void*>(callback));
        return callback;
    }

    aclptiResult PrepareReplay(const ReplayPrepareInfo& info)
    {
        if (!IsValidReplayInfo(info)) {
            npu_compute::detail::DebugLog(
                "aclpti-data", "prepare replay rejected: replay=%llu kind=%d events=%zu",
                static_cast<unsigned long long>(info.replayId), static_cast<int>(info.kind),
                CountConfiguredPmuEvents(info.pmuEventIds));
            return ACLPTI_ERROR_INVALID_PARAMETER;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        npu_compute::detail::DebugLog(
            "aclpti-data", "prepare replay requested: replay=%llu kind=%d events=%zu state=%s active=%d",
            static_cast<unsigned long long>(info.replayId), static_cast<int>(info.kind),
            CountConfiguredPmuEvents(info.pmuEventIds), ModuleStateName(state_), active_ ? 1 : 0);
        if (state_ != ModuleState::Running) {
            npu_compute::detail::DebugLog(
                "aclpti-data", "prepare replay rejected: replay=%llu state=%s",
                static_cast<unsigned long long>(info.replayId), ModuleStateName(state_));
            return ACLPTI_ERROR_NOT_INITIALIZED;
        }
        if (active_) {
            npu_compute::detail::DebugLog(
                "aclpti-data", "prepare replay rejected: replay=%llu activeReplay=%llu",
                static_cast<unsigned long long>(info.replayId),
                static_cast<unsigned long long>(active_->info.replayId));
            return ACLPTI_ERROR_REPLAY_ACTIVE;
        }
        try {
            if (!replayIds_.insert(info.replayId).second) {
                npu_compute::detail::DebugLog(
                    "aclpti-data", "prepare replay rejected: duplicate replay=%llu",
                    static_cast<unsigned long long>(info.replayId));
                return ACLPTI_ERROR_INVALID_PARAMETER;
            }
            auto session = std::make_shared<ReplaySession>(info);
            sessions_.push_back(session);
            active_ = std::move(session);
        } catch (const std::bad_alloc&) {
            replayIds_.erase(info.replayId);
            npu_compute::detail::DebugLog(
                "aclpti-data", "prepare replay failed: replay=%llu out of memory",
                static_cast<unsigned long long>(info.replayId));
            return ACLPTI_ERROR_OUT_OF_MEMORY;
        }
        npu_compute::detail::DebugLog(
            "aclpti-data", "prepare replay accepted: replay=%llu sessions=%zu",
            static_cast<unsigned long long>(info.replayId), sessions_.size());
        return ACLPTI_SUCCESS;
    }

    ReplayResult RecordReplayStatus(const ReplayStopInfo& info)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        npu_compute::detail::DebugLog(
            "aclpti-data", "record replay status requested: replay=%llu stopStatus=%d state=%s active=%d",
            static_cast<unsigned long long>(info.replayId), static_cast<int>(info.stopStatus), ModuleStateName(state_),
            active_ ? 1 : 0);
        ReplayResult result{info.replayId, ACLPTI_SUCCESS, {0, 0, 0, 0, 0, 0, 0}};
        if (state_ != ModuleState::Running) {
            result.status = ACLPTI_ERROR_NOT_INITIALIZED;
            npu_compute::detail::DebugLog(
                "aclpti-data", "record replay status rejected: replay=%llu state=%s",
                static_cast<unsigned long long>(info.replayId), ModuleStateName(state_));
            return result;
        }
        if (!active_) {
            result.status = ACLPTI_ERROR_NO_ACTIVE_REPLAY;
            npu_compute::detail::DebugLog(
                "aclpti-data", "record replay status rejected: replay=%llu no active replay",
                static_cast<unsigned long long>(info.replayId));
            return result;
        }
        if (active_->info.replayId != info.replayId) {
            result.status = ACLPTI_ERROR_REPLAY_NOT_FOUND;
            npu_compute::detail::DebugLog(
                "aclpti-data", "record replay status rejected: replay=%llu activeReplay=%llu",
                static_cast<unsigned long long>(info.replayId),
                static_cast<unsigned long long>(active_->info.replayId));
            return result;
        }

        std::unique_lock<std::mutex> sessionLock(active_->mutex);
        result.callbackStats = active_->stats;
        if (active_->state != ReplayState::Accepting) {
            result.status = ACLPTI_ERROR_INVALID_STATE;
            npu_compute::detail::DebugLog(
                "aclpti-data", "record replay status rejected: replay=%llu replayState=%s",
                static_cast<unsigned long long>(info.replayId), ReplayStateName(active_->state));
            return result;
        }
        active_->state = ReplayState::Closed;
        active_->completed.wait(
            sessionLock, [this] { return active_->completedItemCount == active_->stats.copiedRecordCount; });
        result.status = info.stopStatus == ACLPTI_SUCCESS && active_->failedRecordCount.load() != 0 ?
                            ACLPTI_ERROR_PROFILING_FAILED :
                            info.stopStatus;
        active_->resultStatus = result.status;
        LogCallbackStats("record replay status complete", info.replayId, result.callbackStats, result.status);
        npu_compute::detail::DebugLog(
            "aclpti-data", "record replay diagnostics: replay=%llu failedRecords=%llu",
            static_cast<unsigned long long>(info.replayId),
            static_cast<unsigned long long>(active_->failedRecordCount.load()));
        return result;
    }

    aclptiResult ReleaseReplay(uint64_t replayId)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        npu_compute::detail::DebugLog(
            "aclpti-data", "release replay requested: replay=%llu state=%s active=%d",
            static_cast<unsigned long long>(replayId), ModuleStateName(state_), active_ ? 1 : 0);
        if (state_ != ModuleState::Running) {
            npu_compute::detail::DebugLog(
                "aclpti-data", "release replay rejected: replay=%llu state=%s",
                static_cast<unsigned long long>(replayId), ModuleStateName(state_));
            return ACLPTI_ERROR_NOT_INITIALIZED;
        }
        if (!active_) {
            npu_compute::detail::DebugLog(
                "aclpti-data", "release replay rejected: replay=%llu no active replay",
                static_cast<unsigned long long>(replayId));
            return ACLPTI_ERROR_NO_ACTIVE_REPLAY;
        }
        if (active_->info.replayId != replayId) {
            npu_compute::detail::DebugLog(
                "aclpti-data", "release replay rejected: replay=%llu activeReplay=%llu",
                static_cast<unsigned long long>(replayId), static_cast<unsigned long long>(active_->info.replayId));
            return ACLPTI_ERROR_REPLAY_NOT_FOUND;
        }
        {
            std::lock_guard<std::mutex> sessionLock(active_->mutex);
            if (active_->state != ReplayState::Closed) {
                npu_compute::detail::DebugLog(
                    "aclpti-data", "release replay rejected: replay=%llu replayState=%s",
                    static_cast<unsigned long long>(replayId), ReplayStateName(active_->state));
                return ACLPTI_ERROR_INVALID_STATE;
            }
            bool pushed = false;
            try {
                pushed = rawQueue_.Push(ReplayEnd{active_});
            } catch (const std::bad_alloc&) {
                npu_compute::detail::DebugLog(
                    "aclpti-data", "release replay failed: replay=%llu out of memory",
                    static_cast<unsigned long long>(replayId));
                return ACLPTI_ERROR_OUT_OF_MEMORY;
            }
            if (!pushed) {
                npu_compute::detail::DebugLog(
                    "aclpti-data", "release replay rejected: replay=%llu raw queue is closed",
                    static_cast<unsigned long long>(replayId));
                return ACLPTI_ERROR_INVALID_STATE;
            }
            active_->state = ReplayState::Released;
        }
        active_.reset();
        npu_compute::detail::DebugLog(
            "aclpti-data", "release replay complete: replay=%llu", static_cast<unsigned long long>(replayId));
        return ACLPTI_SUCCESS;
    }

    aclptiResult Shutdown()
    {
        std::lock_guard<std::mutex> routerLock(routerMutex_);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            npu_compute::detail::DebugLog(
                "aclpti-data", "shutdown requested: state=%s active=%d sessions=%zu", ModuleStateName(state_),
                active_ ? 1 : 0, sessions_.size());
            if (state_ == ModuleState::Created || state_ == ModuleState::Stopped) {
                npu_compute::detail::DebugLog(
                    "aclpti-data", "shutdown skipped: state=%s status=%d", ModuleStateName(state_),
                    static_cast<int>(shutdownStatus_));
                return shutdownStatus_;
            }
            if (active_) {
                npu_compute::detail::DebugLog(
                    "aclpti-data", "shutdown rejected: active replay=%llu",
                    static_cast<unsigned long long>(active_->info.replayId));
                return ACLPTI_ERROR_REPLAY_ACTIVE;
            }
            state_ = ModuleState::Stopping;
        }
        if (router_ == this) {
            router_ = nullptr;
        }
        rawQueue_.Close();
        npu_compute::detail::DebugLog("aclpti-data", "shutdown: raw queue closed");
        if (decoder_.joinable()) {
            decoder_.join();
            npu_compute::detail::DebugLog("aclpti-data", "shutdown: decoder joined");
        }
        if (assembler_.joinable()) {
            assembler_.join();
            npu_compute::detail::DebugLog("aclpti-data", "shutdown: assembler joined");
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            state_ = ModuleState::Stopped;
        }
        const std::size_t sessionCount = sessions_.size();
        auto result = std::make_shared<aclptiProfilingDataResult>(aggregate_.Snapshot(sessions_));
        npu_compute::detail::DebugLog(
            "aclpti-data",
            "shutdown aggregate: status=%d sessions=%zu tasks=%zu blocks=%zu pmuBlocks=%zu failedRecords=%llu",
            static_cast<int>(result->status), sessionCount, result->taskLogs.size(), result->blockLogs.size(),
            result->pmuLogs.size(), static_cast<unsigned long long>(result->errorStats.failedRecordCount));
        aclptiResult callbackStatus = ACLPTI_SUCCESS;
        if (HasPublishableResult(*result, sessionCount)) {
            try {
                callbackStatus = callback_(std::move(result));
            } catch (...) {
                callbackStatus = ACLPTI_ERROR_CALLBACK;
            }
            npu_compute::detail::DebugLog(
                "aclpti-data", "shutdown result consumer callback status=%d", static_cast<int>(callbackStatus));
        } else {
            npu_compute::detail::DebugLog("aclpti-data", "shutdown result consumer callback skipped: empty aggregate");
        }
        aclptiDataModuleShutdownCallback shutdownCallback = nullptr;
        void* shutdownUserData = nullptr;
        {
            std::lock_guard<std::mutex> callbackLock(gShutdownCallbackMutex);
            shutdownCallback = gShutdownCallback;
            shutdownUserData = gShutdownCallbackUserData;
        }
        if (shutdownCallback == nullptr) {
            shutdownStatus_ = ACLPTI_SUCCESS;
            npu_compute::detail::DebugLog("aclpti-data", "shutdown complete: no external shutdown callback");
            return shutdownStatus_;
        }
        try {
            shutdownStatus_ = static_cast<aclptiResult>(shutdownCallback(shutdownUserData));
        } catch (...) {
            shutdownStatus_ = ACLPTI_ERROR_INTERNAL;
        }
        npu_compute::detail::DebugLog(
            "aclpti-data", "shutdown complete: external callback status=%d", static_cast<int>(shutdownStatus_));
        return shutdownStatus_;
    }

    aclptiResult ForceShutdown()
    {
        std::shared_ptr<ReplaySession> session;
        aclptiResult releaseStatus = ACLPTI_SUCCESS;
        {
            std::lock_guard<std::mutex> routerLock(routerMutex_);
            std::lock_guard<std::mutex> lock(mutex_);
            if (state_ == ModuleState::Running && active_) {
                session = active_;
                std::lock_guard<std::mutex> sessionLock(session->mutex);
                session->state = ReplayState::Released;
                active_.reset();
                npu_compute::detail::DebugLog(
                    "aclpti-data", "force shutdown releasing active replay=%llu",
                    static_cast<unsigned long long>(session->info.replayId));
            }
        }
        if (session) {
            try {
                if (!rawQueue_.Push(ReplayEnd{session})) {
                    releaseStatus = ACLPTI_ERROR_INVALID_STATE;
                }
            } catch (const std::bad_alloc&) {
                releaseStatus = ACLPTI_ERROR_OUT_OF_MEMORY;
            }
            if (releaseStatus != ACLPTI_SUCCESS) {
                session->failedRecordCount.fetch_add(1);
                npu_compute::detail::DebugLog(
                    "aclpti-data", "force shutdown replay end failed: replay=%llu status=%d",
                    static_cast<unsigned long long>(session->info.replayId), static_cast<int>(releaseStatus));
            }
        }
        const aclptiResult shutdownStatus = Shutdown();
        return releaseStatus == ACLPTI_SUCCESS ? shutdownStatus : releaseStatus;
    }

private:
    static void DecodeThread(void* context) { static_cast<Impl*>(context)->DecodeLoop(); }

    static void AssembleThread(void* context) { static_cast<Impl*>(context)->AssembleLoop(); }

    static std::int32_t RawDataThunk(MsprofRawData* rawData)
    {
        std::lock_guard<std::mutex> lock(routerMutex_);
        if (router_ == nullptr) {
            npu_compute::detail::DebugLog("aclpti-data", "raw callback rejected: router is null");
            return static_cast<std::int32_t>(ACLPTI_ERROR_NOT_INITIALIZED);
        }
        return router_->OnRawData(rawData);
    }

    std::int32_t OnRawData(MsprofRawData* rawData)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != ModuleState::Running || !active_) {
            npu_compute::detail::DebugLog(
                "aclpti-data", "raw callback rejected: state=%s active=%d raw=%p", ModuleStateName(state_),
                active_ ? 1 : 0, static_cast<void*>(rawData));
            return static_cast<std::int32_t>(ACLPTI_ERROR_NO_ACTIVE_REPLAY);
        }

        if (active_->state != ReplayState::Accepting) {
            npu_compute::detail::DebugLog(
                "aclpti-data", "raw callback rejected: replay=%llu replayState=%s",
                static_cast<unsigned long long>(active_->info.replayId), ReplayStateName(active_->state));
            return static_cast<std::int32_t>(ACLPTI_ERROR_INVALID_STATE);
        }
        const auto fail = [this](aclptiResult status) {
            const uint64_t failedRecordCount = active_->failedRecordCount.fetch_add(1) + 1;
            npu_compute::detail::DebugLog(
                "aclpti-data", "raw callback failed: replay=%llu status=%d failedRecords=%llu",
                static_cast<unsigned long long>(active_->info.replayId), static_cast<int>(status),
                static_cast<unsigned long long>(failedRecordCount));
            return static_cast<std::int32_t>(status);
        };
        if (rawData == nullptr) {
            return fail(ACLPTI_ERROR_INVALID_RAW_DATA);
        }
        const RawDataType expectedPrimaryType = PrimaryRawDataType(active_->info.kind);
        if (rawData->type != expectedPrimaryType && rawData->type != static_cast<RawDataType>(kLogDataType)) {
            npu_compute::detail::DebugLog(
                "aclpti-data", "raw callback type mismatch: replay=%llu expected=%d(%s) actual=%d(%s)",
                static_cast<unsigned long long>(active_->info.replayId), static_cast<int>(expectedPrimaryType),
                RawDataTypeName(expectedPrimaryType), static_cast<int>(rawData->type), RawDataTypeName(rawData->type));
            return fail(ACLPTI_ERROR_INVALID_RAW_DATA);
        }
        if (rawData->chunkSize == 0 || rawData->chunkSize > sizeof(rawData->chunk)) {
            npu_compute::detail::DebugLog(
                "aclpti-data",
                "raw callback invalid chunk: replay=%llu type=%d(%s) device=%d chunkSize=%zu offset=%zu isLast=%d",
                static_cast<unsigned long long>(active_->info.replayId), static_cast<int>(rawData->type),
                RawDataTypeName(rawData->type), rawData->deviceId, rawData->chunkSize, rawData->offset,
                rawData->isLastChunk ? 1 : 0);
            return fail(ACLPTI_ERROR_INVALID_RAW_DATA);
        }
        if (rawData->type == BIU_PERF_DATA_TYPE || rawData->type == PC_SAMPLING_DATA_TYPE) {
            if (active_->opaqueLastSeen || rawData->offset != active_->nextOpaqueOffset ||
                rawData->offset > std::numeric_limits<std::size_t>::max() - rawData->chunkSize) {
                npu_compute::detail::DebugLog(
                    "aclpti-data",
                    "opaque chunk sequence rejected: replay=%llu expectedOffset=%zu actualOffset=%zu "
                    "lastSeen=%d",
                    static_cast<unsigned long long>(active_->info.replayId), active_->nextOpaqueOffset, rawData->offset,
                    active_->opaqueLastSeen ? 1 : 0);
                return fail(ACLPTI_ERROR_INVALID_RAW_DATA);
            }
            try {
                aclptiRawDataChunk chunk{
                    active_->info.replayId, rawData->deviceId,    rawData->chunkModule,
                    rawData->offset,        rawData->isLastChunk, {},
                };
                const auto* begin = reinterpret_cast<const uint8_t*>(rawData->chunk);
                chunk.bytes.assign(begin, begin + rawData->chunkSize);
                if (!rawQueue_.Push(OpaqueChunkItem{active_, rawData->type, std::move(chunk)})) {
                    return fail(ACLPTI_ERROR_INVALID_STATE);
                }
            } catch (const std::bad_alloc&) {
                return fail(ACLPTI_ERROR_OUT_OF_MEMORY);
            }
            active_->nextOpaqueOffset = rawData->offset + rawData->chunkSize;
            active_->opaqueLastSeen = rawData->isLastChunk;
            ++active_->stats.copiedRecordCount;
            active_->stats.copiedBytes += rawData->chunkSize;
            active_->stats.receivedBytes += rawData->chunkSize;
            LogCallbackStats("opaque callback complete", active_->info.replayId, active_->stats, ACLPTI_SUCCESS);
            return static_cast<std::int32_t>(ACLPTI_SUCCESS);
        }
        const std::size_t recordSize = rawData->type == PMU_DATA_TYPE                          ? kPmuRecordSize :
                                       rawData->type == static_cast<RawDataType>(kLogDataType) ? kLogRecordSize :
                                                                                                 0;
        if (recordSize == 0 || rawData->chunkSize % recordSize != 0) {
            npu_compute::detail::DebugLog(
                "aclpti-data",
                "raw callback invalid chunk: replay=%llu type=%d(%s) device=%d chunkSize=%zu recordSize=%zu "
                "offset=%zu isLast=%d",
                static_cast<unsigned long long>(active_->info.replayId), static_cast<int>(rawData->type),
                RawDataTypeName(rawData->type), rawData->deviceId, rawData->chunkSize, recordSize, rawData->offset,
                rawData->isLastChunk ? 1 : 0);
            return fail(ACLPTI_ERROR_INVALID_RAW_DATA);
        }
        LogRawDataDiagnostics(*active_, *rawData);
        const std::size_t recordCount = rawData->chunkSize / recordSize;
        npu_compute::detail::DebugLog(
            "aclpti-data",
            "raw callback accepted: replay=%llu type=%d(%s) device=%d chunkSize=%zu recordSize=%zu records=%zu "
            "offset=%zu isLast=%d",
            static_cast<unsigned long long>(active_->info.replayId), static_cast<int>(rawData->type),
            RawDataTypeName(rawData->type), rawData->deviceId, rawData->chunkSize, recordSize, recordCount,
            rawData->offset, rawData->isLastChunk ? 1 : 0);
        for (std::size_t offset = 0; offset < rawData->chunkSize; offset += recordSize) {
            RawRecord record;
            record.session = active_;
            record.size = recordSize;
            record.recordIndex = active_->nextRecordIndex++;
            std::memcpy(record.bytes.data(), rawData->chunk + offset, recordSize);
            // Backpressure the producer instead of dropping raw records when the decoder falls behind.
            const uint64_t recordIndex = record.recordIndex;
            bool pushed = false;
            try {
                pushed = rawQueue_.Push(std::move(record));
            } catch (const std::bad_alloc&) {
                npu_compute::detail::DebugLog(
                    "aclpti-data", "raw callback queue allocation failed: replay=%llu recordIndex=%llu",
                    static_cast<unsigned long long>(active_->info.replayId),
                    static_cast<unsigned long long>(recordIndex));
                return fail(ACLPTI_ERROR_OUT_OF_MEMORY);
            }
            if (!pushed) {
                npu_compute::detail::DebugLog(
                    "aclpti-data", "raw callback queue closed: replay=%llu recordIndex=%llu",
                    static_cast<unsigned long long>(active_->info.replayId),
                    static_cast<unsigned long long>(recordIndex));
                return fail(ACLPTI_ERROR_INVALID_STATE);
            }
            ++active_->stats.copiedRecordCount;
            npu_compute::detail::DebugLog(
                "aclpti-data", "raw record queued: replay=%llu recordIndex=%llu size=%zu",
                static_cast<unsigned long long>(active_->info.replayId),
                static_cast<unsigned long long>(active_->nextRecordIndex - 1), recordSize);
        }
        active_->stats.copiedBytes += rawData->chunkSize;
        active_->stats.receivedBytes += rawData->chunkSize;
        LogCallbackStats("raw callback complete", active_->info.replayId, active_->stats, ACLPTI_SUCCESS);
        return static_cast<std::int32_t>(ACLPTI_SUCCESS);
    }

    void DecodeAndQueueRecord(RawRecord& raw)
    {
        try {
#if defined(NPU_COMPUTE_ENABLE_TEST_CONTROLS)
            const char* forceAllocationFailure = std::getenv("NPU_COMPUTE_TEST_DECODE_OOM");
            if (forceAllocationFailure != nullptr && forceAllocationFailure[0] != '\0') {
                throw std::bad_alloc();
            }
#endif
            npu_compute::detail::DebugLog(
                "aclpti-data", "decode raw record: replay=%llu recordIndex=%llu size=%zu",
                static_cast<unsigned long long>(raw.session->info.replayId),
                static_cast<unsigned long long>(raw.recordIndex), raw.size);
            auto decoded = DecodeRawRecord(raw.bytes.data(), raw.size, raw.recordIndex, raw.session->info.pmuEventIds);
            if (!decoded.Ok()) {
                npu_compute::detail::DebugLog(
                    "aclpti-data", "decode raw record failed: replay=%llu recordIndex=%llu status=%d",
                    static_cast<unsigned long long>(raw.session->info.replayId),
                    static_cast<unsigned long long>(raw.recordIndex), static_cast<int>(decoded.Status()));
                raw.session->failedRecordCount.fetch_add(1);
                MarkCompleted(raw.session);
                return;
            }
            if (!decodedQueue_.Push(DecodedRecordItem{raw.session, decoded.Value()})) {
                npu_compute::detail::DebugLog(
                    "aclpti-data", "decode queue push failed: replay=%llu recordIndex=%llu",
                    static_cast<unsigned long long>(raw.session->info.replayId),
                    static_cast<unsigned long long>(raw.recordIndex));
                raw.session->failedRecordCount.fetch_add(1);
                MarkCompleted(raw.session);
                return;
            }
            npu_compute::detail::DebugLog(
                "aclpti-data", "decode raw record complete: replay=%llu recordIndex=%llu",
                static_cast<unsigned long long>(raw.session->info.replayId),
                static_cast<unsigned long long>(raw.recordIndex));
        } catch (const std::bad_alloc&) {
            raw.session->failedRecordCount.fetch_add(1);
            MarkCompleted(raw.session);
            npu_compute::detail::DebugLog(
                "aclpti-data", "decode raw record allocation failed: replay=%llu recordIndex=%llu",
                static_cast<unsigned long long>(raw.session->info.replayId),
                static_cast<unsigned long long>(raw.recordIndex));
        }
    }

    void DecodeLoop()
    {
        npu_compute::detail::DebugLog("aclpti-data", "decode thread started");
        RawItem item;
        while (rawQueue_.Pop(item)) {
            if (auto* raw = std::get_if<RawRecord>(&item)) {
                DecodeAndQueueRecord(*raw);
            } else if (auto* opaque = std::get_if<OpaqueChunkItem>(&item)) {
                const std::shared_ptr<ReplaySession> session = opaque->session;
                const std::size_t offset = opaque->chunk.offset;
                bool pushed = false;
                try {
                    pushed = decodedQueue_.Push(std::move(*opaque));
                } catch (const std::bad_alloc&) {
                    npu_compute::detail::DebugLog(
                        "aclpti-data", "opaque queue allocation failed: replay=%llu offset=%zu",
                        static_cast<unsigned long long>(session->info.replayId), offset);
                }
                if (!pushed) {
                    npu_compute::detail::DebugLog(
                        "aclpti-data", "opaque chunk queue push failed: replay=%llu offset=%zu",
                        static_cast<unsigned long long>(session->info.replayId), offset);
                    session->failedRecordCount.fetch_add(1);
                    MarkCompleted(session);
                }
            } else {
                const std::shared_ptr<ReplaySession> session = std::get<ReplayEnd>(item).session;
                bool pushed = false;
                try {
                    pushed = decodedQueue_.Push(std::get<ReplayEnd>(item));
                } catch (const std::bad_alloc&) {
                    npu_compute::detail::DebugLog(
                        "aclpti-data", "decode replay end allocation failed: replay=%llu",
                        static_cast<unsigned long long>(session->info.replayId));
                }
                if (!pushed) {
                    npu_compute::detail::DebugLog(
                        "aclpti-data", "decode replay end push failed: replay=%llu",
                        static_cast<unsigned long long>(session->info.replayId));
                    session->failedRecordCount.fetch_add(1);
                } else {
                    npu_compute::detail::DebugLog(
                        "aclpti-data", "decode replay end queued: replay=%llu",
                        static_cast<unsigned long long>(session->info.replayId));
                }
            }
        }
        decodedQueue_.Close();
        npu_compute::detail::DebugLog("aclpti-data", "decode thread stopped");
    }

    void AssembleLoop()
    {
        npu_compute::detail::DebugLog("aclpti-data", "assemble thread started");
        DecodedItem item;
        while (decodedQueue_.Pop(item)) {
            if (auto* decoded = std::get_if<DecodedRecordItem>(&item)) {
                const std::shared_ptr<ReplaySession> session = decoded->session;
                try {
                    aggregate_.Add(*decoded);
                    npu_compute::detail::DebugLog(
                        "aclpti-data", "assemble decoded record: replay=%llu recordIndex=%llu",
                        static_cast<unsigned long long>(session->info.replayId),
                        static_cast<unsigned long long>(decoded->record.recordIndex));
                } catch (const std::bad_alloc&) {
                    session->failedRecordCount.fetch_add(1);
                    npu_compute::detail::DebugLog(
                        "aclpti-data", "assemble decoded record allocation failed: replay=%llu recordIndex=%llu",
                        static_cast<unsigned long long>(session->info.replayId),
                        static_cast<unsigned long long>(decoded->record.recordIndex));
                }
                MarkCompleted(session);
            } else if (auto* opaque = std::get_if<OpaqueChunkItem>(&item)) {
                const std::shared_ptr<ReplaySession> session = opaque->session;
                const std::size_t offset = opaque->chunk.offset;
                try {
                    aggregate_.Add(std::move(*opaque));
                    npu_compute::detail::DebugLog(
                        "aclpti-data", "assemble opaque chunk: replay=%llu offset=%zu",
                        static_cast<unsigned long long>(session->info.replayId), offset);
                } catch (const std::bad_alloc&) {
                    session->failedRecordCount.fetch_add(1);
                    npu_compute::detail::DebugLog(
                        "aclpti-data", "assemble opaque chunk allocation failed: replay=%llu offset=%zu",
                        static_cast<unsigned long long>(session->info.replayId), offset);
                }
                MarkCompleted(session);
            } else {
                npu_compute::detail::DebugLog(
                    "aclpti-data", "assemble replay end: replay=%llu",
                    static_cast<unsigned long long>(std::get<ReplayEnd>(item).session->info.replayId));
            }
        }
        npu_compute::detail::DebugLog("aclpti-data", "assemble thread stopped");
    }

    aclptiProfilingDataCallback callback_;
    std::mutex mutex_;
    ModuleState state_ = ModuleState::Created;
    aclptiResult shutdownStatus_ = ACLPTI_SUCCESS;
    std::shared_ptr<ReplaySession> active_;
    std::unordered_set<uint64_t> replayIds_;
    std::vector<std::shared_ptr<ReplaySession>> sessions_;
    AggregateState aggregate_;
    BoundedQueue<RawItem> rawQueue_{kQueueCapacity};
    BoundedQueue<DecodedItem> decodedQueue_{kQueueCapacity};
    std::thread decoder_;
    std::thread assembler_;

    static std::mutex routerMutex_;
    static Impl* router_;
};

std::mutex Module::Impl::routerMutex_;
Module::Impl* Module::Impl::router_ = nullptr;

aclptiResult RegisterProfilingDataCallback(aclptiProfilingDataCallback callback)
{
    if (!callback) {
        return ACLPTI_ERROR_INVALID_PARAMETER;
    }
    std::lock_guard<std::mutex> lock(gCallbackMutex);
    if (gCallback) {
        return ACLPTI_ERROR_INVALID_STATE;
    }
    gCallback = std::move(callback);
    return ACLPTI_SUCCESS;
}

aclptiResult RegisterShutdownCallback(aclptiDataModuleShutdownCallback callback, void* userData)
{
    if (callback == nullptr) {
        return ACLPTI_ERROR_INVALID_PARAMETER;
    }
    std::lock_guard<std::mutex> lock(gShutdownCallbackMutex);
    if (gShutdownCallback != nullptr) {
        return ACLPTI_ERROR_INVALID_STATE;
    }
    gShutdownCallback = callback;
    gShutdownCallbackUserData = userData;
    return ACLPTI_SUCCESS;
}

Module::Module() : impl_(std::make_unique<Impl>()) {}

Module::Module(aclptiProfilingDataCallback callback) : impl_(std::make_unique<Impl>(std::move(callback))) {}

Module::~Module() = default;

aclptiResult Module::Initialize() { return impl_->Initialize(); }
MsprofRawDataCallback Module::GetRawDataCallback() { return impl_->GetRawDataCallback(); }
aclptiResult Module::PrepareReplay(const ReplayPrepareInfo& info) { return impl_->PrepareReplay(info); }
ReplayResult Module::RecordReplayStatus(const ReplayStopInfo& info) { return impl_->RecordReplayStatus(info); }
aclptiResult Module::ReleaseReplay(uint64_t replayId) { return impl_->ReleaseReplay(replayId); }
aclptiResult Module::Shutdown() { return impl_->Shutdown(); }
aclptiResult Module::ForceShutdown() { return impl_->ForceShutdown(); }

} // namespace npu_compute::aclpti::data
