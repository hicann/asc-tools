/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "acl_pti/data/module.h"
#include "acl_pti/data/raw_data_decoder.h"

#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <condition_variable>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <sys/mman.h>
#include <unistd.h>

#define CHECK(condition)                                                                         \
    do {                                                                                         \
        if (!(condition)) {                                                                      \
            std::fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                                            \
        }                                                                                        \
    } while (false)

namespace data = npu_compute::aclpti::data;
namespace data_detail = npu_compute::aclpti::data::detail;

void StoreWord(std::byte* data, std::size_t index, uint32_t value)
{
    const std::size_t offset = index * sizeof(value);
    for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
        data[offset + byte] = static_cast<std::byte>((value >> (byte * 8U)) & 0xffU);
    }
}

template <std::size_t Size>
MsprofRawData RawData(const std::array<std::byte, Size>& bytes, RawDataType type, std::int32_t deviceId = 0)
{
    MsprofRawData raw{};
    raw.isLastChunk = false;
    raw.chunkModule = 7;
    raw.deviceId = deviceId;
    raw.type = type;
    raw.chunkSize = Size;
    std::memcpy(raw.chunk, bytes.data(), Size);
    return raw;
}

std::array<std::byte, 32> TaskLog(
    uint8_t funcType, uint16_t taskId, uint16_t streamId, uint64_t systemCounter, uint16_t blockId, uint16_t subBlockId,
    aclptiCoreType coreType, uint8_t coreTypeId)
{
    std::array<std::byte, 32> bytes{};
    StoreWord(bytes.data(), 0, 0x6bd30000U | funcType);
    StoreWord(bytes.data(), 1, (uint32_t(taskId) << 16U) | streamId);
    StoreWord(bytes.data(), 2, static_cast<uint32_t>(systemCounter));
    StoreWord(bytes.data(), 3, static_cast<uint32_t>(systemCounter >> 32U));
    StoreWord(bytes.data(), 5, (uint32_t(coreTypeId) << 1U) | (coreType == ACLPTI_CORE_TYPE_AIV ? 1U : 0U));
    StoreWord(bytes.data(), 6, (uint32_t(blockId) << 16U) | subBlockId);
    return bytes;
}

std::array<std::byte, 128> PmuRecord(uint16_t taskId, uint16_t streamId, uint16_t blockId, uint64_t firstCounter)
{
    std::array<std::byte, 128> bytes{};
    StoreWord(bytes.data(), 0, 0x6bd3002aU);
    StoreWord(bytes.data(), 1, (uint32_t(taskId) << 16U) | streamId);
    StoreWord(bytes.data(), 2, 100U);
    StoreWord(bytes.data(), 5, 3U << 8U);
    StoreWord(bytes.data(), 6, uint32_t(blockId) << 16U);
    for (std::size_t index = 0; index < data::kMaxPmuSlots; ++index) {
        const uint64_t value = firstCounter + index;
        StoreWord(bytes.data(), 8 + index * 2, static_cast<uint32_t>(value));
        StoreWord(bytes.data(), 9 + index * 2, static_cast<uint32_t>(value >> 32U));
    }
    return bytes;
}

data::PmuSlots PmuEvents(std::initializer_list<uint32_t> events)
{
    data::PmuSlots slots{};
    slots.fill(data::kInvalidPmuEvent);
    std::copy(events.begin(), events.end(), slots.begin());
    return slots;
}

class ResultSink {
public:
    aclptiResult Accept(std::shared_ptr<const aclptiPmuDataResult> result)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        results_.push_back(std::move(result));
        ready_.notify_all();
        return ACLPTI_SUCCESS;
    }

    std::shared_ptr<const aclptiPmuDataResult> Wait()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!ready_.wait_for(lock, std::chrono::seconds(3), [this] { return !results_.empty(); })) {
            return nullptr;
        }
        return results_.front();
    }

    std::size_t Count()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return results_.size();
    }

private:
    std::mutex mutex_;
    std::condition_variable ready_;
    std::vector<std::shared_ptr<const aclptiPmuDataResult>> results_;
};

template <typename Function>
bool CaptureStderr(Function function, std::string* output)
{
    FILE* capture = std::tmpfile();
    if (capture == nullptr) {
        return false;
    }

    const int savedStderr = dup(STDERR_FILENO);
    if (savedStderr < 0) {
        std::fclose(capture);
        return false;
    }

    bool success = std::fflush(stderr) == 0 && dup2(fileno(capture), STDERR_FILENO) >= 0;
    if (success) {
        function();
        success = std::fflush(stderr) == 0;
    }
    success = dup2(savedStderr, STDERR_FILENO) >= 0 && success;
    close(savedStderr);

    if (success) {
        std::rewind(capture);
        char buffer[256];
        std::size_t count = 0;
        while ((count = std::fread(buffer, 1, sizeof(buffer), capture)) != 0) {
            output->append(buffer, count);
        }
        success = std::ferror(capture) == 0;
    }

    success = std::fclose(capture) == 0 && success;
    return success;
}

struct GuardedRawData {
    void* mapping = MAP_FAILED;
    std::size_t mappingSize = 0;
    MsprofRawData* raw = nullptr;

    ~GuardedRawData()
    {
        if (mapping != MAP_FAILED) {
            munmap(mapping, mappingSize);
        }
    }
};

static_assert(sizeof(MsprofRawData) == offsetof(MsprofRawData, chunk) + RAW_DATA_MAXSIZE);

bool MakeGuardedRawData(std::size_t chunkSize, RawDataType type, GuardedRawData* guarded)
{
    if (guarded == nullptr) {
        return false;
    }

    const long pageSizeValue = sysconf(_SC_PAGESIZE);
    if (pageSizeValue <= 0) {
        return false;
    }

    const std::size_t pageSize = static_cast<std::size_t>(pageSizeValue);
    const std::size_t mappingSize = pageSize * 2;
    void* mapping = mmap(nullptr, mappingSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapping == MAP_FAILED) {
        return false;
    }

    if (mprotect(static_cast<char*>(mapping) + pageSize, pageSize, PROT_NONE) != 0) {
        munmap(mapping, mappingSize);
        return false;
    }

    guarded->mapping = mapping;
    guarded->mappingSize = mappingSize;
    guarded->raw = reinterpret_cast<MsprofRawData*>(
        static_cast<char*>(mapping) + pageSize - (offsetof(MsprofRawData, chunk) + RAW_DATA_MAXSIZE));
    guarded->raw->isLastChunk = false;
    guarded->raw->offset = 0;
    guarded->raw->chunkModule = 7;
    guarded->raw->deviceId = 0;
    guarded->raw->type = type;
    guarded->raw->chunkSize = chunkSize;
    std::memset(guarded->raw->chunk, 0, RAW_DATA_MAXSIZE);
    return true;
}

int TestRawDataDecoder()
{
    std::array<std::byte, 32> taskLog{};
    StoreWord(taskLog.data(), 0, 0x6bd30000U);
    StoreWord(taskLog.data(), 1, 0x12340056U);
    StoreWord(taskLog.data(), 2, 0x89abcdefU);
    StoreWord(taskLog.data(), 3, 0x01234567U);

    const auto start = data_detail::DecodeRawRecord(taskLog.data(), taskLog.size(), 3);
    CHECK(start.Ok());
    CHECK(start.Value().recordIndex == 3);
    CHECK(std::holds_alternative<data_detail::TaskLog32>(start.Value().payload));
    const auto& startLog = std::get<data_detail::TaskLog32>(start.Value().payload);
    CHECK(startLog.funcType == 0x00);
    CHECK(startLog.taskId == 0x1234);
    CHECK(startLog.rtStreamId == 0x0056);
    CHECK(startLog.systemCounter == 0x0123456789abcdefULL);

    StoreWord(taskLog.data(), 0, 0x6bd30001U);
    const auto end = data_detail::DecodeRawRecord(taskLog.data(), taskLog.size(), 4);
    CHECK(end.Ok());
    CHECK(std::get<data_detail::TaskLog32>(end.Value().payload).funcType == 0x01);

    StoreWord(taskLog.data(), 0, 0x6bd30024U);
    StoreWord(taskLog.data(), 5, (0x5aU << 1U) | 1U);
    StoreWord(taskLog.data(), 6, 0x12345678U);
    const auto block = data_detail::DecodeRawRecord(taskLog.data(), taskLog.size(), 6);
    CHECK(block.Ok());
    const auto& blockLog = std::get<data_detail::TaskLog32>(block.Value().payload);
    CHECK(blockLog.funcType == 0x24);
    CHECK(blockLog.taskId == 0x1234);
    CHECK(blockLog.rtStreamId == 0x0056);
    CHECK(blockLog.systemCounter == 0x0123456789abcdefULL);
    CHECK(blockLog.blockId == 0x1234);
    CHECK(blockLog.subBlockId == 0x5678);
    CHECK(blockLog.coreType == ACLPTI_CORE_TYPE_AIV);
    CHECK(blockLog.coreTypeId == 0x5a);

    std::array<std::byte, 128> pmu{};
    StoreWord(pmu.data(), 0, 0x6bd3002aU);
    StoreWord(pmu.data(), 1, 0x43210078U);
    StoreWord(pmu.data(), 2, 0x76543210U);
    StoreWord(pmu.data(), 3, 0xfedcba98U);
    StoreWord(pmu.data(), 4, 1U << 10U);
    StoreWord(pmu.data(), 5, (0x5aU << 8U) | 1U);
    StoreWord(pmu.data(), 6, 0x12345678U);
    StoreWord(pmu.data(), 28, 0x10203040U);
    StoreWord(pmu.data(), 29, 0x50607080U);
    StoreWord(pmu.data(), 30, 0x90a0b0c0U);
    StoreWord(pmu.data(), 31, 0xd0e0f000U);
    for (std::size_t index = 0; index < data::kMaxPmuSlots; ++index) {
        const uint64_t value = 0x100000000ULL + index;
        StoreWord(pmu.data(), 8 + index * 2, static_cast<uint32_t>(value));
        StoreWord(pmu.data(), 9 + index * 2, static_cast<uint32_t>(value >> 32U));
    }

    const auto decoded = data_detail::DecodeRawRecord(
        pmu.data(), pmu.size(), 5, PmuEvents({0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xa0}));
    CHECK(decoded.Ok());
    CHECK(std::holds_alternative<data_detail::PmuRecord128>(decoded.Value().payload));
    const auto& record = std::get<data_detail::PmuRecord128>(decoded.Value().payload);
    CHECK(record.taskId == 0x4321);
    CHECK(record.rtStreamId == 0x0078);
    CHECK(record.totalCycles == 0xfedcba9876543210ULL);
    CHECK(record.taskStartSystemCounter == 0x5060708010203040ULL);
    CHECK(record.taskEndSystemCounter == 0xd0e0f00090a0b0c0ULL);
    CHECK(record.overflow);
    CHECK(record.coreType == ACLPTI_CORE_TYPE_AIV);
    CHECK(record.coreId == 0x5a);
    CHECK(record.blockId == 0x1234);
    CHECK(record.subBlockId == 0x5678);
    CHECK(record.pmuValues.at(0xa0) == 0x100000009ULL);

    StoreWord(pmu.data(), 8, 100U);
    StoreWord(pmu.data(), 9, 0U);
    StoreWord(pmu.data(), 10, 300U);
    StoreWord(pmu.data(), 11, 0U);
    const auto duplicateEvent = data_detail::DecodeRawRecord(pmu.data(), pmu.size(), 7, PmuEvents({0x10, 0x10}));
    CHECK(duplicateEvent.Ok());
    const auto& duplicateRecord = std::get<data_detail::PmuRecord128>(duplicateEvent.Value().payload);
    CHECK(duplicateRecord.pmuValues.at(0x10) == 200.0);

    CHECK(data_detail::DecodeRawRecord(nullptr, 32, 0).Status() == ACLPTI_ERROR_INVALID_RAW_DATA);
    CHECK(data_detail::DecodeRawRecord(pmu.data(), 64, 0).Status() == ACLPTI_ERROR_INVALID_RAW_DATA);
    StoreWord(pmu.data(), 0, 0x0000002aU);
    CHECK(data_detail::DecodeRawRecord(pmu.data(), pmu.size(), 0).Status() == ACLPTI_ERROR_DECODE);
    StoreWord(taskLog.data(), 0, 0x6bd30002U);
    CHECK(data_detail::DecodeRawRecord(taskLog.data(), taskLog.size(), 0).Status() == ACLPTI_ERROR_DECODE);
    return 0;
}

int TestReplayLifecycle()
{
    ResultSink sink;
    std::atomic<bool> callbackCompleted{false};
    data::Module module([&sink, &callbackCompleted](const auto& result) {
        const auto status = sink.Accept(result);
        callbackCompleted.store(true, std::memory_order_release);
        return status;
    });
    CHECK(module.GetRawDataCallback() == nullptr);
    CHECK(module.Initialize() == ACLPTI_SUCCESS);

    data::Module second([](const auto&) { return ACLPTI_SUCCESS; });
    CHECK(second.Initialize() == ACLPTI_ERROR_REPLAY_ACTIVE);

    data::PmuSlots noEvents{};
    noEvents.fill(data::kInvalidPmuEvent);
    CHECK(module.PrepareReplay({40, "", PmuEvents({0x701})}) == ACLPTI_ERROR_INVALID_PARAMETER);
    CHECK(module.PrepareReplay({40, "CallerDefined", noEvents}) == ACLPTI_SUCCESS);
    CHECK(module.RecordReplayStatus({40, ACLPTI_SUCCESS}).status == ACLPTI_SUCCESS);
    CHECK(module.ReleaseReplay(40) == ACLPTI_SUCCESS);
    auto eventHole = PmuEvents({0x701});
    eventHole[2] = 0x22;
    CHECK(module.PrepareReplay({40, "CallerDefined", eventHole}) == ACLPTI_ERROR_INVALID_PARAMETER);

    const auto eventIds = PmuEvents({0x701, 0x22});
    CHECK(module.PrepareReplay({42, "CallerDefined", eventIds}) == ACLPTI_SUCCESS);
    CHECK(module.PrepareReplay({43, "Other", PmuEvents({0x1})}) == ACLPTI_ERROR_REPLAY_ACTIVE);

    const MsprofRawDataCallback callback = module.GetRawDataCallback();
    CHECK(callback != nullptr);
    auto first = RawData(PmuRecord(8, 9, 2, 1000), PMU_DATA_TYPE, 17);
    auto secondRow = RawData(PmuRecord(8, 9, 3, 2000), PMU_DATA_TYPE, 17);
    std::array<std::byte, 256> pmuChunk{};
    const auto thirdRow = PmuRecord(12, 13, 4, 3000);
    std::memcpy(pmuChunk.data(), secondRow.chunk, 128);
    std::memcpy(pmuChunk.data() + 128, thirdRow.data(), 128);
    auto multiRow = RawData(pmuChunk, PMU_DATA_TYPE, 17);
    std::array<std::byte, 64> logChunk{};
    const auto firstLog =
        TaskLog(0x24, 0x1234, 0x0056, 0x0123456789abcdefULL, 0x1111, 0x2222, ACLPTI_CORE_TYPE_AIV, 0x5a);
    const auto secondLog =
        TaskLog(0x24, 0x1234, 0x0056, 0xfedcba9876543210ULL, 0x3333, 0x4444, ACLPTI_CORE_TYPE_AIC, 0x2b);
    std::memcpy(logChunk.data(), firstLog.data(), 32);
    std::memcpy(logChunk.data() + 32, secondLog.data(), 32);
    auto multiLog = RawData(logChunk, LOG_DATA_TYPE);
    multiLog.chunkModule = 1;
    multiLog.offset = 900;
    multiLog.isLastChunk = true;
    multiRow.chunkModule = 99;
    multiRow.offset = 17;
    CHECK(callback(&multiLog) == 0);
    CHECK(callback(&first) == 0);
    CHECK(callback(&multiRow) == 0);

    CHECK(module.ReleaseReplay(42) == ACLPTI_ERROR_INVALID_STATE);
    CHECK(module.RecordReplayStatus({41, ACLPTI_SUCCESS}).status == ACLPTI_ERROR_REPLAY_NOT_FOUND);
    const auto stopped = module.RecordReplayStatus({42, ACLPTI_SUCCESS});
    CHECK(stopped.status == ACLPTI_SUCCESS);
    CHECK(stopped.callbackStats.copiedRecordCount == 5);
    CHECK(stopped.callbackStats.copiedBytes == 448);
    CHECK(stopped.callbackStats.receivedBytes == 448);
    CHECK(stopped.callbackStats.offsetMismatchCount == 0);
    CHECK(stopped.callbackStats.lastChunkCount == 0);
    CHECK(module.ReleaseReplay(42) == ACLPTI_SUCCESS);
    CHECK(module.Shutdown() == ACLPTI_SUCCESS);
    CHECK(callbackCompleted.load(std::memory_order_acquire));

    const auto result = sink.Wait();
    CHECK(result != nullptr);
    CHECK(result->status == ACLPTI_SUCCESS);
    CHECK(result->taskLogs.empty());
    CHECK(result->blockLogs.at(aclptiBlockKey{0x1111, 0x2222}).size() == 1);
    CHECK(result->blockLogs.at(aclptiBlockKey{0x3333, 0x4444}).size() == 1);
    CHECK(result->pmuLogs.size() == 3);
    CHECK(result->pmuLogs.at(aclptiBlockKey{2, 0, ACLPTI_CORE_TYPE_AIC, 3}).values.at(0x701) == 1000.0);
    CHECK(result->pmuLogs.at(aclptiBlockKey{2, 0, ACLPTI_CORE_TYPE_AIC, 3}).values.at(0x22) == 1001.0);
    CHECK(result->pmuLogs.at(aclptiBlockKey{3, 0, ACLPTI_CORE_TYPE_AIC, 3}).values.at(0x701) == 2000.0);
    CHECK(result->pmuLogs.at(aclptiBlockKey{3, 0, ACLPTI_CORE_TYPE_AIC, 3}).values.at(0x22) == 2001.0);
    CHECK(result->pmuLogs.at(aclptiBlockKey{4, 0, ACLPTI_CORE_TYPE_AIC, 3}).values.at(0x701) == 3000.0);
    CHECK(sink.Count() == 1);
    return 0;
}

int TestFailedReplay()
{
    ResultSink sink;
    data::Module module([&sink](const auto& result) { return sink.Accept(result); });
    CHECK(module.Initialize() == ACLPTI_SUCCESS);
    CHECK(module.PrepareReplay({7, "Failure", PmuEvents({0x500})}) == ACLPTI_SUCCESS);
    CHECK(module.Shutdown() == ACLPTI_ERROR_REPLAY_ACTIVE);

    auto invalidType = RawData(PmuRecord(1, 2, 3, 4), PMU_DATA_TYPE);
    invalidType.type = static_cast<RawDataType>(0);
    CHECK(module.GetRawDataCallback()(&invalidType) == static_cast<std::int32_t>(ACLPTI_ERROR_INVALID_RAW_DATA));
    const auto stopped = module.RecordReplayStatus({7, ACLPTI_SUCCESS});
    CHECK(stopped.status == ACLPTI_SUCCESS);
    CHECK(module.ReleaseReplay(7) == ACLPTI_SUCCESS);
    CHECK(module.Shutdown() == ACLPTI_SUCCESS);
    const auto result = sink.Wait();
    CHECK(result != nullptr);
    CHECK(result->status == ACLPTI_SUCCESS);
    CHECK(result->errorStats.failedRecordCount == 1);
    CHECK(result->errorStats.failedRecordCountByReplay.at(7) == 1);
    CHECK(result->pmuLogs.empty());
    CHECK(sink.Count() == 1);

    ResultSink decodeSink;
    data::Module decodeModule([&decodeSink](const auto& decodedResult) { return decodeSink.Accept(decodedResult); });
    CHECK(decodeModule.Initialize() == ACLPTI_SUCCESS);
    CHECK(decodeModule.PrepareReplay({8, "Decode", PmuEvents({0x8, 0xa})}) == ACLPTI_SUCCESS);
    auto malformed = PmuRecord(1, 2, 3, 4);
    StoreWord(malformed.data(), 0, 0x0000002aU);
    auto malformedRaw = RawData(malformed, PMU_DATA_TYPE);
    CHECK(decodeModule.GetRawDataCallback()(&malformedRaw) == 0);
    CHECK(decodeModule.RecordReplayStatus({8, ACLPTI_SUCCESS}).status == ACLPTI_SUCCESS);
    CHECK(decodeModule.ReleaseReplay(8) == ACLPTI_SUCCESS);
    CHECK(decodeModule.Shutdown() == ACLPTI_SUCCESS);
    const auto decodeResult = decodeSink.Wait();
    CHECK(decodeResult != nullptr);
    CHECK(decodeResult->status == ACLPTI_SUCCESS);
    CHECK(decodeResult->errorStats.failedRecordCount == 1);
    CHECK(decodeResult->errorStats.failedRecordCountByReplay.at(8) == 1);
    CHECK(decodeResult->pmuLogs.empty());

    setenv("NPU_COMPUTE_SKIP_DATA_PARSE", "1", 1);
    ResultSink skippedSink;
    data::Module skippedModule([&skippedSink](const auto& skippedResult) { return skippedSink.Accept(skippedResult); });
    CHECK(skippedModule.Initialize() == ACLPTI_SUCCESS);
    CHECK(skippedModule.PrepareReplay({9, "SkipDecode", PmuEvents({0x8, 0xa})}) == ACLPTI_SUCCESS);
    CHECK(skippedModule.GetRawDataCallback()(&malformedRaw) == 0);
    CHECK(skippedModule.RecordReplayStatus({9, ACLPTI_SUCCESS}).status == ACLPTI_SUCCESS);
    CHECK(skippedModule.ReleaseReplay(9) == ACLPTI_SUCCESS);
    CHECK(skippedModule.Shutdown() == ACLPTI_SUCCESS);
    unsetenv("NPU_COMPUTE_SKIP_DATA_PARSE");
    const auto skippedResult = skippedSink.Wait();
    CHECK(skippedResult != nullptr);
    CHECK(skippedResult->status == ACLPTI_SUCCESS);
    CHECK(skippedResult->pmuLogs.empty());

    bool threw = false;
    try {
        data::Module invalid(aclptiPmuDataCallback{});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
    return 0;
}

int TestForceShutdownActiveReplay()
{
    ResultSink sink;
    data::Module module([&sink](const auto& result) { return sink.Accept(result); });
    CHECK(module.Initialize() == ACLPTI_SUCCESS);
    CHECK(module.PrepareReplay({9, "ForcedShutdown", PmuEvents({0x8})}) == ACLPTI_SUCCESS);
    CHECK(module.Shutdown() == ACLPTI_ERROR_REPLAY_ACTIVE);
    CHECK(module.ForceShutdown() == ACLPTI_SUCCESS);
    CHECK(module.Shutdown() == ACLPTI_SUCCESS);

    const auto result = sink.Wait();
    CHECK(result != nullptr);
    CHECK(result->status == ACLPTI_SUCCESS);
    CHECK(result->pmuLogs.empty());
    return 0;
}

int TestInvalidChunkDiagnostics()
{
    const char* previousDebug = std::getenv("NPU_COMPUTE_DEBUG");
    const std::string previousDebugValue = previousDebug == nullptr ? std::string() : std::string(previousDebug);

    ResultSink sink;
    data::Module module([&sink](const auto& result) { return sink.Accept(result); });
    CHECK(module.Initialize() == ACLPTI_SUCCESS);
    CHECK(module.PrepareReplay({9, "InvalidChunk", PmuEvents({0x55})}) == ACLPTI_SUCCESS);
    const MsprofRawDataCallback callback = module.GetRawDataCallback();
    CHECK(callback != nullptr);

    CHECK(setenv("NPU_COMPUTE_DEBUG", "1", 1) == 0);

    GuardedRawData guarded;
    CHECK(MakeGuardedRawData(RAW_DATA_MAXSIZE + 1, PMU_DATA_TYPE, &guarded));

    std::string output;
    std::int32_t status = 0;
    CHECK(CaptureStderr([&] { status = callback(guarded.raw); }, &output));
    CHECK(status == static_cast<std::int32_t>(ACLPTI_ERROR_INVALID_RAW_DATA));
    CHECK(output.find("[DEBUG-rawdata]") == std::string::npos);
    CHECK(output.find("raw callback invalid chunk") != std::string::npos);

    if (previousDebug == nullptr) {
        CHECK(unsetenv("NPU_COMPUTE_DEBUG") == 0);
    } else {
        CHECK(setenv("NPU_COMPUTE_DEBUG", previousDebugValue.c_str(), 1) == 0);
    }

    CHECK(module.RecordReplayStatus({9, ACLPTI_SUCCESS}).status == ACLPTI_SUCCESS);
    CHECK(module.ReleaseReplay(9) == ACLPTI_SUCCESS);
    CHECK(module.Shutdown() == ACLPTI_SUCCESS);
    return 0;
}

int TestProcessCallbackRegistration()
{
    data::Module standalone;
    CHECK(standalone.Initialize() == ACLPTI_SUCCESS);
    CHECK(standalone.Shutdown() == ACLPTI_SUCCESS);

    ResultSink sink;
    data::Module module;
    CHECK(aclptiRegisterPmuDataCallback({}) == ACLPTI_ERROR_INVALID_PARAMETER);
    CHECK(aclptiRegisterPmuDataCallback([&sink](const auto& result) { return sink.Accept(result); }) == ACLPTI_SUCCESS);
    CHECK(aclptiRegisterPmuDataCallback([](const auto&) { return ACLPTI_SUCCESS; }) == ACLPTI_ERROR_INVALID_STATE);

    CHECK(module.Initialize() == ACLPTI_SUCCESS);
    const data::ReplayPrepareInfo replayInfo{99, "Registered", PmuEvents({0x8})};
    CHECK(module.PrepareReplay(replayInfo) == ACLPTI_SUCCESS);
    CHECK(module.RecordReplayStatus({99, ACLPTI_SUCCESS}).status == ACLPTI_SUCCESS);
    CHECK(module.ReleaseReplay(99) == ACLPTI_SUCCESS);
    CHECK(module.Shutdown() == ACLPTI_SUCCESS);

    const auto result = sink.Wait();
    CHECK(result != nullptr);
    CHECK(result->status == ACLPTI_SUCCESS);
    CHECK(result->pmuLogs.empty());
    return 0;
}

int TestShutdownCallback()
{
    static std::atomic<int> shutdownCalls{0};
    shutdownCalls.store(0);
    data::Module module([](const auto&) { return ACLPTI_SUCCESS; });
    CHECK(
        aclptiRegisterDataModuleShutdownCallback(
            [](void* userData) {
                auto* calls = static_cast<std::atomic<int>*>(userData);
                ++(*calls);
                return ACLPTI_SUCCESS;
            },
            &shutdownCalls) == ACLPTI_SUCCESS);
    CHECK(module.Initialize() == ACLPTI_SUCCESS);
    CHECK(module.PrepareReplay({100, "Shutdown", PmuEvents({0x8})}) == ACLPTI_SUCCESS);
    CHECK(module.RecordReplayStatus({100, ACLPTI_SUCCESS}).status == ACLPTI_SUCCESS);
    CHECK(module.ReleaseReplay(100) == ACLPTI_SUCCESS);
    CHECK(module.Shutdown() == ACLPTI_SUCCESS);
    CHECK(shutdownCalls.load() == 1);
    CHECK(module.Shutdown() == ACLPTI_SUCCESS);
    CHECK(shutdownCalls.load() == 1);

    ResultSink sink;
    data::Module emptyModule([&sink](const auto& result) { return sink.Accept(result); });
    CHECK(emptyModule.Initialize() == ACLPTI_SUCCESS);
    CHECK(emptyModule.Shutdown() == ACLPTI_SUCCESS);
    CHECK(shutdownCalls.load() == 2);
    CHECK(sink.Count() == 0);
    return 0;
}

int TestCrossReplayAggregate()
{
    ResultSink sink;
    data::Module module([&sink](const auto& result) { return sink.Accept(result); });
    CHECK(module.Initialize() == ACLPTI_SUCCESS);

    const auto firstEvents = PmuEvents({0x10, 0x20});
    CHECK(module.PrepareReplay({201, "First", firstEvents}) == ACLPTI_SUCCESS);
    auto firstPmu = PmuRecord(7, 8, 2, 10);
    StoreWord(firstPmu.data(), 2, 100);
    StoreWord(firstPmu.data(), 5, 1U << 8U);
    StoreWord(firstPmu.data(), 6, (2U << 16U) | 3U);
    StoreWord(firstPmu.data(), 28, 0x1111);
    StoreWord(firstPmu.data(), 30, 0x2222);
    auto firstRaw = RawData(firstPmu, PMU_DATA_TYPE);
    auto taskStart = RawData(TaskLog(0x00, 7, 8, 1, 0, 0, ACLPTI_CORE_TYPE_AIC, 0), LOG_DATA_TYPE);
    auto blockStart = RawData(TaskLog(0x24, 7, 8, 2, 2, 3, ACLPTI_CORE_TYPE_AIC, 1), LOG_DATA_TYPE);
    CHECK(module.GetRawDataCallback()(&firstRaw) == 0);
    CHECK(module.GetRawDataCallback()(&taskStart) == 0);
    CHECK(module.GetRawDataCallback()(&blockStart) == 0);
    CHECK(module.RecordReplayStatus({201, ACLPTI_SUCCESS}).status == ACLPTI_SUCCESS);
    CHECK(module.ReleaseReplay(201) == ACLPTI_SUCCESS);

    const auto secondEvents = PmuEvents({0x20, 0x30});
    CHECK(module.PrepareReplay({202, "Second", secondEvents}) == ACLPTI_SUCCESS);
    auto secondPmu = PmuRecord(9, 10, 2, 30);
    StoreWord(secondPmu.data(), 2, 300);
    StoreWord(secondPmu.data(), 5, (2U << 8U) | 1U);
    StoreWord(secondPmu.data(), 6, (2U << 16U) | 3U);
    StoreWord(secondPmu.data(), 28, 0x3333);
    StoreWord(secondPmu.data(), 30, 0x4444);
    auto secondRaw = RawData(secondPmu, PMU_DATA_TYPE);
    auto secondBlockStart = RawData(TaskLog(0x24, 9, 10, 3, 2, 3, ACLPTI_CORE_TYPE_AIV, 2), LOG_DATA_TYPE);
    CHECK(module.GetRawDataCallback()(&secondRaw) == 0);
    CHECK(module.GetRawDataCallback()(&secondBlockStart) == 0);
    CHECK(module.RecordReplayStatus({202, ACLPTI_SUCCESS}).status == ACLPTI_SUCCESS);
    CHECK(module.ReleaseReplay(202) == ACLPTI_SUCCESS);

    CHECK(module.Shutdown() == ACLPTI_SUCCESS);
    CHECK(sink.Count() == 1);
    const auto result = sink.Wait();
    CHECK(result != nullptr);
    CHECK(result->status == ACLPTI_SUCCESS);
    CHECK(result->taskLogs.at(7).size() == 1);
    CHECK(result->blockLogs.at(aclptiBlockKey{2, 3}).size() == 2);
    CHECK(result->pmuLogs.size() == 2);
    const auto& aicRow = result->pmuLogs.at(aclptiBlockKey{2, 3, ACLPTI_CORE_TYPE_AIC, 1});
    CHECK(aicRow.totalCycles == 100.0);
    CHECK(aicRow.values.at(0x10) == 10.0);
    CHECK(aicRow.values.at(0x20) == 11.0);
    CHECK(aicRow.systemCounters.size() == 1);
    CHECK(aicRow.systemCounters[0].taskStartSystemCounter == 0x1111);
    CHECK(aicRow.systemCounters[0].taskEndSystemCounter == 0x2222);
    CHECK(aicRow.coreInfos.size() == 1);
    CHECK(aicRow.coreInfos[0].coreType == ACLPTI_CORE_TYPE_AIC);
    CHECK(aicRow.coreInfos[0].coreId == 1);
    CHECK(aicRow.coreInfos[0].count == 1);

    const auto& aivRow = result->pmuLogs.at(aclptiBlockKey{2, 3, ACLPTI_CORE_TYPE_AIV, 2});
    CHECK(aivRow.totalCycles == 300.0);
    CHECK(aivRow.values.at(0x20) == 30.0);
    CHECK(aivRow.values.at(0x30) == 31.0);
    CHECK(aivRow.systemCounters.size() == 1);
    CHECK(aivRow.systemCounters[0].taskStartSystemCounter == 0x3333);
    CHECK(aivRow.systemCounters[0].taskEndSystemCounter == 0x4444);
    CHECK(aivRow.coreInfos.size() == 1);
    CHECK(aivRow.coreInfos[0].coreType == ACLPTI_CORE_TYPE_AIV);
    CHECK(aivRow.coreInfos[0].coreId == 2);
    CHECK(aivRow.coreInfos[0].count == 1);
    return 0;
}

int TestFailedPackageIsDropped()
{
    constexpr std::size_t kMalformedRecordCount = 5000;
    ResultSink sink;
    data::Module module([&sink](const auto& result) { return sink.Accept(result); });
    CHECK(module.Initialize() == ACLPTI_SUCCESS);
    CHECK(module.PrepareReplay({303, "PartialFailure", PmuEvents({0x55})}) == ACLPTI_SUCCESS);

    auto valid = PmuRecord(1, 2, 8, 77);
    StoreWord(valid.data(), 6, (8U << 16U) | 9U);
    auto malformed = valid;
    StoreWord(malformed.data(), 0, 0x0000002aU);
    auto validRaw = RawData(valid, PMU_DATA_TYPE);
    auto malformedRaw = RawData(malformed, PMU_DATA_TYPE);
    const MsprofRawDataCallback callback = module.GetRawDataCallback();
    CHECK(callback != nullptr);
    CHECK(callback(&validRaw) == 0);
    for (std::size_t index = 0; index < kMalformedRecordCount; ++index) {
        malformedRaw.offset = static_cast<uint64_t>(index * malformedRaw.chunkSize);
        CHECK(callback(&malformedRaw) == 0);
    }
    CHECK(module.RecordReplayStatus({303, ACLPTI_SUCCESS}).status == ACLPTI_SUCCESS);
    CHECK(module.ReleaseReplay(303) == ACLPTI_SUCCESS);
    CHECK(module.Shutdown() == ACLPTI_SUCCESS);

    CHECK(sink.Count() == 1);
    const auto result = sink.Wait();
    CHECK(result != nullptr);
    CHECK(result->status == ACLPTI_SUCCESS);
    CHECK(result->errorStats.failedRecordCount == kMalformedRecordCount);
    CHECK(result->errorStats.failedRecordCountByReplay.at(303) == kMalformedRecordCount);
    CHECK(result->pmuLogs.at(aclptiBlockKey{8, 9, ACLPTI_CORE_TYPE_AIC, 3}).values.at(0x55) == 77.0);
    return 0;
}

int TestRawQueueBackpressure()
{
    ResultSink sink;
    data::Module module([&sink](const auto& result) { return sink.Accept(result); });
    CHECK(module.Initialize() == ACLPTI_SUCCESS);
    CHECK(module.PrepareReplay({404, "Backpressure", PmuEvents({0x55})}) == ACLPTI_SUCCESS);

    const MsprofRawDataCallback callback = module.GetRawDataCallback();
    CHECK(callback != nullptr);

    auto payload = PmuRecord(1, 2, 8, 77);
    auto raw = RawData(payload, PMU_DATA_TYPE);
    for (std::size_t index = 0; index < 1500; ++index) {
        raw.offset = static_cast<uint64_t>(index * raw.chunkSize);
        CHECK(callback(&raw) == 0);
    }

    CHECK(module.RecordReplayStatus({404, ACLPTI_SUCCESS}).status == ACLPTI_SUCCESS);
    CHECK(module.ReleaseReplay(404) == ACLPTI_SUCCESS);
    CHECK(module.Shutdown() == ACLPTI_SUCCESS);

    CHECK(sink.Count() == 1);
    const auto result = sink.Wait();
    CHECK(result != nullptr);
    CHECK(result->status == ACLPTI_SUCCESS);
    CHECK(result->errorStats.failedRecordCount == 0);
    CHECK(result->pmuLogs.at(aclptiBlockKey{8, 0, ACLPTI_CORE_TYPE_AIC, 3}).values.at(0x55) == 77.0);
    return 0;
}

int main()
{
    if (TestRawDataDecoder() != 0) {
        return 1;
    }
    if (TestReplayLifecycle() != 0) {
        return 1;
    }
    if (TestFailedReplay() != 0) {
        return 1;
    }
    if (TestForceShutdownActiveReplay() != 0) {
        return 1;
    }
    if (TestInvalidChunkDiagnostics() != 0) {
        return 1;
    }
    if (TestProcessCallbackRegistration() != 0) {
        return 1;
    }
    if (TestShutdownCallback() != 0) {
        return 1;
    }
    if (TestCrossReplayAggregate() != 0) {
        return 1;
    }
    if (TestFailedPackageIsDropped() != 0) {
        return 1;
    }
    return TestRawQueueBackpressure();
}
