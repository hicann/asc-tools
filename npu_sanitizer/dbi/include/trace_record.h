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

constexpr uint16_t PIPELINE_SET_WAIT_FLAG = 1U;
constexpr uint16_t PIPELINE_GET_RLS_BUF = 2U;
constexpr uint16_t PIPELINE_MTE2 = 3U;
constexpr uint16_t PIPELINE_MTE3 = 4U;
constexpr uint16_t PIPELINE_FIXPIPE = 5U;
constexpr uint16_t PIPELINE_MTE1 = 6U;

__aicore__ inline void WriteTraceRecord(
    __gm__ uint8_t* memInfo, int64_t pc, uint32_t bid, uint16_t pipeline, uint16_t apiId, uint64_t arg0, uint64_t arg1,
    uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
    if (memInfo == nullptr) {
        return;
    }

    (void)bid;
    __gm__ aclsan::AscsanTraceBufferHeader* header = reinterpret_cast<__gm__ aclsan::AscsanTraceBufferHeader*>(memInfo);
    const uint32_t blockId = static_cast<uint32_t>(AscendC::GetBlockIdx());
    if (header->magic != aclsan::ASCSAN_TRACE_BUFFER_MAGIC_V1 || blockId >= header->blockCount ||
        header->recordsPerBlock == 0U) {
        return;
    }

    const uint64_t sliceBytes = sizeof(aclsan::AscsanTraceSliceHeader) +
                                static_cast<uint64_t>(header->recordsPerBlock) * sizeof(aclsan::AscsanRawTraceRecord);
    __gm__ uint8_t* sliceAddress =
        memInfo + sizeof(aclsan::AscsanTraceBufferHeader) + static_cast<uint64_t>(blockId) * sliceBytes;
    __gm__ aclsan::AscsanTraceSliceHeader* slice =
        reinterpret_cast<__gm__ aclsan::AscsanTraceSliceHeader*>(sliceAddress);
    const uint32_t index = slice->recordCount;
    if (index >= header->recordsPerBlock) {
        if (slice->overflowCount != UINT32_MAX) {
            slice->overflowCount = slice->overflowCount + 1U;
        }
        dcci(slice, cache_line_t::ENTIRE_DATA_CACHE, dcci_dst_t::CACHELINE_OUT);
        return;
    }

    __gm__ aclsan::AscsanRawTraceRecord* record =
        reinterpret_cast<__gm__ aclsan::AscsanRawTraceRecord*>(sliceAddress + sizeof(aclsan::AscsanTraceSliceHeader)) +
        index;
    record->pc = static_cast<uint64_t>(pc);
    record->args[0] = arg0;
    record->args[1] = arg1;
    record->args[2] = arg2;
    record->args[3] = arg3;
    record->args[4] = arg4;
    record->instrId = static_cast<uint64_t>(apiId);
    record->siteId = 0U;
    record->pipeline = pipeline;
    dcci(record, cache_line_t::ENTIRE_DATA_CACHE, dcci_dst_t::CACHELINE_OUT);
    slice->recordCount = index + 1U;
    dcci(slice, cache_line_t::ENTIRE_DATA_CACHE, dcci_dst_t::CACHELINE_OUT);
}

} // namespace aclsan
