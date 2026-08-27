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
#include <limits>
#include <variant>

namespace aclsan {

namespace {

constexpr uint64_t kUnknownLaunchId = 0;
constexpr uint32_t kDefaultDeviceId = 0;
constexpr uint32_t kBlockTypeAiv = 1;
constexpr uint32_t kPipeMte2 = 2;

using DecodedOperand =
    std::variant<sanitizer::CopyOperand, sanitizer::NdDmaOperand, sanitizer::FlagOperand, sanitizer::BufferParamField>;

uint64_t ExtractBitRange(uint64_t value, uint32_t begin, uint32_t end) noexcept
{
    return (value >> begin) & ((1ULL << (end - begin)) - 1ULL);
}

uint8_t FixpipeDestinationElementBytes(uint64_t quantPre) noexcept
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

bool MultiplyWithoutOverflow(uint64_t left, uint64_t right, uint64_t& product) noexcept
{
    if (left != 0 && right > std::numeric_limits<uint64_t>::max() / left) {
        return false;
    }
    product = left * right;
    return true;
}

uint64_t MultiInstructionElementBytes(uint64_t instructionId) noexcept
{
    switch (static_cast<sanitizer::CceInstructionId>(instructionId)) {
        case sanitizer::CceInstructionId::CopyGmToCbufMultiNd2NzB8:
        case sanitizer::CceInstructionId::CopyGmToCbufMultiDn2NzB8:
            return 1;
        case sanitizer::CceInstructionId::CopyGmToCbufMultiNd2NzB16:
        case sanitizer::CceInstructionId::CopyGmToCbufMultiDn2NzB16:
            return 2;
        case sanitizer::CceInstructionId::CopyGmToCbufMultiNd2NzB32:
        case sanitizer::CceInstructionId::CopyGmToCbufMultiDn2NzB32:
            return 4;
        default:
            return 0;
    }
}

uint32_t ToSyncAction(uint32_t instrId) noexcept
{
    switch (static_cast<sanitizer::CceInstructionId>(instrId)) {
        case sanitizer::CceInstructionId::SetFlag:
        case sanitizer::CceInstructionId::SetFlagI:
        case sanitizer::CceInstructionId::SetFlagV:
        case sanitizer::CceInstructionId::SetFlagIV:
            return ACLSAN_DEVICE_SYNC_ACTION_SET;
        case sanitizer::CceInstructionId::WaitFlag:
        case sanitizer::CceInstructionId::WaitFlagI:
        case sanitizer::CceInstructionId::WaitFlagV:
        case sanitizer::CceInstructionId::WaitFlagIV:
            return ACLSAN_DEVICE_SYNC_ACTION_WAIT;
        case sanitizer::CceInstructionId::GetBuf:
        case sanitizer::CceInstructionId::GetBufI:
        case sanitizer::CceInstructionId::GetBufV:
        case sanitizer::CceInstructionId::GetBufIV:
            return ACLSAN_DEVICE_SYNC_ACTION_GET;
        case sanitizer::CceInstructionId::RlsBuf:
        case sanitizer::CceInstructionId::RlsBufI:
        case sanitizer::CceInstructionId::RlsBufV:
        case sanitizer::CceInstructionId::RlsBufIV:
            return ACLSAN_DEVICE_SYNC_ACTION_RELEASE;
        default:
            return ACLSAN_DEVICE_SYNC_ACTION_UNKNOWN;
    }
}

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
        return;
    }
    if (const auto* value = std::get_if<sanitizer::BufferParamField>(&params)) {
        ASC_SAN_DEBUG(
            "[param] type=SyncBufParamField instr_id=%u pipe=%u bufId=%llu mode=%u", value->instr_id, value->pipe,
            static_cast<unsigned long long>(value->bufId), value->mode);
    }
}

uint64_t MemoryAccessUnitBytes(const AclsanDeviceMemoryAccessData& value) noexcept
{
    switch (value.layoutKind) {
        case ACLSAN_MEM_LAYOUT_SCALAR:
            return value.layout.scalar.bytes;
        case ACLSAN_MEM_LAYOUT_RANGE:
            return value.layout.range.bytes;
        case ACLSAN_MEM_LAYOUT_BLOCK_REPEAT:
            return value.layout.blockRepeat.blockSize;
        case ACLSAN_MEM_LAYOUT_ND_AFFINE:
            return value.layout.ndAffine.elementBytes;
        default:
            return 0;
    }
}

void LogMemoryAccessData(const AclsanDeviceMemoryAccessData& value, uint32_t index) noexcept
{
    ASC_SAN_DEBUG(
        "[cbdata] type=AclsanDeviceMemoryAccessData index=%u address=0x%llx memorySpace=%u accessMode=%u "
        "accessIndex=%u accessCount=%u layoutKind=%u bytes=%llu pc=0x%llx siteId=%u instrExecId=%llu serialNo=%llu "
        "coreId=%u blockId=%u pipeline=%u",
        index, static_cast<unsigned long long>(value.address), value.memorySpace, value.accessMode, value.accessIndex,
        value.accessCount, value.layoutKind, static_cast<unsigned long long>(MemoryAccessUnitBytes(value)),
        static_cast<unsigned long long>(value.header.pc), value.header.siteId,
        static_cast<unsigned long long>(value.header.instrExecId),
        static_cast<unsigned long long>(value.header.serialNo), value.header.coreId, value.header.blockId,
        value.header.pipeline);
}

void LogCallbackData(const CceTraceCallbackData& callbackData) noexcept
{
    if (const auto* memory = std::get_if<MemoryCbdata>(&callbackData)) {
        for (uint32_t index = 0; index < memory->size(); ++index) {
            LogMemoryAccessData((*memory)[index], index);
        }
        return;
    }
    if (const auto* sync = std::get_if<AclsanDeviceSyncData>(&callbackData)) {
        ASC_SAN_DEBUG(
            "[cbdata] type=AclsanDeviceSyncData pc=0x%llx instrExecId=%llu launchId=%llu "
            "blockId=%u coreId=%u syncKind=%u action=%u scope=%u srcPipe=%u dstPipe=%u mode=%u objectId=%llu",
            static_cast<unsigned long long>(sync->header.pc), static_cast<unsigned long long>(sync->header.instrExecId),
            static_cast<unsigned long long>(sync->header.launchId), sync->header.blockId, sync->header.coreId,
            sync->syncKind, sync->action, sync->scope, sync->srcPipe, sync->dstPipe, sync->mode,
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
            return ConvertMemoryField(record, context, MemoryInstructionField{*copy});
        }
        if (const auto* copy = std::get_if<sanitizer::CopyGmToCbufAlignV2ParamField>(&params)) {
            return ConvertMemoryField(record, context, MemoryInstructionField{*copy});
        }
        if (const auto* copy = std::get_if<sanitizer::CopyGmToCbufMultiNd2NzParamField>(&params)) {
            return ConvertMemoryField(
                record, context,
                MemoryInstructionField{MultiNd2NzMemoryField{*copy, static_cast<uint16_t>(record.args[4])}});
        }
        if (const auto* copy = std::get_if<sanitizer::CopyGmToCbufMultiDn2NzParamField>(&params)) {
            return ConvertMemoryField(
                record, context,
                MemoryInstructionField{MultiDn2NzMemoryField{*copy, static_cast<uint16_t>(record.args[4])}});
        }
        if (const auto* copy = std::get_if<sanitizer::CopyUbufToGmAlignV2ParamField>(&params)) {
            return ConvertMemoryField(record, context, MemoryInstructionField{*copy});
        }
        if (const auto* fixpipe = std::get_if<sanitizer::FixL0cToOutParamField>(&params)) {
            return ConvertMemoryField(record, context, MemoryInstructionField{DecodeFixpipeField(*fixpipe, record)});
        }
        if (const auto* flag = std::get_if<sanitizer::FlagParamField>(&params)) {
            return MakeDeviceSyncData(record, context, *flag);
        }
        if (const auto* buffer = std::get_if<sanitizer::BufferParamField>(&params)) {
            return MakeDeviceSyncData(record, context, *buffer);
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
        const auto instructionId = static_cast<sanitizer::CceInstructionId>(instrId);
        static constexpr std::array<TranslationRoute, 31> kRoutes = {{
            {sanitizer::CceInstructionId::CopyGmToUbufAlignV2B8, ParseCopyOperand, ConvertCopyGmToUbufAlignV2},
            {sanitizer::CceInstructionId::CopyGmToUbufAlignV2B16, ParseCopyOperand, ConvertCopyGmToUbufAlignV2},
            {sanitizer::CceInstructionId::CopyGmToUbufAlignV2B32, ParseCopyOperand, ConvertCopyGmToUbufAlignV2},
            {sanitizer::CceInstructionId::CopyGmToCbufAlignV2B8, ParseCopyOperand, ConvertCopyGmToCbufAlignV2},
            {sanitizer::CceInstructionId::CopyGmToCbufAlignV2B16, ParseCopyOperand, ConvertCopyGmToCbufAlignV2},
            {sanitizer::CceInstructionId::CopyGmToCbufAlignV2B32, ParseCopyOperand, ConvertCopyGmToCbufAlignV2},
            {sanitizer::CceInstructionId::CopyGmToCbufMultiNd2NzB8, ParseCopyOperand, ConvertCopyGmToCbufMultiNd2Nz},
            {sanitizer::CceInstructionId::CopyGmToCbufMultiNd2NzB16, ParseCopyOperand, ConvertCopyGmToCbufMultiNd2Nz},
            {sanitizer::CceInstructionId::CopyGmToCbufMultiNd2NzB32, ParseCopyOperand, ConvertCopyGmToCbufMultiNd2Nz},
            {sanitizer::CceInstructionId::CopyGmToCbufMultiDn2NzB8, ParseCopyOperand, ConvertCopyGmToCbufMultiDn2Nz},
            {sanitizer::CceInstructionId::CopyGmToCbufMultiDn2NzB16, ParseCopyOperand, ConvertCopyGmToCbufMultiDn2Nz},
            {sanitizer::CceInstructionId::CopyGmToCbufMultiDn2NzB32, ParseCopyOperand, ConvertCopyGmToCbufMultiDn2Nz},
            {sanitizer::CceInstructionId::CopyUbufToGmAlignV2, ParseCopyOperand, ConvertCopyUbufToGmAlignV2},
            {sanitizer::CceInstructionId::FixL0cToOutF32, ParseCopyOperand, ConvertFixL0cToOut},
            {sanitizer::CceInstructionId::FixL0cToOutS32, ParseCopyOperand, ConvertFixL0cToOut},
            {sanitizer::CceInstructionId::SetFlag, ParseFlagOperand, ConvertFlag},
            {sanitizer::CceInstructionId::SetFlagI, ParseFlagOperand, ConvertFlag},
            {sanitizer::CceInstructionId::WaitFlag, ParseFlagOperand, ConvertFlag},
            {sanitizer::CceInstructionId::WaitFlagI, ParseFlagOperand, ConvertFlag},
            {sanitizer::CceInstructionId::SetFlagV, ParseFlagOperand, ConvertFlag},
            {sanitizer::CceInstructionId::SetFlagIV, ParseFlagOperand, ConvertFlag},
            {sanitizer::CceInstructionId::WaitFlagV, ParseFlagOperand, ConvertFlag},
            {sanitizer::CceInstructionId::WaitFlagIV, ParseFlagOperand, ConvertFlag},
            {sanitizer::CceInstructionId::GetBuf, ParseBufferOperand, ConvertBuffer},
            {sanitizer::CceInstructionId::GetBufI, ParseBufferOperand, ConvertBuffer},
            {sanitizer::CceInstructionId::RlsBuf, ParseBufferOperand, ConvertBuffer},
            {sanitizer::CceInstructionId::RlsBufI, ParseBufferOperand, ConvertBuffer},
            {sanitizer::CceInstructionId::GetBufV, ParseBufferOperand, ConvertBuffer},
            {sanitizer::CceInstructionId::GetBufIV, ParseBufferOperand, ConvertBuffer},
            {sanitizer::CceInstructionId::RlsBufV, ParseBufferOperand, ConvertBuffer},
            {sanitizer::CceInstructionId::RlsBufIV, ParseBufferOperand, ConvertBuffer},
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
        return DecodedOperand{sanitizer::FlagOperand{
            static_cast<uint32_t>(record.instrId), static_cast<uint32_t>(record.args[0]),
            static_cast<uint32_t>(record.args[1]), record.args[2]}};
    }

    static std::optional<DecodedOperand> ParseBufferOperand(const sanitizer::AscsanRawTraceRecord& record) noexcept
    {
        return DecodedOperand{sanitizer::BufferParamField{
            static_cast<uint32_t>(record.instrId), static_cast<uint32_t>(record.args[0]), record.args[1],
            static_cast<uint8_t>(record.args[2])}};
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

    static std::optional<CceInstructionParamField> ConvertCopyGmToCbufMultiNd2Nz(const DecodedOperand& operand) noexcept
    {
        const auto* copyOperand = std::get_if<sanitizer::CopyOperand>(&operand);
        if (copyOperand == nullptr) {
            return std::nullopt;
        }
        return CceInstructionParamField{sanitizer::ConvertCopyGmToCbufMultiNd2NzOperand(*copyOperand)};
    }

    static std::optional<CceInstructionParamField> ConvertCopyGmToCbufMultiDn2Nz(const DecodedOperand& operand) noexcept
    {
        const auto* copyOperand = std::get_if<sanitizer::CopyOperand>(&operand);
        if (copyOperand == nullptr) {
            return std::nullopt;
        }
        return CceInstructionParamField{sanitizer::ConvertCopyGmToCbufMultiDn2NzOperand(*copyOperand)};
    }

    static std::optional<CceInstructionParamField> ConvertFixL0cToOut(const DecodedOperand& operand) noexcept
    {
        const auto* copyOperand = std::get_if<sanitizer::CopyOperand>(&operand);
        if (copyOperand == nullptr) {
            return std::nullopt;
        }
        return CceInstructionParamField{
            sanitizer::FixL0cToOutParamField{copyOperand->instr_id, copyOperand->dstAddr, copyOperand->srcAddr}};
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

    static std::optional<CceInstructionParamField> ConvertBuffer(const DecodedOperand& operand) noexcept
    {
        const auto* bufferOperand = std::get_if<sanitizer::BufferParamField>(&operand);
        if (bufferOperand == nullptr) {
            return std::nullopt;
        }
        return CceInstructionParamField{*bufferOperand};
    }

    static FixpipeMemoryField DecodeFixpipeField(
        const sanitizer::FixL0cToOutParamField& field, const sanitizer::AscsanRawTraceRecord& record) noexcept
    {
        const uint64_t config0 = record.args[2];
        const uint64_t config1 = record.args[3];
        const uint8_t quantPre =
            static_cast<uint8_t>(ExtractBitRange(config1, 34, 39) | (ExtractBitRange(config1, 29, 30) << 5U));
        return {
            field,
            static_cast<uint16_t>(ExtractBitRange(config0, 4, 16)),
            static_cast<uint16_t>(ExtractBitRange(config0, 16, 32)),
            static_cast<uint32_t>(ExtractBitRange(config0, 32, 64)),
            quantPre,
            ExtractBitRange(config1, 42, 43) != 0,
            ExtractBitRange(config1, 43, 44) != 0,
            ExtractBitRange(config1, 62, 63) != 0,
            FixpipeDestinationElementBytes(quantPre),
        };
    }

    static std::optional<CceTraceCallbackData> ConvertMemoryField(
        const sanitizer::AscsanRawTraceRecord& record, const TraceCallbackContext& context,
        MemoryInstructionField field) noexcept
    {
        const MemoryCbdataContext memoryContext{record.pc,      context.instrExecId, context.serialNo, record.siteId,
                                                context.coreId, record.blockId,      record.pipeline};
        MemoryCbdataResult result = MemoryFieldToCbdataConverter{memoryContext}.Convert(field);
        switch (result.status) {
            case MemoryCbdataStatus::SUCCESS:
            case MemoryCbdataStatus::NO_ACCESS:
                return CceTraceCallbackData{std::move(result.data)};
            case MemoryCbdataStatus::INVALID_FIELD:
            case MemoryCbdataStatus::ARITHMETIC_OVERFLOW:
            case MemoryCbdataStatus::RESOURCE_EXHAUSTED:
                return std::nullopt;
        }
        return std::nullopt;
    }

    static AclsanDeviceEventHeader MakeDeviceEventHeader(
        const sanitizer::AscsanRawTraceRecord& record, const TraceCallbackContext& context, uint32_t callbackDataSize,
        uint32_t sourceKind) noexcept
    {
        return {ACLSAN_API_VERSION,  callbackDataSize,
                kUnknownLaunchId,    record.pc,
                record.siteId,       sourceKind,
                context.instrExecId, context.serialNo,
                kDefaultDeviceId,    context.coreId,
                record.blockId,      kBlockTypeAiv,
                record.pipeline,     0};
    }

    static CceTraceCallbackData MakeDeviceSyncData(
        const sanitizer::AscsanRawTraceRecord& record, const TraceCallbackContext& context,
        const sanitizer::FlagParamField& params) noexcept
    {
        return AclsanDeviceSyncData{
            MakeDeviceEventHeader(
                record, context, static_cast<uint32_t>(sizeof(AclsanDeviceSyncData)),
                ACLSAN_DEVICE_SOURCE_SET_WAIT_FLAG),
            ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG,
            ToSyncAction(params.instr_id),
            ACLSAN_DEVICE_SYNC_SCOPE_BLOCK,
            params.srcPipe,
            params.dstPipe,
            0,
            params.eventId,
            {0, 0}};
    }

    static CceTraceCallbackData MakeDeviceSyncData(
        const sanitizer::AscsanRawTraceRecord& record, const TraceCallbackContext& context,
        const sanitizer::BufferParamField& params) noexcept
    {
        return AclsanDeviceSyncData{
            MakeDeviceEventHeader(
                record, context, static_cast<uint32_t>(sizeof(AclsanDeviceSyncData)), ACLSAN_DEVICE_SOURCE_GET_RLS_BUF),
            ACLSAN_DEVICE_SYNC_KIND_GET_RLS_BUF,
            ToSyncAction(params.instr_id),
            ACLSAN_DEVICE_SYNC_SCOPE_PIPE,
            0,
            params.pipe,
            params.mode,
            params.bufId,
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

uint64_t DecodeRawTraceTransferBytes(const sanitizer::AscsanRawTraceRecord& record) noexcept
{
    const std::optional<CceInstructionParamField> params = Translator::Translate(record);
    if (!params.has_value()) {
        return 0;
    }
    if (const auto* copy = std::get_if<sanitizer::CopyGmToUbufAlignV2ParamField>(&*params)) {
        return static_cast<uint64_t>(copy->burstNum) * copy->burstLen;
    }
    if (const auto* copy = std::get_if<sanitizer::CopyGmToCbufAlignV2ParamField>(&*params)) {
        return static_cast<uint64_t>(copy->burstNum) * copy->burstLen;
    }
    if (const auto* copy = std::get_if<sanitizer::CopyUbufToGmAlignV2ParamField>(&*params)) {
        return static_cast<uint64_t>(copy->burstNum) * copy->burstLen;
    }
    uint64_t transferBytes = 0;
    if (const auto* copy = std::get_if<sanitizer::CopyGmToCbufMultiNd2NzParamField>(&*params)) {
        uint64_t matrixElements = 0;
        return MultiplyWithoutOverflow(record.args[4], copy->nValue, matrixElements) &&
                       MultiplyWithoutOverflow(matrixElements, copy->dValue, matrixElements) &&
                       MultiplyWithoutOverflow(
                           matrixElements, MultiInstructionElementBytes(record.instrId), transferBytes) ?
                   transferBytes :
                   0;
    }
    if (const auto* copy = std::get_if<sanitizer::CopyGmToCbufMultiDn2NzParamField>(&*params)) {
        uint64_t matrixElements = 0;
        return MultiplyWithoutOverflow(record.args[4], copy->nValue, matrixElements) &&
                       MultiplyWithoutOverflow(matrixElements, copy->dValue, matrixElements) &&
                       MultiplyWithoutOverflow(
                           matrixElements, MultiInstructionElementBytes(record.instrId), transferBytes) ?
                   transferBytes :
                   0;
    }
    if (std::get_if<sanitizer::FixL0cToOutParamField>(&*params) != nullptr) {
        const uint64_t nSize = ExtractBitRange(record.args[2], 4, 16);
        const uint64_t mSize = ExtractBitRange(record.args[2], 16, 32);
        const uint64_t quantPre =
            ExtractBitRange(record.args[3], 34, 39) | (ExtractBitRange(record.args[3], 29, 30) << 5U);
        const uint64_t elementBytes = FixpipeDestinationElementBytes(quantPre);
        return MultiplyWithoutOverflow(nSize, mSize, transferBytes) &&
                       MultiplyWithoutOverflow(transferBytes, elementBytes, transferBytes) ?
                   transferBytes :
                   0;
    }
    return 0;
}

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
    const uint32_t sourceKind = source.syncKind == ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG ?
                                    ACLSAN_DEVICE_SOURCE_SET_WAIT_FLAG :
                                    ACLSAN_DEVICE_SOURCE_GET_RLS_BUF;
    const AclsanDeviceEventHeader header{
        ACLSAN_API_VERSION,
        static_cast<uint32_t>(sizeof(AclsanDeviceSyncData)),
        kUnknownLaunchId,
        record.pc,
        static_cast<uint32_t>(record.sequence),
        sourceKind,
        source.instrExecId,
        record.serialNo,
        kDefaultDeviceId,
        UINT32_MAX,
        record.blockId,
        kBlockTypeAiv,
        ACLSAN_DEVICE_PIPE_SCALAR,
        0};
    return {header,         source.syncKind, source.action,   scope, source.srcPipe,
            source.dstPipe, source.mode,     source.objectId, {0, 0}};
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
