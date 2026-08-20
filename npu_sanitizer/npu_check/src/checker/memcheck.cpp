// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "checker/memcheck.h"

#include <chrono>
#include <iterator>
#include <limits>
#include <optional>
#include <utility>

namespace npu::sanitizer {
namespace {

using aclsan::cann::NpusanMemcheckPattern;
using aclsan::cann::NpusanMemcheckReport;
using aclsan::cann::NpusanReportAccessMode;
using aclsan::cann::NpusanReportAllocation;
using aclsan::cann::NpusanReportDistanceKind;
using aclsan::cann::NpusanReportExecContext;
using aclsan::cann::NpusanReportMemorySpace;
using aclsan::cann::ReportSeverity;
using aclsan::cann::ReportTool;

constexpr uint32_t kAllocationStateLive = 1;
constexpr uint32_t kAllocationStateFreed = 2;

uint64_t TimestampNs()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

std::optional<uint64_t> RangeEnd(uint64_t base, uint64_t bytes)
{
    if (base > std::numeric_limits<uint64_t>::max() - bytes) {
        return std::nullopt;
    }
    return base + bytes;
}

const char* SourceKindName(uint32_t sourceKind)
{
    switch (sourceKind) {
        case static_cast<uint32_t>(DeviceSourceKind::MTE2):
            return "MTE2";
        case static_cast<uint32_t>(DeviceSourceKind::MTE3):
            return "MTE3";
        case static_cast<uint32_t>(DeviceSourceKind::FIXPIPE):
            return "FIXPIPE";
        case static_cast<uint32_t>(DeviceSourceKind::SET_WAIT_FLAG):
            return "SET_WAIT_FLAG";
        case static_cast<uint32_t>(DeviceSourceKind::GET_RLS_BUF):
            return "GET_RLS_BUF";
        default:
            return "UNKNOWN";
    }
}

NpusanReportAllocation ToReportAllocation(const std::optional<Allocation>& allocation)
{
    NpusanReportAllocation result{};
    if (!allocation) {
        return result;
    }
    result.allocId = allocation->resourceId;
    result.base = allocation->base;
    result.bytes = allocation->bytes;
    result.allocSerialNo = allocation->allocSequence;
    result.freeSerialNo = allocation->freeSequence;
    result.memorySpace = NpusanReportMemorySpace::kGm;
    result.deviceId = allocation->deviceId;
    result.state = allocation->freeSequence == 0 ? kAllocationStateLive : kAllocationStateFreed;
    return result;
}

NpusanReportExecContext ToExecContext(const AclsanDeviceMemoryAccessData& data)
{
    NpusanReportExecContext exec{};
    exec.launchId = data.header.launchId;
    exec.instrExecId = data.header.instrExecId;
    exec.serialNo = data.header.serialNo;
    exec.pc = data.header.pc;
    exec.deviceId = data.header.deviceId;
    exec.coreId = data.header.coreId;
    exec.blockId = data.header.blockId;
    exec.blockType = data.header.blockType;
    exec.pipeId = data.header.pipeline;
    exec.siteId = data.header.siteId;
    exec.pipeName = SourceKindName(data.header.sourceKind);
    return exec;
}

int64_t ClampDistance(uint64_t distance)
{
    constexpr uint64_t kMaximum = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    return static_cast<int64_t>(distance > kMaximum ? kMaximum : distance);
}

void SetDistance(
    uint64_t address, uint64_t bytes, const std::optional<Allocation>& allocation, NpusanMemcheckReport& report)
{
    if (!allocation) {
        return;
    }
    const auto allocationEnd = RangeEnd(allocation->base, allocation->bytes);
    const auto accessEnd = RangeEnd(address, bytes);
    if (!allocationEnd) {
        return;
    }
    if (address < allocation->base) {
        report.distanceKind = NpusanReportDistanceKind::kBefore;
        report.distanceBytes = ClampDistance(allocation->base - address);
        return;
    }
    if (!accessEnd) {
        report.distanceKind = NpusanReportDistanceKind::kAfter;
        report.distanceBytes = std::numeric_limits<int64_t>::max();
        return;
    }
    if (address >= *allocationEnd) {
        report.distanceKind = NpusanReportDistanceKind::kAfter;
        report.distanceBytes = ClampDistance(address - *allocationEnd);
        return;
    }
    if (*accessEnd > *allocationEnd) {
        report.distanceKind = NpusanReportDistanceKind::kAfter;
        report.distanceBytes = ClampDistance(*accessEnd - *allocationEnd);
        return;
    }
    report.distanceKind = NpusanReportDistanceKind::kInside;
}

} // namespace

Memcheck::Memcheck(bool strictUnknown) : strictUnknown_(strictUnknown) {}

void Memcheck::OnAllocation(const AclsanResourceData& data)
{
    if (data.memorySpace != kDeviceMemorySpaceGm || data.common.result != 0) {
        return;
    }
    const auto status =
        allocations_.Register(data.resourceId, reinterpret_cast<uint64_t>(data.ptr), data.bytes, data.deviceId);
    if (status == AllocationUpdateStatus::OK) {
        ++stats_.allocations;
    }
}

void Memcheck::OnFree(const AclsanResourceData& data)
{
    if (data.memorySpace != kDeviceMemorySpaceGm || data.common.result != 0) {
        return;
    }
    const auto status = allocations_.Release(data.resourceId, reinterpret_cast<uint64_t>(data.ptr), data.deviceId);
    if (status == AllocationUpdateStatus::OK) {
        ++stats_.frees;
    }
}

std::vector<NpusanMemcheckReport> Memcheck::CheckAccess(
    const AclsanDeviceMemoryAccessData& data, NpusanReportAccessMode accessMode, uint64_t address, uint64_t bytes)
{
    const RangeResult range = allocations_.Classify(data.header.deviceId, address, bytes);
    if (range.status == RangeStatus::VALID || (range.status == RangeStatus::UNKNOWN && !strictUnknown_)) {
        return {};
    }

    NpusanMemcheckReport report{};
    report.common.reportId = nextReportId_++;
    report.common.groupId = data.header.instrExecId;
    report.common.timestampNs = TimestampNs();
    report.common.tool = ReportTool::kMemcheck;
    report.common.severity = ReportSeverity::kError;
    report.common.flags = aclsan::cann::kNpusanReportCommonHasExecContext;
    report.common.exec = ToExecContext(data);
    report.common.pattern = static_cast<uint32_t>(
        range.status == RangeStatus::USE_AFTER_FREE ? NpusanMemcheckPattern::kUseAfterFree :
                                                      NpusanMemcheckPattern::kInvalidAccess);

    report.access.memorySpace = NpusanReportMemorySpace::kGm;
    report.access.accessMode = accessMode;
    report.access.accessBytes = bytes > std::numeric_limits<uint32_t>::max() ? std::numeric_limits<uint32_t>::max() :
                                                                               static_cast<uint32_t>(bytes);
    report.access.requiredAlign = data.alignSize;
    report.access.address = address;
    report.access.rangeBegin = address;
    report.access.rangeEnd = RangeEnd(address, bytes).value_or(std::numeric_limits<uint64_t>::max());

    report.allocation = ToReportAllocation(range.allocation);
    const auto nearest = range.allocation ? range.allocation : allocations_.Nearest(data.header.deviceId, address);
    report.nearestAllocation = ToReportAllocation(nearest);
    SetDistance(address, bytes, nearest, report);
    return {std::move(report)};
}

void Memcheck::QueueDeviceMemoryAccess(const AclsanDeviceMemoryAccessData& data)
{
    if (data.memorySpace != ACLSAN_DEVICE_MEMORY_SPACE_GM) {
        return;
    }
    ++stats_.deviceOperations;
    if (pendingDeviceAccesses_.size() >= kMaxPendingDeviceOperations) {
        ++stats_.droppedDeviceOperations;
        return;
    }
    pendingDeviceAccesses_.push_back(data);
    stats_.pendingDeviceOperations = pendingDeviceAccesses_.size();
}

std::vector<NpusanMemcheckReport> Memcheck::CheckDeviceMemoryAccess(const AclsanDeviceMemoryAccessData& data)
{
    if (data.header.version != ACLSAN_API_VERSION || data.header.size < sizeof(AclsanDeviceMemoryAccessData) ||
        data.accessCount == 0 || data.accessIndex >= data.accessCount) {
        return {};
    }

    std::vector<NpusanReportAccessMode> accessModes;
    switch (data.accessMode) {
        case ACLSAN_DEVICE_MEMORY_ACCESS_READ:
            accessModes.push_back(NpusanReportAccessMode::kRead);
            break;
        case ACLSAN_DEVICE_MEMORY_ACCESS_WRITE:
            accessModes.push_back(NpusanReportAccessMode::kWrite);
            break;
        case ACLSAN_DEVICE_MEMORY_ACCESS_READ_WRITE:
            accessModes.push_back(NpusanReportAccessMode::kRead);
            accessModes.push_back(NpusanReportAccessMode::kWrite);
            break;
        default:
            return {};
    }

    if (data.memorySpace != kDeviceMemorySpaceGm || ((data.header.flags & kDeviceEventFlagPredicated) != 0 &&
                                                     data.predicateMask0 == 0 && data.predicateMask1 == 0)) {
        return {};
    }

    std::vector<NpusanMemcheckReport> reports;
    auto appendAccess = [&](uint64_t address, uint64_t bytes) {
        for (const NpusanReportAccessMode accessMode : accessModes) {
            auto found = CheckAccess(data, accessMode, address, bytes);
            reports.insert(reports.end(), std::make_move_iterator(found.begin()), std::make_move_iterator(found.end()));
        }
    };

    constexpr uint64_t kMaxLayoutSegments = 1u << 20u;
    const auto addressWithOffset = [](uint64_t base, __int128 offset) -> std::optional<uint64_t> {
        if (offset < -static_cast<__int128>(base) ||
            offset > static_cast<__int128>(std::numeric_limits<uint64_t>::max() - base)) {
            return std::nullopt;
        }
        return static_cast<uint64_t>(static_cast<__int128>(base) + offset);
    };

    switch (data.layoutKind) {
        case ACLSAN_MEM_LAYOUT_SCALAR:
            if (data.layout.scalar.bytes == 0) {
                return {};
            }
            appendAccess(data.address, data.layout.scalar.bytes);
            break;
        case ACLSAN_MEM_LAYOUT_RANGE:
            if (data.layout.range.bytes == 0) {
                return {};
            }
            appendAccess(data.address, data.layout.range.bytes);
            break;
        case ACLSAN_MEM_LAYOUT_BLOCK_REPEAT: {
            const auto& layout = data.layout.blockRepeat;
            if (layout.blockNum == 0 || layout.blockSize == 0 || layout.repeatTimes == 0) {
                return {};
            }
            const uint64_t segmentCount = static_cast<uint64_t>(layout.blockNum) * layout.repeatTimes;
            if (segmentCount > kMaxLayoutSegments) {
                return {};
            }
            for (uint32_t repeat = 0; repeat < layout.repeatTimes; ++repeat) {
                for (uint32_t block = 0; block < layout.blockNum; ++block) {
                    const __int128 offset = static_cast<__int128>(repeat) * layout.repeatStride +
                                            static_cast<__int128>(block) * layout.blockStride;
                    const auto address = addressWithOffset(data.address, offset);
                    if (!address) {
                        return {};
                    }
                    appendAccess(*address, layout.blockSize);
                }
            }
            break;
        }
        case ACLSAN_MEM_LAYOUT_ND_AFFINE: {
            const auto& layout = data.layout.ndAffine;
            if (layout.rank == 0 || layout.rank > 5 || layout.elementBytes == 0) {
                return {};
            }
            uint64_t elementCount = 1;
            bool countOverflow = false;
            for (uint32_t dimension = 0; dimension < layout.rank; ++dimension) {
                if (layout.dims[dimension] == 0 ||
                    elementCount > std::numeric_limits<uint64_t>::max() / layout.dims[dimension]) {
                    countOverflow = true;
                    break;
                }
                elementCount *= layout.dims[dimension];
            }
            if (countOverflow) {
                return {};
            }

            const __int128 kAddressOffsetMin = -static_cast<__int128>(std::numeric_limits<uint64_t>::max());
            const __int128 kAddressOffsetMax = static_cast<__int128>(std::numeric_limits<uint64_t>::max());
            __int128 minOffset = 0;
            __int128 maxOffset = 0;
            bool offsetOverflow = false;
            for (uint32_t dimension = 0; dimension < layout.rank; ++dimension) {
                const __int128 extent = static_cast<__int128>(layout.dims[dimension] - 1) *
                                        static_cast<__int128>(layout.strides[dimension]);
                if (extent < 0) {
                    if (extent < kAddressOffsetMin - minOffset) {
                        offsetOverflow = true;
                        break;
                    }
                    minOffset += extent;
                } else {
                    if (extent > kAddressOffsetMax - maxOffset) {
                        offsetOverflow = true;
                        break;
                    }
                    maxOffset += extent;
                }
            }
            if (offsetOverflow) {
                return {};
            }

            if (elementCount <= kMaxLayoutSegments) {
                for (uint64_t linear = 0; linear < elementCount; ++linear) {
                    uint64_t index = linear;
                    __int128 offset = 0;
                    for (uint32_t dimension = layout.rank; dimension > 0; --dimension) {
                        const uint32_t current = dimension - 1;
                        const uint64_t coordinate = index % layout.dims[current];
                        index /= layout.dims[current];
                        const __int128 term = static_cast<__int128>(coordinate) * layout.strides[current];
                        if (term < 0) {
                            if (term < kAddressOffsetMin - offset) {
                                offsetOverflow = true;
                                break;
                            }
                        } else if (term > kAddressOffsetMax - offset) {
                            offsetOverflow = true;
                            break;
                        }
                        offset += term;
                    }
                    if (offsetOverflow) {
                        return {};
                    }
                    const auto address = addressWithOffset(data.address, offset);
                    if (!address) {
                        return {};
                    }
                    appendAccess(*address, layout.elementBytes);
                }
            } else {
                const auto first = addressWithOffset(data.address, minOffset);
                const auto last = addressWithOffset(data.address, maxOffset);
                if (!first || !last || *last < *first ||
                    *last - *first > std::numeric_limits<uint64_t>::max() - layout.elementBytes) {
                    return {};
                }
                appendAccess(*first, *last - *first + layout.elementBytes);
            }
            break;
        }
        default:
            return {};
    }

    return reports;
}

std::vector<NpusanMemcheckReport> Memcheck::OnSynchronization()
{
    ++stats_.synchronizationEvents;
    std::vector<AclsanDeviceMemoryAccessData> accesses;
    accesses.swap(pendingDeviceAccesses_);
    stats_.pendingDeviceOperations = 0;

    std::vector<NpusanMemcheckReport> reports;
    for (const auto& access : accesses) {
        auto found = CheckDeviceMemoryAccess(access);
        reports.insert(reports.end(), std::make_move_iterator(found.begin()), std::make_move_iterator(found.end()));
    }
    Count(reports);
    return reports;
}

MemcheckStats Memcheck::Stats() const
{
    MemcheckStats stats = stats_;
    stats.pendingDeviceOperations = pendingDeviceAccesses_.size();
    return stats;
}

void Memcheck::Count(const std::vector<NpusanMemcheckReport>& reports)
{
    for (const auto& report : reports) {
        if (report.common.severity == ReportSeverity::kError || report.common.severity == ReportSeverity::kFatal) {
            ++stats_.errors;
        } else if (report.common.severity == ReportSeverity::kWarning) {
            ++stats_.warnings;
        }
    }
}

} // namespace npu::sanitizer
