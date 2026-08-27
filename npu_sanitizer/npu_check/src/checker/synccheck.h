// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#ifndef NPU_CHECK_CHECKER_SYNCCHECK_H
#define NPU_CHECK_CHECKER_SYNCCHECK_H

#include "aclsan/aclsan_cbdata_device.h"
#include "diagnostic/report_renderer.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace npu::sanitizer {

namespace logging {
class Logger;
}

struct SynccheckStats {
    uint64_t syncEvents = 0;
    uint64_t synchronizationEvents = 0;
    uint64_t matchedPairs = 0;
    uint64_t duplicateOpens = 0;
    uint64_t unmatchedCloses = 0;
    uint64_t unconsumedOpens = 0;
    uint64_t invalidEvents = 0;
    uint64_t pendingOpens = 0;
};

class Synccheck {
public:
    explicit Synccheck(logging::Logger* logger = nullptr) noexcept : logger_(logger) {}

    std::vector<aclsan::cann::NpusanSynccheckReport> OnDeviceSync(const AclsanDeviceSyncData& data);
    std::vector<aclsan::cann::NpusanSynccheckReport> OnSynchronization();
    SynccheckStats Stats() const;

private:
    struct SyncPairKey {
        uint64_t launchId = 0;
        uint64_t objectId = 0;
        uint32_t phyCoreId = 0;
        uint32_t blockId = 0;
        uint32_t syncKind = 0;
        uint32_t scope = 0;
        uint32_t srcPipe = 0;
        uint32_t dstPipe = 0;
        uint32_t mode = 0;

        bool operator==(const SyncPairKey& other) const noexcept
        {
            return launchId == other.launchId && objectId == other.objectId && phyCoreId == other.phyCoreId &&
                   blockId == other.blockId && syncKind == other.syncKind && scope == other.scope &&
                   srcPipe == other.srcPipe && dstPipe == other.dstPipe && mode == other.mode;
        }
    };

    struct SyncPairKeyHash {
        size_t operator()(const SyncPairKey& key) const noexcept;
    };

    struct BufferOccupancyKey {
        uint64_t objectId = 0;
        uint32_t phyCoreId = 0;

        bool operator==(const BufferOccupancyKey& other) const noexcept;
    };

    struct BufferOccupancyKeyHash {
        size_t operator()(const BufferOccupancyKey& key) const noexcept;
    };

    struct LaunchSyncState {
        std::unordered_map<SyncPairKey, AclsanDeviceSyncData, SyncPairKeyHash> pending;
        std::unordered_map<BufferOccupancyKey, SyncPairKey, BufferOccupancyKeyHash> activeBuffers;
    };

    using Report = aclsan::cann::NpusanSynccheckReport;
    using Reports = std::vector<Report>;

    static SyncPairKey BuildExactKey(const AclsanDeviceSyncData& data);
    static BufferOccupancyKey BuildBufferOccupancyKey(const AclsanDeviceSyncData& data);
    static bool IsClose(uint32_t action);
    static bool IsValidEvent(const AclsanDeviceSyncData& data);
    static Report BuildMismatch(
        const AclsanDeviceSyncData& trigger, const AclsanDeviceSyncData* related, uint32_t reason);
    static aclsan::cann::NpusanSyncPoint ActualPoint(const AclsanDeviceSyncData& data);
    static aclsan::cann::NpusanSyncPoint ExpectedPoint(const AclsanDeviceSyncData& data, uint32_t reason);

    std::unordered_map<uint64_t, LaunchSyncState> launchStates_;
    SynccheckStats stats_{};
    logging::Logger* logger_ = nullptr;
};

} // namespace npu::sanitizer

#endif
