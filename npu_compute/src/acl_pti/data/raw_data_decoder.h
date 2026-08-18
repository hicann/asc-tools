/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_SRC_ACL_PTI_DATA_RAW_DATA_DECODER_H_
#define NPU_COMPUTE_SRC_ACL_PTI_DATA_RAW_DATA_DECODER_H_

#include "acl_pti/data/module.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <variant>

namespace npu_compute::aclpti::data::detail {

template <typename T>
class ResultOr {
public:
    explicit ResultOr(aclptiResult status) : status_(status) {}
    explicit ResultOr(T value) : value_(std::move(value)) {}

    bool Ok() const { return status_ == ACLPTI_SUCCESS && value_.has_value(); }
    aclptiResult Status() const { return status_; }
    const T& Value() const { return value_.value(); }

private:
    aclptiResult status_ = ACLPTI_SUCCESS;
    std::optional<T> value_;
};

struct TaskLog32 {
    std::uint8_t funcType;
    std::uint16_t taskId;
    std::uint16_t rtStreamId;
    std::uint64_t systemCounter;
    std::uint16_t blockId;
    std::uint16_t subBlockId;
    aclptiCoreType coreType;
    std::uint8_t coreTypeId;
};

struct PmuRecord128 {
    std::uint16_t taskId;
    std::uint16_t rtStreamId;
    std::uint64_t totalCycles;
    std::uint64_t taskStartSystemCounter;
    std::uint64_t taskEndSystemCounter;
    bool overflow;
    aclptiCoreType coreType;
    std::uint8_t coreId;
    std::uint16_t blockId;
    std::uint16_t subBlockId;
    std::map<std::uint32_t, double> pmuValues;
};

using DecodedPayload = std::variant<TaskLog32, PmuRecord128>;

struct DecodedRecord {
    std::uint64_t recordIndex;
    DecodedPayload payload;
};

ResultOr<DecodedRecord> DecodeRawRecord(const std::byte* data, std::size_t size, std::uint64_t recordIndex);

ResultOr<DecodedRecord> DecodeRawRecord(
    const std::byte* data, std::size_t size, std::uint64_t recordIndex, const PmuSlots& pmuEventIds);

} // namespace npu_compute::aclpti::data::detail

#endif // NPU_COMPUTE_SRC_ACL_PTI_DATA_RAW_DATA_DECODER_H_
