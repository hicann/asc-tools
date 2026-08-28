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

#include <cstdint>
#include <variant>
#include <vector>

namespace aclsan {

enum class MemoryCbdataStatus : uint8_t {
    SUCCESS,
    NO_ACCESS,
    INVALID_FIELD,
    ARITHMETIC_OVERFLOW,
    RESOURCE_EXHAUSTED,
};

template <NdNzConversionMode ConversionMode>
struct MultiMemoryField {
    CopyGmToCbufMultiParamField<ConversionMode> field;
    uint16_t matrixNum = 0;
};

using MultiNd2NzMemoryField = MultiMemoryField<NdNzConversionMode::ND2NZ>;
using MultiDn2NzMemoryField = MultiMemoryField<NdNzConversionMode::DN2NZ>;

struct FixpipeMemoryField {
    FixL0cToOutParamField field;
    uint16_t nSize = 0;
    uint16_t mSize = 0;
    uint32_t dstStride = 0;
    uint8_t quantPre = 0;
    bool channelSplit = false;
    bool nz2nd = false;
    bool nz2dn = false;
    uint8_t dstElementBytes = 0;
};

using MemoryInstructionField = std::variant<
    CopyGmToUbufAlignV2ParamField, CopyGmToCbufAlignV2ParamField, MultiNd2NzMemoryField, MultiDn2NzMemoryField,
    CopyUbufToGmAlignV2ParamField, FixpipeMemoryField, LoadGmToCbuf2DV2ParamField, NdDmaOutToUbufParamField>;

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
};

class MemoryFieldToCbdataConverter final {
public:
    explicit MemoryFieldToCbdataConverter(MemoryCbdataContext context) noexcept;

    [[nodiscard]] MemoryCbdataResult Convert(const MemoryInstructionField& field) const noexcept;

private:
    MemoryCbdataContext context_;
};

} // namespace aclsan

#endif // ACLSAN_MEMORY_CBDATA_H
