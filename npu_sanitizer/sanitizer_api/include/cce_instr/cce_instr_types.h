/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef NPU_SANITIZER_SANITIZER_API_CCE_INSTR_TYPES_H_
#define NPU_SANITIZER_SANITIZER_API_CCE_INSTR_TYPES_H_

#include "aclsan/aclsan_cbdata_device.h"

#include <cstdint>

namespace sanitizer {

// instruction.txt 中 apiid 为 ? 的指令暂不进入枚举。
enum class CceInstructionId : uint32_t {
    // MTE2
    CopyGmToUbuf = 1,
    CopyGmToUbufAlignB16 = 6,
    CopyGmToUbufAlignB32 = 7,
    CopyGmToUbufAlignB8 = 8,
    CopyGmToCbufV2 = 73,            // 已完成：CopyGmToCbufV2ParamField
    CopyGmToCbufAlignV2B8 = 74,     // 已完成：CopyGmToCbufAlignV2ParamField
    CopyGmToCbufAlignV2B16 = 75,    // 已完成：CopyGmToCbufAlignV2ParamField
    CopyGmToCbufAlignV2B32 = 76,    // 已完成：CopyGmToCbufAlignV2ParamField
    CopyGmToCbufMultiNd2NzB8 = 77,  // 已完成：CopyGmToCbufMultiNd2NzParamField
    CopyGmToCbufMultiNd2NzB16 = 78, // 已完成：CopyGmToCbufMultiNd2NzParamField
    CopyGmToCbufMultiNd2NzB32 = 79, // 已完成：CopyGmToCbufMultiNd2NzParamField
    CopyGmToCbufMultiDn2NzB8 = 80,  // 已完成：CopyGmToCbufMultiDn2NzParamField
    CopyGmToCbufMultiDn2NzB16 = 81, // 已完成：CopyGmToCbufMultiDn2NzParamField
    CopyGmToCbufMultiDn2NzB32 = 82, // 已完成：CopyGmToCbufMultiDn2NzParamField
    CopyGmToUbufAlignV2B8 = 84,     // 已完成：CopyGmToUbufAlignV2ParamField
    CopyGmToUbufAlignV2B16 = 85,    // 已完成：CopyGmToUbufAlignV2ParamField
    CopyGmToUbufAlignV2B32 = 86,    // 已完成：CopyGmToUbufAlignV2ParamField
    NdDmaOutToUbufB8 = 87,          // CCE 指令：ND_DMA_OUT_TO_UB.b8
    NdDmaOutToUbufB16 = 88,         // CCE 指令：ND_DMA_OUT_TO_UB.b16
    NdDmaOutToUbufB32 = 89,         // CCE 指令：ND_DMA_OUT_TO_UB.b32
    LoadGmToCbuf2DV2 = 72,          // 已完成：LoadGmToCbuf2DV2ParamField
    SetL12DB16 = 149,               // CCE 指令：SET_L1_2D.b16
    SetL12DB32 = 150,               // CCE 指令：SET_L1_2D.b32

    // MTE3
    CopyUbufToGmAlignV2 = 83, // 已完成：CopyUbufToGmAlignV2ParamField
    CopyUbufToCbuf = 173,     // CCE 指令：MOV_UB_TO_L1

    // FIX
    FixL0cToOutF32 = 91, // CCE 指令：FIX_L0C_TO_OUT.f32
    FixL0cToOutS32 = 92, // CCE 指令：FIX_L0C_TO_OUT.s32

    // SYNCCHECK
    SetFlag = 440,       // CCE 指令：SET_FLAG.<src_pipe>.<dst_pipe>
    SetFlagI = 441,      // CCE 指令：SET_FLAGI.<src_pipe>.<dst_pipe>
    WaitFlag = 442,      // CCE 指令：WAIT_FLAG.<src_pipe>.<dst_pipe>
    WaitFlagI = 443,     // CCE 指令：WAIT_FLAGI.<src_pipe>.<dst_pipe>
    WaitFlagDev = 445,   // CCE 指令：WAIT_FLAG_DEV.<pipe>
    WaitFlagDevI = 446,  // CCE 指令：WAIT_FLAG_DEVI.<pipe>
    GetBuf = 448,        // CCE 指令：GET_BUF.<pipe>
    GetBufI = 449,       // CCE 指令：GET_BUFI.<pipe>
    RlsBuf = 450,        // CCE 指令：RLS_BUF.<pipe>
    RlsBufI = 451,       // CCE 指令：RLS_BUFI.<pipe>
    SetFlagV = 456,      // CCE 指令：SET_FLAG_V.<dst_pipe>
    SetFlagIV = 457,     // CCE 指令：SET_FLAGI_V.<dst_pipe>
    WaitFlagV = 458,     // CCE 指令：WAIT_FLAG_V.<src_pipe>
    WaitFlagIV = 459,    // CCE 指令：WAIT_FLAGI_V.<src_pipe>
    GetBufV = 460,       // CCE 指令：GET_BUF_V
    GetBufIV = 461,      // CCE 指令：GET_BUFI_V
    RlsBufV = 462,       // CCE 指令：RLS_BUF_V
    RlsBufIV = 463,      // CCE 指令：RLS_BUFI_V
    WaitFlagDevV = 469,  // CCE 指令：WAIT_FLAG_DEV_V
    WaitFlagDevIV = 470, // CCE 指令：WAIT_FLAG_DEVI_V
};

enum class NdNzConversionMode : uint8_t {
    ND2NZ,
    DN2NZ,
};

AclsanDevicePipeline GetCceInstructionPipeline(CceInstructionId instructionId) noexcept;

} // namespace sanitizer

#endif // NPU_SANITIZER_SANITIZER_API_CCE_INSTR_TYPES_H_
