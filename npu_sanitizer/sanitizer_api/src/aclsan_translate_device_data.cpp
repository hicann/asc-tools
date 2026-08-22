/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "cce_instr/cce_instr_types.h"
#include "internal/aclsan_internal.h"
#include "internal/aclsan_log.h"

#include <array>
#include <cstdint>
#include <variant>

namespace aclsan {

namespace {

constexpr uint64_t kUnknownLaunchId = 0;
constexpr uint32_t kDefaultDeviceId = 0;
constexpr uint32_t kBlockTypeAiv = 1;
constexpr uint32_t kPipeMte2 = 2;

using DecodedOperand = std::variant<sanitizer::CopyOperand, sanitizer::NdDmaOperand, sanitizer::FlagOperand>;

void LogRawRecord(const sanitizer::AscsanRawTraceRecord& record, const TraceCallbackContext& context) noexcept
{
    ASC_SAN_DEBUG(
        "[raw] type=AscsanRawTraceRecord blockId=%u pc=0x%llx instrId=%llu siteId=%u pipeline=%u "
        "args=[0x%llx,0x%llx,0x%llx,0x%llx,0x%llx,0x%llx] transferBytes=%llu instrExecId=%llu "
        "serialNo=%llu coreId=%u",
        record.blockId, static_cast<unsigned long long>(record.pc), static_cast<unsigned long long>(record.instrId),
        record.siteId, record.pipeline, static_cast<unsigned long long>(record.args[0]),
        static_cast<unsigned long long>(record.args[1]), static_cast<unsigned long long>(record.args[2]),
        static_cast<unsigned long long>(record.args[3]), static_cast<unsigned long long>(record.args[4]),
        static_cast<unsigned long long>(record.args[5]), static_cast<unsigned long long>(context.transferBytes),
        static_cast<unsigned long long>(context.instrExecId), static_cast<unsigned long long>(context.serialNo),
        context.coreId);
}

void LogParamField(const CceInstructionParamField& params) noexcept
{
    if (const auto* value = std::get_if<sanitizer::CopyGmToUbufAlignV2ParamField>(&params)) {
        ASC_SAN_DEBUG(
            "[param] type=CopyGmToUbufAlignV2ParamField instr_id=%u dstAddr=0x%llx srcAddr=0x%llx "
            "sid=%u burstNum=%u burstLen=%u leftPaddingCount=%u rightPaddingCount=%u dataSelectBit=%u "
            "l2CacheControl=%u burstSrcStride=%llu burstDstStride=%u",
            value->instr_id, static_cast<unsigned long long>(value->dstAddr),
            static_cast<unsigned long long>(value->srcAddr), value->sid, value->burstNum, value->burstLen,
            value->leftPaddingCount, value->rightPaddingCount, value->dataSelectBit ? 1U : 0U, value->l2CacheControl,
            static_cast<unsigned long long>(value->burstSrcStride), value->burstDstStride);
        return;
    }
    if (const auto* value = std::get_if<sanitizer::CopyGmToCbufAlignV2ParamField>(&params)) {
        ASC_SAN_DEBUG(
            "[param] type=CopyGmToCbufAlignV2ParamField instr_id=%u dstAddr=0x%llx srcAddr=0x%llx "
            "sid=%u burstNum=%u burstLen=%u leftPaddingCount=%u rightPaddingCount=%u dataSelectBit=%u "
            "l2CacheControl=%u burstSrcStride=%llu burstDstStride=%u",
            value->instr_id, static_cast<unsigned long long>(value->dstAddr),
            static_cast<unsigned long long>(value->srcAddr), value->sid, value->burstNum, value->burstLen,
            value->leftPaddingCount, value->rightPaddingCount, value->dataSelectBit ? 1U : 0U, value->l2CacheControl,
            static_cast<unsigned long long>(value->burstSrcStride), value->burstDstStride);
        return;
    }
    if (const auto* value = std::get_if<sanitizer::CopyUbufToGmAlignV2ParamField>(&params)) {
        ASC_SAN_DEBUG(
            "[param] type=CopyUbufToGmAlignV2ParamField instr_id=%u dstAddr=0x%llx srcAddr=0x%llx "
            "sid=%u burstNum=%u burstLen=%u l2CacheControl=%u dstStride=%llu srcStride=%u",
            value->instr_id, static_cast<unsigned long long>(value->dstAddr),
            static_cast<unsigned long long>(value->srcAddr), value->sid, value->burstNum, value->burstLen,
            value->l2CacheControl, static_cast<unsigned long long>(value->dstStride), value->srcStride);
        return;
    }
    if (const auto* value = std::get_if<sanitizer::CopyGmToCbufV2ParamField>(&params)) {
        ASC_SAN_DEBUG(
            "[param] type=CopyGmToCbufV2ParamField instr_id=%u dstAddr=0x%llx srcAddr=0x%llx sid=%u "
            "burstNum=%u burstLen=%u padFunctionMode=%u l2CacheControl=%u srcStride=%llu dstStride=%u",
            value->instr_id, static_cast<unsigned long long>(value->dstAddr),
            static_cast<unsigned long long>(value->srcAddr), value->sid, value->burstNum, value->burstLen,
            value->padFunctionMode, value->l2CacheControl, static_cast<unsigned long long>(value->srcStride),
            value->dstStride);
        return;
    }
    if (const auto* value = std::get_if<sanitizer::FlagParamField>(&params)) {
        ASC_SAN_DEBUG(
            "[param] type=FlagParamField instr_id=%u srcPipe=%u dstPipe=%u eventId=%llu", value->instr_id,
            value->srcPipe, value->dstPipe, static_cast<unsigned long long>(value->eventId));
    }
}

void LogMemoryAccessData(const AclsanDeviceMemoryAccessData& value, uint32_t index) noexcept
{
    ASC_SAN_DEBUG(
        "[cbdata] type=AclsanDeviceMemoryAccessData index=%u address=0x%llx memorySpace=%u accessMode=%u "
        "accessIndex=%u accessCount=%u bytes=%llu pc=0x%llx siteId=%u instrExecId=%llu serialNo=%llu "
        "coreId=%u blockId=%u pipeline=%u",
        index, static_cast<unsigned long long>(value.address), value.memorySpace, value.accessMode, value.accessIndex,
        value.accessCount, static_cast<unsigned long long>(value.layout.range.bytes),
        static_cast<unsigned long long>(value.header.pc), value.header.siteId,
        static_cast<unsigned long long>(value.header.instrExecId),
        static_cast<unsigned long long>(value.header.serialNo), value.header.coreId, value.header.blockId,
        value.header.pipeline);
}

void LogCallbackData(const CceTraceCallbackData& callbackData) noexcept
{
    if (const auto* memory = std::get_if<DeviceMemoryAccessDataArray>(&callbackData)) {
        LogMemoryAccessData((*memory)[0], 0);
        LogMemoryAccessData((*memory)[1], 1);
        return;
    }
    if (const auto* sync = std::get_if<AclsanDeviceSyncData>(&callbackData)) {
        ASC_SAN_DEBUG(
            "[cbdata] type=AclsanDeviceSyncData pc=0x%llx instrExecId=%llu launchId=%llu "
            "blockId=%u phyCoreId=%u syncKind=%u action=%u scope=%u srcPipe=%u dstPipe=%u mode=%u objectId=%llu",
            static_cast<unsigned long long>(sync->pc), static_cast<unsigned long long>(sync->instrExecId),
            static_cast<unsigned long long>(sync->launchId), sync->blockId, sync->phyCoreId, sync->syncKind,
            sync->action, sync->scope, sync->srcPipe, sync->dstPipe, sync->mode,
            static_cast<unsigned long long>(sync->objectId));
    }
}

class Translator final {
public:
    static std::optional<CceInstructionParamField> Translate(const sanitizer::AscsanRawTraceRecord& record) noexcept
    {
        const TranslationRoute* route = FindRoute(record.instrId);
        if (route == nullptr) {
            return std::nullopt;
        }

        const std::optional<DecodedOperand> operand = route->parseOperand(record);
        if (!operand.has_value()) {
            return std::nullopt;
        }
        return route->convertParam(*operand);
    }

    static std::optional<CceTraceCallbackData> TranslateToCallbackData(
        const sanitizer::AscsanRawTraceRecord& record, const TraceCallbackContext& context,
        const CceInstructionParamField& params) noexcept
    {
        if (const auto* copy = std::get_if<sanitizer::CopyGmToUbufAlignV2ParamField>(&params)) {
            return MakeDeviceMemoryAccessData(record, context, *copy);
        }
        if (const auto* copy = std::get_if<sanitizer::CopyGmToCbufAlignV2ParamField>(&params)) {
            return MakeDeviceMemoryAccessData(record, context, *copy);
        }
        if (const auto* copy = std::get_if<sanitizer::CopyUbufToGmAlignV2ParamField>(&params)) {
            return MakeDeviceMemoryAccessData(record, context, *copy);
        }
        if (const auto* flag = std::get_if<sanitizer::FlagParamField>(&params)) {
            return MakeDeviceSyncData(record, context, *flag);
        }
        return std::nullopt;
    }

private:
    using ParseOperandFunc = std::optional<DecodedOperand> (*)(const sanitizer::AscsanRawTraceRecord& record) noexcept;
    using ConvertParamFunc = std::optional<CceInstructionParamField> (*)(const DecodedOperand& operand) noexcept;

    struct TranslationRoute {
        sanitizer::CceInstructionId instructionId;
        ParseOperandFunc parseOperand;
        ConvertParamFunc convertParam;
    };

    static const TranslationRoute* FindRoute(uint64_t instrId) noexcept
    {
        if (instrId > UINT32_MAX) {
            return nullptr;
        }

        const auto instructionId = static_cast<sanitizer::CceInstructionId>(instrId);
        static constexpr std::array<TranslationRoute, 15> kRoutes = {{
            {sanitizer::CceInstructionId::CopyGmToUbufAlignV2B8, ParseCopyOperand, ConvertCopyGmToUbufAlignV2},
            {sanitizer::CceInstructionId::CopyGmToUbufAlignV2B16, ParseCopyOperand, ConvertCopyGmToUbufAlignV2},
            {sanitizer::CceInstructionId::CopyGmToUbufAlignV2B32, ParseCopyOperand, ConvertCopyGmToUbufAlignV2},
            {sanitizer::CceInstructionId::CopyGmToCbufAlignV2B8, ParseCopyOperand, ConvertCopyGmToCbufAlignV2},
            {sanitizer::CceInstructionId::CopyGmToCbufAlignV2B16, ParseCopyOperand, ConvertCopyGmToCbufAlignV2},
            {sanitizer::CceInstructionId::CopyGmToCbufAlignV2B32, ParseCopyOperand, ConvertCopyGmToCbufAlignV2},
            {sanitizer::CceInstructionId::CopyUbufToGmAlignV2, ParseCopyOperand, ConvertCopyUbufToGmAlignV2},
            {sanitizer::CceInstructionId::SetFlag, ParseFlagOperand, ConvertFlag},
            {sanitizer::CceInstructionId::SetFlagI, ParseFlagOperand, ConvertFlag},
            {sanitizer::CceInstructionId::WaitFlag, ParseFlagOperand, ConvertFlag},
            {sanitizer::CceInstructionId::WaitFlagI, ParseFlagOperand, ConvertFlag},
            {sanitizer::CceInstructionId::SetFlagV, ParseFlagOperand, ConvertFlag},
            {sanitizer::CceInstructionId::SetFlagIV, ParseFlagOperand, ConvertFlag},
            {sanitizer::CceInstructionId::WaitFlagV, ParseFlagOperand, ConvertFlag},
            {sanitizer::CceInstructionId::WaitFlagIV, ParseFlagOperand, ConvertFlag},
        }};
        for (const TranslationRoute& route : kRoutes) {
            if (route.instructionId == instructionId) {
                return &route;
            }
        }
        return nullptr;
    }

    static std::optional<DecodedOperand> ParseCopyOperand(const sanitizer::AscsanRawTraceRecord& record) noexcept
    {
        return DecodedOperand{sanitizer::CopyOperand{
            static_cast<uint32_t>(record.instrId), record.args[0], record.args[1], record.args[2], record.args[3]}};
    }

    static std::optional<DecodedOperand> ParseFlagOperand(const sanitizer::AscsanRawTraceRecord& record) noexcept
    {
        if (record.args[0] > UINT32_MAX || record.args[1] > UINT32_MAX) {
            return std::nullopt;
        }
        return DecodedOperand{sanitizer::FlagOperand{
            static_cast<uint32_t>(record.instrId), static_cast<uint32_t>(record.args[0]),
            static_cast<uint32_t>(record.args[1]), record.args[2]}};
    }

    static std::optional<CceInstructionParamField> ConvertCopyGmToUbufAlignV2(const DecodedOperand& operand) noexcept
    {
        const auto* copyOperand = std::get_if<sanitizer::CopyOperand>(&operand);
        if (copyOperand == nullptr) {
            return std::nullopt;
        }
        return CceInstructionParamField{sanitizer::ConvertCopyGmToUbufAlignV2Operand(*copyOperand)};
    }

    static std::optional<CceInstructionParamField> ConvertCopyGmToCbufAlignV2(const DecodedOperand& operand) noexcept
    {
        const auto* copyOperand = std::get_if<sanitizer::CopyOperand>(&operand);
        if (copyOperand == nullptr) {
            return std::nullopt;
        }
        return CceInstructionParamField{sanitizer::ConvertCopyGmToCbufAlignV2Operand(*copyOperand)};
    }

    static std::optional<CceInstructionParamField> ConvertCopyUbufToGmAlignV2(const DecodedOperand& operand) noexcept
    {
        const auto* copyOperand = std::get_if<sanitizer::CopyOperand>(&operand);
        if (copyOperand == nullptr) {
            return std::nullopt;
        }
        return CceInstructionParamField{sanitizer::ConvertCopyUbufToGmAlignV2Operand(*copyOperand)};
    }

    static std::optional<CceInstructionParamField> ConvertFlag(const DecodedOperand& operand) noexcept
    {
        const auto* flagOperand = std::get_if<sanitizer::FlagOperand>(&operand);
        if (flagOperand == nullptr) {
            return std::nullopt;
        }
        return CceInstructionParamField{sanitizer::FlagParamField{
            flagOperand->instr_id, flagOperand->srcPipe, flagOperand->dstPipe, flagOperand->eventId}};
    }

    static AclsanDeviceEventHeader MakeDeviceEventHeader(
        const sanitizer::AscsanRawTraceRecord& record, const TraceCallbackContext& context) noexcept
    {
        return {ACLSAN_API_VERSION,  static_cast<uint32_t>(sizeof(AclsanDeviceMemoryAccessData)),
                kUnknownLaunchId,    record.pc,
                record.siteId,       0,
                context.instrExecId, context.serialNo,
                kDefaultDeviceId,    context.coreId,
                record.blockId,      kBlockTypeAiv,
                record.pipeline,     0};
    }

    static AclsanDeviceMemoryAccessData MakeDeviceMemoryAccessData(
        const AclsanDeviceEventHeader& header, uint64_t address, AclsanDeviceMemorySpace memorySpace,
        uint32_t accessMode, uint32_t accessIndex, uint64_t transferBytes) noexcept
    {
        AclsanDeviceMemoryAccessData callbackData{};
        callbackData.header = header;
        callbackData.address = address;
        callbackData.memorySpace = static_cast<uint32_t>(memorySpace);
        callbackData.accessMode = accessMode;
        callbackData.accessIndex = accessIndex;
        callbackData.accessCount = kDataCopyAccessCount;
        callbackData.layoutKind = ACLSAN_MEM_LAYOUT_RANGE;
        callbackData.layout.range.bytes = transferBytes;
        return callbackData;
    }

    template <typename ParamField>
    static CceTraceCallbackData MakeDeviceMemoryAccessData(
        const sanitizer::AscsanRawTraceRecord& record, const TraceCallbackContext& context,
        const ParamField& params) noexcept
    {
        const AclsanDeviceEventHeader header = MakeDeviceEventHeader(record, context);
        return DeviceMemoryAccessDataArray{
            MakeDeviceMemoryAccessData(
                header, params.srcAddr, ParamField::srcPos, ACLSAN_DEVICE_MEMORY_ACCESS_READ, 0, context.transferBytes),
            MakeDeviceMemoryAccessData(
                header, params.dstAddr, ParamField::dstPos, ACLSAN_DEVICE_MEMORY_ACCESS_WRITE, 1,
                context.transferBytes),
        };
    }

    static CceTraceCallbackData MakeDeviceMemoryAccessData(
        const sanitizer::AscsanRawTraceRecord& record, const TraceCallbackContext& context,
        const sanitizer::CopyUbufToGmAlignV2ParamField& params) noexcept
    {
        const AclsanDeviceEventHeader header = MakeDeviceEventHeader(record, context);
        return DeviceMemoryAccessDataArray{
            MakeDeviceMemoryAccessData(
                header, params.srcAddr, ACLSAN_DEVICE_MEMORY_SPACE_UB, ACLSAN_DEVICE_MEMORY_ACCESS_READ, 0,
                context.transferBytes),
            MakeDeviceMemoryAccessData(
                header, params.dstAddr, ACLSAN_DEVICE_MEMORY_SPACE_GM, ACLSAN_DEVICE_MEMORY_ACCESS_WRITE, 1,
                context.transferBytes),
        };
    }

    static CceTraceCallbackData MakeDeviceSyncData(
        const sanitizer::AscsanRawTraceRecord& record, const TraceCallbackContext& context,
        const sanitizer::FlagParamField& params) noexcept
    {
        const auto instructionId = static_cast<sanitizer::CceInstructionId>(params.instr_id);
        const bool isSet = instructionId == sanitizer::CceInstructionId::SetFlag ||
                           instructionId == sanitizer::CceInstructionId::SetFlagI ||
                           instructionId == sanitizer::CceInstructionId::SetFlagV ||
                           instructionId == sanitizer::CceInstructionId::SetFlagIV;
        const uint32_t action = isSet ? ACLSAN_DEVICE_SYNC_ACTION_SET : ACLSAN_DEVICE_SYNC_ACTION_WAIT;
        return AclsanDeviceSyncData{
            record.pc,
            context.instrExecId,
            kUnknownLaunchId,
            record.blockId,
            context.coreId,
            ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG,
            action,
            ACLSAN_DEVICE_SYNC_SCOPE_BLOCK,
            params.srcPipe,
            params.dstPipe,
            0,
            params.eventId,
            {0, 0}};
    }
};

AclsanDeviceEventHeader MakeDeviceEventHeader(const DeviceRecord& record) noexcept
{
    return {
        ACLSAN_API_VERSION,
        static_cast<uint32_t>(sizeof(AclsanDeviceMemoryAccessData)),
        kUnknownLaunchId,
        record.pc,
        static_cast<uint32_t>(record.sequence),
        0,
        record.sequence,
        record.serialNo,
        kDefaultDeviceId,
        record.coreId,
        record.blockId,
        kBlockTypeAiv,
        kPipeMte2,
        0};
}

AclsanDeviceMemoryAccessData MakeDeviceMemoryAccessData(
    const DeviceRecord& record, uint64_t address, uint32_t accessMode, uint32_t accessIndex) noexcept
{
    AclsanDeviceMemoryAccessData callbackData{};
    callbackData.header = MakeDeviceEventHeader(record);
    callbackData.address = address;
    callbackData.memorySpace = ACLSAN_DEVICE_MEMORY_SPACE_GM;
    callbackData.accessMode = accessMode;
    callbackData.accessIndex = accessIndex;
    callbackData.accessCount = kDataCopyAccessCount;
    callbackData.layoutKind = ACLSAN_MEM_LAYOUT_RANGE;
    callbackData.layout.range.bytes = record.dataCopy.transferBytes;
    return callbackData;
}

} // namespace

DeviceMemoryAccessDataArray TranslateDeviceMemoryAccessData(const DeviceRecord& record) noexcept
{
    return {
        MakeDeviceMemoryAccessData(record, record.dataCopy.sourceAddress, ACLSAN_DEVICE_MEMORY_ACCESS_READ, 0),
        MakeDeviceMemoryAccessData(record, record.dataCopy.destinationAddress, ACLSAN_DEVICE_MEMORY_ACCESS_WRITE, 1),
    };
}

AclsanDeviceSyncData TranslateDeviceSyncData(const DeviceRecord& record) noexcept
{
    const DeviceSyncRecord& source = record.sync;
    const uint32_t scope = source.syncKind == ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG ? ACLSAN_DEVICE_SYNC_SCOPE_BLOCK :
                                                                                      ACLSAN_DEVICE_SYNC_SCOPE_PIPE;
    return {record.pc,      source.instrExecId, kUnknownLaunchId, record.blockId,
            UINT32_MAX,     source.syncKind,    source.action,    scope,
            source.srcPipe, source.dstPipe,     source.mode,      source.objectId,
            {0, 0}};
}

std::optional<CceInstructionParamField> TranslateRawTraceRecord(const sanitizer::AscsanRawTraceRecord& record) noexcept
{
    return Translator::Translate(record);
}

std::optional<CceTraceCallbackData> TranslateRawTraceToCallbackData(
    const sanitizer::AscsanRawTraceRecord& record, const TraceCallbackContext& context) noexcept
{
    LogRawRecord(record, context);
    const std::optional<CceInstructionParamField> params = Translator::Translate(record);
    if (!params.has_value()) {
        ASC_SAN_DEBUG(
            "[param] type=none translation=failed instrId=%llu", static_cast<unsigned long long>(record.instrId));
        return std::nullopt;
    }
    LogParamField(*params);
    const std::optional<CceTraceCallbackData> callbackData =
        Translator::TranslateToCallbackData(record, context, *params);
    if (!callbackData.has_value()) {
        ASC_SAN_DEBUG(
            "[cbdata] type=none translation=failed instrId=%llu", static_cast<unsigned long long>(record.instrId));
        return std::nullopt;
    }
    LogCallbackData(*callbackData);
    return callbackData;
}

} // namespace aclsan
