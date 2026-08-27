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
enum class InstructionId : uint32_t {
    // MTE2
    LoadGmToCbuf2DV2 = 72,          // LOAD_OUT_TO_L1_2DV2
    CopyGmToCbufV2 = 73,            // MOV_OUT_TO_L1_V2
    CopyGmToCbufAlignV2B8 = 74,     // MOV_OUT_TO_L1_ALIGN_V2.b8
    CopyGmToCbufAlignV2B16 = 75,    // MOV_OUT_TO_L1_ALIGN_V2.b16
    CopyGmToCbufAlignV2B32 = 76,    // MOV_OUT_TO_L1_ALIGN_V2.b32
    CopyGmToCbufMultiNd2NzB8 = 77,  // MOV_OUT_TO_L1_MULTI_ND2NZ.b8
    CopyGmToCbufMultiNd2NzB16 = 78, // MOV_OUT_TO_L1_MULTI_ND2NZ.b16
    CopyGmToCbufMultiNd2NzB32 = 79, // MOV_OUT_TO_L1_MULTI_ND2NZ.b32
    CopyGmToCbufMultiDn2NzB8 = 80,  // MOV_OUT_TO_L1_MULTI_DN2NZ.b8
    CopyGmToCbufMultiDn2NzB16 = 81, // MOV_OUT_TO_L1_MULTI_DN2NZ.b16
    CopyGmToCbufMultiDn2NzB32 = 82, // MOV_OUT_TO_L1_MULTI_DN2NZ.b32
    CopyGmToUbufAlignV2B8 = 84,     // MOV_OUT_TO_UB_ALIGN_V2.b8
    CopyGmToUbufAlignV2B16 = 85,    // MOV_OUT_TO_UB_ALIGN_V2.b16
    CopyGmToUbufAlignV2B32 = 86,    // MOV_OUT_TO_UB_ALIGN_V2.b32
    NdDmaOutToUbufB8 = 87,          // ND_DMA_OUT_TO_UB.b8
    NdDmaOutToUbufB16 = 88,         // ND_DMA_OUT_TO_UB.b16
    NdDmaOutToUbufB32 = 89,         // ND_DMA_OUT_TO_UB.b32
    SetL12DB16 = 149,               // SET_L1_2D.b16
    SetL12DB32 = 150,               // SET_L1_2D.b32

    // MTE3
    CopyUbufToGmAlignV2 = 83, // MOV_UB_TO_OUT_ALIGN_V2
    CopyUbufToCbuf = 173,     // MOV_UB_TO_L1

    // FIX
    FixL0cToOutF32 = 91, // FIX_L0C_TO_OUT.f32
    FixL0cToOutS32 = 92, // FIX_L0C_TO_OUT.s32

    // REGISTER
    SetPadding = 392, // SET_PADDING

    // SYNCCHECK
    SetFlag = 440,    // SET_FLAG.<src_pipe>.<dst_pipe>
    SetFlagI = 441,   // SET_FLAGI.<src_pipe>.<dst_pipe>
    WaitFlag = 442,   // WAIT_FLAG.<src_pipe>.<dst_pipe>
    WaitFlagI = 443,  // WAIT_FLAGI.<src_pipe>.<dst_pipe>
    GetBuf = 448,     // GET_BUF.<pipe>
    GetBufI = 449,    // GET_BUFI.<pipe>
    RlsBuf = 450,     // RLS_BUF.<pipe>
    RlsBufI = 451,    // RLS_BUFI.<pipe>
    SetFlagV = 456,   // SET_FLAG_V.<dst_pipe>
    SetFlagIV = 457,  // SET_FLAGI_V.<dst_pipe>
    WaitFlagV = 458,  // WAIT_FLAG_V.<src_pipe>
    WaitFlagIV = 459, // WAIT_FLAGI_V.<src_pipe>
    GetBufV = 460,    // GET_BUF_V
    GetBufIV = 461,   // GET_BUFI_V
    RlsBufV = 462,    // RLS_BUF_V
    RlsBufIV = 463,   // RLS_BUFI_V
};

} // namespace aclsan

#endif // NPU_SANITIZER_SANITIZER_API_DEVICE_INSTR_COMMON_INSTRUCTION_ID_H_
