/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "cce_instr/cce_instr_types.h"

#include <array>
#include <cassert>
#include <cstdint>

namespace {

struct ExpectedPipeline {
    sanitizer::CceInstructionId instructionId;
    AclsanDevicePipeline pipeline;
};

void TestSupportedInstructionPipelines()
{
    static constexpr std::array<ExpectedPipeline, 24> kExpectedPipelines = {{
        {sanitizer::CceInstructionId::CopyGmToUbuf, ACLSAN_DEVICE_PIPE_MTE2},
        {sanitizer::CceInstructionId::CopyGmToUbufAlignB16, ACLSAN_DEVICE_PIPE_MTE2},
        {sanitizer::CceInstructionId::CopyGmToUbufAlignB32, ACLSAN_DEVICE_PIPE_MTE2},
        {sanitizer::CceInstructionId::CopyGmToUbufAlignB8, ACLSAN_DEVICE_PIPE_MTE2},
        {sanitizer::CceInstructionId::CopyUbufToGmAlignV2, ACLSAN_DEVICE_PIPE_MTE3},
        {sanitizer::CceInstructionId::CopyGmToUbufAlignV2B8, ACLSAN_DEVICE_PIPE_MTE2},
        {sanitizer::CceInstructionId::CopyGmToUbufAlignV2B16, ACLSAN_DEVICE_PIPE_MTE2},
        {sanitizer::CceInstructionId::CopyGmToUbufAlignV2B32, ACLSAN_DEVICE_PIPE_MTE2},
        {sanitizer::CceInstructionId::SetFlag, ACLSAN_DEVICE_PIPE_SCALAR},
        {sanitizer::CceInstructionId::SetFlagI, ACLSAN_DEVICE_PIPE_SCALAR},
        {sanitizer::CceInstructionId::WaitFlag, ACLSAN_DEVICE_PIPE_SCALAR},
        {sanitizer::CceInstructionId::WaitFlagI, ACLSAN_DEVICE_PIPE_SCALAR},
        {sanitizer::CceInstructionId::GetBuf, ACLSAN_DEVICE_PIPE_SCALAR},
        {sanitizer::CceInstructionId::GetBufI, ACLSAN_DEVICE_PIPE_SCALAR},
        {sanitizer::CceInstructionId::RlsBuf, ACLSAN_DEVICE_PIPE_SCALAR},
        {sanitizer::CceInstructionId::RlsBufI, ACLSAN_DEVICE_PIPE_SCALAR},
        {sanitizer::CceInstructionId::SetFlagV, ACLSAN_DEVICE_PIPE_SCALAR},
        {sanitizer::CceInstructionId::SetFlagIV, ACLSAN_DEVICE_PIPE_SCALAR},
        {sanitizer::CceInstructionId::WaitFlagV, ACLSAN_DEVICE_PIPE_SCALAR},
        {sanitizer::CceInstructionId::WaitFlagIV, ACLSAN_DEVICE_PIPE_SCALAR},
        {sanitizer::CceInstructionId::GetBufV, ACLSAN_DEVICE_PIPE_SCALAR},
        {sanitizer::CceInstructionId::GetBufIV, ACLSAN_DEVICE_PIPE_SCALAR},
        {sanitizer::CceInstructionId::RlsBufV, ACLSAN_DEVICE_PIPE_SCALAR},
        {sanitizer::CceInstructionId::RlsBufIV, ACLSAN_DEVICE_PIPE_SCALAR},
    }};

    for (const ExpectedPipeline& expected : kExpectedPipelines) {
        const AclsanDevicePipeline pipeline = sanitizer::GetCceInstructionPipeline(expected.instructionId);
        assert(pipeline == expected.pipeline);
    }
}

void TestUnsupportedInstructionReturnsInvalidPipeline()
{
    const AclsanDevicePipeline unsupportedPipeline =
        sanitizer::GetCceInstructionPipeline(sanitizer::CceInstructionId::CopyGmToCbufV2);
    assert(unsupportedPipeline == ACLSAN_DEVICE_PIPE_INVALID);

    const AclsanDevicePipeline invalidPipeline =
        sanitizer::GetCceInstructionPipeline(static_cast<sanitizer::CceInstructionId>(UINT32_MAX));
    assert(invalidPipeline == ACLSAN_DEVICE_PIPE_INVALID);
}

} // namespace

int main()
{
    TestSupportedInstructionPipelines();
    TestUnsupportedInstructionReturnsInvalidPipeline();
    return 0;
}
