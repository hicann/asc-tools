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

#include <cstddef>
#include <cstdint>

namespace aclsan {

constexpr uint64_t ASCSAN_TRACE_BUFFER_MAGIC_V1 = 0x41534353414E3031ULL;
constexpr uint32_t ASCSAN_TRACE_RECORDS_PER_BLOCK_DEFAULT = 256;

// Buffer 的三层布局：
//   [AscsanTraceBufferHeader]
//   [block 0: AscsanTraceSliceHeader][record 0] ... [record recordsPerBlock - 1]
//   [block 1: AscsanTraceSliceHeader][record 0] ... [record recordsPerBlock - 1]
//   ...
//
// 大小及偏移计算：
//   sliceBytes  = sizeof(AscsanTraceSliceHeader) +
//                 recordsPerBlock * sizeof(AscsanRawTraceRecord)
//   bufferBytes = sizeof(AscsanTraceBufferHeader) + blockCount * sliceBytes
//   sliceOffset(blockId) = sizeof(AscsanTraceBufferHeader) + blockId * sliceBytes
//   recordOffset(blockId, index) = sliceOffset(blockId) + sizeof(AscsanTraceSliceHeader) +
//                                  index * sizeof(AscsanRawTraceRecord)
//
// 每个 block 独占一个定长 slice，slice 下标即 block ID。有效记录下标范围为
// [0, recordCount)，且 recordCount <= recordsPerBlock；容量用尽后的记录只累计到 overflowCount。
struct AscsanTraceBufferHeader {
    uint64_t magic; // Trace buffer 的格式及版本标识，例如 ASCSAN_TRACE_BUFFER_MAGIC_V1。
    // launchId在多次launch的时候区分不出来id，先考虑单kernel 单launch
    uint64_t launchId;        // Host 分配的标识，用于将 buffer 与一次 kernel launch 关联。
    uint32_t blockCount;      // Buffer 中分配的 block slice 数量。
    uint32_t recordsPerBlock; // 每个 block slice 最多可保存的原始 trace 记录数。
};

// 单个 block slice 的写入状态；slice 在 buffer 中的下标也是其中记录所属的 block ID。
struct AscsanTraceSliceHeader {
    uint32_t recordCount;   // 当前 slice 中已写入的有效记录数。
    uint32_t overflowCount; // Slice 容量用尽后丢弃的记录数，最大饱和至 UINT32_MAX。
};

// Device probe 生成的一条原始 trace 记录，固定占用 64 字节：
//   [pc: 8][args[0..4]: 40][instrId: 8][siteId: 4][pipeline: 4]
struct AscsanRawTraceRecord {
    uint64_t pc;       // 被插桩指令的 PC。
    uint64_t args[5];  // Probe 捕获的指令参数，具体含义由对应指令协议定义。
    uint64_t instrId;  // 指令标识；probe 写入 apiId，Host 按 CceInstructionId 解析。
    uint32_t siteId;   // 插桩点标识，用于关联 patch 元数据；当前写入路径固定填 0。
    uint32_t pipeline; // WriteTraceRecord 调用方传入的 probe/流水线类别。
};

// 校验长度可放ut
static_assert(sizeof(AscsanTraceBufferHeader) == 24, "trace buffer header ABI changed");
static_assert(sizeof(AscsanTraceSliceHeader) == 8, "trace slice header ABI changed");
static_assert(sizeof(AscsanRawTraceRecord) == 64, "raw trace record ABI changed");
static_assert(offsetof(AscsanRawTraceRecord, pc) == 0, "raw trace PC offset changed");
static_assert(offsetof(AscsanRawTraceRecord, args) == 8, "raw trace arguments offset changed");
static_assert(offsetof(AscsanRawTraceRecord, instrId) == 48, "raw trace instruction ID offset changed");
static_assert(offsetof(AscsanRawTraceRecord, siteId) == 56, "raw trace site ID offset changed");
static_assert(offsetof(AscsanRawTraceRecord, pipeline) == 60, "raw trace pipeline offset changed");

constexpr bool TraceSliceBytes(uint32_t recordsPerBlock, size_t* bytes)
{
    if (recordsPerBlock == 0 || bytes == nullptr ||
        static_cast<size_t>(recordsPerBlock) >
            (SIZE_MAX - sizeof(AscsanTraceSliceHeader)) / sizeof(AscsanRawTraceRecord)) {
        return false;
    }
    *bytes = sizeof(AscsanTraceSliceHeader) + static_cast<size_t>(recordsPerBlock) * sizeof(AscsanRawTraceRecord);
    return true;
}

constexpr bool TraceBufferBytes(uint32_t blockCount, uint32_t recordsPerBlock, size_t* bytes)
{
    size_t sliceBytes = 0;
    if (blockCount == 0 || bytes == nullptr || !TraceSliceBytes(recordsPerBlock, &sliceBytes) ||
        static_cast<size_t>(blockCount) > (SIZE_MAX - sizeof(AscsanTraceBufferHeader)) / sliceBytes) {
        return false;
    }
    *bytes = sizeof(AscsanTraceBufferHeader) + static_cast<size_t>(blockCount) * sliceBytes;
    return true;
}

} // namespace aclsan
