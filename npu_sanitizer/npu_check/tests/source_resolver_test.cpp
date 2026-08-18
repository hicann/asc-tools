// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "diagnostic/source_resolver.h"

#include "aclsan/aclsan_api.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

namespace {

enum class ResolverMode : uint8_t {
    SYMBOLIZED,
    SITE_MAP,
    OVERSIZED_SYMBOL,
    RAW_PC,
};

ResolverMode g_mode = ResolverMode::SYMBOLIZED;

npu::sanitizer::InstructionContext MakeInstruction()
{
    npu::sanitizer::InstructionContext instruction{};
    instruction.present = true;
    instruction.siteId = 7;
    instruction.pc = 0x170;
    return instruction;
}

} // namespace

extern "C" AclsanStatus aclsanSymbolizeDevicePc(
    const AclsanDevicePcQuery* query, char* payload, uint64_t payloadSize, uint64_t* payloadBytes)
{
    if (query == nullptr || query->siteId != 7 || query->pc != 0x170) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (g_mode == ResolverMode::OVERSIZED_SYMBOL) {
        if (payloadBytes == nullptr) {
            return ACLSAN_STATUS_ERROR_INVALID_VALUE;
        }
        *payloadBytes = payloadSize + 1;
        return ACLSAN_STATUS_SUCCESS;
    }
    if (g_mode != ResolverMode::SYMBOLIZED) {
        return ACLSAN_STATUS_ERROR_NOT_FOUND;
    }

    constexpr char kSymbolized[] = "kernel.asc:42:7 in AddKernel [inlined from helper.asc:11:3]";
    constexpr size_t kBytes = sizeof(kSymbolized) - 1;
    if (payload == nullptr || payloadBytes == nullptr || payloadSize < kBytes) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    std::memcpy(payload, kSymbolized, kBytes);
    *payloadBytes = kBytes;
    return ACLSAN_STATUS_SUCCESS;
}

extern "C" AclsanStatus aclsanGetPatchSiteInfo(uint32_t siteId, AclsanPatchSiteInfo* info)
{
    if (siteId != 7 || info == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (g_mode != ResolverMode::SITE_MAP && g_mode != ResolverMode::OVERSIZED_SYMBOL) {
        return ACLSAN_STATUS_ERROR_NOT_FOUND;
    }
    info->sourceFile = "kernel.asc";
    info->sourceLine = 42;
    info->functionName = "AddKernel";
    info->opName = "DataCopy";
    return ACLSAN_STATUS_SUCCESS;
}

namespace npu::sanitizer {
namespace {

TEST(SourceResolverTest, UsesSymbolizedPcWhenAvailable)
{
    g_mode = ResolverMode::SYMBOLIZED;
    SourceResolver resolver;

    EXPECT_EQ(resolver.Resolve(MakeInstruction()), "kernel.asc:42:7 in AddKernel [inlined from helper.asc:11:3]");
}

TEST(SourceResolverTest, FallsBackToPatchSiteMetadata)
{
    g_mode = ResolverMode::SITE_MAP;
    SourceResolver resolver;

    EXPECT_EQ(resolver.Resolve(MakeInstruction()), "kernel.asc:42 in AddKernel op=DataCopy");
}

TEST(SourceResolverTest, RejectsOversizedSymbolPayload)
{
    g_mode = ResolverMode::OVERSIZED_SYMBOL;
    SourceResolver resolver;

    EXPECT_EQ(resolver.Resolve(MakeInstruction()), "kernel.asc:42 in AddKernel op=DataCopy");
}

TEST(SourceResolverTest, FallsBackToRawPc)
{
    g_mode = ResolverMode::RAW_PC;
    SourceResolver resolver;

    EXPECT_EQ(resolver.Resolve(MakeInstruction()), "device pc=0x170 site=7");
}

} // namespace
} // namespace npu::sanitizer
