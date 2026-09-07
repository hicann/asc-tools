/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aclsan/aclsan_api.h"
#include "device_instr/common/instruction_id.h"
#include "internal/aclsan_memory_cbdata.h"

#include <cassert>
#include <array>
#include <cstdint>
#include <limits>

namespace {

using aclsan::CopyGmToUbufAlignV2ParamField;
using aclsan::InstructionId;

uint32_t RawInstructionId(InstructionId instruction) { return static_cast<uint32_t>(instruction); }

void TestEmptyFieldProducesNoCbdata()
{
    CopyGmToUbufAlignV2ParamField field{};
    field.instrId = RawInstructionId(InstructionId::CopyGmToUbufAlignV2B16);
    field.dataBits = 16;

    const auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::NO_ACCESS);
    assert(result.data.empty());
}

void TestFieldAndContextProduceCbdata()
{
    CopyGmToUbufAlignV2ParamField field{};
    field.instrId = RawInstructionId(InstructionId::CopyGmToUbufAlignV2B16);
    field.dataBits = 16;
    field.srcAddr = 0x1000;
    field.burstNum = 3;
    field.burstLen = 32;
    field.burstSrcStride = 64;
    const aclsan::MemoryCbdataContext context{0x2000, 41, 40, 7, 2, 3, ACLSAN_DEVICE_PIPE_MTE2};

    const auto result = aclsan::MemoryFieldToCbdataConverter{context}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 1);
    const AclsanDeviceMemoryAccessData& access = result.data.front();
    assert(access.header.pc == context.pc);
    assert(access.header.instrExecId == context.instrExecId);
    assert(access.header.serialNo == context.serialNo);
    assert(access.header.siteId == context.siteId);
    assert(access.header.phyCoreId == context.coreId);
    assert(access.header.blockId == context.blockId);
    assert(access.header.pipeline == context.pipeline);
    assert(access.address == field.srcAddr);
    assert(access.memorySpace == ACLSAN_DEVICE_MEMORY_SPACE_GM);
    assert(access.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_READ);
    assert(access.accessIndex == 0);
    assert(access.accessCount == 1);
    assert(access.dataBits == field.dataBits);
    assert(access.layoutKind == ACLSAN_MEM_LAYOUT_BLOCK_REPEAT);
    assert(access.layout.blockRepeat.blockNum == 1);
    assert(access.layout.blockRepeat.blockSize == field.burstLen);
    assert(access.layout.blockRepeat.repeatTimes == field.burstNum);
    assert(access.layout.blockRepeat.repeatStride == static_cast<int64_t>(field.burstSrcStride));
}

void TestCubeAndMultiFieldsUseDecodedDataBits()
{
    aclsan::CopyGmToCbufAlignV2ParamField cubeField{};
    cubeField.instrId = RawInstructionId(InstructionId::CopyGmToCbufAlignV2B16);
    cubeField.dataBits = 16;
    cubeField.srcAddr = 0x2000;
    cubeField.burstNum = 1;
    cubeField.burstLen = 8;
    const auto cubeResult = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{cubeField});
    assert(cubeResult.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(cubeResult.data.size() == 1);
    assert(cubeResult.data.front().dataBits == cubeField.dataBits);

    aclsan::CopyGmToCbufMultiNd2NzParamField multiField{};
    multiField.instrId = RawInstructionId(InstructionId::CopyGmToCbufMultiNd2NzB16);
    multiField.dataBits = 16;
    multiField.srcAddr = 0x3000;
    multiField.nValue = 1;
    multiField.dValue = 1;
    aclsan::MemoryRegisterState state{};
    state.mte2Nz = aclsan::Mte2NzParamField{1};
    const auto multiResult =
        aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{multiField});
    assert(multiResult.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(multiResult.data.size() == 1);
    assert(multiResult.data.front().dataBits == multiField.dataBits);
}

void TestNdDmaFieldUsesDecodedDataBits()
{
    aclsan::NdDmaOutToUbufParamField field{};
    field.dataBits = 16;
    field.srcAddr = 0x4000;
    field.loop0Size = 1;
    field.loop1Size = 1;
    field.loop2Size = 1;
    field.loop3Size = 1;
    field.loop4Size = 1;

    const auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 1);
    assert(result.data.front().dataBits == field.dataBits);
}

void TestOneFieldCanProduceMultipleCbdataRecords()
{
    aclsan::FixL0cToOutParamField field{};
    field.instrId = RawInstructionId(InstructionId::FixL0cToOutF32);
    field.dstAddr = 0x8000;
    field.nSize = 48;
    field.mSize = 4;
    field.loopDstStride = 64;
    field.quantPre = 24;

    const auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 2);
    assert(result.data[0].address == field.dstAddr);
    assert(result.data[0].accessIndex == 0);
    assert(result.data[0].accessCount == 2);
    assert(result.data[0].layoutKind == ACLSAN_MEM_LAYOUT_RANGE);
    assert(result.data[0].layout.range.bytes == 128);
    assert(result.data[1].address == field.dstAddr + 64);
    assert(result.data[1].accessIndex == 1);
    assert(result.data[1].accessCount == 2);
    assert(result.data[1].layoutKind == ACLSAN_MEM_LAYOUT_RANGE);
    assert(result.data[1].layout.range.bytes == 64);
}

void TestInvalidFieldIsRejected()
{
    CopyGmToUbufAlignV2ParamField field{};
    field.srcAddr = 0x1000;
    field.burstNum = 1;
    field.burstLen = 32;

    const auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::INVALID_FIELD);
    assert(result.data.empty());
}

void TestUnsupportedDataBitsIsRejected()
{
    CopyGmToUbufAlignV2ParamField field{};
    field.instrId = RawInstructionId(InstructionId::CopyGmToUbufAlignV2B16);
    field.dataBits = 64;
    field.srcAddr = 0x1000;
    field.burstNum = 1;
    field.burstLen = 32;

    const auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::INVALID_FIELD);
    assert(result.data.empty());
}

void TestNdDmaPaddingDoesNotExpandGmReadFootprint()
{
    for (const bool constantPaddingMode : {false, true}) {
        aclsan::NdDmaOutToUbufParamField field{};
        field.instrId = RawInstructionId(InstructionId::NdDmaOutToUbufB16);
        field.dataBits = 16;
        field.srcAddr = 0x4000;
        field.loop0Size = 3;
        field.loop1Size = 2;
        field.loop2Size = 1;
        field.loop3Size = 1;
        field.loop4Size = 1;
        field.loop0LeftPaddingCount = 2;
        field.loop0RightPaddingCount = 7;
        field.paddingMode = constantPaddingMode;

        aclsan::MemoryRegisterState state{};
        state.ndDmaPadCount = aclsan::NdDmaPadCountParamField{{3, 4, 5, 6}, {8, 9, 10, 11}};
        state.ndDmaLoopStrides[0] = aclsan::NdDmaLoopStrideParamField{0, 1};
        state.ndDmaLoopStrides[1] = aclsan::NdDmaLoopStrideParamField{1, 8};
        state.ndDmaLoopStrides[2] = aclsan::NdDmaLoopStrideParamField{2, 0};
        state.ndDmaLoopStrides[3] = aclsan::NdDmaLoopStrideParamField{3, 0};
        state.ndDmaLoopStrides[4] = aclsan::NdDmaLoopStrideParamField{4, 0};
        const auto result =
            aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
        assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
        assert(result.data.size() == 1);
        const auto& access = result.data.front();
        assert(access.address == field.srcAddr);
        assert(access.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_READ);
        assert(access.layoutKind == ACLSAN_MEM_LAYOUT_ND_AFFINE);
        assert(access.layout.ndAffine.rank == 5);
        assert(access.layout.ndAffine.elementBytes == 2);
        assert(access.layout.ndAffine.dims[0] == 3);
        assert(access.layout.ndAffine.dims[1] == 2);
        assert(access.layout.ndAffine.strides[0] == 2);
        assert(access.layout.ndAffine.strides[1] == 16);
    }
}

void TestCopyGmToCbufV2UsesUnifiedConverter()
{
    aclsan::CopyGmToCbufV2ParamField field{};
    field.instrId = RawInstructionId(InstructionId::CopyGmToCbufV2);
    field.srcAddr = 0x5000;
    field.burstNum = 2;
    field.burstLen = 3;
    field.srcStride = 5;

    const auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 1);
    const auto& access = result.data.front();
    assert(access.memorySpace == ACLSAN_DEVICE_MEMORY_SPACE_GM);
    assert(access.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_READ);
    assert(access.header.sourceKind == ACLSAN_DEVICE_SOURCE_MTE2);
    assert(access.header.flags == ACLSAN_DEVICE_EVENT_FLAG_EXACT);
    assert(access.dataBits == 0);
    assert(access.layoutKind == ACLSAN_MEM_LAYOUT_BLOCK_REPEAT);
    assert(access.layout.blockRepeat.blockSize == 96);
    assert(access.layout.blockRepeat.repeatStride == 160);
}

void TestCopyGmToCbufV2ModelsPaddingSourceFootprint()
{
    constexpr std::array<uint64_t, 5> kSourceBytes{1, 2, 4, 8, 16};
    for (uint8_t mode = 1; mode <= 5; ++mode) {
        aclsan::CopyGmToCbufV2ParamField field{};
        field.instrId = RawInstructionId(InstructionId::CopyGmToCbufV2);
        field.srcAddr = 0x5400;
        field.burstNum = 3;
        field.burstLen = 1;
        field.padFunctionMode = mode;

        const auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
        assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
        assert(result.data.size() == 1);
        const auto& access = result.data.front();
        assert(access.layoutKind == ACLSAN_MEM_LAYOUT_RANGE);
        assert(access.layout.range.bytes == 3 * kSourceBytes[mode - 1]);
    }

    for (uint8_t mode = 6; mode <= 8; ++mode) {
        aclsan::CopyGmToCbufV2ParamField field{};
        field.instrId = RawInstructionId(InstructionId::CopyGmToCbufV2);
        field.srcAddr = 0x5500;
        field.burstNum = 2;
        field.burstLen = 2;
        field.padFunctionMode = mode;
        field.srcStride = 3;

        const auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
        assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
        assert(result.data.size() == 1);
        const auto& access = result.data.front();
        assert(access.layoutKind == ACLSAN_MEM_LAYOUT_BLOCK_REPEAT);
        assert(access.layout.blockRepeat.blockSize == 64);
        assert(access.layout.blockRepeat.repeatStride == 96);
    }

    aclsan::CopyGmToCbufV2ParamField invalid{};
    invalid.instrId = RawInstructionId(InstructionId::CopyGmToCbufV2);
    invalid.burstNum = 1;
    invalid.burstLen = 2;
    invalid.padFunctionMode = 1;
    auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{invalid});
    assert(result.status == aclsan::MemoryCbdataStatus::INVALID_FIELD);
    invalid.burstLen = 1;
    invalid.padFunctionMode = 9;
    result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{invalid});
    assert(result.status == aclsan::MemoryCbdataStatus::INVALID_FIELD);
}

void TestContiguousBurstStrideProducesOneRange()
{
    CopyGmToUbufAlignV2ParamField field{};
    field.instrId = RawInstructionId(InstructionId::CopyGmToUbufAlignV2B16);
    field.dataBits = 16;
    field.srcAddr = 0x5800;
    field.burstNum = 3;
    field.burstLen = 32;
    field.burstSrcStride = 32;

    const auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 1);
    assert(result.data.front().layoutKind == ACLSAN_MEM_LAYOUT_RANGE);
    assert(result.data.front().layout.range.bytes == 96);
}

void TestAddressExtentOverflowIsRejected()
{
    CopyGmToUbufAlignV2ParamField field{};
    field.instrId = RawInstructionId(InstructionId::CopyGmToUbufAlignV2B16);
    field.dataBits = 16;
    field.srcAddr = UINT64_MAX - 31;
    field.burstNum = 2;
    field.burstLen = 32;
    field.burstSrcStride = 32;

    const auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::ARITHMETIC_OVERFLOW);
    assert(result.data.empty());
}

void TestDmaOuterLoopsProduceAffineLayout()
{
    CopyGmToUbufAlignV2ParamField field{};
    field.instrId = RawInstructionId(InstructionId::CopyGmToUbufAlignV2B16);
    field.dataBits = 16;
    field.srcAddr = 0x6000;
    field.burstNum = 3;
    field.burstLen = 32;
    field.burstSrcStride = 64;

    aclsan::MemoryRegisterState state{};
    constexpr auto direction = aclsan::DmaLoopDirection::GM_TO_UBUF;
    const auto directionIndex = static_cast<std::size_t>(direction);
    state.dmaLoopSizes[directionIndex] = aclsan::DmaLoopSizeParamField{direction, 2, 4};
    state.dmaLoopStrides[directionIndex][0] = aclsan::DmaLoopStrideParamField{direction, 0, 0x200, 0x20};
    state.dmaLoopStrides[directionIndex][1] = aclsan::DmaLoopStrideParamField{direction, 1, 0x1000, 0x40};

    const auto result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 1);
    const auto& layout = result.data.front().layout.ndAffine;
    assert(result.data.front().layoutKind == ACLSAN_MEM_LAYOUT_ND_AFFINE);
    assert(layout.rank == 3);
    assert(layout.elementBytes == 32);
    assert(layout.dims[0] == 3 && layout.strides[0] == 64);
    assert(layout.dims[1] == 2 && layout.strides[1] == 0x200);
    assert(layout.dims[2] == 4 && layout.strides[2] == 0x1000);
}

void TestDmaOuterLoopsUseTheGmStrideForEveryDirection()
{
    {
        aclsan::CopyUbufToGmAlignV2ParamField field{};
        field.instrId = RawInstructionId(InstructionId::CopyUbufToGmAlignV2);
        field.dstAddr = 0x7000;
        field.burstNum = 2;
        field.burstLen = 16;
        field.dstStride = 64;

        aclsan::MemoryRegisterState state{};
        constexpr auto direction = aclsan::DmaLoopDirection::UBUF_TO_GM;
        const auto directionIndex = static_cast<std::size_t>(direction);
        state.dmaLoopSizes[directionIndex] = aclsan::DmaLoopSizeParamField{direction, 2, 3};
        state.dmaLoopStrides[directionIndex][0] = aclsan::DmaLoopStrideParamField{direction, 0, 0x20, 0x300};
        state.dmaLoopStrides[directionIndex][1] = aclsan::DmaLoopStrideParamField{direction, 1, 0x40, 0x1000};

        const auto result =
            aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
        assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
        const auto& access = result.data.front();
        assert(access.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_WRITE);
        assert(access.layoutKind == ACLSAN_MEM_LAYOUT_ND_AFFINE);
        assert(access.layout.ndAffine.dims[0] == 2 && access.layout.ndAffine.strides[0] == 64);
        assert(access.layout.ndAffine.dims[1] == 2 && access.layout.ndAffine.strides[1] == 0x300);
        assert(access.layout.ndAffine.dims[2] == 3 && access.layout.ndAffine.strides[2] == 0x1000);
    }

    {
        aclsan::CopyGmToCbufV2ParamField field{};
        field.instrId = RawInstructionId(InstructionId::CopyGmToCbufV2);
        field.srcAddr = 0x8000;
        field.burstNum = 2;
        field.burstLen = 3;
        field.srcStride = 5;

        aclsan::MemoryRegisterState state{};
        constexpr auto direction = aclsan::DmaLoopDirection::GM_TO_CBUF;
        const auto directionIndex = static_cast<std::size_t>(direction);
        state.dmaLoopSizes[directionIndex] = aclsan::DmaLoopSizeParamField{direction, 2, 3};
        state.dmaLoopStrides[directionIndex][0] = aclsan::DmaLoopStrideParamField{direction, 0, 0x400, 0x20};
        state.dmaLoopStrides[directionIndex][1] = aclsan::DmaLoopStrideParamField{direction, 1, 0x2000, 0x40};

        const auto result =
            aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
        assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
        const auto& access = result.data.front();
        assert(access.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_READ);
        assert(access.layoutKind == ACLSAN_MEM_LAYOUT_ND_AFFINE);
        assert(access.layout.ndAffine.elementBytes == 96);
        assert(access.layout.ndAffine.dims[0] == 2 && access.layout.ndAffine.strides[0] == 160);
        assert(access.layout.ndAffine.dims[1] == 2 && access.layout.ndAffine.strides[1] == 0x400);
        assert(access.layout.ndAffine.dims[2] == 3 && access.layout.ndAffine.strides[2] == 0x2000);
    }
}

void TestDmaOuterLoopStateDistinguishesDefaultZeroAndMissingStride()
{
    CopyGmToUbufAlignV2ParamField field{};
    field.instrId = RawInstructionId(InstructionId::CopyGmToUbufAlignV2B16);
    field.dataBits = 16;
    field.srcAddr = 0x9000;
    field.burstNum = 1;
    field.burstLen = 32;

    aclsan::MemoryRegisterState state{};
    constexpr auto direction = aclsan::DmaLoopDirection::GM_TO_UBUF;
    const auto directionIndex = static_cast<std::size_t>(direction);
    state.dmaLoopSizes[directionIndex] = aclsan::DmaLoopSizeParamField{direction, 2, 1};
    auto result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::MISSING_REGISTER_STATE);
    assert(result.requiredRegisterInstructionId == RawInstructionId(InstructionId::Loop1StrideGmToUbuf));

    state.dmaLoopSizes[directionIndex] = aclsan::DmaLoopSizeParamField{direction, 0, 1};
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::NO_ACCESS);

    result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.front().layoutKind == ACLSAN_MEM_LAYOUT_RANGE);
}

void TestFixpipeConversionModesConsumeIndependentLoop3State()
{
    aclsan::FixL0cToOutParamField field{};
    field.instrId = RawInstructionId(InstructionId::FixL0cToOutF32);
    field.dstAddr = 0xa000;
    field.nSize = 32;
    field.mSize = 3;
    field.loopDstStride = 40;
    field.quantPre = 0;
    field.nz2ndEnable = true;

    auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::MISSING_REGISTER_STATE);
    assert(result.requiredRegisterInstructionId == RawInstructionId(InstructionId::Loop3Param));

    aclsan::MemoryRegisterState state{};
    state.loop3 = aclsan::Loop3ParamField{2, 7, 100};
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    const auto& rowMajor = result.data.front();
    assert(rowMajor.layoutKind == ACLSAN_MEM_LAYOUT_ND_AFFINE);
    assert(rowMajor.layout.ndAffine.elementBytes == 128);
    assert(rowMajor.layout.ndAffine.dims[0] == 3 && rowMajor.layout.ndAffine.strides[0] == 160);
    assert(rowMajor.layout.ndAffine.dims[1] == 2 && rowMajor.layout.ndAffine.strides[1] == 400);

    field.nz2ndEnable = false;
    field.nz2dnEnable = true;
    field.nSize = 5;
    field.mSize = 32;
    state.loop3 = aclsan::Loop3ParamField{3, 9, 200};
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    const auto& columnMajor = result.data.front();
    assert(columnMajor.layoutKind == ACLSAN_MEM_LAYOUT_BLOCK_REPEAT);
    assert(columnMajor.layout.blockRepeat.blockSize == 128);
    assert(columnMajor.layout.blockRepeat.repeatTimes == 15);
    assert(columnMajor.layout.blockRepeat.repeatStride == 160);

    state.loop3 = aclsan::Loop3ParamField{0, 9, 200};
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::NO_ACCESS);
}

void TestFixpipeZeroDestinationStrideStillWritesGm()
{
    aclsan::FixL0cToOutParamField field{};
    field.instrId = RawInstructionId(InstructionId::FixL0cToOutF32);
    field.dstAddr = 0xa800;
    field.nSize = 48;
    field.mSize = 2;
    field.loopDstStride = 0;
    field.quantPre = 24;

    const auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 2);
    assert(result.data[0].address == field.dstAddr);
    assert(result.data[0].layoutKind == ACLSAN_MEM_LAYOUT_RANGE);
    assert(result.data[0].layout.range.bytes == 64);
    assert(result.data[1].address == field.dstAddr);
    assert(result.data[1].layoutKind == ACLSAN_MEM_LAYOUT_RANGE);
    assert(result.data[1].layout.range.bytes == 32);
}

void TestFixpipeUnprovenLowLevelModeIsRejected()
{
    aclsan::FixL0cToOutParamField field{};
    field.instrId = RawInstructionId(InstructionId::FixL0cToOutF32);
    field.nSize = 16;
    field.mSize = 2;
    field.loopDstStride = 32;
    field.quantPre = 0;
    field.loopEnhanceEnable = true;

    const auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::INVALID_FIELD);
}

void TestFixpipeDumpTensorC0PaddingUsesRawNzFootprint()
{
    aclsan::FixL0cToOutParamField field{};
    field.instrId = RawInstructionId(InstructionId::FixL0cToOutF32);
    field.dstAddr = 0xac00;
    field.nSize = 32;
    field.mSize = 16;
    field.loopDstStride = 256;
    field.loopSrtStride = 16;
    field.c0PadEnable = true;

    auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 1);
    assert(result.data.front().dataBits == 32);
    assert(result.data.front().address == field.dstAddr);
    assert(result.data.front().layoutKind == ACLSAN_MEM_LAYOUT_RANGE);
    assert(result.data.front().layout.range.bytes == 2048);

    field.nSize = 16;
    result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 1);
    assert(result.data.front().layout.range.bytes == 1024);

    field.quantPre = 1;
    result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::INVALID_FIELD);
}

void TestMultiStateDistinguishesMissingAndObservedZero()
{
    aclsan::CopyGmToCbufMultiNd2NzParamField field{};
    field.instrId = RawInstructionId(InstructionId::CopyGmToCbufMultiNd2NzB16);
    field.dataBits = 16;
    field.srcAddr = 0xb000;
    field.nValue = 2;
    field.dValue = 16;
    field.loop1SrcStride = 32;
    field.loop4SrcStride = 128;

    auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::MISSING_REGISTER_STATE);
    assert(result.requiredRegisterInstructionId == RawInstructionId(InstructionId::SetMte2NzPara));

    aclsan::MemoryRegisterState state{};
    state.mte2Nz = aclsan::Mte2NzParamField{0, 3, 4, 5};
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::NO_ACCESS);
}

void TestLargeMultiAccessRemainsNdAffineInsteadOfTruncatingRepeatCount()
{
    aclsan::CopyGmToCbufMultiDn2NzParamField field{};
    field.instrId = RawInstructionId(InstructionId::CopyGmToCbufMultiDn2NzB16);
    field.dataBits = 16;
    field.srcAddr = 0xb800;
    field.nValue = 1;
    field.dValue = (UINT32_C(1) << 20U) - 1U;
    field.loop1SrcStride = 4;
    field.loop4SrcStride = static_cast<uint64_t>(field.dValue) * field.loop1SrcStride;

    aclsan::MemoryRegisterState state{};
    state.mte2Nz = aclsan::Mte2NzParamField{UINT16_MAX};
    const auto result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 1);
    assert(result.data.front().layoutKind == ACLSAN_MEM_LAYOUT_ND_AFFINE);
    assert(result.data.front().layout.ndAffine.rank == 2);
}

void TestNdDmaRequiresAllStrideStates()
{
    aclsan::NdDmaOutToUbufParamField field{};
    field.instrId = RawInstructionId(InstructionId::NdDmaOutToUbufB8);
    field.dataBits = 8;
    field.srcAddr = 0xc000;
    field.loop0Size = 1;
    field.loop1Size = 2;
    field.loop2Size = 1;
    field.loop3Size = 1;
    field.loop4Size = 1;

    aclsan::MemoryRegisterState state{};
    auto result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::MISSING_REGISTER_STATE);
    assert(result.requiredRegisterInstructionId == RawInstructionId(InstructionId::NdDmaLoop0Stride));

    state.ndDmaLoopStrides[0] = aclsan::NdDmaLoopStrideParamField{0, 0};
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::MISSING_REGISTER_STATE);
    assert(result.requiredRegisterInstructionId == RawInstructionId(InstructionId::NdDmaLoop1Stride));
    state.ndDmaLoopStrides[1] = aclsan::NdDmaLoopStrideParamField{1, 8};
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::MISSING_REGISTER_STATE);
    assert(result.requiredRegisterInstructionId == RawInstructionId(InstructionId::NdDmaLoop2Stride));
    state.ndDmaLoopStrides[2] = aclsan::NdDmaLoopStrideParamField{2, 0};
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::MISSING_REGISTER_STATE);
    assert(result.requiredRegisterInstructionId == RawInstructionId(InstructionId::NdDmaLoop3Stride));
    state.ndDmaLoopStrides[3] = aclsan::NdDmaLoopStrideParamField{3, 0};
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::MISSING_REGISTER_STATE);
    assert(result.requiredRegisterInstructionId == RawInstructionId(InstructionId::NdDmaLoop4Stride));
    state.ndDmaLoopStrides[4] = aclsan::NdDmaLoopStrideParamField{4, 0};
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);

    field.loop4Size = 0;
    state.ndDmaLoopStrides[4].reset();
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::MISSING_REGISTER_STATE);
    assert(result.requiredRegisterInstructionId == RawInstructionId(InstructionId::NdDmaLoop4Stride));
    state.ndDmaLoopStrides[4] = aclsan::NdDmaLoopStrideParamField{4, 0};
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::NO_ACCESS);
}

void TestLoadGmToCbuf2DV2UsesModeZeroFractalSize()
{
    aclsan::LoadGmToCbuf2DV2ParamField field{};
    field.instrId = RawInstructionId(InstructionId::LoadGmToCbuf2DV2);
    field.srcAddr = 0x100000;
    field.mStartPosition = 2;
    field.kStartPosition = 1;
    field.mStep = 2;
    field.kStep = 2;
    aclsan::MemoryRegisterState state{};
    state.mte2Source = aclsan::Mte2SourceParamField{4};

    const auto result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 1);
    const auto& access = result.data.front();
    assert(access.address == field.srcAddr + 6 * 512);
    assert(access.layoutKind == ACLSAN_MEM_LAYOUT_BLOCK_REPEAT);
    assert(access.layout.blockRepeat.blockSize == 2 * 512);
    assert(access.layout.blockRepeat.repeatTimes == 2);
    assert(access.layout.blockRepeat.repeatStride == 4 * 512);
}

void TestLoadGmToCbuf2DV2SkipsNonzeroModes()
{
    aclsan::LoadGmToCbuf2DV2ParamField field{};
    field.instrId = RawInstructionId(InstructionId::LoadGmToCbuf2DV2);
    field.srcAddr = 0x100000;
    field.mStep = 1;
    field.kStep = 1;

    for (uint8_t mode = 1; mode <= 7; ++mode) {
        field.decompMode = mode;
        const auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
        assert(result.status == aclsan::MemoryCbdataStatus::NO_ACCESS);
        assert(result.data.empty());
        assert(result.requiredRegisterInstructionId == 0);
    }
}

void TestLoadGmToCbuf2DV2SupportsNegativeAndZeroSourceStride()
{
    aclsan::LoadGmToCbuf2DV2ParamField field{};
    field.instrId = RawInstructionId(InstructionId::LoadGmToCbuf2DV2);
    field.srcAddr = 0x100000;
    field.mStartPosition = 2;
    field.kStartPosition = 3;
    field.mStep = 2;
    field.kStep = 3;
    aclsan::MemoryRegisterState state{};
    state.mte2Source = aclsan::Mte2SourceParamField{-4};
    auto result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 1);
    assert(result.data.front().address == field.srcAddr + 6 * 512);
    assert(result.data.front().layoutKind == ACLSAN_MEM_LAYOUT_BLOCK_REPEAT);
    assert(result.data.front().layout.blockRepeat.blockSize == 2 * 512);
    assert(result.data.front().layout.blockRepeat.repeatTimes == 3);
    assert(result.data.front().layout.blockRepeat.repeatStride == 4 * 512);

    state.mte2Source = aclsan::Mte2SourceParamField{0};
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 1);
    assert(result.data.front().address == field.srcAddr + 2 * 512);
    assert(result.data.front().layoutKind == ACLSAN_MEM_LAYOUT_RANGE);
    assert(result.data.front().layout.range.bytes == 2 * 512);
}

void TestLoadGmToCbuf2DV2RejectsAddressOverflow()
{
    aclsan::LoadGmToCbuf2DV2ParamField field{};
    field.instrId = RawInstructionId(InstructionId::LoadGmToCbuf2DV2);
    field.mStep = 1;
    field.kStep = 1;
    field.decompMode = 0;
    aclsan::MemoryRegisterState state{};
    state.mte2Source = aclsan::Mte2SourceParamField{1};
    field.kStep = 2;
    state.mte2Source = aclsan::Mte2SourceParamField{-1};
    auto result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::ARITHMETIC_OVERFLOW);

    field.kStep = 1;
    field.mStartPosition = 1;
    field.srcAddr = std::numeric_limits<uint64_t>::max();
    state.mte2Source = aclsan::Mte2SourceParamField{0};
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::ARITHMETIC_OVERFLOW);

    field.srcAddr = 0;
    field.mStartPosition = 0;
    state.mte2Source = aclsan::Mte2SourceParamField{std::numeric_limits<int64_t>::min()};
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::ARITHMETIC_OVERFLOW);
}

void TestLoadGmToCbuf2DV2HandlesMissingStateEmptyStepsAndTailOverflow()
{
    aclsan::LoadGmToCbuf2DV2ParamField field{};
    field.instrId = RawInstructionId(InstructionId::LoadGmToCbuf2DV2);
    field.srcAddr = 0x1000;
    field.mStep = 1;
    field.kStep = 1;

    auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::MISSING_REGISTER_STATE);
    assert(result.requiredRegisterInstructionId == RawInstructionId(InstructionId::Mte2SrcPara));

    aclsan::MemoryRegisterState state{};
    state.mte2Source = aclsan::Mte2SourceParamField{1};
    field.mStep = 0;
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::NO_ACCESS);
    field.mStep = 1;
    field.kStep = 0;
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::NO_ACCESS);

    field.kStep = 2;
    field.srcAddr = std::numeric_limits<uint64_t>::max() - 511;
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::ARITHMETIC_OVERFLOW);
}

void TestFixpipePacked4ProducesExactByteRanges()
{
    aclsan::FixL0cToOutParamField field{};
    field.instrId = RawInstructionId(InstructionId::FixL0cToOutF32);
    field.dstAddr = 0x1000;
    field.nSize = 128;
    field.mSize = 16;
    field.loopDstStride = 1200;
    field.quantPre = 25;

    auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 2);
    assert(result.data[0].dataBits == 4 && result.data[0].address == 0x1000);
    assert(result.data[0].layoutKind == ACLSAN_MEM_LAYOUT_RANGE && result.data[0].layout.range.bytes == 512);
    assert(result.data[1].address == 0x1258 && result.data[1].layout.range.bytes == 512);
    for (std::size_t index = 0; index < result.data.size(); ++index) {
        assert(result.data[index].accessIndex == static_cast<uint32_t>(index));
        assert(result.data[index].accessCount == result.data.size());
    }

    field.quantPre = 21;
    result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::INVALID_FIELD);

    field.instrId = RawInstructionId(InstructionId::FixL0cToOutF32);
    field.quantPre = 25;
    field.nSize = 48;
    result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::INVALID_FIELD);

    field.nSize = 64;
    field.quantPre = 25;
    field.splitEnable = true;
    result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::INVALID_FIELD);
}

void TestFixpipeB8ChannelMergeUsesThirtyTwoChannelGroups()
{
    aclsan::FixL0cToOutParamField field{};
    field.instrId = RawInstructionId(InstructionId::FixL0cToOutF32);
    field.dstAddr = 0x1800;
    field.nSize = 48;
    field.mSize = 16;
    field.loopDstStride = 1024;
    field.quantPre = 24;

    const auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 2);
    assert(result.data[0].dataBits == 8 && result.data[0].address == 0x1800);
    assert(result.data[0].layoutKind == ACLSAN_MEM_LAYOUT_RANGE && result.data[0].layout.range.bytes == 512);
    assert(result.data[1].dataBits == 8 && result.data[1].address == 0x1c00);
    assert(result.data[1].layoutKind == ACLSAN_MEM_LAYOUT_RANGE && result.data[1].layout.range.bytes == 256);

    aclsan::MemoryRegisterState state{};
    state.loop3 = aclsan::Loop3ParamField{2, 16, 512};
    field.nSize = 16;
    field.mSize = 16;
    field.loopDstStride = 32;
    field.nz2ndEnable = true;
    auto converted = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(converted.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(converted.data.size() == 1);
    assert(converted.data.front().layoutKind == ACLSAN_MEM_LAYOUT_BLOCK_REPEAT);
    assert(converted.data.front().layout.blockRepeat.blockSize == 16);
    assert(converted.data.front().layout.blockRepeat.repeatTimes == 32);
    assert(converted.data.front().layout.blockRepeat.repeatStride == 32);

    field.nz2ndEnable = false;
    field.nz2dnEnable = true;
    converted = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(converted.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(converted.data.size() == 1);
    assert(converted.data.front().layoutKind == ACLSAN_MEM_LAYOUT_BLOCK_REPEAT);
    assert(converted.data.front().layout.blockRepeat.blockSize == 16);
    assert(converted.data.front().layout.blockRepeat.repeatTimes == 32);
    assert(converted.data.front().layout.blockRepeat.repeatStride == 32);
}

void TestFixpipeNzRejectsUnsupportedNSizeRemainders()
{
    aclsan::FixL0cToOutParamField field{};
    field.instrId = RawInstructionId(InstructionId::FixL0cToOutF32);
    field.dstAddr = 0x1000;
    field.mSize = 16;
    field.loopDstStride = 128;

    field.splitEnable = true;
    field.nSize = 9;
    auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::INVALID_FIELD);

    field.nSize = 8;
    result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 1);
    assert(result.data.front().layoutKind == ACLSAN_MEM_LAYOUT_RANGE);
    assert(result.data.front().layout.range.bytes == 512);

    field.splitEnable = false;
    field.nSize = 17;
    result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::INVALID_FIELD);

    field.nSize = 16;
    result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 1);
    assert(result.data.front().layoutKind == ACLSAN_MEM_LAYOUT_RANGE);
    assert(result.data.front().layout.range.bytes == 1024);
}

void TestFixpipePacked4ConversionModeUsesLoop3State()
{
    aclsan::FixL0cToOutParamField field{};
    field.instrId = RawInstructionId(InstructionId::FixL0cToOutS32);
    field.dstAddr = 0x2000;
    field.nSize = 5;
    field.mSize = 2;
    field.loopDstStride = 7;
    field.quantPre = 21;
    field.nz2ndEnable = true;

    auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::MISSING_REGISTER_STATE);
    assert(result.requiredRegisterInstructionId == RawInstructionId(InstructionId::Loop3Param));

    aclsan::MemoryRegisterState state{};
    state.loop3 = aclsan::Loop3ParamField{2, 16, 20};
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 4);
    assert(result.data[0].address == 0x2000 && result.data[0].layout.range.bytes == 3);
    assert(result.data[1].address == 0x200a && result.data[1].layout.range.bytes == 3);
    assert(result.data[2].address == 0x2003 && result.data[2].layout.range.bytes == 3);
    assert(result.data[3].address == 0x200d && result.data[3].layout.range.bytes == 3);

    field.nz2ndEnable = false;
    field.nz2dnEnable = true;
    result = aclsan::MemoryFieldToCbdataConverter{{}, state}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 10);
    assert(result.data.front().address == 0x2000 && result.data.front().layout.range.bytes == 1);
    assert(result.data.back().address == 0x2018 && result.data.back().layout.range.bytes == 1);
}

} // namespace

int main()
{
    TestEmptyFieldProducesNoCbdata();
    TestFieldAndContextProduceCbdata();
    TestCubeAndMultiFieldsUseDecodedDataBits();
    TestNdDmaFieldUsesDecodedDataBits();
    TestOneFieldCanProduceMultipleCbdataRecords();
    TestInvalidFieldIsRejected();
    TestUnsupportedDataBitsIsRejected();
    TestNdDmaPaddingDoesNotExpandGmReadFootprint();
    TestCopyGmToCbufV2UsesUnifiedConverter();
    TestCopyGmToCbufV2ModelsPaddingSourceFootprint();
    TestContiguousBurstStrideProducesOneRange();
    TestAddressExtentOverflowIsRejected();
    TestDmaOuterLoopsProduceAffineLayout();
    TestDmaOuterLoopsUseTheGmStrideForEveryDirection();
    TestDmaOuterLoopStateDistinguishesDefaultZeroAndMissingStride();
    TestFixpipeConversionModesConsumeIndependentLoop3State();
    TestFixpipeZeroDestinationStrideStillWritesGm();
    TestFixpipeUnprovenLowLevelModeIsRejected();
    TestFixpipeDumpTensorC0PaddingUsesRawNzFootprint();
    TestMultiStateDistinguishesMissingAndObservedZero();
    TestLargeMultiAccessRemainsNdAffineInsteadOfTruncatingRepeatCount();
    TestNdDmaRequiresAllStrideStates();
    TestLoadGmToCbuf2DV2UsesModeZeroFractalSize();
    TestLoadGmToCbuf2DV2SkipsNonzeroModes();
    TestLoadGmToCbuf2DV2SupportsNegativeAndZeroSourceStride();
    TestLoadGmToCbuf2DV2RejectsAddressOverflow();
    TestLoadGmToCbuf2DV2HandlesMissingStateEmptyStepsAndTailOverflow();
    TestFixpipePacked4ProducesExactByteRanges();
    TestFixpipeB8ChannelMergeUsesThirtyTwoChannelGroups();
    TestFixpipeNzRejectsUnsupportedNSizeRemainders();
    TestFixpipePacked4ConversionModeUsesLoop3State();
    return 0;
}
