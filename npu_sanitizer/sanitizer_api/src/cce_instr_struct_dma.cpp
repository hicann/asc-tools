/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "cce_instr/cce_instr_struct_dma.h"

namespace sanitizer {
namespace {

// 从uint64_t value中，提取从beginBit位到endBit位(不包含endBit)的连续位段
uint64_t ExtractBitRange(uint64_t value, uint8_t beginBit, uint8_t endBit)
{
    const uint8_t width = endBit - beginBit;
    const uint64_t mask = (1ULL << width) - 1ULL;
    return (value >> beginBit) & mask;
}

template <typename ParamField>
ParamField CreateCopyParamField(const CopyOperand& operand)
{
    ParamField params{};
    params.instr_id = operand.instr_id;
    params.dstAddr = operand.dstAddr;
    params.srcAddr = operand.srcAddr;
    return params;
}

template <typename ParamField>
ParamField ConvertMovAlignV2Operand(const CopyOperand& operand)
{
    ParamField params = CreateCopyParamField<ParamField>(operand);
    params.sid = static_cast<uint8_t>(ExtractBitRange(operand.config0, 0, 3));
    params.burstNum = static_cast<uint32_t>(ExtractBitRange(operand.config0, 4, 24));
    params.burstLen = static_cast<uint32_t>(ExtractBitRange(operand.config0, 25, 45));
    params.leftPaddingCount = static_cast<uint8_t>(ExtractBitRange(operand.config0, 46, 51));
    params.rightPaddingCount = static_cast<uint8_t>(ExtractBitRange(operand.config0, 52, 57));
    params.dataSelectBit = ExtractBitRange(operand.config0, 58, 59) != 0;
    params.l2CacheControl = static_cast<uint8_t>(ExtractBitRange(operand.config0, 60, 63));
    params.burstSrcStride = ExtractBitRange(operand.config1, 0, 39);
    params.burstDstStride = static_cast<uint32_t>(ExtractBitRange(operand.config1, 40, 60));
    return params;
}

template <typename ParamField>
ParamField ConvertCopyGmToCbufMultiOperand(const CopyOperand& operand)
{
    ParamField params = CreateCopyParamField<ParamField>(operand);
    params.sid = static_cast<uint8_t>(ExtractBitRange(operand.config0, 0, 3));
    params.loop1SrcStride = ExtractBitRange(operand.config0, 4, 43);
    params.l2CacheControl = static_cast<uint8_t>(ExtractBitRange(operand.config0, 44, 47));
    params.nValue = static_cast<uint16_t>(ExtractBitRange(operand.config0, 48, 63));
    params.dValue = static_cast<uint32_t>(ExtractBitRange(operand.config1, 0, 20));
    params.loop4SrcStride = ExtractBitRange(operand.config1, 21, 60);
    params.smallC0Enable = ExtractBitRange(operand.config1, 61, 62) != 0;
    return params;
}

} // namespace

CopyGmToUbufAlignV2ParamField ConvertCopyGmToUbufAlignV2Operand(const CopyGmToUbufAlignV2Operand& operand)
{
    return ConvertMovAlignV2Operand<CopyGmToUbufAlignV2ParamField>(operand);
}

CopyGmToCbufAlignV2ParamField ConvertCopyGmToCbufAlignV2Operand(const CopyGmToCbufAlignV2Operand& operand)
{
    return ConvertMovAlignV2Operand<CopyGmToCbufAlignV2ParamField>(operand);
}

CopyUbufToGmAlignV2ParamField ConvertCopyUbufToGmAlignV2Operand(const CopyUbufToGmAlignV2Operand& operand)
{
    CopyUbufToGmAlignV2ParamField params = CreateCopyParamField<CopyUbufToGmAlignV2ParamField>(operand);
    params.sid = static_cast<uint8_t>(ExtractBitRange(operand.config0, 0, 3));
    params.burstNum = static_cast<uint32_t>(ExtractBitRange(operand.config0, 4, 24));
    params.burstLen = static_cast<uint32_t>(ExtractBitRange(operand.config0, 25, 45));
    params.l2CacheControl = static_cast<uint8_t>(ExtractBitRange(operand.config0, 60, 63));
    params.dstStride = ExtractBitRange(operand.config1, 0, 39);
    params.srcStride = static_cast<uint32_t>(ExtractBitRange(operand.config1, 40, 60));
    return params;
}

CopyGmToCbufMultiDn2NzParamField ConvertCopyGmToCbufMultiDn2NzOperand(const CopyGmToCbufMultiDn2NzOperand& operand)
{
    return ConvertCopyGmToCbufMultiOperand<CopyGmToCbufMultiDn2NzParamField>(operand);
}

CopyGmToCbufMultiNd2NzParamField ConvertCopyGmToCbufMultiNd2NzOperand(const CopyGmToCbufMultiNd2NzOperand& operand)
{
    return ConvertCopyGmToCbufMultiOperand<CopyGmToCbufMultiNd2NzParamField>(operand);
}

CopyGmToCbufV2ParamField ConvertCopyGmToCbufV2Operand(const CopyGmToCbufV2Operand& operand)
{
    CopyGmToCbufV2ParamField params = CreateCopyParamField<CopyGmToCbufV2ParamField>(operand);
    params.sid = static_cast<uint8_t>(ExtractBitRange(operand.config0, 0, 3));
    params.burstNum = static_cast<uint32_t>(ExtractBitRange(operand.config0, 4, 20));
    params.burstLen = static_cast<uint32_t>(ExtractBitRange(operand.config0, 25, 41));
    params.padFunctionMode = static_cast<uint8_t>(ExtractBitRange(operand.config0, 56, 59));
    params.l2CacheControl = static_cast<uint8_t>(ExtractBitRange(operand.config0, 60, 63));
    params.srcStride = ExtractBitRange(operand.config1, 0, 5);
    params.dstStride = static_cast<uint32_t>(ExtractBitRange(operand.config1, 40, 56));
    return params;
}

LoadGmToCbuf2DV2ParamField ConvertLoadGmToCbuf2DV2Operand(const LoadGmToCbuf2DV2Operand& operand)
{
    LoadGmToCbuf2DV2ParamField params = CreateCopyParamField<LoadGmToCbuf2DV2ParamField>(operand);
    params.mStartPosition = static_cast<uint32_t>(ExtractBitRange(operand.config0, 0, 31));
    params.kStartPosition = static_cast<uint32_t>(ExtractBitRange(operand.config0, 32, 63));
    params.dstStride = static_cast<uint16_t>(ExtractBitRange(operand.config1, 0, 11));
    params.mStep = static_cast<uint16_t>(ExtractBitRange(operand.config1, 12, 23));
    params.kStep = static_cast<uint16_t>(ExtractBitRange(operand.config1, 24, 35));
    params.sid = static_cast<uint8_t>(ExtractBitRange(operand.config1, 36, 39));
    params.decompMode = static_cast<uint8_t>(ExtractBitRange(operand.config1, 40, 42));
    params.l2CacheControl = static_cast<uint8_t>(ExtractBitRange(operand.config1, 60, 63));
    return params;
}

} // namespace sanitizer
