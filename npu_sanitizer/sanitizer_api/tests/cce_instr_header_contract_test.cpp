/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "cce_instr/cce_instr_struct_dma.h"
#include "cce_instr/cce_instr_struct_sync.h"
#include "cce_instr/cce_instr_types.h"

#include <cstdint>
#include <type_traits>

namespace {

template <typename T>
constexpr bool IsCompleteType()
{
    return sizeof(T) > 0;
}

static_assert(std::is_enum_v<sanitizer::CceInstructionId>);
static_assert(std::is_enum_v<sanitizer::NdNzConversionMode>);

using GmToUbParamField = sanitizer::MovAlignV2ParamField<ACLSAN_DEVICE_MEMORY_SPACE_GM, ACLSAN_DEVICE_MEMORY_SPACE_UB>;
static_assert(IsCompleteType<GmToUbParamField>());
static_assert(std::is_same_v<decltype(GmToUbParamField::srcPos), const AclsanDeviceMemorySpace>);
static_assert(std::is_same_v<decltype(GmToUbParamField::dstPos), const AclsanDeviceMemorySpace>);
static_assert(IsCompleteType<sanitizer::CopyGmToUbufAlignV2ParamField>());
static_assert(IsCompleteType<sanitizer::CopyGmToCbufAlignV2ParamField>());
static_assert(IsCompleteType<sanitizer::CopyGmToCbufMultiParamField<sanitizer::NdNzConversionMode::ND2NZ>>());
static_assert(IsCompleteType<sanitizer::CopyGmToCbufMultiDn2NzParamField>());
static_assert(IsCompleteType<sanitizer::CopyGmToCbufMultiNd2NzParamField>());
static_assert(IsCompleteType<sanitizer::CopyGmToCbufV2ParamField>());
static_assert(IsCompleteType<sanitizer::LoadGmToCbuf2DV2ParamField>());
static_assert(IsCompleteType<sanitizer::CopyUbufToGmAlignV2ParamField>());

static_assert(IsCompleteType<sanitizer::FlagParamField>());
static_assert(IsCompleteType<sanitizer::DeviceFlagParamField>());
static_assert(IsCompleteType<sanitizer::VectorFlagParamField>());
static_assert(IsCompleteType<sanitizer::VectorDeviceFlagParamField>());
static_assert(IsCompleteType<sanitizer::SyncBufParamField>());
static_assert(IsCompleteType<sanitizer::SyncBufvParamField>());
static_assert(IsCompleteType<sanitizer::HardwareFlagParamField>());
static_assert(IsCompleteType<sanitizer::BufferParamField>());
static_assert(IsCompleteType<sanitizer::VectorBufferParamField>());
static_assert(IsCompleteType<sanitizer::SetFlagParamField>());
static_assert(IsCompleteType<sanitizer::SetFlagIParamField>());
static_assert(IsCompleteType<sanitizer::WaitFlagParamField>());
static_assert(IsCompleteType<sanitizer::WaitFlagIParamField>());
static_assert(IsCompleteType<sanitizer::WaitFlagDevParamField>());
static_assert(IsCompleteType<sanitizer::WaitFlagDevIParamField>());
static_assert(IsCompleteType<sanitizer::SetFlagVParamField>());
static_assert(IsCompleteType<sanitizer::SetFlagIVParamField>());
static_assert(IsCompleteType<sanitizer::WaitFlagVParamField>());
static_assert(IsCompleteType<sanitizer::WaitFlagIVParamField>());
static_assert(IsCompleteType<sanitizer::WaitFlagDevVParamField>());
static_assert(IsCompleteType<sanitizer::WaitFlagDevIVParamField>());
static_assert(IsCompleteType<sanitizer::HSetFlagParamField>());
static_assert(IsCompleteType<sanitizer::HSetFlagIParamField>());
static_assert(IsCompleteType<sanitizer::HWaitFlagParamField>());
static_assert(IsCompleteType<sanitizer::HWaitFlagIParamField>());
static_assert(IsCompleteType<sanitizer::GetBufParamField>());
static_assert(IsCompleteType<sanitizer::GetBufIParamField>());
static_assert(IsCompleteType<sanitizer::RlsBufParamField>());
static_assert(IsCompleteType<sanitizer::RlsBufIParamField>());
static_assert(IsCompleteType<sanitizer::GetBufVParamField>());
static_assert(IsCompleteType<sanitizer::GetBufIVParamField>());
static_assert(IsCompleteType<sanitizer::RlsBufVParamField>());
static_assert(IsCompleteType<sanitizer::RlsBufIVParamField>());

static_assert(IsCompleteType<sanitizer::NdDmaParamField>());
static_assert(IsCompleteType<sanitizer::NdDmaOutToUbufParamField>());
static_assert(IsCompleteType<sanitizer::SetL12DParamField>());
static_assert(IsCompleteType<sanitizer::FixL0cToOutParamField>());

} // namespace

int main() { return 0; }
