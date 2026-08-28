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
#include "internal/aclsan_device_data.h"

#include <cassert>
#include <cstdint>

namespace {

void TestCallbackUsesRawRecordPipeline()
{
    aclsan::AclsanRawTraceRecord record{};
    record.instrId = static_cast<uint32_t>(aclsan::InstructionId::SetFlag);
    record.pipeline = ACLSAN_DEVICE_PIPE_MTE3;
    record.args[0] = ACLSAN_DEVICE_PIPE_SCALAR;
    record.args[1] = ACLSAN_DEVICE_PIPE_MTE2;
    record.args[2] = 7;

    aclsan::ParsedTraceRecord parsed{};
    parsed.record = record;
    const aclsan::DeviceInstructionDecoder* decoder = aclsan::FindDeviceInstructionDecoder("Ascend950PR_9599");
    assert(decoder != nullptr);
    const auto decoded = decoder->decode(record);
    assert(decoded.has_value());
    const auto callback = aclsan::TranslateDecodedTraceToCallbackData(parsed, *decoded);
    assert(callback.has_value());
    const auto* sync = std::get_if<AclsanDeviceSyncData>(&*callback);
    assert(sync != nullptr);
    assert(sync->header.pipeline == record.pipeline);
}

void TestMemoryCallbackUsesRawRecordPipeline()
{
    aclsan::AclsanRawTraceRecord record{};
    record.instrId = static_cast<uint32_t>(aclsan::InstructionId::CopyGmToCbufV2);
    record.pipeline = ACLSAN_DEVICE_PIPE_MTE3;
    record.args[0] = 0x2000;
    record.args[1] = 0x1000;
    record.args[2] = (UINT64_C(1) << 4U) | (UINT64_C(1) << 25U);

    aclsan::ParsedTraceRecord parsed{};
    parsed.record = record;
    const aclsan::DeviceInstructionDecoder* decoder = aclsan::FindDeviceInstructionDecoder("Ascend950PR_9599");
    assert(decoder != nullptr);
    const auto decoded = decoder->decode(record);
    assert(decoded.has_value());
    const auto callback = aclsan::TranslateDecodedTraceToCallbackData(parsed, *decoded);
    assert(callback.has_value());
    const auto* accesses = std::get_if<aclsan::DeviceMemoryAccessDataList>(&*callback);
    assert(accesses != nullptr);
    assert(!accesses->empty());
    for (const AclsanDeviceMemoryAccessData& access : *accesses) {
        assert(access.header.pipeline == record.pipeline);
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
    TestCallbackUsesRawRecordPipeline();
    TestMemoryCallbackUsesRawRecordPipeline();
    TestKnownUnhandledInstructionReturnsNoDecodedResult();
    TestUnknownInstructionReturnsNoDecodedResult();
    return 0;
}
