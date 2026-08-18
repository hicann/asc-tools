// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "checker/memcheck.h"

#include <gtest/gtest.h>

#include <cstdint>

namespace npu::sanitizer {
namespace {

template <typename T>
void InitCommon(T& data, uint64_t correlation = 0, int result = 0)
{
    data = {};
    data.common.version = ACLSAN_API_VERSION;
    data.common.size = sizeof(T);
    data.common.correlationId = correlation;
    data.common.result = result;
}

AclsanResourceData AllocationEvent(uint64_t base, uint64_t bytes, uint64_t id, uint32_t deviceId = 0)
{
    AclsanResourceData data{};
    InitCommon(data);
    data.ptr = reinterpret_cast<void*>(base);
    data.bytes = bytes;
    data.memorySpace = ACLSAN_MEMORY_SPACE_DEVICE;
    data.deviceId = deviceId;
    data.resourceId = id;
    return data;
}

AclsanDeviceMemoryAccessData Access(uint32_t pipeline, uint64_t address, uint64_t bytes, uint32_t deviceId = 0)
{
    AclsanDeviceMemoryAccessData data{};
    data.header.version = ACLSAN_API_VERSION;
    data.header.size = sizeof(data);
    data.header.pipeline = pipeline;
    data.header.sourceKind = pipeline == ACLSAN_PATCH_PIPELINE_MTE2 ? ACLSAN_DEVICE_SOURCE_MTE2 :
                             pipeline == ACLSAN_PATCH_PIPELINE_MTE3 ? ACLSAN_DEVICE_SOURCE_MTE3 :
                                                                      ACLSAN_DEVICE_SOURCE_FIXPIPE;
    data.header.siteId = 7;
    data.header.blockId = 3;
    data.header.deviceId = deviceId;
    data.header.pc = 0x170;
    data.address = address;
    data.memorySpace = ACLSAN_DEVICE_MEMORY_SPACE_GM;
    data.accessMode =
        pipeline == ACLSAN_PATCH_PIPELINE_MTE2 ? ACLSAN_DEVICE_MEMORY_ACCESS_READ : ACLSAN_DEVICE_MEMORY_ACCESS_WRITE;
    data.accessCount = 1;
    data.layoutKind = ACLSAN_MEM_LAYOUT_RANGE;
    data.layout.range.bytes = bytes;
    return data;
}

TEST(MemcheckTest, ReportsOutOfBoundsReadAtSynchronization)
{
    Memcheck checker(true);
    checker.OnAllocation(AllocationEvent(0x100000, 4096, 1));
    const auto validRead = Access(ACLSAN_PATCH_PIPELINE_MTE2, 0x100100, 64);
    const auto invalidRead = Access(ACLSAN_PATCH_PIPELINE_MTE2, 0x100ff0, 64);
    checker.QueueDeviceMemoryAccess(validRead);
    checker.QueueDeviceMemoryAccess(invalidRead);

    EXPECT_EQ(checker.Stats().pendingDeviceOperations, 2U);
    const auto diagnostics = checker.OnSynchronization();

    ASSERT_EQ(diagnostics.size(), 1U);
    EXPECT_EQ(diagnostics.front().kind, DiagnosticKind::OUT_OF_BOUNDS);
    EXPECT_EQ(diagnostics.front().access, AccessKind::READ);
    EXPECT_EQ(diagnostics.front().instruction.pc, invalidRead.header.pc);
    EXPECT_EQ(checker.Stats().pendingDeviceOperations, 0U);
    EXPECT_EQ(checker.Stats().errors, 1U);
    EXPECT_TRUE(checker.OnSynchronization().empty());
}

TEST(MemcheckTest, ReportsOutOfBoundsWriteAndIgnoresNonGmAccesses)
{
    Memcheck checker(true);
    const auto invalidWrite = Access(ACLSAN_PATCH_PIPELINE_MTE3, 0x200000, 64);
    checker.QueueDeviceMemoryAccess(invalidWrite);

    const auto diagnostics = checker.OnSynchronization();
    ASSERT_EQ(diagnostics.size(), 1U);
    EXPECT_EQ(diagnostics.front().access, AccessKind::WRITE);

    auto nonGm = invalidWrite;
    nonGm.memorySpace = ACLSAN_DEVICE_MEMORY_SPACE_UB;
    checker.QueueDeviceMemoryAccess(nonGm);
    EXPECT_EQ(checker.Stats().pendingDeviceOperations, 0U);
    EXPECT_TRUE(checker.OnSynchronization().empty());
}

TEST(MemcheckTest, ExpandsBlockRepeatAndAffineLayouts)
{
    Memcheck checker(true);
    checker.OnAllocation(AllocationEvent(0x200000, 512, 2));

    auto blockRepeat = Access(ACLSAN_PATCH_PIPELINE_MTE3, 0x2001e8, 1);
    blockRepeat.layoutKind = ACLSAN_MEM_LAYOUT_BLOCK_REPEAT;
    blockRepeat.layout.blockRepeat.blockNum = 1;
    blockRepeat.layout.blockRepeat.blockSize = 16;
    blockRepeat.layout.blockRepeat.repeatTimes = 2;
    blockRepeat.layout.blockRepeat.repeatStride = 16;
    checker.QueueDeviceMemoryAccess(blockRepeat);
    auto diagnostics = checker.OnSynchronization();
    ASSERT_EQ(diagnostics.size(), 1U);
    EXPECT_EQ(diagnostics.front().kind, DiagnosticKind::OUT_OF_BOUNDS);

    auto ndAffine = Access(ACLSAN_PATCH_PIPELINE_MTE3, 0x2001f8, 1);
    ndAffine.layoutKind = ACLSAN_MEM_LAYOUT_ND_AFFINE;
    ndAffine.layout.ndAffine.rank = 1;
    ndAffine.layout.ndAffine.elementBytes = 8;
    ndAffine.layout.ndAffine.dims[0] = 2;
    ndAffine.layout.ndAffine.strides[0] = 4;
    checker.QueueDeviceMemoryAccess(ndAffine);
    diagnostics = checker.OnSynchronization();
    ASSERT_EQ(diagnostics.size(), 1U);
    EXPECT_EQ(diagnostics.front().kind, DiagnosticKind::OUT_OF_BOUNDS);
}

TEST(MemcheckTest, SeparatesReadWriteAndHonorsPredicate)
{
    Memcheck checker(true);
    checker.OnAllocation(AllocationEvent(0x200000, 512, 2));

    auto readWrite = Access(ACLSAN_PATCH_PIPELINE_MTE3, 0x2001fc, 8);
    readWrite.accessMode = ACLSAN_DEVICE_MEMORY_ACCESS_READ_WRITE;
    checker.QueueDeviceMemoryAccess(readWrite);
    const auto diagnostics = checker.OnSynchronization();

    ASSERT_EQ(diagnostics.size(), 2U);
    EXPECT_EQ(diagnostics[0].access, AccessKind::READ);
    EXPECT_EQ(diagnostics[1].access, AccessKind::WRITE);

    readWrite.header.flags = ACLSAN_DEVICE_EVENT_FLAG_PREDICATED;
    readWrite.predicateMask0 = 0;
    readWrite.predicateMask1 = 0;
    checker.QueueDeviceMemoryAccess(readWrite);
    EXPECT_TRUE(checker.OnSynchronization().empty());
}

TEST(MemcheckTest, HonorsStrictModeAndFreedRanges)
{
    Memcheck nonStrict(false);
    nonStrict.QueueDeviceMemoryAccess(Access(ACLSAN_PATCH_PIPELINE_MTE2, 0x500000, 8));
    EXPECT_TRUE(nonStrict.OnSynchronization().empty());

    Memcheck checker(true);
    const auto released = AllocationEvent(0x300000, 128, 21);
    checker.OnAllocation(released);
    checker.OnFree(released);
    checker.QueueDeviceMemoryAccess(Access(ACLSAN_PATCH_PIPELINE_MTE2, 0x300010, 16));
    const auto diagnostics = checker.OnSynchronization();

    ASSERT_EQ(diagnostics.size(), 1U);
    EXPECT_EQ(diagnostics.front().kind, DiagnosticKind::OUT_OF_BOUNDS);
}

TEST(MemcheckTest, UsesDeviceSpecificAllocationRanges)
{
    Memcheck checker(true);
    checker.OnAllocation(AllocationEvent(0x600000, 64, 31, 0));
    checker.OnAllocation(AllocationEvent(0x600000, 128, 31, 1));
    checker.QueueDeviceMemoryAccess(Access(ACLSAN_PATCH_PIPELINE_MTE2, 0x600030, 32, 0));

    const auto diagnostics = checker.OnSynchronization();

    ASSERT_EQ(diagnostics.size(), 1U);
    EXPECT_EQ(diagnostics.front().kind, DiagnosticKind::OUT_OF_BOUNDS);
    EXPECT_EQ(diagnostics.front().instruction.deviceId, 0U);
}

} // namespace
} // namespace npu::sanitizer
