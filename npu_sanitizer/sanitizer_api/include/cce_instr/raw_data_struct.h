/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef DEMO_PROBE_ADD_PROBE_RAW_DATA_STRUCT_H_
#define DEMO_PROBE_ADD_PROBE_RAW_DATA_STRUCT_H_

#include <cstdint>

namespace sanitizer {

typedef struct AscsanTraceBufferHeader {
    uint64_t magic;           /* 包含格式版本的魔数，例如 ASCSAN_TRACE_BUFFER_MAGIC_V1。 */
    uint64_t launchId;        /* Host 分配的 kernel launch 关联标识。 */
    uint32_t blockCount;      /* buffer 中分配的 block slice 数量。 */
    uint32_t recordsPerBlock; /* 每个 slice 最多保存的 trace 记录数。 */
} AscsanTraceBufferHeader;
/*
 * 单个 block slice 的写入状态。
 */
typedef struct AscsanTraceSliceHeader {
    uint32_t recordCount;   /* 当前 slice 中已经写入的有效记录数。 */
    uint32_t overflowCount; /* 因 slice 容量不足而未能写入的记录数。 */
} AscsanTraceSliceHeader;
/*
 * Device probe 生成的一条原始 trace 记录。
 */
typedef struct AscsanRawTraceRecord {
    uint32_t blockId;  // TODO: 其实应该别的地方传出来
    uint64_t pc;       /* 被插桩指令的 PC。 */
    uint64_t instrId;  // TODO: 对应的CceInstructionId
    uint64_t args[6];  /* Probe 参数，具体含义由对应 pipeline 的协议定义。 */
    uint32_t siteId;   /* 插桩点标识，用于关联 AscsanPatchSiteInfo。 */
    uint32_t pipeline; /* 插桩流水线，取值见 AclsanDevicePipeline */
} AscsanRawTraceRecord;

// Operand: [dst], [src], config0, config1
// 适用指令：
// - MOV_OUT_TO_L1_ALIGN_V2.<b8/b16/b32>      DONE
// - MOV_OUT_TO_L1_MULTI_DN2NZ.<b8/b16/b32>   DONE
// - MOV_OUT_TO_L1_MULTI_ND2NZ.<b8/b16/b32>   DONE
// - MOV_OUT_TO_L1_V2                         DONE
// - MOV_OUT_TO_UB_ALIGN_V2.<b8/b16/b32>      DONE
// - LOAD_OUT_TO_L1_2DV2                      DONE
// - MOV_UB_TO_OUT_ALIGN_V2                   DONE
// - FIX_L0C_TO_OUT.<f32/s32>
struct CopyOperand {
    uint32_t instr_id = 0; // 指令id
    uint64_t dstAddr = 0;  // args[0]
    uint64_t srcAddr = 0;  // args[1]
    uint64_t config0 = 0;  // args[2]
    uint64_t config1 = 0;  // args[3]
};

// Operand: [dst], [src], config, secConfig
// 适用指令：
// - ND_DMA_OUT_TO_UB.<b8/b16/b32>
// - ND_DMA_UB_TO_UB.<b8/b16/b32>
struct NdDmaOperand {
    uint32_t instr_id = 0;
    uint64_t dstAddr = 0;
    uint64_t srcAddr = 0;
    uint64_t config = 0;
    uint64_t secConfig = 0;
};

// Operand: [dst], config
// 适用指令：SET_L1_2D.<b16/b32>
struct SetL12DOperand {
    uint32_t instr_id = 0;
    uint64_t dstAddr = 0;
    uint64_t config = 0;
};

// Operand: [dst], [src], config0, config1, #transpose
// 适用指令：
// - LOAD_L1_TO_L0A_2DV2.<b4/b8/b16/b32>
// - LOAD_L1_TO_L0B_2DV2.<b4/b8/b16/b32>
struct CopyWithTransposeOperand {
    uint32_t instr_id = 0;
    uint64_t dstAddr = 0;
    uint64_t srcAddr = 0;
    uint64_t config0 = 0;
    uint64_t config1 = 0;
    uint64_t transpose = 0;
};

// Operand: [dst], [src], config, fracStride
// 适用指令：LOAD_L1_TO_L0B_2D_TRANSPOSE.<b4/b8/b16/b32>
struct CopyWithFracStrideOperand {
    uint32_t instr_id = 0;
    uint64_t dstAddr = 0;
    uint64_t srcAddr = 0;
    uint64_t config = 0;
    uint64_t fracStride = 0;
};

// Operand: [src_pipe], [dst_pipe], eventID
// 适用指令：SET_FLAG、SET_FLAGI、WAIT_FLAG、WAIT_FLAGI
struct FlagOperand {
    uint32_t instr_id = 0;
    uint32_t srcPipe = 0;
    uint32_t dstPipe = 0;
    uint64_t eventId = 0;
};

// Operand: [pipe], flagID
// 适用指令：WAIT_FLAG_DEV、WAIT_FLAG_DEVI
struct DeviceFlagOperand {
    uint32_t instr_id = 0;
    uint32_t pipe = 0;
    uint64_t flagId = 0;
};

// Operand: [pipe], eventID
// 适用指令：SET_FLAG_V、SET_FLAGI_V、WAIT_FLAG_V、WAIT_FLAGI_V
struct VectorFlagOperand {
    uint32_t instr_id = 0;
    uint32_t pipe = 0;
    uint64_t eventId = 0;
};

// Operand: flagID
// 适用指令：WAIT_FLAG_DEV_V、WAIT_FLAG_DEVI_V
struct VectorDeviceFlagOperand {
    uint32_t instr_id = 0;
    uint64_t flagId = 0;
};

// Operand: [src_pipe], [dst_pipe], eventID, memory, isVirtual
// 适用指令：HSET_FLAG、HSET_FLAGI、HWAIT_FLAG、HWAIT_FLAGI
struct HardwareFlagOperand {
    uint32_t instr_id = 0;
    uint32_t srcPipe = 0;
    uint32_t dstPipe = 0;
    uint64_t eventId = 0;
    uint64_t memory = 0;
    bool isVirtual = false;
};

// Operand: [pipe], [bufId], mode
// 适用指令：GET_BUF、GET_BUFI、RLS_BUF、RLS_BUFI
struct BufferOperand {
    uint32_t instr_id = 0;
    uint32_t pipe = 0;
    uint64_t bufId = 0;
    uint8_t mode = 0;
};

// Operand: [bufId], mode
// 适用指令：GET_BUF_V、GET_BUFI_V、RLS_BUF_V、RLS_BUFI_V
struct VectorBufferOperand {
    uint32_t instr_id = 0;
    uint64_t bufId = 0;
    uint8_t mode = 0;
};

// MTE2
using CopyGmToCbufAlignV2Operand = CopyOperand;
using CopyGmToCbufMultiDn2NzOperand = CopyOperand;
using CopyGmToCbufMultiNd2NzOperand = CopyOperand;
using CopyGmToCbufV2Operand = CopyOperand;
using CopyGmToUbufAlignV2Operand = CopyOperand;
using LoadGmToCa2DV2Operand = CopyOperand;
using LoadGmToCb2DV2Operand = CopyOperand;
using LoadGmToCbuf2DV2Operand = CopyOperand;
using NdDmaOutToUbufOperand = NdDmaOperand;
using NdDmaUbufToUbufOperand = NdDmaOperand;

// MTE3
using CopyUbufToGmAlignV2Operand = CopyOperand;

// MTE1

using Img2ColCbufToCaOperand = CopyOperand;
using Img2ColCbufToCbOperand = CopyOperand;
using LoadCbufToCa2DV2Operand = CopyWithTransposeOperand;
using LoadCbufToCb2DV2Operand = CopyWithTransposeOperand;
using LoadCbufToCbTransposeOperand = CopyWithFracStrideOperand;

// FIX
using FixL0cToL1Operand = CopyOperand;
using FixL0cToOutOperand = CopyOperand;
using FixL0cToUbufOperand = CopyOperand;

// SYNCCHECK
using SetFlagOperand = FlagOperand;
using SetFlagIOperand = FlagOperand;
using WaitFlagOperand = FlagOperand;
using WaitFlagIOperand = FlagOperand;
using WaitFlagDevOperand = DeviceFlagOperand;
using WaitFlagDevIOperand = DeviceFlagOperand;
using SetFlagVOperand = VectorFlagOperand;
using SetFlagIVOperand = VectorFlagOperand;
using WaitFlagVOperand = VectorFlagOperand;
using WaitFlagIVOperand = VectorFlagOperand;
using WaitFlagDevVOperand = VectorDeviceFlagOperand;
using WaitFlagDevIVOperand = VectorDeviceFlagOperand;
using HSetFlagOperand = HardwareFlagOperand;
using HSetFlagIOperand = HardwareFlagOperand;
using HWaitFlagOperand = HardwareFlagOperand;
using HWaitFlagIOperand = HardwareFlagOperand;
using GetBufOperand = BufferOperand;
using GetBufIOperand = BufferOperand;
using RlsBufOperand = BufferOperand;
using RlsBufIOperand = BufferOperand;
using GetBufVOperand = VectorBufferOperand;
using GetBufIVOperand = VectorBufferOperand;
using RlsBufVOperand = VectorBufferOperand;
using RlsBufIVOperand = VectorBufferOperand;

} // namespace sanitizer

#endif // DEMO_PROBE_ADD_PROBE_RAW_DATA_STRUCT_H_
