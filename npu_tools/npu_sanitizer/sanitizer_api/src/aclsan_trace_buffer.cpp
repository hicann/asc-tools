/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "internal/aclsan_trace_buffer.h"

#include "dbi/trace_buffer_abi.h"

#include <cstring>
#include <map>

namespace aclsan {
namespace {

struct TraceBlockKey {
    uint32_t blockType;
    uint32_t blockId;

    bool operator<(const TraceBlockKey& other) const noexcept
    {
        return blockType < other.blockType || (blockType == other.blockType && blockId < other.blockId);
    }
};

} // namespace

bool InitializeTraceBuffer(
    std::vector<uint8_t>& buffer, uint32_t physicalCoreCount, uint32_t blockCount, uint32_t recordsPerCore,
    uint64_t launchId, std::string& error)
{
    size_t bytes = 0;
    if (blockCount == 0U || !aclsan::TraceBufferBytes(physicalCoreCount, recordsPerCore, &bytes)) {
        buffer.clear();
        error = "trace buffer dimensions are zero or overflow the layout";
        return false;
    }

    buffer.assign(bytes, 0);
    aclsan::AclsanTraceBufferHeader header{};
    header.magic = aclsan::ASCSAN_TRACE_BUFFER_MAGIC;
    header.launchId = launchId;
    header.segmentBytes = static_cast<uint64_t>(bytes);
    header.blockCount = blockCount;
    header.recordsPerCore = recordsPerCore;
    header.physicalCoreCount = physicalCoreCount;
    std::memcpy(buffer.data(), &header, sizeof(header));

    size_t sliceBytes = 0;
    (void)aclsan::TraceSliceBytes(recordsPerCore, &sliceBytes);
    const aclsan::AclsanTraceSliceHeader emptySlice{};
    for (uint32_t sliceIndex = 0; sliceIndex < physicalCoreCount; ++sliceIndex) {
        const size_t sliceOffset = sizeof(header) + static_cast<size_t>(sliceIndex) * sliceBytes;
        std::memcpy(buffer.data() + sliceOffset, &emptySlice, sizeof(emptySlice));
    }
    error.clear();
    return true;
}

TraceBufferParseResult ParseTraceBuffer(
    const uint8_t* buffer, size_t bytes, uint32_t expectedPhysicalCoreCount, uint32_t expectedBlockCount,
    uint32_t expectedRecordsPerCore, uint64_t expectedLaunchId, uint32_t deviceId)
{
    TraceBufferParseResult result;
    size_t requiredBytes = 0;
    size_t sliceBytes = 0;
    if (buffer == nullptr || expectedBlockCount == 0U ||
        !aclsan::TraceBufferBytes(expectedPhysicalCoreCount, expectedRecordsPerCore, &requiredBytes) ||
        !aclsan::TraceSliceBytes(expectedRecordsPerCore, &sliceBytes) || bytes < requiredBytes) {
        result.error = "trace buffer is null, truncated, or has invalid expected dimensions";
        return result;
    }

    aclsan::AclsanTraceBufferHeader header{};
    std::memcpy(&header, buffer, sizeof(header));
    if (header.magic != aclsan::ASCSAN_TRACE_BUFFER_MAGIC || header.launchId != expectedLaunchId ||
        header.segmentBytes != requiredBytes || header.blockCount != expectedBlockCount ||
        header.recordsPerCore != expectedRecordsPerCore || header.physicalCoreCount != expectedPhysicalCoreCount) {
        result.error = "trace segment header does not match the launch-owned layout";
        return result;
    }

    std::map<TraceBlockKey, uint64_t> instructionCounts;
    result.records.reserve(static_cast<size_t>(expectedPhysicalCoreCount) * expectedRecordsPerCore);
    for (uint32_t sliceIndex = 0; sliceIndex < expectedPhysicalCoreCount; ++sliceIndex) {
        const size_t sliceOffset = sizeof(header) + static_cast<size_t>(sliceIndex) * sliceBytes;
        aclsan::AclsanTraceSliceHeader slice{};
        std::memcpy(&slice, buffer + sliceOffset, sizeof(slice));
        if (slice.recordCount > expectedRecordsPerCore) {
            result.records.clear();
            result.error = "trace slice record count exceeds its launch-owned capacity";
            return result;
        }
        result.overflowCount += slice.overflowCount;
        if (slice.recordCount == 0) {
            continue;
        }

        const bool isAic = aclsan::IsAicPhysicalCore(sliceIndex, expectedPhysicalCoreCount);
        const uint32_t expectedPhyCoreId = sliceIndex;
        const uint32_t blockType =
            isAic ? ACLSAN_DEVICE_BLOCK_TYPE_AICORE_CUBE : ACLSAN_DEVICE_BLOCK_TYPE_AICORE_VECTOR;
        if (slice.phyCoreId != expectedPhyCoreId) {
            result.records.clear();
            result.error = "trace slice physical core ID does not match its layout index";
            return result;
        }

        for (uint32_t index = 0; index < slice.recordCount; ++index) {
            const size_t recordOffset =
                sliceOffset + sizeof(slice) + static_cast<size_t>(index) * sizeof(aclsan::AclsanRawTraceRecord);
            aclsan::AclsanRawTraceRecord wire{};
            std::memcpy(&wire, buffer + recordOffset, sizeof(wire));

            ParsedTraceRecord parsed{};
            parsed.record.pc = wire.pc;
            parsed.record.instrId = wire.instrId;
            std::memcpy(parsed.record.args, wire.args, sizeof(wire.args));
            parsed.record.siteId = wire.siteId;
            parsed.record.category = wire.category;
            parsed.record.pipeline = wire.pipeline;
            parsed.record.blockId = wire.blockId;
            parsed.record.reserved = wire.reserved;
            if (!aclsan::IsTraceBlockIdValid(wire.blockId, isAic, expectedBlockCount)) {
                result.records.clear();
                result.error = "raw trace logical block ID exceeds the launch-owned range";
                return result;
            }
            const TraceBlockKey blockKey{blockType, wire.blockId};
            parsed.instrExecId = ++instructionCounts[blockKey];
            parsed.launchId = header.launchId;
            parsed.blockId = wire.blockId;
            parsed.blockType = blockType;
            parsed.phyCoreId = expectedPhyCoreId;
            parsed.deviceId = deviceId;
            result.records.push_back(parsed);
        }
    }
    result.ok = true;
    return result;
}

} // namespace aclsan
