// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "checker/allocation_registry.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

namespace npu::sanitizer {
namespace {

TEST(AllocationRegistryTest, RejectsInvalidAndOverlappingRanges)
{
    AllocationRegistry registry;

    EXPECT_EQ(registry.Register(1, 0, 64, 0).status, AllocationUpdateStatus::INVALID_RANGE);
    EXPECT_EQ(registry.Register(1, 0x1000, 0, 0).status, AllocationUpdateStatus::INVALID_RANGE);
    EXPECT_EQ(registry.Register(1, 0x1000, 0x100, 0).status, AllocationUpdateStatus::OK);
    EXPECT_EQ(registry.Register(2, 0x1080, 0x100, 0).status, AllocationUpdateStatus::OVERLAP);
    EXPECT_EQ(registry.Register(1, 0x2000, 0x100, 0).status, AllocationUpdateStatus::OVERLAP);
    EXPECT_EQ(registry.LiveCount(), 1U);
}

TEST(AllocationRegistryTest, ClassifiesValidAndOutOfBoundsRanges)
{
    AllocationRegistry registry;
    ASSERT_EQ(registry.Register(1, 0x1000, 0x100, 0).status, AllocationUpdateStatus::OK);

    EXPECT_EQ(registry.Classify(0x1000, 0x100).status, RangeStatus::VALID);
    EXPECT_EQ(registry.Classify(0x10ff, 1).status, RangeStatus::VALID);
    EXPECT_EQ(registry.Classify(0x10f0, 0x20).status, RangeStatus::OUT_OF_BOUNDS);
    EXPECT_EQ(registry.Classify(0x1100, 1).status, RangeStatus::OUT_OF_BOUNDS);
    EXPECT_EQ(registry.Classify(0x0ff8, 0x10).status, RangeStatus::OUT_OF_BOUNDS);
    EXPECT_EQ(registry.Classify(0x8000, 32).status, RangeStatus::UNKNOWN);
    EXPECT_EQ(registry.Classify(std::numeric_limits<uint64_t>::max() - 3, 8).status, RangeStatus::OVERFLOW);
    EXPECT_EQ(registry.Classify(0, 0).status, RangeStatus::VALID);
}

TEST(AllocationRegistryTest, TracksAllocationLifetimeAndAddressReuse)
{
    AllocationRegistry registry;
    ASSERT_EQ(registry.Register(1, 0x1000, 0x100, 0).status, AllocationUpdateStatus::OK);
    ASSERT_EQ(registry.Register(2, 0x1000, 0x100, 1).status, AllocationUpdateStatus::OK);
    ASSERT_EQ(registry.Release(1, 0x1000, 0).status, AllocationUpdateStatus::OK);

    EXPECT_EQ(registry.Classify(0x1010, 8).status, RangeStatus::AMBIGUOUS);
    ASSERT_EQ(registry.Release(2, 0x1000, 1).status, AllocationUpdateStatus::OK);
    EXPECT_EQ(registry.Classify(0x1010, 8).status, RangeStatus::USE_AFTER_FREE);
    EXPECT_EQ(registry.Release(2, 0x1000, 1).status, AllocationUpdateStatus::DOUBLE_FREE);

    ASSERT_EQ(registry.Register(3, 0x1000, 0x100, 1).status, AllocationUpdateStatus::OK);
    EXPECT_EQ(registry.Classify(0x1010, 8).status, RangeStatus::AMBIGUOUS);
    EXPECT_EQ(registry.Release(2, 0x1000, 1).status, AllocationUpdateStatus::NOT_FOUND);
    EXPECT_EQ(registry.Classify(0x1010, 8).status, RangeStatus::AMBIGUOUS);
    EXPECT_EQ(registry.Release(0, 0x1010, 1).status, AllocationUpdateStatus::NOT_FOUND);
    EXPECT_EQ(registry.Release(0, 0x1000, 1).status, AllocationUpdateStatus::OK);
}

TEST(AllocationRegistryTest, PreservesNonReusedPartsOfFreedRanges)
{
    AllocationRegistry registry;
    ASSERT_EQ(registry.Register(31, 0x4000, 0x100, 0).status, AllocationUpdateStatus::OK);
    ASSERT_EQ(registry.Release(31, 0x4000, 0).status, AllocationUpdateStatus::OK);
    ASSERT_EQ(registry.Register(32, 0x4040, 0x40, 0).status, AllocationUpdateStatus::OK);

    EXPECT_EQ(registry.Classify(0x4050, 8).status, RangeStatus::VALID);
    EXPECT_EQ(registry.Classify(0x4010, 8).status, RangeStatus::USE_AFTER_FREE);
    EXPECT_EQ(registry.Classify(0x4090, 8).status, RangeStatus::USE_AFTER_FREE);
}

TEST(AllocationRegistryTest, SeparatesRangesByDevice)
{
    AllocationRegistry registry;
    ASSERT_EQ(registry.Register(77, 0x5000, 0x40, 0).status, AllocationUpdateStatus::OK);
    ASSERT_EQ(registry.Register(77, 0x6000, 0x40, 1).status, AllocationUpdateStatus::OK);
    ASSERT_EQ(registry.Release(77, 0x5000, 0).status, AllocationUpdateStatus::OK);

    EXPECT_EQ(registry.Classify(0, 0x5010, 8).status, RangeStatus::USE_AFTER_FREE);
    EXPECT_EQ(registry.Classify(1, 0x6010, 8).status, RangeStatus::VALID);
    EXPECT_EQ(registry.Classify(0, 0x6010, 8).status, RangeStatus::UNKNOWN);
}

TEST(AllocationRegistryTest, ReportsAmbiguousCrossDeviceRanges)
{
    AllocationRegistry registry;
    ASSERT_EQ(registry.Register(81, 0x7000, 0x40, 0).status, AllocationUpdateStatus::OK);
    ASSERT_EQ(registry.Register(82, 0x7000, 0x80, 1).status, AllocationUpdateStatus::OK);

    EXPECT_EQ(registry.Classify(0x7030, 0x20).status, RangeStatus::AMBIGUOUS);
}

} // namespace
} // namespace npu::sanitizer
