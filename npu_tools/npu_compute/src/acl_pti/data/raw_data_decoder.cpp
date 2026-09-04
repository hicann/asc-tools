/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "raw_data_decoder.h"

#include <map>

namespace npu_compute::aclpti::data::detail {
namespace {

constexpr std::size_t kTaskLogSize = 32;
constexpr std::size_t kPmuRecordSize = 128;
constexpr uint32_t kPmuMagic = 0x6bd3U;
constexpr uint32_t kTaskStartFunction = 0x00U;
constexpr uint32_t kTaskEndFunction = 0x01U;
constexpr uint32_t kBlockStartFunction = 0x24U;
constexpr uint32_t kBlockEndFunction = 0x25U;

uint32_t Word(const std::byte* data, std::size_t index)
{
    const std::byte* bytes = data + index * sizeof(uint32_t);
    uint32_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value |= uint32_t(std::to_integer<uint8_t>(bytes[index])) << (index * 8U);
    }
    return value;
}

uint64_t Counter(const std::byte* data, std::size_t lowWord)
{
    return Word(data, lowWord) | (uint64_t(Word(data, lowWord + 1)) << 32U);
}

} // namespace

ResultOr<DecodedRecord> DecodeRawRecord(const std::byte* data, std::size_t size, uint64_t recordIndex)
{
    PmuSlots noEvents{};
    noEvents.fill(kInvalidPmuEvent);
    return DecodeRawRecord(data, size, recordIndex, noEvents);
}

ResultOr<DecodedRecord> DecodeRawRecord(
    const std::byte* data, std::size_t size, uint64_t recordIndex, const PmuSlots& pmuEventIds)
{
    if (data == nullptr || (size != kTaskLogSize && size != kPmuRecordSize)) {
        return ResultOr<DecodedRecord>(ACLPTI_ERROR_INVALID_RAW_DATA);
    }

    const uint32_t function = Word(data, 0) & 0x3fU;
    const uint32_t taskAndStream = Word(data, 1);
    const auto taskId = static_cast<uint16_t>(taskAndStream >> 16U);
    const auto streamId = static_cast<uint16_t>(taskAndStream);
    if (size == kTaskLogSize) {
        if ((Word(data, 0) >> 16U) != kPmuMagic || (function != kTaskStartFunction && function != kTaskEndFunction &&
                                                    function != kBlockStartFunction && function != kBlockEndFunction)) {
            return ResultOr<DecodedRecord>(ACLPTI_ERROR_DECODE);
        }

        TaskLog32 record{};
        record.funcType = static_cast<uint8_t>(function);
        record.taskId = taskId;
        record.rtStreamId = streamId;
        record.systemCounter = Counter(data, 2);
        if (function == kBlockStartFunction || function == kBlockEndFunction) {
            record.blockId = static_cast<uint16_t>(Word(data, 6) >> 16U);
            record.subBlockId = static_cast<uint16_t>(Word(data, 6));
            record.coreType = (Word(data, 5) & 1U) == 0 ? ACLPTI_CORE_TYPE_AIC : ACLPTI_CORE_TYPE_AIV;
            record.coreTypeId = static_cast<uint8_t>((Word(data, 5) >> 1U) & 0x7fU);
        }
        return ResultOr<DecodedRecord>(DecodedRecord{recordIndex, record});
    }

    if ((Word(data, 0) >> 16U) != kPmuMagic ||
        (function != kBlockPmuFunctionType && function != kTaskPmuFunctionType)) {
        return ResultOr<DecodedRecord>(ACLPTI_ERROR_DECODE);
    }

    PmuRecord128 record{};
    record.funcType = static_cast<uint8_t>(function);
    record.taskId = taskId;
    record.rtStreamId = streamId;
    record.totalCycles = Counter(data, 2);
    record.taskStartSystemCounter = Counter(data, 28);
    record.taskEndSystemCounter = Counter(data, 30);
    record.overflow = (Word(data, 4) & (1U << 10U)) != 0;
    record.coreType = (Word(data, 5) & 1U) == 0 ? ACLPTI_CORE_TYPE_AIC : ACLPTI_CORE_TYPE_AIV;
    record.coreId = static_cast<uint8_t>(Word(data, 5) >> 8U);
    record.blockId = static_cast<uint16_t>(Word(data, 6) >> 16U);
    record.subBlockId = static_cast<uint16_t>(Word(data, 6));
    struct EventAccumulator {
        long double sum = 0.0L;
        std::size_t count = 0;
    };
    std::map<uint32_t, EventAccumulator> eventValues;
    for (std::size_t index = 0; index < kMaxPmuSlots; ++index) {
        const uint32_t eventId = pmuEventIds[index];
        if (eventId == kInvalidPmuEvent) {
            break;
        }
        auto& accumulator = eventValues[eventId];
        accumulator.sum += static_cast<long double>(Counter(data, 8 + index * 2));
        ++accumulator.count;
    }
    for (const auto& [eventId, accumulator] : eventValues) {
        record.pmuValues.emplace(
            eventId, static_cast<double>(accumulator.sum / static_cast<long double>(accumulator.count)));
    }
    return ResultOr<DecodedRecord>(DecodedRecord{recordIndex, record});
}

} // namespace npu_compute::aclpti::data::detail
