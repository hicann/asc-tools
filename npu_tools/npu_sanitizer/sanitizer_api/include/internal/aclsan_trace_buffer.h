/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#pragma once

#include "aclsan/aclsan_cbdata_device.h"
#include "dbi/trace_buffer_abi.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aclsan {

struct ParsedTraceRecord {
    AclsanRawTraceRecord record{};
    uint64_t instrExecId = 0; // 该(blockType, blockId)运行的第几条probe指令，从1开始
    uint64_t launchId = 0;
    uint32_t blockId = 0;
    uint32_t blockType = ACLSAN_DEVICE_BLOCK_TYPE_AICORE;
    uint32_t phyCoreId = 0;
    uint32_t deviceId = 0;
};

struct TraceBufferParseResult {
    bool ok = false;
    uint64_t overflowCount = 0;
    std::vector<ParsedTraceRecord> records;
    std::string error;
};

bool InitializeTraceBuffer(
    std::vector<uint8_t>& buffer, uint32_t physicalCoreCount, uint32_t blockCount, uint32_t recordsPerCore,
    uint64_t launchId, std::string& error);

TraceBufferParseResult ParseTraceBuffer(
    const uint8_t* buffer, size_t bytes, uint32_t expectedPhysicalCoreCount, uint32_t expectedBlockCount,
    uint32_t expectedRecordsPerCore, uint64_t expectedLaunchId, uint32_t deviceId);

} // namespace aclsan
