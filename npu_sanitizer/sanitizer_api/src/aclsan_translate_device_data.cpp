/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "internal/aclsan_internal.h"

#include <cstdint>

namespace aclsan {

namespace {

constexpr uint64_t kMockLaunchId = 42;
constexpr uint32_t kMockDeviceId = 0;
constexpr uint32_t kBlockTypeAiv = 1;
constexpr uint32_t kPipeMte2 = 2;

AclsanDeviceEventHeader MakeDeviceEventHeader(const DeviceRecord& record) noexcept
{
    return {
        ACLSAN_API_VERSION,
        static_cast<uint32_t>(sizeof(AclsanDeviceMemoryAccessData)),
        kMockLaunchId,
        record.pc,
        static_cast<uint32_t>(record.sequence),
        0,
        record.sequence,
        record.serialNo,
        kMockDeviceId,
        record.coreId,
        record.blockId,
        kBlockTypeAiv,
        kPipeMte2,
        0};
}

AclsanDeviceMemoryAccessData MakeDeviceMemoryAccessData(
    const DeviceRecord& record, uint64_t address, uint32_t accessMode, uint32_t accessIndex) noexcept
{
    AclsanDeviceMemoryAccessData callbackData{};
    callbackData.header = MakeDeviceEventHeader(record);
    callbackData.address = address;
    callbackData.memorySpace = ACLSAN_DEVICE_MEMORY_SPACE_GM;
    callbackData.accessMode = accessMode;
    callbackData.accessIndex = accessIndex;
    callbackData.accessCount = kDataCopyAccessCount;
    callbackData.layoutKind = ACLSAN_MEM_LAYOUT_RANGE;
    callbackData.layout.range.bytes = record.dataCopy.transferBytes;
    return callbackData;
}

} // namespace

DeviceMemoryAccessDataArray TranslateDeviceMemoryAccessData(const DeviceRecord& record) noexcept
{
    return {
        MakeDeviceMemoryAccessData(record, record.dataCopy.sourceAddress, ACLSAN_DEVICE_MEMORY_ACCESS_READ, 0),
        MakeDeviceMemoryAccessData(record, record.dataCopy.destinationAddress, ACLSAN_DEVICE_MEMORY_ACCESS_WRITE, 1),
    };
}

AclsanDeviceSyncData TranslateDeviceSyncData(const DeviceRecord& record) noexcept
{
    const DeviceSyncRecord& source = record.sync;
    const uint32_t scope = source.syncKind == ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG ? ACLSAN_DEVICE_SYNC_SCOPE_BLOCK :
                                                                                      ACLSAN_DEVICE_SYNC_SCOPE_PIPE;
    return {record.pc,       source.instrExecId,
            kMockLaunchId,   source.instrType,
            record.blockId,  UINT32_MAX,
            source.syncKind, source.action,
            scope,           source.srcPipe,
            source.dstPipe,  source.mode,
            source.objectId, {0, 0}};
}

} // namespace aclsan
