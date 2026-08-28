/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef NPU_SANITIZER_SANITIZER_API_DEVICE_INSTR_COMMON_INSTRUCTION_ID_H_
#define NPU_SANITIZER_SANITIZER_API_DEVICE_INSTR_COMMON_INSTRUCTION_ID_H_

#include <cstdint>

namespace aclsan {

// Instruction IDs are stable across chips. Each architecture decoder defines the subset it handles.
// “已完成”表示该指令已完成 RawData -> ParamField -> CBData 全链路转换。
enum class InstructionId : uint32_t {
    // MTE2
    LoadGmToCbuf2DV2 = 72,          // 已完成：LoadGmToCbuf2DV2ParamField
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
    Mte2SrcPara = 124,              // MTE2_SRC_PARA
    NdDmaLoop0Stride = 132,         // LOOP0_STRIDE_NDDMA
    NdDmaLoop1Stride = 133,         // LOOP1_STRIDE_NDDMA
    NdDmaLoop2Stride = 134,         // LOOP2_STRIDE_NDDMA
    NdDmaLoop3Stride = 135,         // LOOP3_STRIDE_NDDMA
    NdDmaLoop4Stride = 136,         // LOOP4_STRIDE_NDDMA
    NdDmaOutToUbufB8 = 87,          // 已完成：NdDmaOutToUbufParamField
    NdDmaOutToUbufB16 = 88,         // 已完成：NdDmaOutToUbufParamField
    NdDmaOutToUbufB32 = 89,         // 已完成：NdDmaOutToUbufParamField
    SetL12DB16 = 149,               // SET_L1_2D.b16
    SetL12DB32 = 150,               // SET_L1_2D.b32
    SetMte2NzPara = 399,            // SET_MTE2_NZ_PARA

    // MTE3
    CopyUbufToGmAlignV2 = 83, // 已完成：CopyUbufToGmAlignV2ParamField
    CopyUbufToCbuf = 173,     // MOV_UB_TO_L1

    // FIX
    FixL0cToOutF32 = 91, // 已完成：FixL0cToOutParamField
    FixL0cToOutS32 = 92, // 已完成：FixL0cToOutParamField

    // REGISTER
    SetPadding = 392, // SET_PADDING

    // SYNCCHECK
    SetFlag = 440,    // 已完成：FlagParamField
    SetFlagI = 441,   // 已完成：FlagParamField
    WaitFlag = 442,   // 已完成：FlagParamField
    WaitFlagI = 443,  // 已完成：FlagParamField
    GetBuf = 448,     // 已完成：SyncBufParamField
    GetBufI = 449,    // 已完成：SyncBufParamField
    RlsBuf = 450,     // 已完成：SyncBufParamField
    RlsBufI = 451,    // 已完成：SyncBufParamField
    SetFlagV = 456,   // 已完成：FlagParamField
    SetFlagIV = 457,  // 已完成：FlagParamField
    WaitFlagV = 458,  // 已完成：FlagParamField
    WaitFlagIV = 459, // 已完成：FlagParamField
    GetBufV = 460,    // 已完成：SyncBufParamField
    GetBufIV = 461,   // 已完成：SyncBufParamField
    RlsBufV = 462,    // 已完成：SyncBufParamField
    RlsBufIV = 463,   // 已完成：SyncBufParamField
};

} // namespace aclsan

#endif // NPU_SANITIZER_SANITIZER_API_DEVICE_INSTR_COMMON_INSTRUCTION_ID_H_
