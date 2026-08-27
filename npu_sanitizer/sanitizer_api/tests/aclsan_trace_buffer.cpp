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

using aclsan::AclsanRawTraceRecord;
using aclsan::AclsanTraceBufferHeader;
using aclsan::AclsanTraceSliceHeader;
using aclsan::DeviceInstructionCategory;

static_assert(aclsan::ASCSAN_TRACE_BUFFER_MAGIC == 0x41534353414E3035ULL);
static_assert(sizeof(AclsanTraceBufferHeader) == 32);
static_assert(sizeof(AclsanTraceSliceHeader) == 16);
static_assert(sizeof(AclsanRawTraceRecord) == 72);
static_assert(sizeof(AclsanTraceSliceHeader) % alignof(AclsanRawTraceRecord) == 0);
static_assert(offsetof(AclsanRawTraceRecord, pc) == 0);
static_assert(offsetof(AclsanRawTraceRecord, args) == 8);
static_assert(sizeof(((AclsanRawTraceRecord*)nullptr)->args) == 5 * sizeof(uint64_t));
static_assert(offsetof(AclsanRawTraceRecord, instrId) == 48);
static_assert(offsetof(AclsanRawTraceRecord, siteId) == 56);
static_assert(sizeof(DeviceInstructionCategory) == sizeof(uint16_t));
static_assert(offsetof(AclsanRawTraceRecord, category) == 60);
static_assert(offsetof(AclsanRawTraceRecord, pipeline) == 62);
static_assert(offsetof(AclsanRawTraceRecord, blockId) == 64);
static_assert(offsetof(AclsanRawTraceRecord, reserved) == 68);

AclsanTraceSliceHeader* SliceAt(std::vector<uint8_t>& buffer, uint32_t sliceIndex)
{
    auto* header = reinterpret_cast<AclsanTraceBufferHeader*>(buffer.data());
    size_t sliceBytes = 0;
    if (!aclsan::TraceSliceBytes(header->recordsPerCore, &sliceBytes)) {
        return nullptr;
    }
    const size_t sliceOffset = sizeof(*header) + static_cast<size_t>(sliceIndex) * sliceBytes;
    return reinterpret_cast<AclsanTraceSliceHeader*>(buffer.data() + sliceOffset);
}

bool PutRecord(
    std::vector<uint8_t>& buffer, uint32_t sliceIndex, uint32_t index, uint32_t blockId, uint64_t pc, uint32_t pipeline,
    uint32_t phyCoreId)
{
    auto* slice = SliceAt(buffer, sliceIndex);
    CHECK(slice != nullptr);
    auto* record = reinterpret_cast<AclsanRawTraceRecord*>(
        reinterpret_cast<uint8_t*>(slice) + sizeof(*slice) + static_cast<size_t>(index) * sizeof(AclsanRawTraceRecord));
    record->pc = pc;
    for (uint32_t arg = 0; arg < 5; ++arg) {
        record->args[arg] = pc + arg + 1;
    }
    record->instrId = pc + 6;
    record->siteId = static_cast<uint32_t>(pc);
    record->category = DeviceInstructionCategory::MemoryAccess;
    record->pipeline = static_cast<uint16_t>(pipeline);
    record->blockId = blockId;
    record->reserved = 0;
    slice->phyCoreId = phyCoreId;
    slice->recordCount = index + 1;
    return true;
}

bool InitializesDynamicPhysicalCoreSlices()
{
    std::vector<uint8_t> first;
    std::vector<uint8_t> second;
    std::string error;
    CHECK(aclsan::InitializeTraceBuffer(first, 12, 1, 2, 17, error));
    CHECK(aclsan::InitializeTraceBuffer(second, 12, 4096, 2, 18, error));
    CHECK(first.size() == second.size());

    const auto* header = reinterpret_cast<const AclsanTraceBufferHeader*>(first.data());
    CHECK(header->magic == aclsan::ASCSAN_TRACE_BUFFER_MAGIC);
    CHECK(header->blockCount == 1);
    CHECK(header->recordsPerCore == 2);
    CHECK(header->physicalCoreCount == 12);
    CHECK(header->reserved == 0);
    size_t sliceBytes = 0;
    CHECK(aclsan::TraceSliceBytes(header->recordsPerCore, &sliceBytes));
    CHECK(first.size() == sizeof(*header) + header->physicalCoreCount * sliceBytes);
    for (uint32_t sliceIndex = 0; sliceIndex < header->physicalCoreCount; ++sliceIndex) {
        const auto* slice = SliceAt(first, sliceIndex);
        CHECK(slice != nullptr);
        CHECK(slice->recordCount == 0);
        CHECK(slice->overflowCount == 0);
        CHECK(slice->phyCoreId == 0);
        CHECK(slice->reserved == 0);
    }

    std::vector<uint8_t> larger;
    CHECK(aclsan::InitializeTraceBuffer(larger, 18, 1, 2, 19, error));
    CHECK(larger.size() == sizeof(*header) + 18U * sliceBytes);
    return true;
}

bool ParsesMultipleLogicalBlocksInOnePhysicalSlice()
{
    std::vector<uint8_t> buffer;
    std::string error;
    CHECK(aclsan::InitializeTraceBuffer(buffer, 12, 20, 5, 19, error));
    constexpr uint32_t blockIds[] = {7, 7, 11, 11, 7};
    for (uint32_t index = 0; index < 5; ++index) {
        CHECK(PutRecord(buffer, 1, index, blockIds[index], 0x100 + index, ACLSAN_DEVICE_PIPE_MTE2, 1));
    }

    constexpr uint32_t deviceId = 3;
    const auto parsed = aclsan::ParseTraceBuffer(buffer.data(), buffer.size(), 12, 20, 5, 19, deviceId);
    CHECK(parsed.ok);
    CHECK(parsed.records.size() == 5);
    constexpr uint64_t instructionIds[] = {1, 2, 1, 2, 3};
    for (uint32_t index = 0; index < 5; ++index) {
        CHECK(parsed.records[index].blockId == blockIds[index]);
        CHECK(parsed.records[index].blockType == ACLSAN_DEVICE_BLOCK_TYPE_AICORE_CUBE);
        CHECK(parsed.records[index].phyCoreId == 1);
        CHECK(parsed.records[index].instrExecId == instructionIds[index]);
        CHECK(parsed.records[index].deviceId == deviceId);
        CHECK(parsed.records[index].launchId == 19);
        CHECK(parsed.records[index].record.category == DeviceInstructionCategory::MemoryAccess);
        CHECK(parsed.records[index].record.pipeline == ACLSAN_DEVICE_PIPE_MTE2);
    }
    return true;
}

bool ParsesTwoPartPhysicalCoreTopology()
{
    std::vector<uint8_t> buffer;
    std::string error;
    CHECK(aclsan::InitializeTraceBuffer(buffer, 12, 2, 1, 21, error));
    CHECK(PutRecord(buffer, 1, 0, 1, 0x200, ACLSAN_DEVICE_PIPE_MTE2, 1));
    CHECK(PutRecord(buffer, 2, 0, 2, 0x300, ACLSAN_DEVICE_PIPE_MTE3, 2));
    CHECK(PutRecord(buffer, 7, 0, 1, 0x400, ACLSAN_DEVICE_PIPE_MTE2, 7));
    CHECK(PutRecord(buffer, 8, 0, 2, 0x500, ACLSAN_DEVICE_PIPE_MTE3, 8));

    const auto parsed = aclsan::ParseTraceBuffer(buffer.data(), buffer.size(), 12, 2, 1, 21, 0);
    CHECK(parsed.ok);
    CHECK(parsed.records.size() == 4);
    CHECK(parsed.records[0].blockId == 1);
    CHECK(parsed.records[0].blockType == ACLSAN_DEVICE_BLOCK_TYPE_AICORE_CUBE);
    CHECK(parsed.records[0].phyCoreId == 1);
    CHECK(parsed.records[0].instrExecId == 1);
    CHECK(parsed.records[1].blockId == 2);
    CHECK(parsed.records[1].blockType == ACLSAN_DEVICE_BLOCK_TYPE_AICORE_VECTOR);
    CHECK(parsed.records[1].phyCoreId == 2);
    CHECK(parsed.records[1].instrExecId == 1);
    CHECK(parsed.records[2].blockId == 1);
    CHECK(parsed.records[2].blockType == ACLSAN_DEVICE_BLOCK_TYPE_AICORE_CUBE);
    CHECK(parsed.records[2].phyCoreId == 7);
    CHECK(parsed.records[3].blockId == 2);
    CHECK(parsed.records[3].blockType == ACLSAN_DEVICE_BLOCK_TYPE_AICORE_VECTOR);
    CHECK(parsed.records[3].phyCoreId == 8);
    return true;
}

bool SupportsFullAicBlockCountRange()
{
    constexpr uint32_t maxBlockCount = std::numeric_limits<uint32_t>::max();
    std::vector<uint8_t> buffer;
    std::string error;
    CHECK(aclsan::InitializeTraceBuffer(buffer, 12, maxBlockCount, 1, 22, error));
    CHECK(PutRecord(buffer, 0, 0, maxBlockCount - 1U, 0x450, ACLSAN_DEVICE_PIPE_MTE2, 0));

    const auto parsed = aclsan::ParseTraceBuffer(buffer.data(), buffer.size(), 12, maxBlockCount, 1, 22, 0);
    CHECK(parsed.ok);
    CHECK(parsed.records.size() == 1);
    CHECK(parsed.records[0].blockType == ACLSAN_DEVICE_BLOCK_TYPE_AICORE_CUBE);
    CHECK(parsed.records[0].blockId == maxBlockCount - 1U);
    return true;
}

bool RejectsUnrepresentableLogicalBlockIds()
{
    constexpr uint32_t blockCount = std::numeric_limits<uint32_t>::max();
    CHECK(aclsan::IsTraceBlockIdValid(blockCount - 1U, true, blockCount));
    CHECK(!aclsan::IsTraceBlockIdValid(blockCount, true, blockCount));
    CHECK(aclsan::IsTraceBlockIdValid(blockCount, false, blockCount));
    CHECK(!aclsan::IsTraceBlockIdValid(static_cast<uint64_t>(blockCount) + 1U, false, blockCount));
    return true;
}

bool ReportsOverflowAndKeepsRecords()
{
    std::vector<uint8_t> buffer;
    std::string error;
    CHECK(aclsan::InitializeTraceBuffer(buffer, 12, 1, 1, 23, error));
    CHECK(PutRecord(buffer, 0, 0, 0, 0x500, ACLSAN_DEVICE_PIPE_FIXPIPE, 0));
    auto* slice = reinterpret_cast<AclsanTraceSliceHeader*>(buffer.data() + sizeof(AclsanTraceBufferHeader));
    slice->overflowCount = 9;

    const auto parsed = aclsan::ParseTraceBuffer(buffer.data(), buffer.size(), 12, 1, 1, 23, 0);
    CHECK(parsed.ok);
    CHECK(parsed.overflowCount == 9);
    CHECK(parsed.records.size() == 1);
    return true;
}

bool RejectsMalformedBuffers()
{
    std::vector<uint8_t> buffer;
    std::string error;
    CHECK(aclsan::InitializeTraceBuffer(buffer, 12, 2, 1, 29, error));

    auto corrupt = buffer;
    reinterpret_cast<AclsanTraceBufferHeader*>(corrupt.data())->magic ^= 1;
    CHECK(!aclsan::ParseTraceBuffer(corrupt.data(), corrupt.size(), 12, 2, 1, 29, 0).ok);
    CHECK(!aclsan::ParseTraceBuffer(buffer.data(), buffer.size(), 12, 2, 1, 30, 0).ok);
    CHECK(!aclsan::ParseTraceBuffer(buffer.data(), buffer.size() - 1, 12, 2, 1, 29, 0).ok);

    corrupt = buffer;
    reinterpret_cast<AclsanTraceBufferHeader*>(corrupt.data())->physicalCoreCount = 18;
    CHECK(!aclsan::ParseTraceBuffer(corrupt.data(), corrupt.size(), 12, 2, 1, 29, 0).ok);

    corrupt = buffer;
    reinterpret_cast<AclsanTraceSliceHeader*>(corrupt.data() + sizeof(AclsanTraceBufferHeader))->recordCount = 2;
    CHECK(!aclsan::ParseTraceBuffer(corrupt.data(), corrupt.size(), 12, 2, 1, 29, 0).ok);

    corrupt = buffer;
    CHECK(PutRecord(corrupt, 1, 0, 0, 0x600, ACLSAN_DEVICE_PIPE_MTE2, 2));
    CHECK(!aclsan::ParseTraceBuffer(corrupt.data(), corrupt.size(), 12, 2, 1, 29, 0).ok);

    corrupt = buffer;
    CHECK(PutRecord(corrupt, 1, 0, 2, 0x700, ACLSAN_DEVICE_PIPE_MTE2, 1));
    CHECK(!aclsan::ParseTraceBuffer(corrupt.data(), corrupt.size(), 12, 2, 1, 29, 0).ok);

    corrupt = buffer;
    CHECK(PutRecord(corrupt, 2, 0, 4, 0x800, ACLSAN_DEVICE_PIPE_MTE3, 2));
    CHECK(!aclsan::ParseTraceBuffer(corrupt.data(), corrupt.size(), 12, 2, 1, 29, 0).ok);
    return true;
}

bool RejectsInvalidShape()
{
    std::vector<uint8_t> buffer;
    std::string error;
    CHECK(!aclsan::InitializeTraceBuffer(buffer, 0, 1, 1, 1, error));
    CHECK(!aclsan::InitializeTraceBuffer(buffer, 10, 1, 1, 1, error));
    CHECK(!aclsan::InitializeTraceBuffer(buffer, 12, 0, 1, 1, error));
    CHECK(!aclsan::InitializeTraceBuffer(buffer, 12, 1, 0, 1, error));

    size_t bytes = 0;
    CHECK(!aclsan::TraceBufferBytes(0, 1, &bytes));
    CHECK(!aclsan::TraceBufferBytes(10, 1, &bytes));
    CHECK(!aclsan::TraceBufferBytes(12, 0, &bytes));
    CHECK(!aclsan::TraceBufferBytes(12, 1, nullptr));
    return true;
}

} // namespace

int main()
{
    return InitializesDynamicPhysicalCoreSlices() && ParsesMultipleLogicalBlocksInOnePhysicalSlice() &&
                   ParsesTwoPartPhysicalCoreTopology() && SupportsFullAicBlockCountRange() &&
                   RejectsUnrepresentableLogicalBlockIds() && ReportsOverflowAndKeepsRecords() &&
                   RejectsMalformedBuffers() && RejectsInvalidShape() ?
               0 :
               1;
}
