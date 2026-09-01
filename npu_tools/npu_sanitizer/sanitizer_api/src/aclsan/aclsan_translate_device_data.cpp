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
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace aclsan {

namespace {

const char* MemoryCbdataStatusName(MemoryCbdataStatus status) noexcept
{
    switch (status) {
        case MemoryCbdataStatus::SUCCESS:
            return "SUCCESS";
        case MemoryCbdataStatus::NO_ACCESS:
            return "NO_ACCESS";
        case MemoryCbdataStatus::INVALID_FIELD:
            return "INVALID_FIELD";
        case MemoryCbdataStatus::MISSING_REGISTER_STATE:
            return "MISSING_REGISTER_STATE";
        case MemoryCbdataStatus::ARITHMETIC_OVERFLOW:
            return "ARITHMETIC_OVERFLOW";
        case MemoryCbdataStatus::RESOURCE_EXHAUSTED:
            return "RESOURCE_EXHAUSTED";
    }
    return "UNKNOWN";
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
        const ParsedTraceRecord& parsed, const aclsan::DecodedInstruction& decoded,
        const MemoryRegisterState& registerState) noexcept
    {
        const auto pipeline = static_cast<AclsanDevicePipeline>(parsed.record.pipeline);
        return std::visit(
            [&](const auto& value) noexcept -> std::optional<DeviceCallbackData> {
                using ParamField = std::decay_t<decltype(value)>;
                if constexpr (IsMemoryAccessParamField<ParamField>()) {
                    return MakeDeviceMemoryAccessCallbackData(parsed, pipeline, value, registerState);
                } else if constexpr (std::is_same_v<ParamField, aclsan::LocalMemoryTransferParamField>) {
                    ASC_SAN_DEBUG(
                        "[cbdata] no GM access for local-only memory instruction instrId=%u kind=%u", value.instrId,
                        static_cast<unsigned int>(value.kind));
                    return DeviceCallbackData{DeviceMemoryAccessDataList{}};
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

    template <typename ParamField>
    static std::optional<DeviceCallbackData> MakeDeviceMemoryAccessCallbackData(
        const ParsedTraceRecord& parsed, AclsanDevicePipeline pipeline, const ParamField& params,
        const MemoryRegisterState& registerState) noexcept
    {
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
        MemoryCbdataResult result =
            MemoryFieldToCbdataConverter{context, registerState}.Convert(MemoryInstructionField{params});
        if (result.status == MemoryCbdataStatus::SUCCESS || result.status == MemoryCbdataStatus::NO_ACCESS) {
            return DeviceCallbackData{std::move(result.data)};
        }
        ASC_SAN_ERROR(
            "acl_san trace: cannot resolve GM memory access status=%s instrId=%llu pc=0x%llx blockType=%u "
            "blockId=%u requiredSetInstrId=%llu",
            MemoryCbdataStatusName(result.status), static_cast<unsigned long long>(parsed.record.instrId),
            static_cast<unsigned long long>(parsed.record.pc), parsed.blockType, parsed.blockId,
            static_cast<unsigned long long>(result.requiredRegisterInstructionId));
        return std::nullopt;
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
    const ParsedTraceRecord& parsed, const aclsan::DecodedInstruction& decoded,
    const MemoryRegisterState& registerState) noexcept
{
    LogRawRecord(parsed);
    // 判断是否找到对应的paramfield  raw data -> param field
    if (std::holds_alternative<std::monostate>(decoded.params)) {
        ASC_SAN_DEBUG("[param] rawData -> paramField translation failed instrId=%u", parsed.record.instrId);
        return std::nullopt;
    }
    LogParamField(decoded.params);

    std::optional<DeviceCallbackData> cbdata = Translator::TranslateToCallbackData(parsed, decoded, registerState);
    // 判断 param field -> cbdata 的转换是否成功
    if (cbdata == std::nullopt) {
        ASC_SAN_DEBUG("[cbdata] paramField -> cbdata translation failed instrId=%u", parsed.record.instrId);
        return std::nullopt;
    }
    LogCallbackData(*cbdata);
    return cbdata;
}

} // namespace aclsan
