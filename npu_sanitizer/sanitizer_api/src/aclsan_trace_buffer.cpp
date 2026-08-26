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

#include "trace_buffer_abi.h"

#include <cstring>

namespace aclsan {

bool InitializeTraceBuffer(
    std::vector<uint8_t>& buffer, uint32_t blockCount, uint32_t recordsPerBlock, uint64_t launchId, std::string& error)
{
    size_t bytes = 0;
    if (!aclsan::TraceBufferBytes(blockCount, recordsPerBlock, &bytes)) {
        buffer.clear();
        error = "trace buffer dimensions are zero or overflow the V1 layout";
        return false;
    }

    buffer.assign(bytes, 0);
    aclsan::AscsanTraceBufferHeader header{};
    header.magic = aclsan::ASCSAN_TRACE_BUFFER_MAGIC_V1;
    header.launchId = launchId;
    header.blockCount = blockCount;
    header.recordsPerBlock = recordsPerBlock;
    std::memcpy(buffer.data(), &header, sizeof(header));
    error.clear();
    return true;
}

TraceBufferParseResult ParseTraceBuffer(
    const uint8_t* buffer, size_t bytes, uint32_t expectedBlockCount, uint32_t expectedRecordsPerBlock,
    uint64_t expectedLaunchId)
{
    TraceBufferParseResult result;
    size_t requiredBytes = 0;
    size_t sliceBytes = 0;
    if (buffer == nullptr || !aclsan::TraceBufferBytes(expectedBlockCount, expectedRecordsPerBlock, &requiredBytes) ||
        !aclsan::TraceSliceBytes(expectedRecordsPerBlock, &sliceBytes) || bytes < requiredBytes) {
        result.error = "trace buffer is null, truncated, or has invalid expected dimensions";
        return result;
    }

    aclsan::AscsanTraceBufferHeader header{};
    std::memcpy(&header, buffer, sizeof(header));
    if (header.magic != aclsan::ASCSAN_TRACE_BUFFER_MAGIC_V1 || header.launchId != expectedLaunchId ||
        header.blockCount != expectedBlockCount || header.recordsPerBlock != expectedRecordsPerBlock) {
        result.error = "trace buffer header does not match the launch-owned V1 layout";
        return result;
    }

    result.records.reserve(static_cast<size_t>(expectedBlockCount) * expectedRecordsPerBlock);
    for (uint32_t block = 0; block < expectedBlockCount; ++block) {
        const size_t sliceOffset = sizeof(header) + static_cast<size_t>(block) * sliceBytes;
        aclsan::AscsanTraceSliceHeader slice{};
        std::memcpy(&slice, buffer + sliceOffset, sizeof(slice));
        if (slice.recordCount > expectedRecordsPerBlock) {
            result.records.clear();
            result.error = "trace slice record count exceeds its launch-owned capacity";
            return result;
        }
        result.overflowCount += slice.overflowCount;

        for (uint32_t index = 0; index < slice.recordCount; ++index) {
            const size_t recordOffset =
                sliceOffset + sizeof(slice) + static_cast<size_t>(index) * sizeof(aclsan::AscsanRawTraceRecord);
            aclsan::AscsanRawTraceRecord wire{};
            std::memcpy(&wire, buffer + recordOffset, sizeof(wire));

            sanitizer::AscsanRawTraceRecord record{};
            record.blockId = block;
            record.pc = wire.pc;
            record.instrId = wire.instrId;
            std::memcpy(record.args, wire.args, sizeof(wire.args));
            record.siteId = wire.siteId;
            record.pipeline = wire.pipeline;
            result.records.push_back(record);
        }
    }
    result.ok = true;
    return result;
}

} // namespace aclsan
