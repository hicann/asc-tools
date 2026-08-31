/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#pragma once

#include "kernel_operator.h"
#include "trace_buffer_abi.h"

#define BUILD_DYNAMIC_PROBE 1

namespace aclsan {

__aicore__ inline void WriteTraceRecord(
    __gm__ uint8_t* memInfo, int64_t pc, uint32_t bid, DeviceInstructionCategory category, uint16_t pipeline,
    uint16_t apiId, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
    if (memInfo == nullptr) {
        return;
    }

    (void)bid;
    __gm__ aclsan::AclsanTraceBufferHeader* header = reinterpret_cast<__gm__ aclsan::AclsanTraceBufferHeader*>(memInfo);
    const uint32_t phyCoreId = static_cast<uint32_t>(get_coreid());
    const uint32_t blockCount = header->blockCount;
    const uint32_t physicalCoreCount = header->physicalCoreCount;
    if (header->magic != aclsan::ASCSAN_TRACE_BUFFER_MAGIC || blockCount == 0U || header->recordsPerCore == 0U ||
        physicalCoreCount == 0U || physicalCoreCount % aclsan::ASCSAN_PHYSICAL_CORE_TOPOLOGY_UNIT != 0U ||
        phyCoreId >= physicalCoreCount) {
        return;
    }

    const uint32_t blockId = static_cast<uint32_t>(AscendC::GetBlockIdx());
    const uint32_t coresPerPart = physicalCoreCount / aclsan::ASCSAN_PHYSICAL_CORE_PART_COUNT;
    const bool isAic = phyCoreId % coresPerPart < coresPerPart / aclsan::ASCSAN_AIC_CORE_RATIO_DENOMINATOR;
    const uint64_t blockLimit = isAic ? static_cast<uint64_t>(blockCount) : 2ULL * blockCount;
    if (static_cast<uint64_t>(blockId) >= blockLimit) {
        return;
    }

    const uint64_t sliceBytes = sizeof(aclsan::AclsanTraceSliceHeader) +
                                static_cast<uint64_t>(header->recordsPerCore) * sizeof(aclsan::AclsanRawTraceRecord);
    __gm__ uint8_t* sliceAddress =
        memInfo + sizeof(aclsan::AclsanTraceBufferHeader) + static_cast<uint64_t>(phyCoreId) * sliceBytes;
    __gm__ aclsan::AclsanTraceSliceHeader* slice =
        reinterpret_cast<__gm__ aclsan::AclsanTraceSliceHeader*>(sliceAddress);
    const uint32_t index = slice->recordCount;
    if (index >= header->recordsPerCore) {
        if (slice->overflowCount != UINT32_MAX) {
            slice->overflowCount = slice->overflowCount + 1U;
        }
        dcci(slice, cache_line_t::ENTIRE_DATA_CACHE, dcci_dst_t::CACHELINE_OUT);
        return;
    }

    __gm__ aclsan::AclsanRawTraceRecord* record =
        reinterpret_cast<__gm__ aclsan::AclsanRawTraceRecord*>(sliceAddress + sizeof(aclsan::AclsanTraceSliceHeader)) +
        index;
    record->pc = static_cast<uint64_t>(pc);
    record->args[0] = arg0;
    record->args[1] = arg1;
    record->args[2] = arg2;
    record->args[3] = arg3;
    record->args[4] = arg4;
    record->instrId = apiId;
    record->siteId = 0U;
    record->category = category;
    record->pipeline = pipeline;
    record->blockId = blockId;
    record->reserved = 0U;
    dcci(record, cache_line_t::ENTIRE_DATA_CACHE, dcci_dst_t::CACHELINE_OUT);
    slice->phyCoreId = phyCoreId;
    slice->recordCount = index + 1U;
    dcci(slice, cache_line_t::ENTIRE_DATA_CACHE, dcci_dst_t::CACHELINE_OUT);
}

} // namespace aclsan
