// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "checker/synccheck.h"

#include <gtest/gtest.h>

namespace npucheck {
namespace {

AclsanDeviceSyncData MakeSyncEvent(
    uint32_t syncKind, uint32_t action, uint64_t objectId = 7, uint32_t dstPipe = ACLSAN_DEVICE_PIPE_MTE2,
    uint32_t mode = 0, uint64_t launchId = 0)
{
    AclsanDeviceSyncData data{};
    data.header.launchId = launchId;
    data.header.phyCoreId = 3;
    data.header.blockId = 5;
    data.syncKind = syncKind;
    data.action = action;
    data.scope = syncKind == ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG ? ACLSAN_DEVICE_SYNC_SCOPE_BLOCK :
                                                                     ACLSAN_DEVICE_SYNC_SCOPE_PIPE;
    data.srcPipe = syncKind == ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG ? ACLSAN_DEVICE_PIPE_VECTOR : 0;
    data.dstPipe = dstPipe;
    data.mode = mode;
    data.objectId = objectId;
    return data;
}

TEST(SynccheckTest, TracksFlagAndSyncBufStatesIndependently)
{
    Synccheck synccheck;

    synccheck.OnDeviceSync(MakeSyncEvent(ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG, ACLSAN_DEVICE_SYNC_ACTION_SET));
    synccheck.OnDeviceSync(MakeSyncEvent(ACLSAN_DEVICE_SYNC_KIND_GET_RLS_BUF, ACLSAN_DEVICE_SYNC_ACTION_GET));
    EXPECT_EQ(synccheck.Stats().pendingOpens, 2U);

    synccheck.OnDeviceSync(MakeSyncEvent(ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG, ACLSAN_DEVICE_SYNC_ACTION_WAIT));
    EXPECT_EQ(synccheck.Stats().pendingOpens, 1U);

    synccheck.OnDeviceSync(MakeSyncEvent(ACLSAN_DEVICE_SYNC_KIND_GET_RLS_BUF, ACLSAN_DEVICE_SYNC_ACTION_RELEASE));
    const SynccheckStats stats = synccheck.Stats();
    EXPECT_EQ(stats.pendingOpens, 0U);
    EXPECT_EQ(stats.matchedPairs, 2U);
}

TEST(SynccheckTest, SynchronizationReportsPendingFlagAndSyncBuf)
{
    Synccheck synccheck;
    synccheck.OnDeviceSync(MakeSyncEvent(ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG, ACLSAN_DEVICE_SYNC_ACTION_SET));
    synccheck.OnDeviceSync(MakeSyncEvent(ACLSAN_DEVICE_SYNC_KIND_GET_RLS_BUF, ACLSAN_DEVICE_SYNC_ACTION_GET));

    const auto reports = synccheck.OnSynchronization();

    EXPECT_EQ(reports.size(), 2U);
    const SynccheckStats stats = synccheck.Stats();
    EXPECT_EQ(stats.unconsumedOpens, 2U);
    EXPECT_EQ(stats.pendingOpens, 0U);
}

TEST(SynccheckTest, SynchronizationClearsAllReportedLaunches)
{
    Synccheck synccheck;
    synccheck.OnDeviceSync(MakeSyncEvent(
        ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG, ACLSAN_DEVICE_SYNC_ACTION_SET, 7, ACLSAN_DEVICE_PIPE_MTE2, 0, 1));
    synccheck.OnDeviceSync(MakeSyncEvent(
        ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG, ACLSAN_DEVICE_SYNC_ACTION_SET, 7, ACLSAN_DEVICE_PIPE_MTE2, 0, 2));

    const auto reports = synccheck.OnSynchronization();

    EXPECT_EQ(reports.size(), 2U);
    EXPECT_EQ(synccheck.Stats().unconsumedOpens, 2U);
    EXPECT_EQ(synccheck.Stats().pendingOpens, 0U);
}

TEST(SynccheckTest, DetectsDuplicateGetAcrossDifferentExactKeys)
{
    Synccheck synccheck;
    synccheck.OnDeviceSync(MakeSyncEvent(ACLSAN_DEVICE_SYNC_KIND_GET_RLS_BUF, ACLSAN_DEVICE_SYNC_ACTION_GET));

    synccheck.OnDeviceSync(MakeSyncEvent(
        ACLSAN_DEVICE_SYNC_KIND_GET_RLS_BUF, ACLSAN_DEVICE_SYNC_ACTION_GET, 7, ACLSAN_DEVICE_PIPE_MTE3, 1));
    const SynccheckStats stats = synccheck.Stats();
    EXPECT_EQ(stats.duplicateOpens, 1U);
    EXPECT_EQ(stats.pendingOpens, 1U);
}

TEST(SynccheckTest, DefersImmediateMismatchesUntilSynchronization)
{
    Synccheck synccheck;
    synccheck.OnDeviceSync(MakeSyncEvent(ACLSAN_DEVICE_SYNC_KIND_GET_RLS_BUF, ACLSAN_DEVICE_SYNC_ACTION_GET));

    synccheck.OnDeviceSync(MakeSyncEvent(
        ACLSAN_DEVICE_SYNC_KIND_GET_RLS_BUF, ACLSAN_DEVICE_SYNC_ACTION_GET, 7, ACLSAN_DEVICE_PIPE_MTE3, 1));

    const auto synchronizationReports = synccheck.OnSynchronization();
    ASSERT_EQ(synchronizationReports.size(), 2U);
    EXPECT_EQ(synccheck.Stats().duplicateOpens, 1U);
    EXPECT_EQ(synccheck.Stats().unconsumedOpens, 1U);
}

TEST(SynccheckTest, PreservesDeviceBlockTypeInReport)
{
    Synccheck synccheck;
    AclsanDeviceSyncData data = MakeSyncEvent(ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG, ACLSAN_DEVICE_SYNC_ACTION_WAIT);
    data.header.blockType = ACLSAN_DEVICE_BLOCK_TYPE_AICORE_VECTOR;

    synccheck.OnDeviceSync(data);
    const auto reports = synccheck.OnSynchronization();

    ASSERT_EQ(reports.size(), 1U);
    EXPECT_EQ(reports.front().triggerPoint.exec.blockType, ACLSAN_DEVICE_BLOCK_TYPE_AICORE_VECTOR);
}

} // namespace
} // namespace npucheck
