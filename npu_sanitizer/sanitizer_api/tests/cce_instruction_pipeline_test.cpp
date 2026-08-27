/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "device_instr/common/instruction_id.h"
#include "device_instr/decoder_registry.h"

#include <array>
#include <cassert>
#include <cstdint>

namespace {

struct ExpectedPipeline {
    aclsan::InstructionId instructionId;
    AclsanDevicePipeline pipeline;
};

void TestSupportedInstructionPipelines()
{
    static constexpr std::array<ExpectedPipeline, 36> EXPECTED_PIPELINES = {{
        {aclsan::InstructionId::LoadGmToCbuf2DV2, ACLSAN_DEVICE_PIPE_MTE2},
        {aclsan::InstructionId::CopyGmToCbufV2, ACLSAN_DEVICE_PIPE_MTE2},
        {aclsan::InstructionId::CopyGmToCbufMultiNd2NzB8, ACLSAN_DEVICE_PIPE_MTE2},
        {aclsan::InstructionId::CopyGmToCbufMultiNd2NzB16, ACLSAN_DEVICE_PIPE_MTE2},
        {aclsan::InstructionId::CopyGmToCbufMultiNd2NzB32, ACLSAN_DEVICE_PIPE_MTE2},
        {aclsan::InstructionId::CopyGmToCbufMultiDn2NzB8, ACLSAN_DEVICE_PIPE_MTE2},
        {aclsan::InstructionId::CopyGmToCbufMultiDn2NzB16, ACLSAN_DEVICE_PIPE_MTE2},
        {aclsan::InstructionId::CopyGmToCbufMultiDn2NzB32, ACLSAN_DEVICE_PIPE_MTE2},
        {aclsan::InstructionId::CopyUbufToGmAlignV2, ACLSAN_DEVICE_PIPE_MTE3},
        {aclsan::InstructionId::CopyGmToUbufAlignV2B8, ACLSAN_DEVICE_PIPE_MTE2},
        {aclsan::InstructionId::CopyGmToUbufAlignV2B16, ACLSAN_DEVICE_PIPE_MTE2},
        {aclsan::InstructionId::CopyGmToUbufAlignV2B32, ACLSAN_DEVICE_PIPE_MTE2},
        {aclsan::InstructionId::NdDmaOutToUbufB8, ACLSAN_DEVICE_PIPE_MTE2},
        {aclsan::InstructionId::NdDmaOutToUbufB16, ACLSAN_DEVICE_PIPE_MTE2},
        {aclsan::InstructionId::NdDmaOutToUbufB32, ACLSAN_DEVICE_PIPE_MTE2},
        {aclsan::InstructionId::FixL0cToOutF32, ACLSAN_DEVICE_PIPE_FIXPIPE},
        {aclsan::InstructionId::FixL0cToOutS32, ACLSAN_DEVICE_PIPE_FIXPIPE},
        {aclsan::InstructionId::SetL12DB16, ACLSAN_DEVICE_PIPE_MTE2},
        {aclsan::InstructionId::SetL12DB32, ACLSAN_DEVICE_PIPE_MTE2},
        {aclsan::InstructionId::SetPadding, ACLSAN_DEVICE_PIPE_SCALAR},
        {aclsan::InstructionId::SetFlag, ACLSAN_DEVICE_PIPE_SCALAR},
        {aclsan::InstructionId::SetFlagI, ACLSAN_DEVICE_PIPE_SCALAR},
        {aclsan::InstructionId::WaitFlag, ACLSAN_DEVICE_PIPE_SCALAR},
        {aclsan::InstructionId::WaitFlagI, ACLSAN_DEVICE_PIPE_SCALAR},
        {aclsan::InstructionId::GetBuf, ACLSAN_DEVICE_PIPE_SCALAR},
        {aclsan::InstructionId::GetBufI, ACLSAN_DEVICE_PIPE_SCALAR},
        {aclsan::InstructionId::RlsBuf, ACLSAN_DEVICE_PIPE_SCALAR},
        {aclsan::InstructionId::RlsBufI, ACLSAN_DEVICE_PIPE_SCALAR},
        {aclsan::InstructionId::SetFlagV, ACLSAN_DEVICE_PIPE_SCALAR},
        {aclsan::InstructionId::SetFlagIV, ACLSAN_DEVICE_PIPE_SCALAR},
        {aclsan::InstructionId::WaitFlagV, ACLSAN_DEVICE_PIPE_SCALAR},
        {aclsan::InstructionId::WaitFlagIV, ACLSAN_DEVICE_PIPE_SCALAR},
        {aclsan::InstructionId::GetBufV, ACLSAN_DEVICE_PIPE_SCALAR},
        {aclsan::InstructionId::GetBufIV, ACLSAN_DEVICE_PIPE_SCALAR},
        {aclsan::InstructionId::RlsBufV, ACLSAN_DEVICE_PIPE_SCALAR},
        {aclsan::InstructionId::RlsBufIV, ACLSAN_DEVICE_PIPE_SCALAR},
    }};

    const aclsan::DeviceInstructionDecoder* decoder = aclsan::FindDeviceInstructionDecoder("Ascend950PR_9599");
    assert(decoder != nullptr);
    for (const ExpectedPipeline& expected : EXPECTED_PIPELINES) {
        aclsan::AclsanRawTraceRecord record{};
        record.instrId = static_cast<uint32_t>(expected.instructionId);
        const auto decoded = decoder->decode(record);
        assert(decoded.has_value());
        assert(decoded->pipeline == expected.pipeline);
    }
}

void TestKnownUnhandledInstructionReturnsNoDecodedResult()
{
    const aclsan::DeviceInstructionDecoder* decoder = aclsan::FindDeviceInstructionDecoder("Ascend950PR_9599");
    assert(decoder != nullptr);
    aclsan::AclsanRawTraceRecord record{};
    record.instrId = static_cast<uint32_t>(aclsan::InstructionId::CopyUbufToCbuf);
    assert(!decoder->decode(record).has_value());
}

void TestUnknownInstructionReturnsNoDecodedResult()
{
    const aclsan::DeviceInstructionDecoder* decoder = aclsan::FindDeviceInstructionDecoder("Ascend950PR_9599");
    assert(decoder != nullptr);
    aclsan::AclsanRawTraceRecord record{};
    record.instrId = UINT32_MAX;
    assert(!decoder->decode(record).has_value());
}

} // namespace

int main()
{
    TestSupportedInstructionPipelines();
    TestKnownUnhandledInstructionReturnsNoDecodedResult();
    TestUnknownInstructionReturnsNoDecodedResult();
    return 0;
}
