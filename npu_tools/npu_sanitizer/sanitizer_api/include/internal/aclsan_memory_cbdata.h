/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_MEMORY_CBDATA_H
#define ACLSAN_MEMORY_CBDATA_H

#include "aclsan/aclsan_cbdata_device.h"
#include "device_instr/common/device_instr_struct_dma.h"
#include "device_instr/common/device_instr_struct_register.h"

#include <array>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

namespace aclsan {

enum class MemoryCbdataStatus : uint8_t {
    SUCCESS,
    NO_ACCESS,
    INVALID_FIELD,
    MISSING_REGISTER_STATE,
    ARITHMETIC_OVERFLOW,
    RESOURCE_EXHAUSTED,
};

using MemoryInstructionField = std::variant<
    CopyGmToUbufAlignV2ParamField, CopyGmToCbufAlignV2ParamField, CopyGmToCbufMultiNd2NzParamField,
    CopyGmToCbufMultiDn2NzParamField, CopyGmToCbufV2ParamField, CopyUbufToGmAlignV2ParamField, FixL0cToOutParamField,
    LoadGmToCbuf2DV2ParamField, NdDmaOutToUbufParamField>;

struct MemoryRegisterState {
    std::optional<Mte2SourceParamField> mte2Source;
    std::optional<NdDmaPadCountParamField> ndDmaPadCount;
    std::array<std::optional<NdDmaLoopStrideParamField>, 5> ndDmaLoopStrides{};
    std::optional<Mte2NzParamField> mte2Nz;
    std::optional<Loop3ParamField> loop3;
    std::array<std::optional<DmaLoopSizeParamField>, 3> dmaLoopSizes{};
    std::array<std::array<std::optional<DmaLoopStrideParamField>, 2>, 3> dmaLoopStrides{};
};

struct MemoryCbdataContext {
    uint64_t pc = 0;
    uint64_t instrExecId = 0;
    uint64_t serialNo = 0;
    uint32_t siteId = 0;
    uint32_t coreId = 0;
    uint32_t blockId = 0;
    uint32_t pipeline = 0;
    uint64_t launchId = 0;
    uint32_t deviceId = 0;
    uint32_t blockType = ACLSAN_DEVICE_BLOCK_TYPE_AICORE;
};

using MemoryCbdata = std::vector<AclsanDeviceMemoryAccessData>;

struct MemoryCbdataResult {
    MemoryCbdataStatus status = MemoryCbdataStatus::INVALID_FIELD;
    MemoryCbdata data;
    uint64_t requiredRegisterInstructionId = 0;
};

class MemoryFieldToCbdataConverter final {
public:
    explicit MemoryFieldToCbdataConverter(MemoryCbdataContext context, MemoryRegisterState registerState = {}) noexcept;

    [[nodiscard]] MemoryCbdataResult Convert(const MemoryInstructionField& field) const noexcept;

private:
    MemoryCbdataContext context_;
    MemoryRegisterState registerState_;
};

} // namespace aclsan

#endif // ACLSAN_MEMORY_CBDATA_H
