/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "internal/aclsan_device_data.h"
#include "internal/aclsan_device_data_log.h"
#include "internal/aclsan_log.h"
#include "internal/aclsan_memory_cbdata.h"

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace aclsan {

namespace {

constexpr uint64_t C0_SIZE = 32; // L1 中的 stride 和 burstLen 以 32 Byte 为单位

uint8_t FixpipeDestinationElementBytes(uint8_t quantPre) noexcept
{
    switch (quantPre) {
        case 0:
        case 14:
        case 15:
            return 4;
        case 1:
        case 10:
        case 11:
        case 16:
        case 31:
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
            return 2;
        case 2:
        case 3:
        case 4:
        case 5:
        case 8:
        case 9:
        case 12:
        case 13:
        case 23:
        case 24:
            return 1;
        default:
            return 0;
    }
}

// burstBytes 和 stride 单位都应该是Bytes
bool ConfigureMemoryAccessLayout(
    AclsanDeviceMemoryAccessData& callbackData, uint32_t burstNum, uint64_t burstBytes, uint64_t stride) noexcept
{
    // 连续场景：单次burst / 多次burst为连续 / burst长度为0(整体长度为0的连续)
    if (burstNum <= 1 || stride == burstBytes || burstBytes == 0) {
        // burstNum * burstBytes溢出场景
        if (burstNum != 0 && burstBytes > std::numeric_limits<uint64_t>::max() / burstNum) {
            ASC_SAN_ERROR(
                "ConfigureMemoryAccessLayout failed: burstBytes * burstNum overflows uint64_t, "
                "burstBytes=%llu burstNum=%u",
                static_cast<unsigned long long>(burstBytes), burstNum);
            return false;
        }
        callbackData.layoutKind = ACLSAN_MEM_LAYOUT_RANGE;
        callbackData.layout.range.bytes = burstBytes * burstNum;
        return true;
    }

    callbackData.layoutKind = ACLSAN_MEM_LAYOUT_BLOCK_REPEAT;
    callbackData.layout.blockRepeat.blockNum = burstNum;
    callbackData.layout.blockRepeat.blockSize = static_cast<uint32_t>(burstBytes);
    callbackData.layout.blockRepeat.blockStride = static_cast<int64_t>(stride);
    // TODO: 其实这个场景下是不是没有意义，不涉及repeat
    callbackData.layout.blockRepeat.repeatTimes = 1;
    callbackData.layout.blockRepeat.repeatStride = 0;
    return true;
}

template <typename ParamField>
constexpr bool IsMemoryAccessParamField() noexcept
{
    return std::is_same_v<ParamField, aclsan::CopyGmToUbufAlignV2ParamField> ||
           std::is_same_v<ParamField, aclsan::CopyGmToCbufAlignV2ParamField> ||
           std::is_same_v<ParamField, aclsan::CopyGmToCbufMultiNd2NzParamField> ||
           std::is_same_v<ParamField, aclsan::CopyGmToCbufMultiDn2NzParamField> ||
           std::is_same_v<ParamField, aclsan::CopyUbufToGmAlignV2ParamField> ||
           std::is_same_v<ParamField, aclsan::CopyGmToCbufV2ParamField> ||
           std::is_same_v<ParamField, aclsan::NdDmaParamField> ||
           std::is_same_v<ParamField, aclsan::LoadGmToCbuf2DV2ParamField> ||
           std::is_same_v<ParamField, aclsan::FixL0cToOutParamField>;
}

template <typename ParamField>
constexpr bool IsSyncParamField() noexcept
{
    return std::is_same_v<ParamField, aclsan::FlagParamField> || std::is_same_v<ParamField, aclsan::SyncBufParamField>;
}

class Translator final {
public:
    static std::optional<DeviceCallbackData> TranslateToCallbackData(
        const ParsedTraceRecord& parsed, const aclsan::DecodedInstruction& decoded) noexcept
    {
        const auto pipeline = static_cast<AclsanDevicePipeline>(parsed.record.pipeline);
        return std::visit(
            [&](const auto& value) noexcept -> std::optional<DeviceCallbackData> {
                using ParamField = std::decay_t<decltype(value)>;
                if constexpr (IsMemoryAccessParamField<ParamField>()) {
                    return MakeDeviceMemoryAccessCallbackData(parsed, pipeline, value);
                } else if constexpr (IsSyncParamField<ParamField>()) {
                    return MakeDeviceSyncData(parsed, decoded.kind, pipeline, value);
                } else {
                    return std::nullopt;
                }
            },
            decoded.params);
    }

private:
    static AclsanDeviceEventHeader MakeDeviceEventHeader(
        const ParsedTraceRecord& parsed, AclsanDevicePipeline pipeline, uint64_t serialNo, uint32_t dataSize) noexcept
    {
        return {
            ACLSAN_API_VERSION,
            dataSize,
            parsed.launchId,
            parsed.record.pc,
            parsed.record.siteId,
            0,
            parsed.instrExecId,
            serialNo,
            parsed.deviceId,
            parsed.phyCoreId,
            parsed.blockId,
            parsed.blockType,
            static_cast<uint32_t>(pipeline),
            0};
    }

    static AclsanDeviceMemoryAccessData MakeBaseDeviceMemoryAccessData(
        const AclsanDeviceEventHeader& header, uint64_t address, AclsanDeviceMemorySpace memorySpace,
        AclsanDeviceMemoryAccessMode accessMode, uint32_t dataBits = 0) noexcept
    {
        AclsanDeviceMemoryAccessData callbackData{};
        callbackData.header = header;
        callbackData.address = address;
        callbackData.memorySpace = static_cast<uint32_t>(memorySpace);
        callbackData.accessMode = static_cast<uint32_t>(accessMode);
        callbackData.dataBits = dataBits;
        return callbackData;
    }

    static std::optional<DeviceMemoryAccessDataList> MakeDeviceMemoryAccessDataList(
        const AclsanDeviceEventHeader& header, std::initializer_list<AclsanDeviceMemoryAccessData> accessData) noexcept
    {
        try {
            DeviceMemoryAccessDataList accesses(accessData);
            for (std::size_t index = 0; index < accesses.size(); ++index) {
                AclsanDeviceMemoryAccessData& access = accesses[index];
                access.header.serialNo = index;
                access.accessIndex = static_cast<uint32_t>(index);
                access.accessCount = static_cast<uint32_t>(accesses.size());
            }
            return accesses;
        } catch (const std::bad_alloc&) {
            ASC_SAN_ERROR(
                "MakeDeviceMemoryAccessData failed: cannot allocate memory access list, pc=0x%llx",
                static_cast<unsigned long long>(header.pc));
            return std::nullopt;
        }
    }

    static std::optional<DeviceMemoryAccessDataList> MakeDeviceMemoryAccessData(
        const AclsanDeviceEventHeader& header, const aclsan::CopyGmToCbufV2ParamField& params) noexcept
    {
        AclsanDeviceMemoryAccessData source = MakeBaseDeviceMemoryAccessData(
            header, params.srcAddr, ACLSAN_DEVICE_MEMORY_SPACE_GM, ACLSAN_DEVICE_MEMORY_ACCESS_READ);
        if (!ConfigureMemoryAccessLayout(
                source, params.burstNum, params.burstLen * C0_SIZE, params.srcStride * C0_SIZE)) {
            return std::nullopt;
        }

        AclsanDeviceMemoryAccessData destination = MakeBaseDeviceMemoryAccessData(
            header, params.dstAddr, ACLSAN_DEVICE_MEMORY_SPACE_L1, ACLSAN_DEVICE_MEMORY_ACCESS_WRITE);
        if (!ConfigureMemoryAccessLayout(
                destination, params.burstNum, params.burstLen * C0_SIZE, params.dstStride * C0_SIZE)) {
            return std::nullopt;
        }
        return MakeDeviceMemoryAccessDataList(header, {source, destination});
    }

    template <typename ParamField>
    static std::optional<DeviceCallbackData> MakeDeviceMemoryAccessCallbackData(
        const ParsedTraceRecord& parsed, AclsanDevicePipeline pipeline, const ParamField& params) noexcept
    {
        constexpr bool useExactGmConverter = std::is_same_v<ParamField, CopyGmToUbufAlignV2ParamField> ||
                                             std::is_same_v<ParamField, CopyGmToCbufAlignV2ParamField> ||
                                             std::is_same_v<ParamField, CopyGmToCbufMultiNd2NzParamField> ||
                                             std::is_same_v<ParamField, CopyGmToCbufMultiDn2NzParamField> ||
                                             std::is_same_v<ParamField, CopyUbufToGmAlignV2ParamField> ||
                                             std::is_same_v<ParamField, FixL0cToOutParamField> ||
                                             std::is_same_v<ParamField, LoadGmToCbuf2DV2ParamField> ||
                                             std::is_same_v<ParamField, NdDmaOutToUbufParamField>;
        if constexpr (useExactGmConverter) {
            MemoryInstructionField memoryField;
            if constexpr (std::is_same_v<ParamField, CopyGmToCbufMultiNd2NzParamField>) {
                memoryField = MultiNd2NzMemoryField{params, static_cast<uint16_t>(parsed.record.args[4])};
            } else if constexpr (std::is_same_v<ParamField, CopyGmToCbufMultiDn2NzParamField>) {
                memoryField = MultiDn2NzMemoryField{params, static_cast<uint16_t>(parsed.record.args[4])};
            } else if constexpr (std::is_same_v<ParamField, FixL0cToOutParamField>) {
                memoryField = FixpipeMemoryField{
                    params,
                    params.nSize,
                    params.mSize,
                    params.loopDstStride,
                    params.quantPre,
                    params.splitEnable,
                    params.nz2ndEnable,
                    params.nz2dnEnable,
                    FixpipeDestinationElementBytes(params.quantPre)};
            } else {
                memoryField = params;
            }

            const MemoryCbdataContext context{
                parsed.record.pc,
                parsed.instrExecId,
                0,
                parsed.record.siteId,
                parsed.phyCoreId,
                parsed.blockId,
                static_cast<uint32_t>(pipeline),
                parsed.launchId,
                parsed.deviceId,
                parsed.blockType};
            MemoryCbdataResult result = MemoryFieldToCbdataConverter{context}.Convert(memoryField);
            if (result.status == MemoryCbdataStatus::SUCCESS || result.status == MemoryCbdataStatus::NO_ACCESS) {
                return DeviceCallbackData{std::move(result.data)};
            }
            return std::nullopt;
        } else {
            std::optional<DeviceMemoryAccessDataList> accesses = MakeDeviceMemoryAccessData(
                MakeDeviceEventHeader(parsed, pipeline, 0, static_cast<uint32_t>(sizeof(AclsanDeviceMemoryAccessData))),
                params);
            if (!accesses.has_value()) {
                return std::nullopt;
            }
            return DeviceCallbackData{std::move(*accesses)};
        }
    }

    static DeviceCallbackData MakeDeviceSyncData(
        const ParsedTraceRecord& parsed, aclsan::DeviceInstructionKind kind, AclsanDevicePipeline pipeline,
        const aclsan::FlagParamField& params) noexcept
    {
        const bool isSet = kind == aclsan::DeviceInstructionKind::SetFlag;
        AclsanDeviceSyncData callbackData{};
        callbackData.header =
            MakeDeviceEventHeader(parsed, pipeline, 0, static_cast<uint32_t>(sizeof(AclsanDeviceSyncData)));
        callbackData.header.sourceKind = ACLSAN_DEVICE_SOURCE_SET_WAIT_FLAG;
        callbackData.syncKind = ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG;
        callbackData.action = isSet ? ACLSAN_DEVICE_SYNC_ACTION_SET : ACLSAN_DEVICE_SYNC_ACTION_WAIT;
        callbackData.scope = ACLSAN_DEVICE_SYNC_SCOPE_BLOCK;
        callbackData.srcPipe = params.srcPipe;
        callbackData.dstPipe = params.dstPipe;
        callbackData.objectId = params.eventId;
        return callbackData;
    }

    static DeviceCallbackData MakeDeviceSyncData(
        const ParsedTraceRecord& parsed, aclsan::DeviceInstructionKind kind, AclsanDevicePipeline pipeline,
        const aclsan::SyncBufParamField& params) noexcept
    {
        const bool isGet = kind == aclsan::DeviceInstructionKind::GetBuf;
        AclsanDeviceSyncData callbackData{};
        callbackData.header =
            MakeDeviceEventHeader(parsed, pipeline, 0, static_cast<uint32_t>(sizeof(AclsanDeviceSyncData)));
        callbackData.header.sourceKind = ACLSAN_DEVICE_SOURCE_GET_RLS_BUF;
        callbackData.syncKind = ACLSAN_DEVICE_SYNC_KIND_GET_RLS_BUF;
        callbackData.action = isGet ? ACLSAN_DEVICE_SYNC_ACTION_GET : ACLSAN_DEVICE_SYNC_ACTION_RELEASE;
        callbackData.scope = ACLSAN_DEVICE_SYNC_SCOPE_PIPE;
        callbackData.dstPipe = params.pipe;
        callbackData.mode = params.mode;
        callbackData.objectId = params.bufId;
        return callbackData;
    }
};

} // namespace

std::optional<DeviceCallbackData> TranslateDecodedTraceToCallbackData(
    const ParsedTraceRecord& parsed, const aclsan::DecodedInstruction& decoded) noexcept
{
    LogRawRecord(parsed);
    // 判断是否找到对应的paramfield  raw data -> param field
    if (std::holds_alternative<std::monostate>(decoded.params)) {
        ASC_SAN_DEBUG(
            "[param] rawData -> paramField translation failed instrId=%llu",
            static_cast<unsigned long long>(parsed.record.instrId));
        return std::nullopt;
    }
    LogParamField(decoded.params);

    std::optional<DeviceCallbackData> cbdata = Translator::TranslateToCallbackData(parsed, decoded);
    // 判断 param field -> cbdata 的转换是否成功
    if (cbdata == std::nullopt) {
        ASC_SAN_DEBUG(
            "[cbdata] paramField -> cbdata translation failed instrId=%llu",
            static_cast<unsigned long long>(parsed.record.instrId));
        return std::nullopt;
    }
    LogCallbackData(*cbdata);
    return cbdata;
}

} // namespace aclsan
