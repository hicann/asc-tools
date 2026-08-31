// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#ifndef NPU_CHECK_CHECKER_ALLOCATION_REGISTRY_H
#define NPU_CHECK_CHECKER_ALLOCATION_REGISTRY_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>

namespace npu::sanitizer {

struct Allocation {
    uint64_t resourceId = 0;
    uint64_t base = 0;
    uint64_t bytes = 0;
    uint32_t deviceId = 0;
    uint64_t allocSequence = 0;
    uint64_t freeSequence = 0;
};

enum class AllocationUpdateStatus : uint8_t {
    OK,
    INVALID_RANGE,
    OVERLAP,
    NOT_FOUND,
    DOUBLE_FREE,
    STALE_RESOURCE,
};

enum class RangeStatus : uint8_t {
    VALID,
    OUT_OF_BOUNDS,
    USE_AFTER_FREE,
    AMBIGUOUS,
    UNKNOWN,
    OVERFLOW,
};

struct RangeResult {
    RangeStatus status = RangeStatus::UNKNOWN;
    std::optional<Allocation> allocation;
};

class AllocationRegistry {
public:
    AllocationUpdateStatus Register(uint64_t resourceId, uint64_t base, uint64_t bytes, uint32_t deviceId);
    AllocationUpdateStatus Release(uint64_t resourceId, uint64_t base, uint32_t deviceId);
    RangeResult Classify(uint32_t deviceId, uint64_t address, uint64_t bytes) const;
    std::optional<Allocation> Nearest(uint32_t deviceId, uint64_t address) const;

private:
    using AllocationMap = std::map<uint64_t, Allocation>;
    using ResourceKey = std::pair<uint32_t, uint64_t>;

    static std::optional<uint64_t> RangeEnd(uint64_t base, uint64_t bytes);
    static bool Overlaps(uint64_t firstBase, uint64_t firstBytes, uint64_t secondBase, uint64_t secondBytes);
    static std::optional<Allocation> FindStartingAllocation(const AllocationMap& allocations, uint64_t address);
    static std::optional<Allocation> FindCrossedAllocation(
        const AllocationMap& allocations, uint64_t address, uint64_t end);
    static uint64_t DistanceToAllocation(const Allocation& allocation, uint64_t address);
    size_t TombstoneCount() const;
    void EraseReusedTombstones(uint64_t base, uint64_t bytes, uint32_t deviceId);
    void TrimTombstones();

    std::unordered_map<uint32_t, AllocationMap> live_;
    std::unordered_map<uint32_t, AllocationMap> tombstones_;
    std::map<ResourceKey, Allocation> liveByResource_;
    uint64_t nextSequence_ = 1;
    static constexpr size_t kMaxTombstones = 4096;
};

} // namespace npu::sanitizer

#endif
