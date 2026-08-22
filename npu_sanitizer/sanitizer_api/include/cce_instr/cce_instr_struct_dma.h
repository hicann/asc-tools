/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef NPU_SANITIZER_SANITIZER_API_CCE_INSTR_STRUCT_DMA_H_
#define NPU_SANITIZER_SANITIZER_API_CCE_INSTR_STRUCT_DMA_H_

#include "cce_instr_types.h"
#include "raw_data_struct.h"

#include <cstdint>

namespace sanitizer {

// 位域注释规则：
// 1. bit index 从 0 开始。
// 2. [begin:end] 是左闭右开区间，读取 begin 到 end - 1；例如 [4:24] 读取 bit 4~23。
// 3. [bit] 只读取指定的一个 bit；例如 [58] 只读取 bit 58。
// 4. 未被任何区间覆盖的 bit 是空隙位，转换时忽略，不自动并入相邻字段。
// 5. 新增字段时，区间必须满足 0 <= begin < end <= 64，并明确写出来源 config0 或 config1。

// 已更新 参数已确认
// 对应 CCE 指令：
// - MOV_OUT_TO_L1_ALIGN_V2.<b8/b16/b32>
// - MOV_OUT_TO_UB_ALIGN_V2.<b8/b16/b32>
template <AclsanDeviceMemorySpace SrcSpace, AclsanDeviceMemorySpace DstSpace>
struct MovAlignV2ParamField {
    static constexpr AclsanDeviceMemorySpace srcPos = SrcSpace;
    static constexpr AclsanDeviceMemorySpace dstPos = DstSpace;

    uint32_t instr_id = 0; // 对应CopyOperand的instr_id
    uint64_t dstAddr = 0;
    uint64_t srcAddr = 0;
    uint8_t sid = 0;               // config0 [0:3]
    uint32_t burstNum = 0;         // config0 [4:24]
    uint32_t burstLen = 0;         // config0 [25:45]
    uint8_t leftPaddingCount = 0;  // config0 [46:51]
    uint8_t rightPaddingCount = 0; // config0 [52:57]
    bool dataSelectBit = false;    // config0 [58]
    uint8_t l2CacheControl = 0;    // config0 [60:63]
    uint64_t burstSrcStride = 0;   // config1 [0:39]
    uint32_t burstDstStride = 0;   // config1 [40:60]
};

using CopyGmToUbufAlignV2ParamField =
    MovAlignV2ParamField<ACLSAN_DEVICE_MEMORY_SPACE_GM, ACLSAN_DEVICE_MEMORY_SPACE_UB>;
using CopyGmToCbufAlignV2ParamField =
    MovAlignV2ParamField<ACLSAN_DEVICE_MEMORY_SPACE_GM, ACLSAN_DEVICE_MEMORY_SPACE_L1>;

// 已更新 参数已确认
// 对应 CCE 指令：
// - MOV_OUT_TO_L1_MULTI_DN2NZ.<b8/b16/b32>
// - MOV_OUT_TO_L1_MULTI_ND2NZ.<b8/b16/b32>
template <NdNzConversionMode ConversionMode>
struct CopyGmToCbufMultiParamField {
    static constexpr NdNzConversionMode conversionMode = ConversionMode;

    uint32_t instr_id = 0;
    uint64_t dstAddr = 0;
    uint64_t srcAddr = 0;
    uint8_t sid = 0;             // config0 [0:3]
    uint64_t loop1SrcStride = 0; // config0 [4:43]
    uint8_t l2CacheControl = 0;  // config0 [44:47]
    uint16_t nValue = 0;         // config0 [48:63]
    uint32_t dValue = 0;         // config1 [0:20]
    uint64_t loop4SrcStride = 0; // config1 [21:60]
    bool smallC0Enable = false;  // config1 [61]
};

using CopyGmToCbufMultiDn2NzParamField = CopyGmToCbufMultiParamField<NdNzConversionMode::DN2NZ>;
using CopyGmToCbufMultiNd2NzParamField = CopyGmToCbufMultiParamField<NdNzConversionMode::ND2NZ>;

// 已更新 参数已确认
// 对应 CCE 指令：MOV_OUT_TO_L1_V2
struct CopyGmToCbufV2ParamField {
    uint32_t instr_id = 0;
    uint64_t dstAddr = 0;
    uint64_t srcAddr = 0;
    uint8_t sid = 0;             // config0 [0:3]
    uint32_t burstNum = 0;       // config0 [4:20]
    uint32_t burstLen = 0;       // config0 [25:41]
    uint8_t padFunctionMode = 0; // config0 [56:59]
    uint8_t l2CacheControl = 0;  // config0 [60:63]
    uint64_t srcStride = 0;      // config1 [0:5]
    uint32_t dstStride = 0;      // config1 [40:56]
};

// 已更新 参数已确认
// 对应 CCE 指令：LOAD_OUT_TO_L1_2DV2
struct LoadGmToCbuf2DV2ParamField {
    uint32_t instr_id = 0;
    uint64_t dstAddr = 0;
    uint64_t srcAddr = 0;
    uint32_t mStartPosition = 0; // config0 [0:31]
    uint32_t kStartPosition = 0; // config0 [32:63]
    uint16_t dstStride = 0;      // config1 [0:11]
    uint16_t mStep = 0;          // config1 [12:23]
    uint16_t kStep = 0;          // config1 [24:35]
    uint8_t sid = 0;             // config1 [36:39]
    uint8_t decompMode = 0;      // config1 [40:42]
    uint8_t l2CacheControl = 0;  // config1 [60:63]
};

// asc_copy_ub2gm_align_impl
// 已更新 参数已确认
// 对应 CCE 指令：MOV_UB_TO_OUT_ALIGN_V2
struct CopyUbufToGmAlignV2ParamField {
    uint32_t instr_id = 0;
    uint64_t dstAddr = 0;
    uint64_t srcAddr = 0;
    uint8_t sid = 0;            // config0 [0:3]
    uint32_t burstNum = 0;      // config0 [4:24]
    uint32_t burstLen = 0;      // config0 [25:45]
    uint8_t l2CacheControl = 0; // config0 [60:63]
    uint64_t dstStride = 0;     // config1 [0:39]
    uint32_t srcStride = 0;     // config1 [40:60]
};

// DONE 指令的 Operand -> ParamField 映射：
// MOV_OUT_TO_UB_ALIGN_V2: CopyGmToUbufAlignV2Operand -> CopyGmToUbufAlignV2ParamField
CopyGmToUbufAlignV2ParamField ConvertCopyGmToUbufAlignV2Operand(const CopyGmToUbufAlignV2Operand& operand);

// MOV_OUT_TO_L1_ALIGN_V2: CopyGmToCbufAlignV2Operand -> CopyGmToCbufAlignV2ParamField
CopyGmToCbufAlignV2ParamField ConvertCopyGmToCbufAlignV2Operand(const CopyGmToCbufAlignV2Operand& operand);

// MOV_OUT_TO_L1_MULTI_DN2NZ:
// CopyGmToCbufMultiDn2NzOperand -> CopyGmToCbufMultiDn2NzParamField
CopyGmToCbufMultiDn2NzParamField ConvertCopyGmToCbufMultiDn2NzOperand(const CopyGmToCbufMultiDn2NzOperand& operand);

// MOV_OUT_TO_L1_MULTI_ND2NZ:
// CopyGmToCbufMultiNd2NzOperand -> CopyGmToCbufMultiNd2NzParamField
CopyGmToCbufMultiNd2NzParamField ConvertCopyGmToCbufMultiNd2NzOperand(const CopyGmToCbufMultiNd2NzOperand& operand);

// MOV_OUT_TO_L1_V2: CopyGmToCbufV2Operand -> CopyGmToCbufV2ParamField
CopyGmToCbufV2ParamField ConvertCopyGmToCbufV2Operand(const CopyGmToCbufV2Operand& operand);

// LOAD_OUT_TO_L1_2DV2: LoadGmToCbuf2DV2Operand -> LoadGmToCbuf2DV2ParamField
LoadGmToCbuf2DV2ParamField ConvertLoadGmToCbuf2DV2Operand(const LoadGmToCbuf2DV2Operand& operand);

// MOV_UB_TO_OUT_ALIGN_V2: CopyUbufToGmAlignV2Operand -> CopyUbufToGmAlignV2ParamField
CopyUbufToGmAlignV2ParamField ConvertCopyUbufToGmAlignV2Operand(const CopyUbufToGmAlignV2Operand& operand);

// 已更新 参数已确认
// 对应 CCE 指令：
// - ND_DMA_OUT_TO_UB.<b8/b16/b32>
struct NdDmaParamField {
    uint32_t instr_id = 0;
    uint64_t dstAddr = 0;
    uint64_t srcAddr = 0;
    uint8_t sid = 0;                    // config0 [0:3]
    uint32_t loop0Size = 0;             // config0 [4:23]
    uint32_t loop1Size = 0;             // config0 [24:43]
    uint32_t loop2Size = 0;             // config0 [44:63]
    uint32_t loop3Size = 0;             // config1 [0:19]
    uint32_t loop4Size = 0;             // config1 [20:39]
    uint8_t loop0LeftPaddingCount = 0;  // config1 [40:47]
    uint8_t loop0RightPaddingCount = 0; // config1 [48:55]
    bool paddingMode = false;           // config1 [56]
    uint8_t l2CacheControl = 0;         // config1 [60:63]
};

using NdDmaOutToUbufParamField = NdDmaParamField;

// 已更新 参数已确认
// 对应 CCE 指令：SET_L1_2D.<b16/b32>
// 1个dst + 1个64B config
struct SetL12DParamField {
    uint32_t instr_id = 0;
    uint64_t dstAddr = 0;
    uint16_t repeatTimes = 0; // config0 [0:14]
    uint16_t blockNum = 0;    // config0 [16:30]
    uint16_t repeatGap = 0;   // config0 [32:46]
};

// TODO: 还没补充完整
// 对应 CCE 指令：FIX_L0C_TO_OUT.<f32/s32>
struct FixL0cToOutParamField {
    uint32_t instr_id = 0;
    uint64_t dstAddr = 0;
    uint64_t srcAddr = 0;
};

} // namespace sanitizer

#endif // NPU_SANITIZER_SANITIZER_API_CCE_INSTR_STRUCT_DMA_H_
