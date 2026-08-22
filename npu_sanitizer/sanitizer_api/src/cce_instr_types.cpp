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

namespace sanitizer {
namespace {

struct CceInstructionPipelineEntry {
    CceInstructionId instructionId;
    AclsanDevicePipeline pipeline;
};

// Sync instructions retain the existing callback classification. Their operand pipes remain in raw args.
constexpr std::array<CceInstructionPipelineEntry, 24> kInstructionPipelines = {{
    {CceInstructionId::CopyGmToUbuf, ACLSAN_DEVICE_PIPE_MTE2},
    {CceInstructionId::CopyGmToUbufAlignB16, ACLSAN_DEVICE_PIPE_MTE2},
    {CceInstructionId::CopyGmToUbufAlignB32, ACLSAN_DEVICE_PIPE_MTE2},
    {CceInstructionId::CopyGmToUbufAlignB8, ACLSAN_DEVICE_PIPE_MTE2},
    {CceInstructionId::CopyGmToUbufAlignV2B8, ACLSAN_DEVICE_PIPE_MTE2},
    {CceInstructionId::CopyGmToUbufAlignV2B16, ACLSAN_DEVICE_PIPE_MTE2},
    {CceInstructionId::CopyGmToUbufAlignV2B32, ACLSAN_DEVICE_PIPE_MTE2},

    {CceInstructionId::CopyUbufToGmAlignV2, ACLSAN_DEVICE_PIPE_MTE3},

    {CceInstructionId::SetFlag, ACLSAN_DEVICE_PIPE_SCALAR},
    {CceInstructionId::SetFlagI, ACLSAN_DEVICE_PIPE_SCALAR},
    {CceInstructionId::WaitFlag, ACLSAN_DEVICE_PIPE_SCALAR},
    {CceInstructionId::WaitFlagI, ACLSAN_DEVICE_PIPE_SCALAR},
    {CceInstructionId::GetBuf, ACLSAN_DEVICE_PIPE_SCALAR},
    {CceInstructionId::GetBufI, ACLSAN_DEVICE_PIPE_SCALAR},
    {CceInstructionId::RlsBuf, ACLSAN_DEVICE_PIPE_SCALAR},
    {CceInstructionId::RlsBufI, ACLSAN_DEVICE_PIPE_SCALAR},
    {CceInstructionId::SetFlagV, ACLSAN_DEVICE_PIPE_SCALAR},
    {CceInstructionId::SetFlagIV, ACLSAN_DEVICE_PIPE_SCALAR},
    {CceInstructionId::WaitFlagV, ACLSAN_DEVICE_PIPE_SCALAR},
    {CceInstructionId::WaitFlagIV, ACLSAN_DEVICE_PIPE_SCALAR},
    {CceInstructionId::GetBufV, ACLSAN_DEVICE_PIPE_SCALAR},
    {CceInstructionId::GetBufIV, ACLSAN_DEVICE_PIPE_SCALAR},
    {CceInstructionId::RlsBufV, ACLSAN_DEVICE_PIPE_SCALAR},
    {CceInstructionId::RlsBufIV, ACLSAN_DEVICE_PIPE_SCALAR},
}};

} // namespace

AclsanDevicePipeline GetCceInstructionPipeline(CceInstructionId instructionId) noexcept
{
    for (const CceInstructionPipelineEntry& entry : kInstructionPipelines) {
        if (entry.instructionId == instructionId) {
            return entry.pipeline;
        }
    }
    return ACLSAN_DEVICE_PIPE_INVALID;
}

} // namespace sanitizer
