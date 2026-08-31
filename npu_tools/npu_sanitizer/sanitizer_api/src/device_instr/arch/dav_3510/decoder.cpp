/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "device_instr/arch/dav_3510/decoder.h"
#include "device_instr/arch/dav_3510/dma_layout.h"
#include "device_instr/common/bit_range.h"
#include "device_instr/common/instruction_id.h"

namespace aclsan::dav3510 {
namespace {

constexpr uint32_t DATA_BITS_B8 = 8;
constexpr uint32_t DATA_BITS_B16 = 16;
constexpr uint32_t DATA_BITS_B32 = 32;

constexpr uint32_t RawId(InstructionId instructionId) noexcept { return static_cast<uint32_t>(instructionId); }

// TODO: 待确认这个文件中的比特位场景，bool与uint8_t的映射是否符合预期

template <typename ParamField>
ParamField DecodeMovAlignV2Params(const aclsan::AclsanRawTraceRecord& record, uint32_t dataBits) noexcept
{
    ParamField params{};
    params.instrId = record.instrId;
    params.dataBits = dataBits;
    params.dstAddr = record.args[0];
    params.srcAddr = record.args[1];
    params.sid = static_cast<uint8_t>(ExtractBitRange(record.args[2], MovAlignV2Layout::SID));
    params.burstNum = static_cast<uint32_t>(ExtractBitRange(record.args[2], MovAlignV2Layout::BURST_NUM));
    params.burstLen = static_cast<uint32_t>(ExtractBitRange(record.args[2], MovAlignV2Layout::BURST_LEN));
    params.leftPaddingCount =
        static_cast<uint8_t>(ExtractBitRange(record.args[2], MovAlignV2Layout::LEFT_PADDING_COUNT));
    params.rightPaddingCount =
        static_cast<uint8_t>(ExtractBitRange(record.args[2], MovAlignV2Layout::RIGHT_PADDING_COUNT));
    params.dataSelectBit = ExtractBitRange(record.args[2], MovAlignV2Layout::DATA_SELECT_BIT) != 0;
    params.l2CacheControl = static_cast<uint8_t>(ExtractBitRange(record.args[2], MovAlignV2Layout::L2_CACHE_CONTROL));
    params.burstSrcStride = ExtractBitRange(record.args[3], MovAlignV2Layout::BURST_SRC_STRIDE);
    params.burstDstStride = static_cast<uint32_t>(ExtractBitRange(record.args[3], MovAlignV2Layout::BURST_DST_STRIDE));

    return params;
}

template <typename ParamField>
std::optional<DecodedInstruction> DecodeMovAlignV2(
    const aclsan::AclsanRawTraceRecord& record, DeviceInstructionKind kind, uint32_t dataBits) noexcept
{
    const ParamField params = DecodeMovAlignV2Params<ParamField>(record, dataBits);
    return DecodedInstruction{kind, params};
}

std::optional<DecodedInstruction> DecodeCopyUbufToGmAlignV2(const aclsan::AclsanRawTraceRecord& record) noexcept
{
    CopyUbufToGmAlignV2ParamField params{};
    params.instrId = record.instrId;
    params.dstAddr = record.args[0];
    params.srcAddr = record.args[1];
    params.sid = static_cast<uint8_t>(ExtractBitRange(record.args[2], MovAlignV2Layout::SID));
    params.burstNum = static_cast<uint32_t>(ExtractBitRange(record.args[2], MovAlignV2Layout::BURST_NUM));
    params.burstLen = static_cast<uint32_t>(ExtractBitRange(record.args[2], MovAlignV2Layout::BURST_LEN));
    params.l2CacheControl = static_cast<uint8_t>(ExtractBitRange(record.args[2], MovAlignV2Layout::L2_CACHE_CONTROL));
    params.dstStride = ExtractBitRange(record.args[3], MovAlignV2Layout::BURST_SRC_STRIDE);
    params.srcStride = static_cast<uint32_t>(ExtractBitRange(record.args[3], MovAlignV2Layout::BURST_DST_STRIDE));

    return DecodedInstruction{DeviceInstructionKind::CopyUbufToGmAlignV2, params};
}

template <typename ParamField>
DecodedInstruction DecodeCopyGmToCbufMulti(const aclsan::AclsanRawTraceRecord& record, uint32_t dataBits) noexcept
{
    ParamField params{};
    params.instrId = record.instrId;
    params.dataBits = dataBits;
    params.dstAddr = record.args[0];
    params.srcAddr = record.args[1];
    params.sid = static_cast<uint8_t>(ExtractBitRange(record.args[2], CopyGmToCbufMultiLayout::SID));
    params.loop1SrcStride = ExtractBitRange(record.args[2], CopyGmToCbufMultiLayout::LOOP1_SRC_STRIDE);
    params.l2CacheControl =
        static_cast<uint8_t>(ExtractBitRange(record.args[2], CopyGmToCbufMultiLayout::L2_CACHE_CONTROL));
    params.nValue = static_cast<uint16_t>(ExtractBitRange(record.args[2], CopyGmToCbufMultiLayout::N_VALUE));
    params.dValue = static_cast<uint32_t>(ExtractBitRange(record.args[3], CopyGmToCbufMultiLayout::D_VALUE));
    params.loop4SrcStride = ExtractBitRange(record.args[3], CopyGmToCbufMultiLayout::LOOP4_SRC_STRIDE);
    params.smallC0Enable = ExtractBitRange(record.args[3], CopyGmToCbufMultiLayout::SMALL_C0_ENABLE) != 0;

    return DecodedInstruction{DeviceInstructionKind::CopyGmToCbufMulti, params};
}

std::optional<DecodedInstruction> DecodeCopyGmToCbufV2(const aclsan::AclsanRawTraceRecord& record) noexcept
{
    CopyGmToCbufV2ParamField params{};
    params.instrId = record.instrId;
    params.dstAddr = record.args[0];
    params.srcAddr = record.args[1];
    params.sid = static_cast<uint8_t>(ExtractBitRange(record.args[2], CopyGmToCbufV2Layout::SID));
    params.burstNum = static_cast<uint32_t>(ExtractBitRange(record.args[2], CopyGmToCbufV2Layout::BURST_NUM));
    params.burstLen = static_cast<uint32_t>(ExtractBitRange(record.args[2], CopyGmToCbufV2Layout::BURST_LEN));
    params.padFunctionMode =
        static_cast<uint8_t>(ExtractBitRange(record.args[2], CopyGmToCbufV2Layout::PAD_FUNCTION_MODE));
    params.l2CacheControl =
        static_cast<uint8_t>(ExtractBitRange(record.args[2], CopyGmToCbufV2Layout::L2_CACHE_CONTROL));
    params.srcStride = ExtractBitRange(record.args[3], CopyGmToCbufV2Layout::SRC_STRIDE);
    params.dstStride = static_cast<uint32_t>(ExtractBitRange(record.args[3], CopyGmToCbufV2Layout::DST_STRIDE));

    return DecodedInstruction{DeviceInstructionKind::CopyGmToCbufV2, params};
}

DecodedInstruction DecodeLoadGmToCbuf2DV2(const aclsan::AclsanRawTraceRecord& record) noexcept
{
    LoadGmToCbuf2DV2ParamField params{};
    params.instrId = record.instrId;
    params.dstAddr = record.args[0];
    params.srcAddr = record.args[1];
    params.mStartPosition =
        static_cast<uint32_t>(ExtractBitRange(record.args[2], LoadGmToCbuf2DV2Layout::M_START_POSITION));
    params.kStartPosition =
        static_cast<uint32_t>(ExtractBitRange(record.args[2], LoadGmToCbuf2DV2Layout::K_START_POSITION));
    params.dstStride = static_cast<uint16_t>(ExtractBitRange(record.args[3], LoadGmToCbuf2DV2Layout::DST_STRIDE));
    params.mStep = static_cast<uint16_t>(ExtractBitRange(record.args[3], LoadGmToCbuf2DV2Layout::M_STEP));
    params.kStep = static_cast<uint16_t>(ExtractBitRange(record.args[3], LoadGmToCbuf2DV2Layout::K_STEP));
    params.sid = static_cast<uint8_t>(ExtractBitRange(record.args[3], LoadGmToCbuf2DV2Layout::SID));
    params.decompMode = static_cast<uint8_t>(ExtractBitRange(record.args[3], LoadGmToCbuf2DV2Layout::DECOMP_MODE));
    params.l2CacheControl =
        static_cast<uint8_t>(ExtractBitRange(record.args[3], LoadGmToCbuf2DV2Layout::L2_CACHE_CONTROL));

    return DecodedInstruction{DeviceInstructionKind::LoadGmToCbuf2DV2, params};
}

DecodedInstruction DecodeNdDmaOutToUbuf(const aclsan::AclsanRawTraceRecord& record, uint32_t dataBits) noexcept
{
    NdDmaOutToUbufParamField params{};
    params.instrId = record.instrId;
    params.dataBits = dataBits;
    params.dstAddr = record.args[0];
    params.srcAddr = record.args[1];
    params.sid = static_cast<uint8_t>(ExtractBitRange(record.args[2], NdDmaLayout::SID));
    params.loop0Size = static_cast<uint32_t>(ExtractBitRange(record.args[2], NdDmaLayout::LOOP0_SIZE));
    params.loop1Size = static_cast<uint32_t>(ExtractBitRange(record.args[2], NdDmaLayout::LOOP1_SIZE));
    params.loop2Size = static_cast<uint32_t>(ExtractBitRange(record.args[2], NdDmaLayout::LOOP2_SIZE));
    params.loop3Size = static_cast<uint32_t>(ExtractBitRange(record.args[3], NdDmaLayout::LOOP3_SIZE));
    params.loop4Size = static_cast<uint32_t>(ExtractBitRange(record.args[3], NdDmaLayout::LOOP4_SIZE));
    params.loop0LeftPaddingCount =
        static_cast<uint8_t>(ExtractBitRange(record.args[3], NdDmaLayout::LOOP0_LEFT_PADDING_COUNT));
    params.loop0RightPaddingCount =
        static_cast<uint8_t>(ExtractBitRange(record.args[3], NdDmaLayout::LOOP0_RIGHT_PADDING_COUNT));
    params.paddingMode = ExtractBitRange(record.args[3], NdDmaLayout::PADDING_MODE) != 0;
    params.l2CacheControl = static_cast<uint8_t>(ExtractBitRange(record.args[3], NdDmaLayout::L2_CACHE_CONTROL));

    return DecodedInstruction{DeviceInstructionKind::NdDmaOutToUbuf, params};
}

DecodedInstruction DecodeSetL12D(const aclsan::AclsanRawTraceRecord& record, uint32_t dataBits) noexcept
{
    SetL12DParamField params{};
    params.instrId = record.instrId;
    params.dataBits = dataBits;
    params.dstAddr = record.args[0];
    params.repeatTimes = static_cast<uint16_t>(ExtractBitRange(record.args[1], SetL12DLayout::REPEAT_TIMES));
    params.blockNum = static_cast<uint16_t>(ExtractBitRange(record.args[1], SetL12DLayout::BLOCK_NUM));
    params.repeatGap = static_cast<uint16_t>(ExtractBitRange(record.args[1], SetL12DLayout::REPEAT_GAP));

    return DecodedInstruction{DeviceInstructionKind::SetL12D, params};
}

DecodedInstruction DecodeSetPadding(const aclsan::AclsanRawTraceRecord& record) noexcept
{
    const SetPaddingParamField params{record.args[0]};
    return DecodedInstruction{DeviceInstructionKind::SetPadding, params};
}

DecodedInstruction DecodeMte2SourceParam(const aclsan::AclsanRawTraceRecord& record) noexcept
{
    const uint32_t bits = static_cast<uint32_t>(record.args[0]);
    const uint64_t stride = (bits & (1U << 31U)) == 0 ? bits : static_cast<uint64_t>(~bits) + 1U;
    return DecodedInstruction{DeviceInstructionKind::Mte2SourceParam, Mte2SourceParamField{stride}};
}

DecodedInstruction DecodeNdDmaLoopStride(const aclsan::AclsanRawTraceRecord& record) noexcept
{
    const uint32_t first = static_cast<uint32_t>(InstructionId::NdDmaLoop0Stride);
    const auto loopIndex = record.instrId - first;
    const uint64_t stride = (record.args[0] >> 20U) & ((1ULL << 40U) - 1U);
    return DecodedInstruction{DeviceInstructionKind::NdDmaLoopStride, NdDmaLoopStrideParamField{loopIndex, stride}};
}

DecodedInstruction DecodeMte2NzParam(const aclsan::AclsanRawTraceRecord& record) noexcept
{
    return DecodedInstruction{
        DeviceInstructionKind::Mte2NzParam, Mte2NzParamField{static_cast<uint16_t>(record.args[0])}};
}

DecodedInstruction DecodeFixL0cToOut(const aclsan::AclsanRawTraceRecord& record, uint32_t dataBits) noexcept
{
    FixL0cToOutParamField params{};
    params.instrId = record.instrId;
    params.dataBits = dataBits;
    params.dstAddr = record.args[0];
    params.srcAddr = record.args[1];
    params.sid = static_cast<uint8_t>(ExtractBitRange(record.args[2], FixL0cToOutLayout::SID));
    params.nSize = static_cast<uint16_t>(ExtractBitRange(record.args[2], FixL0cToOutLayout::N_SIZE));
    params.mSize = static_cast<uint16_t>(ExtractBitRange(record.args[2], FixL0cToOutLayout::M_SIZE));
    params.loopDstStride = static_cast<uint32_t>(ExtractBitRange(record.args[2], FixL0cToOutLayout::LOOP_DST_STRIDE));
    params.loopSrtStride = static_cast<uint16_t>(ExtractBitRange(record.args[3], FixL0cToOutLayout::LOOP_SRT_STRIDE));
    params.l2CacheControl = static_cast<uint8_t>(ExtractBitRange(record.args[3], FixL0cToOutLayout::L2_CACHE_CONTROL));
    params.clipReluPre = static_cast<uint8_t>(ExtractBitRange(record.args[3], FixL0cToOutLayout::CLIP_RELU_PRE));
    params.unitFlag = static_cast<uint8_t>(ExtractBitRange(record.args[3], FixL0cToOutLayout::UNIT_FLAG));
    const uint8_t quantPreHigh =
        static_cast<uint8_t>(ExtractBitRange(record.args[3], FixL0cToOutLayout::QUANT_PRE_HIGH));
    const uint8_t quantPreLow = static_cast<uint8_t>(ExtractBitRange(record.args[3], FixL0cToOutLayout::QUANT_PRE_LOW));
    params.quantPre = static_cast<uint8_t>((quantPreHigh << 4U) | quantPreLow);
    params.reluPre = static_cast<uint8_t>(ExtractBitRange(record.args[3], FixL0cToOutLayout::RELU_PRE));
    params.splitEnable = ExtractBitRange(record.args[3], FixL0cToOutLayout::SPLIT_ENABLE) != 0;
    params.nz2ndEnable = ExtractBitRange(record.args[3], FixL0cToOutLayout::NZ2ND_ENABLE) != 0;
    params.quantPost = static_cast<uint8_t>(ExtractBitRange(record.args[3], FixL0cToOutLayout::QUANT_POST));
    params.reluPost = static_cast<uint8_t>(ExtractBitRange(record.args[3], FixL0cToOutLayout::RELU_POST));
    params.clipReluPost = ExtractBitRange(record.args[3], FixL0cToOutLayout::CLIP_RELU_POST) != 0;
    params.loopEnhanceEnable = ExtractBitRange(record.args[3], FixL0cToOutLayout::LOOP_ENHANCE_ENABLE) != 0;
    params.eltwiseOp = static_cast<uint8_t>(ExtractBitRange(record.args[3], FixL0cToOutLayout::ELTWISE_OP));
    params.eltwiseAntqEnable = ExtractBitRange(record.args[3], FixL0cToOutLayout::ELTWISE_ANTQ_ENABLE) != 0;
    params.loopEnhanceMergeEnable = ExtractBitRange(record.args[3], FixL0cToOutLayout::LOOP_ENHANCE_MERGE_ENABLE) != 0;
    params.c0PadEnable = ExtractBitRange(record.args[3], FixL0cToOutLayout::C0_PAD_ENABLE) != 0;
    params.winoPostEnable = ExtractBitRange(record.args[3], FixL0cToOutLayout::WINO_POST_ENABLE) != 0;
    params.brcbEnable = ExtractBitRange(record.args[3], FixL0cToOutLayout::BRCB_ENABLE) != 0;
    params.nz2dnEnable = ExtractBitRange(record.args[3], FixL0cToOutLayout::NZ2DN_ENABLE) != 0;

    return DecodedInstruction{DeviceInstructionKind::FixL0cToOut, params};
}

std::optional<DecodedInstruction> DecodeFlag(
    const aclsan::AclsanRawTraceRecord& record, DeviceInstructionKind kind) noexcept
{
    const FlagParamField params{
        record.instrId, static_cast<uint32_t>(record.args[0]), static_cast<uint32_t>(record.args[1]), record.args[2]};
    return DecodedInstruction{kind, params};
}

std::optional<DecodedInstruction> DecodeBuffer(
    const aclsan::AclsanRawTraceRecord& record, DeviceInstructionKind kind) noexcept
{
    if (record.args[0] > UINT32_MAX || record.args[2] > UINT8_MAX) {
        return std::nullopt;
    }
    const SyncBufParamField params{
        record.instrId, static_cast<uint32_t>(record.args[0]), record.args[1], static_cast<uint8_t>(record.args[2])};
    return DecodedInstruction{kind, params};
}

std::optional<DecodedInstruction> Decode(const aclsan::AclsanRawTraceRecord& record) noexcept
{
    switch (record.instrId) {
        case RawId(InstructionId::LoadGmToCbuf2DV2):
            return DecodeLoadGmToCbuf2DV2(record);
        case RawId(InstructionId::CopyGmToCbufV2):
            return DecodeCopyGmToCbufV2(record);

        case RawId(InstructionId::CopyGmToCbufAlignV2B8):
            return DecodeMovAlignV2<CopyGmToCbufAlignV2ParamField>(
                record, DeviceInstructionKind::CopyGmToCbufAlignV2, DATA_BITS_B8);
        case RawId(InstructionId::CopyGmToCbufAlignV2B16):
            return DecodeMovAlignV2<CopyGmToCbufAlignV2ParamField>(
                record, DeviceInstructionKind::CopyGmToCbufAlignV2, DATA_BITS_B16);
        case RawId(InstructionId::CopyGmToCbufAlignV2B32):
            return DecodeMovAlignV2<CopyGmToCbufAlignV2ParamField>(
                record, DeviceInstructionKind::CopyGmToCbufAlignV2, DATA_BITS_B32);

        case RawId(InstructionId::CopyGmToCbufMultiNd2NzB8):
            return DecodeCopyGmToCbufMulti<CopyGmToCbufMultiNd2NzParamField>(record, DATA_BITS_B8);
        case RawId(InstructionId::CopyGmToCbufMultiNd2NzB16):
            return DecodeCopyGmToCbufMulti<CopyGmToCbufMultiNd2NzParamField>(record, DATA_BITS_B16);
        case RawId(InstructionId::CopyGmToCbufMultiNd2NzB32):
            return DecodeCopyGmToCbufMulti<CopyGmToCbufMultiNd2NzParamField>(record, DATA_BITS_B32);

        case RawId(InstructionId::CopyGmToCbufMultiDn2NzB8):
            return DecodeCopyGmToCbufMulti<CopyGmToCbufMultiDn2NzParamField>(record, DATA_BITS_B8);
        case RawId(InstructionId::CopyGmToCbufMultiDn2NzB16):
            return DecodeCopyGmToCbufMulti<CopyGmToCbufMultiDn2NzParamField>(record, DATA_BITS_B16);
        case RawId(InstructionId::CopyGmToCbufMultiDn2NzB32):
            return DecodeCopyGmToCbufMulti<CopyGmToCbufMultiDn2NzParamField>(record, DATA_BITS_B32);

        case RawId(InstructionId::CopyUbufToGmAlignV2):
            return DecodeCopyUbufToGmAlignV2(record);

        case RawId(InstructionId::CopyGmToUbufAlignV2B8):
            return DecodeMovAlignV2<CopyGmToUbufAlignV2ParamField>(
                record, DeviceInstructionKind::CopyGmToUbufAlignV2, DATA_BITS_B8);
        case RawId(InstructionId::CopyGmToUbufAlignV2B16):
            return DecodeMovAlignV2<CopyGmToUbufAlignV2ParamField>(
                record, DeviceInstructionKind::CopyGmToUbufAlignV2, DATA_BITS_B16);
        case RawId(InstructionId::CopyGmToUbufAlignV2B32):
            return DecodeMovAlignV2<CopyGmToUbufAlignV2ParamField>(
                record, DeviceInstructionKind::CopyGmToUbufAlignV2, DATA_BITS_B32);

        case RawId(InstructionId::NdDmaOutToUbufB8):
            return DecodeNdDmaOutToUbuf(record, DATA_BITS_B8);
        case RawId(InstructionId::NdDmaOutToUbufB16):
            return DecodeNdDmaOutToUbuf(record, DATA_BITS_B16);
        case RawId(InstructionId::NdDmaOutToUbufB32):
            return DecodeNdDmaOutToUbuf(record, DATA_BITS_B32);

        case RawId(InstructionId::Mte2SrcPara):
            return DecodeMte2SourceParam(record);
        case RawId(InstructionId::NdDmaLoop0Stride):
        case RawId(InstructionId::NdDmaLoop1Stride):
        case RawId(InstructionId::NdDmaLoop2Stride):
        case RawId(InstructionId::NdDmaLoop3Stride):
        case RawId(InstructionId::NdDmaLoop4Stride):
            return DecodeNdDmaLoopStride(record);
        case RawId(InstructionId::SetMte2NzPara):
            return DecodeMte2NzParam(record);

        case RawId(InstructionId::FixL0cToOutF32):
            return DecodeFixL0cToOut(record, DATA_BITS_B32);
        case RawId(InstructionId::FixL0cToOutS32):
            return DecodeFixL0cToOut(record, DATA_BITS_B32);

        case RawId(InstructionId::SetL12DB16):
            return DecodeSetL12D(record, DATA_BITS_B16);
        case RawId(InstructionId::SetL12DB32):
            return DecodeSetL12D(record, DATA_BITS_B32);

        case RawId(InstructionId::SetPadding):
            return DecodeSetPadding(record);

        case RawId(InstructionId::SetFlag):
        case RawId(InstructionId::SetFlagI):
        case RawId(InstructionId::SetFlagV):
        case RawId(InstructionId::SetFlagIV):
            return DecodeFlag(record, DeviceInstructionKind::SetFlag);
        case RawId(InstructionId::WaitFlag):
        case RawId(InstructionId::WaitFlagI):
        case RawId(InstructionId::WaitFlagV):
        case RawId(InstructionId::WaitFlagIV):
            return DecodeFlag(record, DeviceInstructionKind::WaitFlag);
        case RawId(InstructionId::GetBuf):
        case RawId(InstructionId::GetBufI):
        case RawId(InstructionId::GetBufV):
        case RawId(InstructionId::GetBufIV):
            return DecodeBuffer(record, DeviceInstructionKind::GetBuf);
        case RawId(InstructionId::RlsBuf):
        case RawId(InstructionId::RlsBufI):
        case RawId(InstructionId::RlsBufV):
        case RawId(InstructionId::RlsBufIV):
            return DecodeBuffer(record, DeviceInstructionKind::RlsBuf);
        default:
            return std::nullopt;
    }
}

} // namespace

const DeviceInstructionDecoder& GetDeviceInstructionDecoder() noexcept
{
    static const DeviceInstructionDecoder DECODER{"dav_3510", Decode};
    return DECODER;
}

} // namespace aclsan::dav3510
