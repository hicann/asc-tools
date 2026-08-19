/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_DEVICE_RECORD_H
#define ACLSAN_DEVICE_RECORD_H

#include <cstdint>

namespace aclsan {

enum class DeviceRecordKind : uint32_t {
    DataCopy,
    SetFlag,
    WaitFlag,
    GetBuf,
    RlsBuf,
    MemoryAlloc,
    MemoryFree,
};

struct DataCopyRecord {
    uint64_t sourceAddress;
    uint64_t destinationAddress;
    uint64_t transferBytes;
};

struct DeviceSyncRecord {
    uint64_t instrExecId;
    uint32_t syncKind;
    uint32_t action;
    uint32_t srcPipe;
    uint32_t dstPipe;
    uint64_t objectId;
    uint32_t instrType;
    uint32_t mode;
};

struct DeviceRecord {
    DeviceRecordKind kind;
    uint64_t sequence;
    uint64_t serialNo;
    uint64_t pc;
    uint32_t coreId;
    uint32_t blockId;
    DataCopyRecord dataCopy;
    DeviceSyncRecord sync;
};

} // namespace aclsan

#endif
