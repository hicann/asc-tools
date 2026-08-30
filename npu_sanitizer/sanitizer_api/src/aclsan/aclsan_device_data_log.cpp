/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "internal/aclsan_device_data_log.h"
#include "internal/aclsan_log.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <variant>

namespace aclsan {

namespace {

template <typename ParamField>
constexpr bool HasParamFieldLogger() noexcept
{
    return std::is_same_v<ParamField, aclsan::CopyGmToUbufAlignV2ParamField> ||
           std::is_same_v<ParamField, aclsan::CopyGmToCbufAlignV2ParamField> ||
           std::is_same_v<ParamField, aclsan::CopyUbufToGmAlignV2ParamField> ||
           std::is_same_v<ParamField, aclsan::CopyGmToCbufV2ParamField> ||
           std::is_same_v<ParamField, aclsan::LoadGmToCbuf2DV2ParamField> ||
           std::is_same_v<ParamField, aclsan::NdDmaParamField> ||
           std::is_same_v<ParamField, aclsan::FixL0cToOutParamField> ||
           std::is_same_v<ParamField, aclsan::SetPaddingParamField> ||
           std::is_same_v<ParamField, aclsan::FlagParamField> || std::is_same_v<ParamField, aclsan::SyncBufParamField>;
}

const char* BlockTypeName(uint32_t blockType) noexcept
{
    switch (blockType) {
        case ACLSAN_DEVICE_BLOCK_TYPE_AICORE:
            return "AICORE";
        case ACLSAN_DEVICE_BLOCK_TYPE_AICORE_VECTOR:
            return "AIV";
        case ACLSAN_DEVICE_BLOCK_TYPE_AICORE_CUBE:
            return "AIC";
        default:
            return "UNKNOWN";
    }
}

void LogParamField(const aclsan::SetPaddingParamField& value) noexcept
{
    ASC_SAN_DEBUG("[param] type=SetPaddingParamField value=0x%llx", static_cast<unsigned long long>(value.value));
}

void LogParamField(const aclsan::CopyGmToUbufAlignV2ParamField& value) noexcept
{
    ASC_SAN_DEBUG(
        "[param] type=CopyGmToUbufAlignV2ParamField instrId=%u dstAddr=0x%llx srcAddr=0x%llx "
        "sid=%u burstNum=%u burstLen=%u leftPaddingCount=%u rightPaddingCount=%u dataSelectBit=%u l2CacheControl=%u "
        "burstSrcStride=%llu burstDstStride=%u",
        value.instrId, static_cast<unsigned long long>(value.dstAddr), static_cast<unsigned long long>(value.srcAddr),
        value.sid, value.burstNum, value.burstLen, value.leftPaddingCount, value.rightPaddingCount,
        value.dataSelectBit ? 1U : 0U, value.l2CacheControl, static_cast<unsigned long long>(value.burstSrcStride),
        value.burstDstStride);
}

void LogParamField(const aclsan::CopyGmToCbufAlignV2ParamField& value) noexcept
{
    ASC_SAN_DEBUG(
        "[param] type=CopyGmToCbufAlignV2ParamField instrId=%u dstAddr=0x%llx srcAddr=0x%llx "
        "sid=%u burstNum=%u burstLen=%u leftPaddingCount=%u rightPaddingCount=%u dataSelectBit=%u l2CacheControl=%u "
        "burstSrcStride=%llu burstDstStride=%u",
        value.instrId, static_cast<unsigned long long>(value.dstAddr), static_cast<unsigned long long>(value.srcAddr),
        value.sid, value.burstNum, value.burstLen, value.leftPaddingCount, value.rightPaddingCount,
        value.dataSelectBit ? 1U : 0U, value.l2CacheControl, static_cast<unsigned long long>(value.burstSrcStride),
        value.burstDstStride);
}

void LogParamField(const aclsan::CopyUbufToGmAlignV2ParamField& value) noexcept
{
    ASC_SAN_DEBUG(
        "[param] type=CopyUbufToGmAlignV2ParamField instrId=%u dstAddr=0x%llx srcAddr=0x%llx "
        "sid=%u burstNum=%u burstLen=%u l2CacheControl=%u dstStride=%llu srcStride=%u",
        value.instrId, static_cast<unsigned long long>(value.dstAddr), static_cast<unsigned long long>(value.srcAddr),
        value.sid, value.burstNum, value.burstLen, value.l2CacheControl,
        static_cast<unsigned long long>(value.dstStride), value.srcStride);
}

void LogParamField(const aclsan::CopyGmToCbufV2ParamField& value) noexcept
{
    ASC_SAN_DEBUG(
        "[param] type=CopyGmToCbufV2ParamField instrId=%u dstAddr=0x%llx srcAddr=0x%llx sid=%u "
        "burstNum=%u burstLen=%u padFunctionMode=%u l2CacheControl=%u srcStride=%llu dstStride=%u",
        value.instrId, static_cast<unsigned long long>(value.dstAddr), static_cast<unsigned long long>(value.srcAddr),
        value.sid, value.burstNum, value.burstLen, value.padFunctionMode, value.l2CacheControl,
        static_cast<unsigned long long>(value.srcStride), value.dstStride);
}

void LogParamField(const aclsan::LoadGmToCbuf2DV2ParamField& value) noexcept
{
    ASC_SAN_DEBUG(
        "[param] type=LoadGmToCbuf2DV2ParamField instrId=%u dstAddr=0x%llx srcAddr=0x%llx "
        "mStartPosition=%u kStartPosition=%u dstStride=%u mStep=%u kStep=%u sid=%u decompMode=%u "
        "l2CacheControl=%u",
        value.instrId, static_cast<unsigned long long>(value.dstAddr), static_cast<unsigned long long>(value.srcAddr),
        value.mStartPosition, value.kStartPosition, value.dstStride, value.mStep, value.kStep, value.sid,
        value.decompMode, value.l2CacheControl);
}

void LogParamField(const aclsan::NdDmaParamField& value) noexcept
{
    ASC_SAN_DEBUG(
        "[param] type=NdDmaParamField instrId=%u dataBits=%u dstAddr=0x%llx srcAddr=0x%llx sid=%u "
        "loopSizes=[%u,%u,%u,%u,%u] loop0LeftPaddingCount=%u loop0RightPaddingCount=%u paddingMode=%u "
        "l2CacheControl=%u",
        value.instrId, value.dataBits, static_cast<unsigned long long>(value.dstAddr),
        static_cast<unsigned long long>(value.srcAddr), value.sid, value.loop0Size, value.loop1Size, value.loop2Size,
        value.loop3Size, value.loop4Size, value.loop0LeftPaddingCount, value.loop0RightPaddingCount,
        value.paddingMode ? 1U : 0U, value.l2CacheControl);
}

void LogParamField(const aclsan::FixL0cToOutParamField& value) noexcept
{
    ASC_SAN_DEBUG(
        "[param] type=FixL0cToOutParamField instrId=%u dataBits=%u dstAddr=0x%llx srcAddr=0x%llx sid=%u "
        "nSize=%u mSize=%u loopDstStride=%u loopSrtStride=%u",
        value.instrId, value.dataBits, static_cast<unsigned long long>(value.dstAddr),
        static_cast<unsigned long long>(value.srcAddr), value.sid, value.nSize, value.mSize, value.loopDstStride,
        value.loopSrtStride);
}

void LogParamField(const aclsan::FlagParamField& value) noexcept
{
    ASC_SAN_DEBUG(
        "[param] type=FlagParamField instrId=%u srcPipe=%u dstPipe=%u eventId=%llu", value.instrId, value.srcPipe,
        value.dstPipe, static_cast<unsigned long long>(value.eventId));
}

void LogParamField(const aclsan::SyncBufParamField& value) noexcept
{
    ASC_SAN_DEBUG(
        "[param] type=SyncBufParamField instrId=%u pipe=%u bufId=0x%llx mode=%u", value.instrId, value.pipe,
        static_cast<unsigned long long>(value.bufId), static_cast<uint32_t>(value.mode));
}

void LogMemoryAccessData(const AclsanDeviceMemoryAccessData& value, uint32_t index) noexcept
{
    const uint64_t rangeBytes = value.layoutKind == ACLSAN_MEM_LAYOUT_RANGE ? value.layout.range.bytes : 0;
    ASC_SAN_DEBUG(
        "[cbdata] deviceId=%u phyCoreId=%u blockId=%u blockType=%s  instrExecId=%llu launchId=%llu  "
        "type=AclsanDeviceMemoryAccessData index=%u address=0x%llx memorySpace=%u accessMode=%u "
        "accessIndex=%u accessCount=%u bytes=%llu layoutKind=%u pc=0x%llx siteId=%u serialNo=%llu pipeline=%u",
        value.header.deviceId, value.header.phyCoreId, value.header.blockId, BlockTypeName(value.header.blockType),
        static_cast<unsigned long long>(value.header.instrExecId),
        static_cast<unsigned long long>(value.header.launchId), index, static_cast<unsigned long long>(value.address),
        value.memorySpace, value.accessMode, value.accessIndex, value.accessCount,
        static_cast<unsigned long long>(rangeBytes), value.layoutKind, static_cast<unsigned long long>(value.header.pc),
        value.header.siteId, static_cast<unsigned long long>(value.header.serialNo), value.header.pipeline);

    if (value.layoutKind == ACLSAN_MEM_LAYOUT_RANGE) {
        ASC_SAN_DEBUG("[cbdata] layout=range bytes=%llu", static_cast<unsigned long long>(value.layout.range.bytes));
    } else if (value.layoutKind == ACLSAN_MEM_LAYOUT_BLOCK_REPEAT) {
        ASC_SAN_DEBUG(
            "[cbdata] layout=block_repeat blockNum=%u blockSize=%u blockStride=%lld repeatTimes=%u "
            "repeatStride=%lld",
            value.layout.blockRepeat.blockNum, value.layout.blockRepeat.blockSize,
            static_cast<long long>(value.layout.blockRepeat.blockStride), value.layout.blockRepeat.repeatTimes,
            static_cast<long long>(value.layout.blockRepeat.repeatStride));
    }
}

void LogCallbackData(const DeviceMemoryAccessDataList& memory) noexcept
{
    for (std::size_t index = 0; index < memory.size(); ++index) {
        LogMemoryAccessData(memory[index], static_cast<uint32_t>(index));
    }
}

void LogCallbackData(const AclsanDeviceSyncData& sync) noexcept
{
    ASC_SAN_DEBUG(
        "[cbdata] deviceId=%u phyCoreId=%u blockId=%u blockType=%s  instrExecId=%llu launchId=%llu  "
        "type=AclsanDeviceSyncData pc=0x%llx serialNo=%llu syncKind=%u action=%u scope=%u srcPipe=%u "
        "dstPipe=%u mode=%u objectId=%llu",
        sync.header.deviceId, sync.header.phyCoreId, sync.header.blockId, BlockTypeName(sync.header.blockType),
        static_cast<unsigned long long>(sync.header.instrExecId), static_cast<unsigned long long>(sync.header.launchId),
        static_cast<unsigned long long>(sync.header.pc), static_cast<unsigned long long>(sync.header.serialNo),
        sync.syncKind, sync.action, sync.scope, sync.srcPipe, sync.dstPipe, sync.mode,
        static_cast<unsigned long long>(sync.objectId));
}

} // namespace

void LogRawRecord(const ParsedTraceRecord& parsed) noexcept
{
    const AclsanRawTraceRecord& record = parsed.record;
    ASC_SAN_DEBUG(
        "[raw] deviceId=%u phyCoreId=%u blockId=%u blockType=%s  instrExecId=%llu launchId=%llu  "
        "type=AclsanRawTraceRecord pc=0x%llx instrId=%u siteId=%u category=%u pipeline=%u "
        "args=[0x%llx,0x%llx,0x%llx,0x%llx,0x%llx]",
        parsed.deviceId, parsed.phyCoreId, parsed.blockId, BlockTypeName(parsed.blockType),
        static_cast<unsigned long long>(parsed.instrExecId), static_cast<unsigned long long>(parsed.launchId),
        static_cast<unsigned long long>(record.pc), record.instrId, record.siteId,
        static_cast<uint32_t>(record.category), record.pipeline, static_cast<unsigned long long>(record.args[0]),
        static_cast<unsigned long long>(record.args[1]), static_cast<unsigned long long>(record.args[2]),
        static_cast<unsigned long long>(record.args[3]), static_cast<unsigned long long>(record.args[4]));
}

void LogParamField(const aclsan::DeviceInstructionParamField& params) noexcept
{
    std::visit(
        [](const auto& value) noexcept {
            using ParamField = std::decay_t<decltype(value)>;
            if constexpr (HasParamFieldLogger<ParamField>()) {
                LogParamField(value);
            }
        },
        params);
}

void LogCallbackData(const DeviceCallbackData& callbackData) noexcept
{
    std::visit([](const auto& value) noexcept { LogCallbackData(value); }, callbackData);
}

} // namespace aclsan
