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
#include <cstdint>

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
    const aclsan::MultiNd2NzMemoryField input{multiField, 1};
    const auto multiResult = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{input});
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
    aclsan::FixpipeMemoryField field{};
    field.field.instrId = RawInstructionId(InstructionId::FixL0cToOutF32);
    field.field.dstAddr = 0x8000;
    field.nSize = 18;
    field.mSize = 4;
    field.dstStride = 64;
    field.dstElementBytes = 4;

    const auto result = aclsan::MemoryFieldToCbdataConverter{{}}.Convert(aclsan::MemoryInstructionField{field});
    assert(result.status == aclsan::MemoryCbdataStatus::SUCCESS);
    assert(result.data.size() == 2);
    assert(result.data[0].address == field.field.dstAddr);
    assert(result.data[0].accessIndex == 0);
    assert(result.data[0].accessCount == 2);
    assert(result.data[0].layoutKind == ACLSAN_MEM_LAYOUT_RANGE);
    assert(result.data[0].layout.range.bytes == 256);
    assert(result.data[1].address == field.field.dstAddr + 256);
    assert(result.data[1].accessIndex == 1);
    assert(result.data[1].accessCount == 2);
    assert(result.data[1].layoutKind == ACLSAN_MEM_LAYOUT_RANGE);
    assert(result.data[1].layout.range.bytes == 32);
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
    return 0;
}
