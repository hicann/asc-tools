/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aclsan/aclsan_api.h"
#include "internal/aclsan_internal.h"
#include "internal/aclsan_log.h"

#include <array>

namespace aclsan {

namespace {

constexpr std::size_t kDeviceRecordCount = 8; // TODO: 等device数据可传回后可删除
constexpr uint32_t kPipeNotApplicable = 0;
constexpr uint32_t kPipeMte2 = 2;
constexpr uint32_t kPipeVector = 3;
constexpr uint32_t kInstrTypeSetFlag = 1;
constexpr uint32_t kInstrTypeWaitFlag = 2;
constexpr uint32_t kInstrTypeGetBuf = 3;
constexpr uint32_t kInstrTypeRlsBuf = 4;
constexpr uint32_t kSyncModeNotApplicable = 0;
constexpr uint32_t kSyncModeBlocking = 1;

DeviceRecord MakeDataCopyRecord(
    uint64_t sequence, uint64_t pc, uint32_t coreId, uint32_t blockId, uint64_t sourceAddress,
    uint64_t destinationAddress, uint64_t transferBytes)
{
    DeviceRecord record{};
    record.kind = DeviceRecordKind::DataCopy;
    record.sequence = sequence;
    record.serialNo = sequence;
    record.pc = pc;
    record.coreId = coreId;
    record.blockId = blockId;
    record.dataCopy = {sourceAddress, destinationAddress, transferBytes};
    return record;
}

DeviceRecord MakeSyncRecord(
    DeviceRecordKind kind, uint64_t pc, uint64_t instrExecId, uint64_t serialNo, uint32_t syncKind, uint32_t action,
    uint32_t srcPipe, uint32_t dstPipe, uint64_t objectId, uint32_t instrType, uint32_t mode)
{
    DeviceRecord record{};
    record.kind = kind;
    record.sequence = serialNo;
    record.serialNo = serialNo;
    record.pc = pc;
    record.sync = {instrExecId, syncKind, action, srcPipe, dstPipe, objectId, instrType, mode};
    return record;
}

DeviceRecord MakeFilteredRecord(DeviceRecordKind kind, uint64_t serialNo)
{
    DeviceRecord record{};
    record.kind = kind;
    record.sequence = serialNo;
    record.serialNo = serialNo;
    return record;
}

std::array<DeviceRecord, kDeviceRecordCount> MakeMockDeviceRecords()
{
    return {
        MakeDataCopyRecord(0, 0x100, 0, 0, 0x100000, 0x200000, 64),
        MakeDataCopyRecord(2, 0x108, 1, 1, 0x100040, 0x200040, 128),
        MakeSyncRecord(
            DeviceRecordKind::SetFlag, 0x1000, 1001, 10, ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG,
            ACLSAN_DEVICE_SYNC_ACTION_SET, kPipeMte2, kPipeVector, 7, kInstrTypeSetFlag, kSyncModeNotApplicable),
        MakeFilteredRecord(DeviceRecordKind::MemoryAlloc, 11),
        MakeSyncRecord(
            DeviceRecordKind::WaitFlag, 0x1010, 1003, 12, ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG,
            ACLSAN_DEVICE_SYNC_ACTION_WAIT, kPipeMte2, kPipeVector, 7, kInstrTypeWaitFlag, kSyncModeNotApplicable),
        MakeFilteredRecord(DeviceRecordKind::MemoryFree, 13),
        MakeSyncRecord(
            DeviceRecordKind::GetBuf, 0x1020, 1005, 14, ACLSAN_DEVICE_SYNC_KIND_GET_RLS_BUF,
            ACLSAN_DEVICE_SYNC_ACTION_GET, kPipeNotApplicable, kPipeVector, 3, kInstrTypeGetBuf, kSyncModeBlocking),
        MakeSyncRecord(
            DeviceRecordKind::RlsBuf, 0x1030, 1006, 15, ACLSAN_DEVICE_SYNC_KIND_GET_RLS_BUF,
            ACLSAN_DEVICE_SYNC_ACTION_RELEASE, kPipeNotApplicable, kPipeVector, 3, kInstrTypeRlsBuf, kSyncModeBlocking),
    };
}

void DispatchDeviceRecords(const std::array<DeviceRecord, kDeviceRecordCount>& records) noexcept
{
    for (const DeviceRecord& record : records) {
        if (record.kind == DeviceRecordKind::DataCopy) {
            DispatchDeviceMemoryAccessData(TranslateDeviceMemoryAccessData(record));
        } else if (
            record.kind == DeviceRecordKind::SetFlag || record.kind == DeviceRecordKind::WaitFlag ||
            record.kind == DeviceRecordKind::GetBuf || record.kind == DeviceRecordKind::RlsBuf) {
            DispatchDeviceSyncData(TranslateDeviceSyncData(record));
        }
    }
}

} // namespace

void DispatchMockDeviceRecords() noexcept
{
    const auto records = MakeMockDeviceRecords();
    ASC_SAN_DEBUG("[HOOK aclrtSynchronizeStream] records=%zu", records.size());
    DispatchDeviceRecords(records);
}

} // namespace aclsan
