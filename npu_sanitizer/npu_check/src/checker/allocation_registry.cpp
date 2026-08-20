// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "checker/allocation_registry.h"

#include <algorithm>
#include <limits>
#include <vector>

namespace npu::sanitizer {

std::optional<uint64_t> AllocationRegistry::RangeEnd(uint64_t base, uint64_t bytes)
{
    if (bytes == 0 || base > std::numeric_limits<uint64_t>::max() - bytes) {
        return std::nullopt;
    }
    return base + bytes;
}

bool AllocationRegistry::Overlaps(uint64_t firstBase, uint64_t firstBytes, uint64_t secondBase, uint64_t secondBytes)
{
    const auto firstEnd = RangeEnd(firstBase, firstBytes);
    const auto secondEnd = RangeEnd(secondBase, secondBytes);
    if (!firstEnd || !secondEnd) {
        return true;
    }
    return firstBase < *secondEnd && secondBase < *firstEnd;
}

AllocationUpdateStatus AllocationRegistry::Register(
    uint64_t resourceId, uint64_t base, uint64_t bytes, uint32_t deviceId)
{
    if (base == 0 || !RangeEnd(base, bytes)) {
        return AllocationUpdateStatus::INVALID_RANGE;
    }
    const ResourceKey resourceKey{deviceId, resourceId};
    auto resource = liveByResource_.find(resourceKey);
    if (resourceId != 0 && resource != liveByResource_.end()) {
        return AllocationUpdateStatus::OVERLAP;
    }

    auto& deviceAllocations = live_[deviceId];
    auto next = deviceAllocations.lower_bound(base);
    if (next != deviceAllocations.end() && Overlaps(base, bytes, next->second.base, next->second.bytes)) {
        return AllocationUpdateStatus::OVERLAP;
    }
    if (next != deviceAllocations.begin()) {
        const auto previous = std::prev(next);
        if (Overlaps(base, bytes, previous->second.base, previous->second.bytes)) {
            return AllocationUpdateStatus::OVERLAP;
        }
    }

    EraseReusedTombstones(base, bytes, deviceId);
    Allocation allocation{};
    allocation.resourceId = resourceId;
    allocation.base = base;
    allocation.bytes = bytes;
    allocation.deviceId = deviceId;
    allocation.allocSequence = nextSequence_++;
    deviceAllocations.emplace(base, allocation);
    if (resourceId != 0) {
        liveByResource_[resourceKey] = allocation;
    }
    return AllocationUpdateStatus::OK;
}

AllocationUpdateStatus AllocationRegistry::Release(uint64_t resourceId, uint64_t base, uint32_t deviceId)
{
    std::optional<Allocation> selected;
    if (resourceId != 0) {
        const ResourceKey resourceKey{deviceId, resourceId};
        auto resource = liveByResource_.find(resourceKey);
        if (resource != liveByResource_.end()) {
            if (base != 0 && resource->second.base != base) {
                return AllocationUpdateStatus::STALE_RESOURCE;
            }
            selected = resource->second;
        } else {
            const auto device = tombstones_.find(deviceId);
            if (device != tombstones_.end()) {
                for (const auto& allocation : device->second) {
                    if (allocation.second.resourceId == resourceId) {
                        return AllocationUpdateStatus::DOUBLE_FREE;
                    }
                }
            }
            return AllocationUpdateStatus::NOT_FOUND;
        }
    }
    if (resourceId == 0 && !selected.has_value()) {
        auto device = live_.find(deviceId);
        if (device != live_.end()) {
            auto allocation = device->second.find(base);
            if (allocation != device->second.end()) {
                selected = allocation->second;
            }
        }
    }
    if (!selected.has_value()) {
        auto device = tombstones_.find(deviceId);
        if (device != tombstones_.end() && device->second.find(base) != device->second.end()) {
            return AllocationUpdateStatus::DOUBLE_FREE;
        }
        return AllocationUpdateStatus::NOT_FOUND;
    }

    auto& deviceAllocations = live_[selected->deviceId];
    deviceAllocations.erase(selected->base);
    if (selected->resourceId != 0) {
        liveByResource_.erase(ResourceKey{selected->deviceId, selected->resourceId});
    }
    selected->freeSequence = nextSequence_++;
    tombstones_[selected->deviceId][selected->base] = *selected;
    TrimTombstones();
    return AllocationUpdateStatus::OK;
}

std::optional<Allocation> AllocationRegistry::FindStartingAllocation(const AllocationMap& allocations, uint64_t address)
{
    auto upper = allocations.upper_bound(address);
    if (upper == allocations.begin()) {
        return std::nullopt;
    }
    const auto candidate = std::prev(upper);
    const auto end = RangeEnd(candidate->second.base, candidate->second.bytes);
    if (end && address < *end) {
        return candidate->second;
    }
    return std::nullopt;
}

std::optional<Allocation> AllocationRegistry::FindCrossedAllocation(
    const AllocationMap& allocations, uint64_t address, uint64_t end)
{
    const auto next = allocations.lower_bound(address);
    if (next != allocations.end() && next->second.base < end) {
        return next->second;
    }
    return std::nullopt;
}

uint64_t AllocationRegistry::DistanceToAllocation(const Allocation& allocation, uint64_t address)
{
    if (address < allocation.base) {
        return allocation.base - address;
    }
    const auto end = RangeEnd(allocation.base, allocation.bytes);
    if (end && address >= *end) {
        return address - *end;
    }
    return 0;
}

RangeResult AllocationRegistry::Classify(uint32_t deviceId, uint64_t address, uint64_t bytes) const
{
    if (bytes == 0) {
        return {RangeStatus::VALID, std::nullopt};
    }
    if (address > std::numeric_limits<uint64_t>::max() - bytes) {
        return {RangeStatus::OVERFLOW, std::nullopt};
    }
    const uint64_t end = address + bytes;
    std::optional<Allocation> liveValid;
    std::optional<Allocation> liveOutOfBounds;
    std::optional<Allocation> freed;

    const auto liveDevice = live_.find(deviceId);
    if (liveDevice != live_.end()) {
        const auto allocation = FindStartingAllocation(liveDevice->second, address);
        if (allocation.has_value()) {
            const auto allocationEnd = RangeEnd(allocation->base, allocation->bytes);
            if (allocationEnd && end <= *allocationEnd) {
                liveValid = allocation;
            } else {
                liveOutOfBounds = allocation;
            }
        } else {
            auto crossed = FindCrossedAllocation(liveDevice->second, address, end);
            if (crossed.has_value()) {
                liveOutOfBounds = crossed;
            } else {
                auto upper = liveDevice->second.lower_bound(address);
                if (upper != liveDevice->second.begin()) {
                    const auto previous = std::prev(upper);
                    const auto previousEnd = RangeEnd(previous->second.base, previous->second.bytes);
                    if (previousEnd && *previousEnd == address) {
                        liveOutOfBounds = previous->second;
                    }
                }
            }
        }
    }

    const auto tombstoneDevice = tombstones_.find(deviceId);
    if (tombstoneDevice != tombstones_.end()) {
        auto allocation = FindStartingAllocation(tombstoneDevice->second, address);
        if (!allocation.has_value()) {
            allocation = FindCrossedAllocation(tombstoneDevice->second, address, end);
        }
        if (allocation.has_value()) {
            freed = allocation;
        }
    }

    const unsigned matchedStates = static_cast<unsigned>(liveValid.has_value()) +
                                   static_cast<unsigned>(liveOutOfBounds.has_value()) +
                                   static_cast<unsigned>(freed.has_value());
    if (matchedStates > 1) {
        const auto representative =
            liveOutOfBounds.has_value() ? liveOutOfBounds : (freed.has_value() ? freed : liveValid);
        return {RangeStatus::AMBIGUOUS, representative};
    }
    if (liveValid.has_value()) {
        return {RangeStatus::VALID, liveValid};
    }
    if (liveOutOfBounds.has_value()) {
        return {RangeStatus::OUT_OF_BOUNDS, liveOutOfBounds};
    }
    if (freed.has_value()) {
        return {RangeStatus::USE_AFTER_FREE, freed};
    }
    return {RangeStatus::UNKNOWN, std::nullopt};
}

std::optional<Allocation> AllocationRegistry::Nearest(uint32_t deviceId, uint64_t address) const
{
    const auto device = live_.find(deviceId);
    if (device == live_.end() || device->second.empty()) {
        return std::nullopt;
    }

    const auto next = device->second.lower_bound(address);
    if (next == device->second.begin()) {
        return next->second;
    }
    const auto previous = std::prev(next);
    if (next == device->second.end() ||
        DistanceToAllocation(previous->second, address) <= DistanceToAllocation(next->second, address)) {
        return previous->second;
    }
    return next->second;
}

size_t AllocationRegistry::TombstoneCount() const
{
    size_t count = 0;
    for (const auto& device : tombstones_) {
        count += device.second.size();
    }
    return count;
}

void AllocationRegistry::EraseReusedTombstones(uint64_t base, uint64_t bytes, uint32_t deviceId)
{
    auto device = tombstones_.find(deviceId);
    if (device == tombstones_.end()) {
        return;
    }
    std::vector<Allocation> preserved;
    const auto reusedEnd = RangeEnd(base, bytes);
    if (!reusedEnd) {
        return;
    }
    for (auto it = device->second.begin(); it != device->second.end();) {
        if (Overlaps(base, bytes, it->second.base, it->second.bytes)) {
            const Allocation original = it->second;
            const auto originalEnd = RangeEnd(original.base, original.bytes);
            if (!originalEnd) {
                it = device->second.erase(it);
                continue;
            }
            if (original.base < base) {
                Allocation left = original;
                left.bytes = base - original.base;
                preserved.push_back(left);
            }
            if (*reusedEnd < *originalEnd) {
                Allocation right = original;
                right.base = *reusedEnd;
                right.bytes = *originalEnd - *reusedEnd;
                preserved.push_back(right);
            }
            it = device->second.erase(it);
        } else {
            ++it;
        }
    }
    for (const auto& allocation : preserved) {
        device->second[allocation.base] = allocation;
    }
}

void AllocationRegistry::TrimTombstones()
{
    while (TombstoneCount() > kMaxTombstones) {
        auto oldestDevice = tombstones_.end();
        AllocationMap::iterator oldestAllocation;
        uint64_t oldestSequence = std::numeric_limits<uint64_t>::max();
        for (auto device = tombstones_.begin(); device != tombstones_.end(); ++device) {
            for (auto allocation = device->second.begin(); allocation != device->second.end(); ++allocation) {
                if (allocation->second.freeSequence < oldestSequence) {
                    oldestSequence = allocation->second.freeSequence;
                    oldestDevice = device;
                    oldestAllocation = allocation;
                }
            }
        }
        if (oldestDevice == tombstones_.end()) {
            return;
        }
        oldestDevice->second.erase(oldestAllocation);
    }
}

} // namespace npu::sanitizer
