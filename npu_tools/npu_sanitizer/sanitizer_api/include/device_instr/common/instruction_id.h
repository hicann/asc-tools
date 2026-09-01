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
// “已完成”表示搬运指令已完成 RawData -> ParamField -> CBData 转换，或 SET 指令已作为独立状态被后续
// 搬运指令的 CBData 转换消费。
enum class InstructionId : uint32_t {
    // MTE2
    LoadGmToCbuf2DV2 = 72,          // 已完成：LoadGmToCbuf2DV2ParamField（decompMode 0；非零跳过）
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
    Mte2SrcPara = 124,              // 已完成：Mte2SourceParamField（状态；支持正、零、负 stride）
    LoopSizeUbufToGm = 125,         // 已完成：DmaLoopSizeParamField（状态）
    Loop1StrideUbufToGm = 126,      // 已完成：DmaLoopStrideParamField（状态）
    Loop2StrideUbufToGm = 127,      // 已完成：DmaLoopStrideParamField（状态）
    LoopSizeGmToUbuf = 128,         // 已完成：DmaLoopSizeParamField（状态）
    Loop1StrideGmToUbuf = 129,      // 已完成：DmaLoopStrideParamField（状态）
    Loop2StrideGmToUbuf = 130,      // 已完成：DmaLoopStrideParamField（状态）
    NdDmaPadCount = 131,            // 已完成：NdDmaPadCountParamField（状态）
    NdDmaLoop0Stride = 132,         // 已完成：NdDmaLoopStrideParamField（状态）
    NdDmaLoop1Stride = 133,         // 已完成：NdDmaLoopStrideParamField（状态）
    NdDmaLoop2Stride = 134,         // 已完成：NdDmaLoopStrideParamField（状态）
    NdDmaLoop3Stride = 135,         // 已完成：NdDmaLoopStrideParamField（状态）
    NdDmaLoop4Stride = 136,         // 已完成：NdDmaLoopStrideParamField（状态）
    NdDmaOutToUbufB8 = 87,          // 已完成：NdDmaOutToUbufParamField
    NdDmaOutToUbufB16 = 88,         // 已完成：NdDmaOutToUbufParamField
    NdDmaOutToUbufB32 = 89,         // 已完成：NdDmaOutToUbufParamField
    Loop3Param = 90,                // 已完成：Loop3ParamField（状态）
    SetL12DB16 = 149,               // SET_L1_2D.b16
    SetL12DB32 = 150,               // SET_L1_2D.b32
    SetMte2NzPara = 399,            // 已完成：Mte2NzParamField（状态）

    // MTE3
    CopyUbufToGmAlignV2 = 83, // 已完成：CopyUbufToGmAlignV2ParamField
    CopyUbufToCbuf = 173, // 已完成：LocalMemoryTransferParamField（仅访问片上存储，不生成 GM CBData）

    // FIX
    FixL0cToOutF32 = 91,   // 已完成：FixL0cToOutParamField
    FixL0cToOutS32 = 92,   // 已完成：FixL0cToOutParamField
    CopyCbufToFbuf = 167,  // 已完成：LocalMemoryTransferParamField（仅访问片上存储，不生成 GM CBData）
    FixL0cToCbufF32 = 168, // 已完成：LocalMemoryTransferParamField（仅访问片上存储，不生成 GM CBData）
    FixL0cToCbufS32 = 169, // 已完成：LocalMemoryTransferParamField（仅访问片上存储，不生成 GM CBData）
    FixL0cToUbufF32 = 170, // 已完成：LocalMemoryTransferParamField（仅访问片上存储，不生成 GM CBData）
    FixL0cToUbufS32 = 171, // 已完成：LocalMemoryTransferParamField（仅访问片上存储，不生成 GM CBData）

    // REGISTER
    SetPadding = 392,          // SET_PADDING
    LoopSizeGmToCbuf = 394,    // 已完成：DmaLoopSizeParamField（状态）
    Loop1StrideGmToCbuf = 395, // 已完成：DmaLoopStrideParamField（状态）
    Loop2StrideGmToCbuf = 396, // 已完成：DmaLoopStrideParamField（状态）

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

constexpr bool IsDefinedInstructionId(uint32_t instructionId) noexcept
{
    switch (static_cast<InstructionId>(instructionId)) {
        case InstructionId::LoadGmToCbuf2DV2:
        case InstructionId::CopyGmToCbufV2:
        case InstructionId::CopyGmToCbufAlignV2B8:
        case InstructionId::CopyGmToCbufAlignV2B16:
        case InstructionId::CopyGmToCbufAlignV2B32:
        case InstructionId::CopyGmToCbufMultiNd2NzB8:
        case InstructionId::CopyGmToCbufMultiNd2NzB16:
        case InstructionId::CopyGmToCbufMultiNd2NzB32:
        case InstructionId::CopyGmToCbufMultiDn2NzB8:
        case InstructionId::CopyGmToCbufMultiDn2NzB16:
        case InstructionId::CopyGmToCbufMultiDn2NzB32:
        case InstructionId::CopyGmToUbufAlignV2B8:
        case InstructionId::CopyGmToUbufAlignV2B16:
        case InstructionId::CopyGmToUbufAlignV2B32:
        case InstructionId::Mte2SrcPara:
        case InstructionId::LoopSizeUbufToGm:
        case InstructionId::Loop1StrideUbufToGm:
        case InstructionId::Loop2StrideUbufToGm:
        case InstructionId::LoopSizeGmToUbuf:
        case InstructionId::Loop1StrideGmToUbuf:
        case InstructionId::Loop2StrideGmToUbuf:
        case InstructionId::NdDmaPadCount:
        case InstructionId::NdDmaLoop0Stride:
        case InstructionId::NdDmaLoop1Stride:
        case InstructionId::NdDmaLoop2Stride:
        case InstructionId::NdDmaLoop3Stride:
        case InstructionId::NdDmaLoop4Stride:
        case InstructionId::NdDmaOutToUbufB8:
        case InstructionId::NdDmaOutToUbufB16:
        case InstructionId::NdDmaOutToUbufB32:
        case InstructionId::Loop3Param:
        case InstructionId::SetL12DB16:
        case InstructionId::SetL12DB32:
        case InstructionId::SetMte2NzPara:
        case InstructionId::CopyUbufToGmAlignV2:
        case InstructionId::CopyUbufToCbuf:
        case InstructionId::FixL0cToOutF32:
        case InstructionId::FixL0cToOutS32:
        case InstructionId::CopyCbufToFbuf:
        case InstructionId::FixL0cToCbufF32:
        case InstructionId::FixL0cToCbufS32:
        case InstructionId::FixL0cToUbufF32:
        case InstructionId::FixL0cToUbufS32:
        case InstructionId::SetPadding:
        case InstructionId::LoopSizeGmToCbuf:
        case InstructionId::Loop1StrideGmToCbuf:
        case InstructionId::Loop2StrideGmToCbuf:
        case InstructionId::SetFlag:
        case InstructionId::SetFlagI:
        case InstructionId::WaitFlag:
        case InstructionId::WaitFlagI:
        case InstructionId::GetBuf:
        case InstructionId::GetBufI:
        case InstructionId::RlsBuf:
        case InstructionId::RlsBufI:
        case InstructionId::SetFlagV:
        case InstructionId::SetFlagIV:
        case InstructionId::WaitFlagV:
        case InstructionId::WaitFlagIV:
        case InstructionId::GetBufV:
        case InstructionId::GetBufIV:
        case InstructionId::RlsBufV:
        case InstructionId::RlsBufIV:
            return true;
        default:
            return false;
    }
}

} // namespace aclsan

#endif // NPU_SANITIZER_SANITIZER_API_DEVICE_INSTR_COMMON_INSTRUCTION_ID_H_
