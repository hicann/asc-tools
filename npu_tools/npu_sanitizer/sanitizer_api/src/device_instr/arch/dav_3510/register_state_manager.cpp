/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "device_instr/arch/dav_3510/register_state_manager.h"
#include "internal/aclsan_log.h"

#include <functional>

namespace aclsan::dav3510 {

bool Dav3510CoreKey::operator==(const Dav3510CoreKey& other) const noexcept
{
    return blockType == other.blockType && blockId == other.blockId;
}

Dav3510RegisterStateManager::Dav3510RegisterStateManager(uint64_t launchId) noexcept : launchId_(launchId) {}

void Dav3510RegisterStateManager::Update(const Dav3510CoreKey& key, const VectorMaskParamField& params) noexcept
{
    states_[key].vectorMask = params;
    ASC_SAN_DEBUG(
        "[register] action=update register=vector_mask launchId=%llu blockType=%u blockId=%u "
        "vectorMask0=0x%llx vectorMask1=0x%llx",
        static_cast<unsigned long long>(launchId_), key.blockType, key.blockId,
        static_cast<unsigned long long>(params.vectorMask0), static_cast<unsigned long long>(params.vectorMask1));
}

void Dav3510RegisterStateManager::Update(const Dav3510CoreKey& key, const SetL12DParamField& params) noexcept
{
    states_[key].setL12D = params;
    ASC_SAN_DEBUG(
        "[register] action=update register=set_l1_2d launchId=%llu blockType=%u blockId=%u "
        "instrId=%u dataBits=%u dstAddr=0x%llx repeatTimes=%u blockNum=%u repeatGap=%u",
        static_cast<unsigned long long>(launchId_), key.blockType, key.blockId, params.instrId, params.dataBits,
        static_cast<unsigned long long>(params.dstAddr), static_cast<unsigned int>(params.repeatTimes),
        static_cast<unsigned int>(params.blockNum), static_cast<unsigned int>(params.repeatGap));
}

void Dav3510RegisterStateManager::Update(const Dav3510CoreKey& key, const SetPaddingParamField& params) noexcept
{
    states_[key].setPadding = params;
    ASC_SAN_DEBUG(
        "[register] action=update register=set_padding launchId=%llu blockType=%u blockId=%u value=0x%llx",
        static_cast<unsigned long long>(launchId_), key.blockType, key.blockId,
        static_cast<unsigned long long>(params.value));
}

void Dav3510RegisterStateManager::Update(const Dav3510CoreKey& key, const Mte2SourceParamField& params) noexcept
{
    states_[key].mte2Source = params;
    ASC_SAN_DEBUG(
        "[register] action=update register=mte2_source launchId=%llu blockType=%u blockId=%u srcStride=%llu",
        static_cast<unsigned long long>(launchId_), key.blockType, key.blockId,
        static_cast<unsigned long long>(params.srcStride));
}

void Dav3510RegisterStateManager::Update(const Dav3510CoreKey& key, const NdDmaLoopStrideParamField& params) noexcept
{
    if (params.loopIndex >= states_[key].ndDmaLoopStrides.size()) {
        return;
    }
    states_[key].ndDmaLoopStrides[params.loopIndex] = params;
    ASC_SAN_DEBUG(
        "[register] action=update register=nddma_loop_stride launchId=%llu blockType=%u blockId=%u "
        "loopIndex=%u srcStride=%llu",
        static_cast<unsigned long long>(launchId_), key.blockType, key.blockId, params.loopIndex,
        static_cast<unsigned long long>(params.srcStride));
}

void Dav3510RegisterStateManager::Update(const Dav3510CoreKey& key, const Mte2NzParamField& params) noexcept
{
    states_[key].mte2Nz = params;
    ASC_SAN_DEBUG(
        "[register] action=update register=mte2_nz launchId=%llu blockType=%u blockId=%u matrixNum=%u",
        static_cast<unsigned long long>(launchId_), key.blockType, key.blockId,
        static_cast<unsigned int>(params.matrixNum));
}

std::optional<Dav3510CoreRegisterState> Dav3510RegisterStateManager::Get(const Dav3510CoreKey& key) const noexcept
{
    const auto state = states_.find(key);
    if (state == states_.end()) {
        return std::nullopt;
    }
    return state->second;
}

uint64_t Dav3510RegisterStateManager::GetLaunchId() const noexcept { return launchId_; }

void Dav3510RegisterStateManager::Reset() noexcept { states_.clear(); }

std::size_t Dav3510RegisterStateManager::CoreKeyHash::operator()(const Dav3510CoreKey& key) const noexcept
{
    std::size_t result = std::hash<uint32_t>{}(key.blockType);
    result ^= std::hash<uint32_t>{}(key.blockId) + (result << 6U) + (result >> 2U);
    return result;
}

} // namespace aclsan::dav3510
