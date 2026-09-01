// Copyright (c) 2026 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "tool_manager/tool_manager.h"

#include <gtest/gtest.h>

namespace npu::sanitizer {
namespace {

AclsanSynchronizeData CollectionData(void* stream, AclsanTraceCollectionStatus status, uint32_t pendingLaunches)
{
    AclsanSynchronizeData data{};
    data.stream = stream;
    data.traceCollectionStatus = status;
    data.pendingTraceLaunches = pendingLaunches;
    return data;
}

TEST(TraceCollectionTrackerTest, DeferredCollectionRemainsPendingUntilSuccessfulRetry)
{
    TraceCollectionTracker tracker;
    int streamStorage = 0;
    void* const stream = &streamStorage;

    EXPECT_TRUE(tracker.Observe(CollectionData(stream, ACLSAN_TRACE_COLLECTION_DEFERRED, 2)));
    EXPECT_FALSE(tracker.IsComplete());
    EXPECT_EQ(tracker.PendingStreams(), 1U);
    EXPECT_EQ(tracker.Failures(), 0U);

    EXPECT_TRUE(tracker.Observe(CollectionData(stream, ACLSAN_TRACE_COLLECTION_NOT_REQUIRED, 0)));
    EXPECT_FALSE(tracker.IsComplete());
    EXPECT_EQ(tracker.PendingStreams(), 1U);

    EXPECT_TRUE(tracker.Observe(CollectionData(stream, ACLSAN_TRACE_COLLECTION_COMPLETE, 0)));
    EXPECT_TRUE(tracker.IsComplete());
    EXPECT_EQ(tracker.PendingStreams(), 0U);
}

TEST(TraceCollectionTrackerTest, IrrecoverableCollectionFailureKeepsAnalysisIncomplete)
{
    TraceCollectionTracker tracker;
    int streamStorage = 0;
    void* const stream = &streamStorage;

    EXPECT_TRUE(tracker.Observe(CollectionData(stream, ACLSAN_TRACE_COLLECTION_FAILED, 0)));
    EXPECT_FALSE(tracker.IsComplete());
    EXPECT_EQ(tracker.Failures(), 1U);

    EXPECT_TRUE(tracker.Observe(CollectionData(stream, ACLSAN_TRACE_COLLECTION_COMPLETE, 0)));
    EXPECT_FALSE(tracker.IsComplete());
    EXPECT_EQ(tracker.Failures(), 1U);
}

TEST(TraceCollectionTrackerTest, RejectsInconsistentCollectionStatus)
{
    TraceCollectionTracker tracker;
    int streamStorage = 0;
    void* const stream = &streamStorage;

    EXPECT_FALSE(tracker.Observe(CollectionData(stream, ACLSAN_TRACE_COLLECTION_DEFERRED, 0)));
    EXPECT_FALSE(tracker.Observe(CollectionData(stream, ACLSAN_TRACE_COLLECTION_COMPLETE, 1)));
    EXPECT_FALSE(tracker.Observe(CollectionData(stream, static_cast<AclsanTraceCollectionStatus>(99), 0)));
    EXPECT_TRUE(tracker.IsComplete());
}

} // namespace
} // namespace npu::sanitizer
