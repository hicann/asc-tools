/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "device_instr/common/device_instr_struct_register.h"
#include "device_instr/common/instruction_id.h"
#include "device_instr/arch/dav_3510/register_state_manager.h"
#include "device_instr/common/bit_range.h"
#include "device_instr/common/device_instr_struct_dma.h"
#include "device_instr/common/device_instr_struct_sync.h"
#include "device_instr/common/device_instr_types.h"

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <unistd.h>

namespace {

template <typename T>
constexpr bool IsCompleteType()
{
    return sizeof(T) > 0;
}

static_assert(std::is_enum_v<aclsan::InstructionId>);
static_assert(std::is_enum_v<aclsan::NdNzConversionMode>);
static_assert(IsCompleteType<aclsan::dav3510::Dav3510RegisterStateManager>());
static_assert(std::is_same_v<decltype(aclsan::dav3510::Dav3510CoreKey{}.blockType), uint32_t>);
static_assert(std::is_same_v<decltype(aclsan::dav3510::Dav3510CoreKey{}.blockId), uint32_t>);
static_assert(std::is_same_v<decltype(aclsan::VectorMaskParamField{}.vectorMask0), uint64_t>);
static_assert(
    std::is_same_v<
        decltype(aclsan::dav3510::Dav3510CoreRegisterState{}.vectorMask), std::optional<aclsan::VectorMaskParamField>>);
static_assert(std::is_same_v<
              decltype(aclsan::dav3510::Dav3510CoreRegisterState{}.loop3), std::optional<aclsan::Loop3ParamField>>);
static_assert(std::is_same_v<decltype(aclsan::SetPaddingParamField{}.value), uint64_t>);
static_assert(
    std::is_same_v<
        decltype(aclsan::dav3510::Dav3510CoreRegisterState{}.setPadding), std::optional<aclsan::SetPaddingParamField>>);

using GmToUbParamField = aclsan::MovAlignV2ParamField<ACLSAN_DEVICE_MEMORY_SPACE_GM, ACLSAN_DEVICE_MEMORY_SPACE_UB>;
static_assert(IsCompleteType<GmToUbParamField>());
static_assert(std::is_same_v<decltype(GmToUbParamField::SRC_POS), const AclsanDeviceMemorySpace>);
static_assert(std::is_same_v<decltype(GmToUbParamField::DST_POS), const AclsanDeviceMemorySpace>);
static_assert(IsCompleteType<aclsan::CopyGmToUbufAlignV2ParamField>());
static_assert(IsCompleteType<aclsan::CopyGmToCbufAlignV2ParamField>());
static_assert(IsCompleteType<aclsan::CopyGmToCbufMultiParamField<aclsan::NdNzConversionMode::ND2NZ>>());
static_assert(IsCompleteType<aclsan::CopyGmToCbufMultiDn2NzParamField>());
static_assert(IsCompleteType<aclsan::CopyGmToCbufMultiNd2NzParamField>());
static_assert(IsCompleteType<aclsan::CopyGmToCbufV2ParamField>());
static_assert(IsCompleteType<aclsan::LoadGmToCbuf2DV2ParamField>());
static_assert(IsCompleteType<aclsan::CopyUbufToGmAlignV2ParamField>());
static_assert(std::is_same_v<decltype(aclsan::CopyUbufToGmAlignV2ParamField::SRC_POS), const AclsanDeviceMemorySpace>);
static_assert(std::is_same_v<decltype(aclsan::CopyUbufToGmAlignV2ParamField::DST_POS), const AclsanDeviceMemorySpace>);
static_assert(aclsan::CopyUbufToGmAlignV2ParamField::SRC_POS == ACLSAN_DEVICE_MEMORY_SPACE_UB);
static_assert(aclsan::CopyUbufToGmAlignV2ParamField::DST_POS == ACLSAN_DEVICE_MEMORY_SPACE_GM);

static_assert(IsCompleteType<aclsan::FlagParamField>());
static_assert(IsCompleteType<aclsan::DeviceFlagParamField>());
static_assert(IsCompleteType<aclsan::SyncBufParamField>());
static_assert(IsCompleteType<aclsan::HardwareFlagParamField>());
static_assert(IsCompleteType<aclsan::NdDmaParamField>());
static_assert(IsCompleteType<aclsan::NdDmaOutToUbufParamField>());
static_assert(IsCompleteType<aclsan::NdDmaPadCountParamField>());
static_assert(IsCompleteType<aclsan::SetL12DParamField>());
static_assert(IsCompleteType<aclsan::Loop3ParamField>());
static_assert(IsCompleteType<aclsan::DmaLoopSizeParamField>());
static_assert(IsCompleteType<aclsan::DmaLoopStrideParamField>());
static_assert(IsCompleteType<aclsan::FixL0cToOutParamField>());
static_assert(std::is_same_v<decltype(aclsan::CopyGmToUbufAlignV2ParamField{}.dataBits), uint32_t>);
static_assert(std::is_same_v<decltype(aclsan::CopyGmToCbufMultiDn2NzParamField{}.dataBits), uint32_t>);
static_assert(std::is_same_v<decltype(aclsan::NdDmaParamField{}.dataBits), uint32_t>);
static_assert(std::is_same_v<decltype(aclsan::SetL12DParamField{}.dataBits), uint32_t>);
static_assert(std::is_same_v<decltype(aclsan::FixL0cToOutParamField{}.dataBits), uint32_t>);

template <typename Action>
std::string CaptureErrorLogs(Action action)
{
    int pipeFds[2] = {-1, -1};
    assert(pipe(pipeFds) == 0);
    const int savedStderr = dup(STDERR_FILENO);
    assert(savedStderr >= 0);
    assert(dup2(pipeFds[1], STDERR_FILENO) >= 0);
    assert(close(pipeFds[1]) == 0);

    action();
    assert(std::fflush(stderr) == 0);
    assert(dup2(savedStderr, STDERR_FILENO) >= 0);
    assert(close(savedStderr) == 0);

    std::string logs;
    char buffer[256] = {};
    ssize_t bytesRead = 0;
    while ((bytesRead = read(pipeFds[0], buffer, sizeof(buffer))) > 0) {
        logs.append(buffer, static_cast<size_t>(bytesRead));
    }
    assert(bytesRead == 0);
    assert(close(pipeFds[0]) == 0);
    return logs;
}

void TestExtractBitRangeLogsInvalidRange()
{
    const std::string logs = CaptureErrorLogs([] {
        assert(aclsan::ExtractBitRange(0xF0ULL, aclsan::BitRange{8, 7}) == 0);
        assert(aclsan::ExtractBitRange(0xF0ULL, aclsan::BitRange{1, 64}) == 0);
    });

    assert(
        logs.find("ExtractBitRange failed: invalid range begin=8 end=7, expected 0 <= begin <= end < 64") !=
        std::string::npos);
    assert(
        logs.find("ExtractBitRange failed: invalid range begin=1 end=64, expected 0 <= begin <= end < 64") !=
        std::string::npos);
}

} // namespace

int main()
{
    assert(aclsan::ExtractBitRange(0xFULL, aclsan::BitRange{0, 3}) == 0xFULL);
    assert(aclsan::ExtractBitRange(0x8ULL, aclsan::BitRange{3, 3}) == 1ULL);
    assert(
        aclsan::ExtractBitRange(std::numeric_limits<uint64_t>::max(), aclsan::BitRange{0, 63}) ==
        std::numeric_limits<uint64_t>::max());
    TestExtractBitRangeLogsInvalidRange();
    return 0;
}
