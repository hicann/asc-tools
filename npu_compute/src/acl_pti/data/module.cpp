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
#include "raw_data_decoder.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
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

constexpr std::size_t kQueueCapacity = 1024;
constexpr std::size_t kPmuRecordSize = 128;
constexpr std::size_t kLogRecordSize = 32;
constexpr int kLogDataType = 7;

std::mutex gCallbackMutex;
aclptiPmuDataCallback gCallback;
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
    CallbackStats stats{0, 0, 0, 0, 0, 0, 0, ACLPTI_SUCCESS};
    aclptiResult stopStatus = ACLPTI_SUCCESS;
    aclptiResult workerStatus = ACLPTI_SUCCESS;
    std::uint64_t nextRecordIndex = 0;
    std::uint64_t failedRecordCount = 0;
};

struct RawRecord {
    std::shared_ptr<ReplaySession> session;
    std::array<std::byte, 128> bytes{};
    std::size_t size = 0;
    std::uint64_t recordIndex = 0;
};

struct ReplayEnd {
    std::shared_ptr<ReplaySession> session;
};

struct DecodedRecordItem {
    std::shared_ptr<ReplaySession> session;
    DecodedRecord record;
};

using RawItem = std::variant<RawRecord, ReplayEnd>;
using DecodedItem = std::variant<DecodedRecordItem, ReplayEnd>;

aclptiResult FirstError(const ReplaySession& session);

struct PmuValueAccumulator {
    long double sum = 0.0L;
    std::uint64_t count = 0;
};

struct CorePmuAccumulator {
    aclptiCoreType coreType = ACLPTI_CORE_TYPE_AIC;
    std::uint8_t coreId = 0;
    std::uint64_t sampleCount = 0;
    bool overflow = false;
    long double totalCyclesSum = 0.0L;
    std::map<std::uint32_t, PmuValueAccumulator> values;
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
    std::uint8_t coreId = 0;
    std::map<std::pair<aclptiCoreType, std::uint8_t>, std::uint64_t> coreInfoCounts;
    std::map<std::pair<aclptiCoreType, std::uint8_t>, CorePmuAccumulator> coreData;
    bool overflow = false;
    long double totalCyclesSum = 0.0L;
    std::uint64_t totalCyclesCount = 0;
    std::map<std::uint32_t, PmuValueAccumulator> values;
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
    std::map<std::uint16_t, std::vector<aclptiTaskLogRow>> taskLogs;
    std::map<aclptiBlockKey, std::vector<aclptiTaskLogRow>> blockLogs;
    std::map<aclptiBlockKey, PmuAccumulator> pmuLogs;

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
        pmuLogs[aclptiBlockKey{record.blockId, record.subBlockId}].Add(record);
    }

    aclptiPmuDataResult::ErrorStats ErrorStats(const std::vector<std::shared_ptr<ReplaySession>>& sessions) const
    {
        aclptiPmuDataResult::ErrorStats result;
        for (const auto& session : sessions) {
            std::lock_guard<std::mutex> lock(session->mutex);
            result.failedRecordCount += session->failedRecordCount;
            if (session->failedRecordCount != 0) {
                result.failedRecordCountByReplay[session->info.replayId] = session->failedRecordCount;
            }
            const aclptiResult status = FirstError(*session);
            if (result.firstError == ACLPTI_SUCCESS && status != ACLPTI_SUCCESS) {
                result.firstError = status;
            }
        }
        return result;
    }

    aclptiPmuDataResult Snapshot(const std::vector<std::shared_ptr<ReplaySession>>& sessions) const
    {
        aclptiPmuDataResult result;
        result.errorStats = ErrorStats(sessions);
        result.status = result.errorStats.firstError;
        for (const auto& [taskId, logs] : taskLogs) {
            result.taskLogs.emplace(taskId, logs);
        }
        for (const auto& [key, logs] : blockLogs) {
            result.blockLogs.emplace(key, logs);
        }
        for (const auto& [key, aggregate] : pmuLogs) {
            result.pmuLogs.emplace(key, aggregate.Snapshot(key));
        }
        return result;
    }
};

void SetWorkerError(const std::shared_ptr<ReplaySession>& session, aclptiResult status)
{
    std::lock_guard<std::mutex> lock(session->mutex);
    if (session->workerStatus == ACLPTI_SUCCESS) {
        session->workerStatus = status;
    }
}

aclptiResult FirstError(const ReplaySession& session)
{
    if (session.stopStatus != ACLPTI_SUCCESS) {
        return session.stopStatus;
    }
    if (session.stats.firstError != ACLPTI_SUCCESS) {
        return session.stats.firstError;
    }
    if (session.workerStatus != ACLPTI_SUCCESS) {
        return session.workerStatus;
    }
    return ACLPTI_SUCCESS;
}

bool IsValidReplayInfo(const ReplayPrepareInfo& info)
{
    if (info.sectionName.empty()) {
        return false;
    }

    bool unusedSlotSeen = false;
    for (const std::uint32_t eventId : info.pmuEventIds) {
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

} // namespace

class Module::Impl {
public:
    Impl() = default;

    explicit Impl(aclptiPmuDataCallback callback) : callback_(std::move(callback))
    {
        if (!callback_) {
            throw std::invalid_argument("PTI data callback is required");
        }
    }

    ~Impl() { ForceShutdown(); }

    aclptiResult Initialize()
    {
#if defined(NPU_COMPUTE_ENABLE_TEST_CONTROLS)
        const char* initializeFailure = std::getenv("NPU_COMPUTE_TEST_PTI_INITIALIZE_FAILURE");
        if (initializeFailure != nullptr && initializeFailure[0] != '\0') {
            return ACLPTI_ERROR_INTERNAL;
        }
#endif
        std::lock_guard<std::mutex> routerLock(routerMutex_);
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == ModuleState::Running) {
            return ACLPTI_SUCCESS;
        }
        if (state_ != ModuleState::Created) {
            return ACLPTI_ERROR_INVALID_STATE;
        }
        if (router_ != nullptr) {
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
            callback_ = [](std::shared_ptr<const aclptiPmuDataResult>) { return ACLPTI_SUCCESS; };
        }
        router_ = this;
        state_ = ModuleState::Running;
        try {
            assembler_ = std::thread(&AssembleThread, static_cast<void*>(this));
            decoder_ = std::thread(&DecodeThread, static_cast<void*>(this));
        } catch (...) {
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
        return ACLPTI_SUCCESS;
    }

    MsprofRawDataCallback GetRawDataCallback()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == ModuleState::Running ? &RawDataThunk : nullptr;
    }

    aclptiResult PrepareReplay(const ReplayPrepareInfo& info)
    {
        if (!IsValidReplayInfo(info)) {
            return ACLPTI_ERROR_INVALID_PARAMETER;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != ModuleState::Running) {
            return ACLPTI_ERROR_NOT_INITIALIZED;
        }
        if (active_) {
            return ACLPTI_ERROR_REPLAY_ACTIVE;
        }
        if (!replayIds_.insert(info.replayId).second) {
            return ACLPTI_ERROR_INVALID_PARAMETER;
        }
        active_ = std::make_shared<ReplaySession>(info);
        sessions_.push_back(active_);
        return ACLPTI_SUCCESS;
    }

    ReplayResult RecordReplayStatus(const ReplayStopInfo& info)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ReplayResult result{info.replayId, ACLPTI_SUCCESS, {0, 0, 0, 0, 0, 0, 0, ACLPTI_SUCCESS}};
        if (state_ != ModuleState::Running) {
            result.status = ACLPTI_ERROR_NOT_INITIALIZED;
            return result;
        }
        if (!active_) {
            result.status = ACLPTI_ERROR_NO_ACTIVE_REPLAY;
            return result;
        }
        if (active_->info.replayId != info.replayId) {
            result.status = ACLPTI_ERROR_REPLAY_NOT_FOUND;
            return result;
        }

        std::lock_guard<std::mutex> sessionLock(active_->mutex);
        result.callbackStats = active_->stats;
        if (active_->state != ReplayState::Accepting) {
            result.status = ACLPTI_ERROR_INVALID_STATE;
            return result;
        }
        active_->state = ReplayState::Closed;
        active_->stopStatus = info.stopStatus;
        result.status = FirstError(*active_);
        return result;
    }

    aclptiResult ReleaseReplay(std::uint64_t replayId)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != ModuleState::Running) {
            return ACLPTI_ERROR_NOT_INITIALIZED;
        }
        if (!active_) {
            return ACLPTI_ERROR_NO_ACTIVE_REPLAY;
        }
        if (active_->info.replayId != replayId) {
            return ACLPTI_ERROR_REPLAY_NOT_FOUND;
        }
        {
            std::lock_guard<std::mutex> sessionLock(active_->mutex);
            if (active_->state != ReplayState::Closed) {
                return ACLPTI_ERROR_INVALID_STATE;
            }
            if (!rawQueue_.Push(ReplayEnd{active_})) {
                return ACLPTI_ERROR_INVALID_STATE;
            }
            active_->state = ReplayState::Released;
        }
        active_.reset();
        return ACLPTI_SUCCESS;
    }

    aclptiResult Shutdown()
    {
        std::lock_guard<std::mutex> routerLock(routerMutex_);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (state_ == ModuleState::Created || state_ == ModuleState::Stopped) {
                return shutdownStatus_;
            }
            if (active_) {
                return ACLPTI_ERROR_REPLAY_ACTIVE;
            }
            state_ = ModuleState::Stopping;
        }
        if (router_ == this) {
            router_ = nullptr;
        }
        rawQueue_.Close();
        if (decoder_.joinable()) {
            decoder_.join();
        }
        if (assembler_.joinable()) {
            assembler_.join();
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            state_ = ModuleState::Stopped;
        }
        auto result = std::make_shared<aclptiPmuDataResult>(aggregate_.Snapshot(sessions_));
        try {
            callback_(std::move(result));
        } catch (...) {
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
            return shutdownStatus_;
        }
        try {
            shutdownStatus_ = static_cast<aclptiResult>(shutdownCallback(shutdownUserData));
        } catch (...) {
            shutdownStatus_ = ACLPTI_ERROR_INTERNAL;
        }
        return shutdownStatus_;
    }

private:
    static void DecodeThread(void* context) { static_cast<Impl*>(context)->DecodeLoop(); }

    static void AssembleThread(void* context) { static_cast<Impl*>(context)->AssembleLoop(); }

    static std::int32_t RawDataThunk(MsprofRawData* rawData)
    {
        std::lock_guard<std::mutex> lock(routerMutex_);
        return router_ == nullptr ? static_cast<std::int32_t>(ACLPTI_ERROR_NOT_INITIALIZED) :
                                    router_->OnRawData(rawData);
    }

    std::int32_t OnRawData(MsprofRawData* rawData)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != ModuleState::Running || !active_) {
            return static_cast<std::int32_t>(ACLPTI_ERROR_NO_ACTIVE_REPLAY);
        }

        std::lock_guard<std::mutex> sessionLock(active_->mutex);
        if (active_->state != ReplayState::Accepting) {
            return static_cast<std::int32_t>(ACLPTI_ERROR_INVALID_STATE);
        }
        const auto fail = [this](aclptiResult status) {
            if (active_->stats.firstError == ACLPTI_SUCCESS) {
                active_->stats.firstError = status;
            }
            ++active_->failedRecordCount;
            return static_cast<std::int32_t>(status);
        };
        if (rawData == nullptr) {
            return fail(ACLPTI_ERROR_INVALID_RAW_DATA);
        }
        const std::size_t recordSize = rawData->type == PMU_DATA_TYPE                          ? kPmuRecordSize :
                                       rawData->type == static_cast<RawDataType>(kLogDataType) ? kLogRecordSize :
                                                                                                 0;
        if (recordSize == 0 || rawData->chunkSize == 0 || rawData->chunkSize > sizeof(rawData->chunk) ||
            rawData->chunkSize % recordSize != 0) {
            return fail(ACLPTI_ERROR_INVALID_RAW_DATA);
        }
        for (std::size_t offset = 0; offset < rawData->chunkSize; offset += recordSize) {
            RawRecord record;
            record.session = active_;
            record.size = recordSize;
            record.recordIndex = active_->nextRecordIndex++;
            std::memcpy(record.bytes.data(), rawData->chunk + offset, recordSize);
            if (!rawQueue_.TryPush(std::move(record))) {
                return fail(ACLPTI_ERROR_QUEUE_FULL);
            }
            ++active_->stats.copiedRecordCount;
        }
        active_->stats.copiedBytes += rawData->chunkSize;
        active_->stats.receivedBytes += rawData->chunkSize;
        return static_cast<std::int32_t>(ACLPTI_SUCCESS);
    }

    void DecodeLoop()
    {
        RawItem item;
        while (rawQueue_.Pop(item)) {
            if (auto* raw = std::get_if<RawRecord>(&item)) {
                auto decoded =
                    DecodeRawRecord(raw->bytes.data(), raw->size, raw->recordIndex, raw->session->info.pmuEventIds);
                if (!decoded.Ok()) {
                    SetWorkerError(raw->session, decoded.Status());
                    std::lock_guard<std::mutex> lock(raw->session->mutex);
                    ++raw->session->failedRecordCount;
                    continue;
                }
                if (!decodedQueue_.Push(DecodedRecordItem{raw->session, decoded.Value()})) {
                    SetWorkerError(raw->session, ACLPTI_ERROR_INTERNAL);
                    std::lock_guard<std::mutex> lock(raw->session->mutex);
                    ++raw->session->failedRecordCount;
                }
            } else if (!decodedQueue_.Push(std::get<ReplayEnd>(item))) {
                SetWorkerError(std::get<ReplayEnd>(item).session, ACLPTI_ERROR_INTERNAL);
                std::lock_guard<std::mutex> lock(std::get<ReplayEnd>(item).session->mutex);
                ++std::get<ReplayEnd>(item).session->failedRecordCount;
            }
        }
        decodedQueue_.Close();
    }

    void AssembleLoop()
    {
        DecodedItem item;
        while (decodedQueue_.Pop(item)) {
            if (auto* decoded = std::get_if<DecodedRecordItem>(&item)) {
                aggregate_.Add(*decoded);
            }
        }
    }

    void ForceShutdown()
    {
        std::shared_ptr<ReplaySession> session;
        {
            std::lock_guard<std::mutex> routerLock(routerMutex_);
            std::lock_guard<std::mutex> lock(mutex_);
            if (state_ == ModuleState::Running && active_) {
                session = active_;
                std::lock_guard<std::mutex> sessionLock(session->mutex);
                session->state = ReplayState::Released;
                session->stopStatus = ACLPTI_ERROR_INTERNAL;
                active_.reset();
            }
        }
        if (session) {
            rawQueue_.Push(ReplayEnd{std::move(session)});
        }
        Shutdown();
    }

    aclptiPmuDataCallback callback_;
    std::mutex mutex_;
    ModuleState state_ = ModuleState::Created;
    aclptiResult shutdownStatus_ = ACLPTI_SUCCESS;
    std::shared_ptr<ReplaySession> active_;
    std::unordered_set<std::uint64_t> replayIds_;
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

aclptiResult RegisterPmuDataCallback(aclptiPmuDataCallback callback)
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

Module::Module(aclptiPmuDataCallback callback) : impl_(std::make_unique<Impl>(std::move(callback))) {}

Module::~Module() = default;

aclptiResult Module::Initialize() { return impl_->Initialize(); }
MsprofRawDataCallback Module::GetRawDataCallback() { return impl_->GetRawDataCallback(); }
aclptiResult Module::PrepareReplay(const ReplayPrepareInfo& info) { return impl_->PrepareReplay(info); }
ReplayResult Module::RecordReplayStatus(const ReplayStopInfo& info) { return impl_->RecordReplayStatus(info); }
aclptiResult Module::ReleaseReplay(std::uint64_t replayId) { return impl_->ReleaseReplay(replayId); }
aclptiResult Module::Shutdown() { return impl_->Shutdown(); }

} // namespace npu_compute::aclpti::data
