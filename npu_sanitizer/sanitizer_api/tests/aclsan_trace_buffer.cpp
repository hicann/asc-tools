/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aclsan/aclsan_cbdata_device.h"
#include "internal/aclsan_trace_buffer.h"
#include "trace_buffer_abi.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {

#define CHECK(expression)                                                                 \
    do {                                                                                  \
        if (!(expression)) {                                                              \
            std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expression); \
            return false;                                                                 \
        }                                                                                 \
    } while (false)

using aclsan::AscsanRawTraceRecord;
using aclsan::AscsanTraceBufferHeader;
using aclsan::AscsanTraceSliceHeader;

static_assert(sizeof(AscsanTraceBufferHeader) == 24);
static_assert(sizeof(AscsanTraceSliceHeader) == 8);
static_assert(sizeof(AscsanRawTraceRecord) == 64);
static_assert(offsetof(AscsanRawTraceRecord, pc) == 0);
static_assert(offsetof(AscsanRawTraceRecord, args) == 8);
static_assert(sizeof(((AscsanRawTraceRecord*)nullptr)->args) == 5 * sizeof(uint64_t));
static_assert(offsetof(AscsanRawTraceRecord, instrId) == 48);
static_assert(offsetof(AscsanRawTraceRecord, siteId) == 56);
static_assert(offsetof(AscsanRawTraceRecord, pipeline) == 60);

bool PutRecord(std::vector<uint8_t>& buffer, uint32_t block, uint32_t index, uint64_t pc, uint32_t pipeline)
{
    auto* header = reinterpret_cast<AscsanTraceBufferHeader*>(buffer.data());
    size_t sliceBytes = 0;
    CHECK(aclsan::TraceSliceBytes(header->recordsPerBlock, &sliceBytes));
    const size_t sliceOffset = sizeof(*header) + static_cast<size_t>(block) * sliceBytes;
    auto* slice = reinterpret_cast<AscsanTraceSliceHeader*>(buffer.data() + sliceOffset);
    auto* record = reinterpret_cast<AscsanRawTraceRecord*>(
        buffer.data() + sliceOffset + sizeof(*slice) + static_cast<size_t>(index) * sizeof(AscsanRawTraceRecord));
    record->pc = pc;
    for (uint32_t arg = 0; arg < 5; ++arg) {
        record->args[arg] = pc + arg + 1;
    }
    record->instrId = pc + 6;
    record->siteId = static_cast<uint32_t>(pc);
    record->pipeline = pipeline;
    slice->recordCount = index + 1;
    return true;
}

bool ParsesRecordsAndReconstructsBlockId()
{
    std::vector<uint8_t> buffer;
    std::string error;
    CHECK(aclsan::InitializeTraceBuffer(buffer, 2, 2, 17, error));
    CHECK(PutRecord(buffer, 0, 0, 0x100, ACLSAN_DEVICE_PIPE_MTE2));
    CHECK(PutRecord(buffer, 1, 0, 0x200, ACLSAN_DEVICE_PIPE_MTE3));

    const auto parsed = aclsan::ParseTraceBuffer(buffer.data(), buffer.size(), 2, 2, 17);
    CHECK(parsed.ok);
    CHECK(parsed.records.size() == 2);
    CHECK(parsed.records[0].blockId == 0);
    CHECK(parsed.records[0].pc == 0x100);
    CHECK(parsed.records[0].instrId == 0x106);
    CHECK(parsed.records[1].blockId == 1);
    CHECK(parsed.records[1].pipeline == ACLSAN_DEVICE_PIPE_MTE3);
    return true;
}

bool ReportsOverflowAndKeepsRecords()
{
    std::vector<uint8_t> buffer;
    std::string error;
    CHECK(aclsan::InitializeTraceBuffer(buffer, 1, 1, 23, error));
    CHECK(PutRecord(buffer, 0, 0, 0x300, ACLSAN_DEVICE_PIPE_FIXPIPE));
    auto* slice = reinterpret_cast<AscsanTraceSliceHeader*>(buffer.data() + sizeof(AscsanTraceBufferHeader));
    slice->overflowCount = 9;

    const auto parsed = aclsan::ParseTraceBuffer(buffer.data(), buffer.size(), 1, 1, 23);
    CHECK(parsed.ok);
    CHECK(parsed.overflowCount == 9);
    CHECK(parsed.records.size() == 1);
    return true;
}

bool RejectsMalformedBuffers()
{
    std::vector<uint8_t> buffer;
    std::string error;
    CHECK(aclsan::InitializeTraceBuffer(buffer, 1, 1, 29, error));

    auto corrupt = buffer;
    reinterpret_cast<AscsanTraceBufferHeader*>(corrupt.data())->magic ^= 1;
    CHECK(!aclsan::ParseTraceBuffer(corrupt.data(), corrupt.size(), 1, 1, 29).ok);
    CHECK(!aclsan::ParseTraceBuffer(buffer.data(), buffer.size(), 1, 1, 30).ok);
    CHECK(!aclsan::ParseTraceBuffer(buffer.data(), buffer.size() - 1, 1, 1, 29).ok);

    corrupt = buffer;
    reinterpret_cast<AscsanTraceSliceHeader*>(corrupt.data() + sizeof(AscsanTraceBufferHeader))->recordCount = 2;
    CHECK(!aclsan::ParseTraceBuffer(corrupt.data(), corrupt.size(), 1, 1, 29).ok);
    return true;
}

bool RejectsInvalidShape()
{
    std::vector<uint8_t> buffer;
    std::string error;
    CHECK(!aclsan::InitializeTraceBuffer(buffer, 0, 1, 1, error));
    CHECK(!aclsan::InitializeTraceBuffer(buffer, 1, 0, 1, error));

    size_t bytes = 0;
    CHECK(
        !aclsan::TraceBufferBytes(std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max(), &bytes));
    return true;
}

} // namespace

int main()
{
    return ParsesRecordsAndReconstructsBlockId() && ReportsOverflowAndKeepsRecords() && RejectsMalformedBuffers() &&
                   RejectsInvalidShape() ?
               0 :
               1;
}
