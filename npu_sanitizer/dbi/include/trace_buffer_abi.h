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

constexpr uint64_t ASCSAN_TRACE_BUFFER_MAGIC = 0x41534353414E3035ULL;
constexpr uint32_t ASCSAN_TRACE_RECORDS_PER_CORE_DEFAULT = 256U;
constexpr uint32_t ASCSAN_PHYSICAL_CORE_PART_COUNT = 2U;
constexpr uint32_t ASCSAN_AIC_CORE_RATIO_DENOMINATOR = 3U;
constexpr uint32_t ASCSAN_PHYSICAL_CORE_TOPOLOGY_UNIT =
    ASCSAN_PHYSICAL_CORE_PART_COUNT * ASCSAN_AIC_CORE_RATIO_DENOMINATOR;

enum class DeviceInstructionCategory : uint16_t {
    Invalid = 0,
    MemoryAccess,
    Synchronization,
    RegisterState,
};

constexpr bool IsTracePhysicalCoreTopologyValid(uint32_t physicalCoreCount)
{
    return physicalCoreCount != 0U && physicalCoreCount % ASCSAN_PHYSICAL_CORE_TOPOLOGY_UNIT == 0U;
}

constexpr bool IsAicPhysicalCore(uint32_t phyCoreId, uint32_t physicalCoreCount)
{
    if (!IsTracePhysicalCoreTopologyValid(physicalCoreCount) || phyCoreId >= physicalCoreCount) {
        return false;
    }
    const uint32_t coresPerPart = physicalCoreCount / ASCSAN_PHYSICAL_CORE_PART_COUNT;
    return phyCoreId % coresPerPart < coresPerPart / ASCSAN_AIC_CORE_RATIO_DENOMINATOR;
}

constexpr bool IsTraceBlockIdValid(uint64_t blockId, bool isAic, uint32_t blockCount)
{
    if (blockCount == 0U || blockId > UINT32_MAX) {
        return false;
    }
    const uint64_t blockLimit = isAic ? static_cast<uint64_t>(blockCount) : 2ULL * blockCount;
    return blockId < blockLimit;
}

// Buffer 的三层布局：
//   [AclsanTraceBufferHeader]
//   [physical core slice 0: AclsanTraceSliceHeader][record 0] ... [record recordsPerCore - 1]
//   ...
//
// 大小及偏移计算：
//   sliceBytes  = sizeof(AclsanTraceSliceHeader) +
//                 recordsPerCore * sizeof(AclsanRawTraceRecord)
//   sliceCount  = physicalCoreCount
//   bufferBytes = sizeof(AclsanTraceBufferHeader) + sliceCount * sliceBytes
//   sliceOffset(sliceIndex) = sizeof(AclsanTraceBufferHeader) + sliceIndex * sliceBytes
//
// blockCount 保存 launch blockDim A，只用于校验 record 携带的逻辑 blockId。物理核分为两部分，
// 每部分的前 1/3 为 AIC、后 2/3 为 AIV。sliceIndex 等于 get_coreid()；每个物理核执行的
// 所有逻辑 block 共享一个 slice，有效记录下标范围为 [0, recordCount)。
struct AclsanTraceBufferHeader {
    uint64_t magic; // Trace buffer 格式标识，固定为 ASCSAN_TRACE_BUFFER_MAGIC。
    // launchId在多次launch的时候区分不出来id，先考虑单kernel 单launch
    uint64_t launchId;          // Host 分配的标识，用于将 buffer 与一次 kernel launch 关联。
    uint32_t blockCount;        // 本次 kernel launch 的 blockDim，即 A。
    uint32_t recordsPerCore;    // 每个物理核 slice 最多可保存的原始 trace 记录数。
    uint32_t physicalCoreCount; // Host 查询得到的 Cube 与 Vector 物理核数量之和。
    uint32_t reserved;          // 保持 header 的 64 位对齐，固定写 0。
};

// 单个物理 slice 的写入状态。phyCoreId 仅在 recordCount 大于 0 时有效。
struct AclsanTraceSliceHeader {
    uint32_t recordCount;   // 当前 slice 中已写入的有效记录数。
    uint32_t overflowCount; // Slice 容量用尽后丢弃的记录数，最大饱和至 UINT32_MAX。
    uint32_t phyCoreId;     // Device 通过 get_coreid() 获取的物理核 ID。
    uint32_t reserved;      // 保持后续 AclsanRawTraceRecord 的 64 位对齐，固定写 0。
};

// Device probe 生成的一条原始 trace 记录，固定占用 72 字节：
//   [pc: 8][args[0..4]: 40][instrId: 8][siteId: 4][category: 2][pipeline: 2][blockId: 4][reserved: 4]
struct AclsanRawTraceRecord {
    uint64_t pc;                        // 被插桩指令的 PC。
    uint64_t args[5];                   // Probe 捕获的指令参数，具体含义由对应指令协议定义。
    uint64_t instrId;                   // 指令标识；probe 写入 apiId，Host 按 CceInstructionId 解析。
    uint32_t siteId;                    // 插桩点标识，用于关联 patch 元数据；当前写入固定填 0。
    DeviceInstructionCategory category; // 指令功能分类。
    uint16_t pipeline;                  // 指令实际执行的 Device PIPE_* 值。
    uint32_t blockId;                   // AscendC::GetBlockIdx() 返回的逻辑 block ID。
    uint32_t reserved;                  // 显式保留字段，固定写 0。
};

constexpr bool TraceSliceBytes(uint32_t recordsPerCore, size_t* bytes)
{
    if (recordsPerCore == 0 || bytes == nullptr ||
        static_cast<size_t>(recordsPerCore) >
            (SIZE_MAX - sizeof(AclsanTraceSliceHeader)) / sizeof(AclsanRawTraceRecord)) {
        return false;
    }
    *bytes = sizeof(AclsanTraceSliceHeader) + static_cast<size_t>(recordsPerCore) * sizeof(AclsanRawTraceRecord);
    return true;
}

constexpr bool TraceBufferBytes(uint32_t physicalCoreCount, uint32_t recordsPerCore, size_t* bytes)
{
    size_t sliceBytes = 0;
    if (bytes == nullptr || !IsTracePhysicalCoreTopologyValid(physicalCoreCount) ||
        !TraceSliceBytes(recordsPerCore, &sliceBytes)) {
        return false;
    }
    if (static_cast<size_t>(physicalCoreCount) > (SIZE_MAX - sizeof(AclsanTraceBufferHeader)) / sliceBytes) {
        return false;
    }
    *bytes = sizeof(AclsanTraceBufferHeader) + static_cast<size_t>(physicalCoreCount) * sliceBytes;
    return true;
}

} // namespace aclsan
