/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "cann_sanitizer_context.h"

namespace aclsan::cann {

ParsedInstruction ParseInstruction(const AclsanDeviceInstructionData& instruction)
{
    ParsedInstruction parsed{};
    parsed.pipeline = static_cast<AclsanPatchPipeline>(instruction.pipeline);
    parsed.cbid = instruction.cbid;
    parsed.siteId = instruction.siteId;
    parsed.pc = instruction.pc;

    switch (parsed.pipeline) {
        case ACLSAN_PATCH_PIPELINE_MTE2:
            parsed.kind = ParsedInstruction::Kind::MemoryTransfer;
            parsed.op = "MTE2";
            parsed.src = instruction.rawArgs[0];
            parsed.dst = instruction.rawArgs[1];
            parsed.bytes = instruction.rawArgs[2];
            break;
        case ACLSAN_PATCH_PIPELINE_MTE3:
            parsed.kind = ParsedInstruction::Kind::MemoryTransfer;
            parsed.op = "MTE3";
            parsed.src = instruction.rawArgs[0];
            parsed.dst = instruction.rawArgs[1];
            parsed.bytes = instruction.rawArgs[2];
            break;
        case ACLSAN_PATCH_PIPELINE_FIXPIPE:
            parsed.kind = ParsedInstruction::Kind::Fixpipe;
            parsed.op = "FIXPIPE";
            parsed.src = instruction.rawArgs[0];
            parsed.dst = instruction.rawArgs[1];
            parsed.bytes = instruction.rawArgs[2];
            parsed.aux0 = instruction.rawArgs[3];
            parsed.aux1 = instruction.rawArgs[4];
            break;
        case ACLSAN_PATCH_PIPELINE_SET_WAIT_FLAG:
            parsed.kind = ParsedInstruction::Kind::SyncFlag;
            parsed.op = "SET_WAIT_FLAG";
            parsed.aux0 = instruction.rawArgs[0];
            parsed.aux1 = instruction.rawArgs[1];
            break;
        case ACLSAN_PATCH_PIPELINE_GET_RLS_BUF:
            parsed.kind = ParsedInstruction::Kind::BufferLifetime;
            parsed.op = "GET_RLS_BUF";
            parsed.src = instruction.rawArgs[0];
            parsed.bytes = instruction.rawArgs[1];
            parsed.aux0 = instruction.rawArgs[2];
            break;
        default:
            parsed.kind = ParsedInstruction::Kind::Unknown;
            parsed.op = "UNKNOWN";
            break;
    }
    return parsed;
}

} // namespace aclsan::cann
