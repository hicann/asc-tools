/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "probe_parser.h"

#include "aclsan/aclsan_cbdata_device.h"
#include "cce_instr/cce_instr_types.h"

#include <cstring>
#include <limits>
#include <ostream>
#include <utility>

namespace sanitizer {
namespace {

template <typename Value>
bool ReadValue(const uint8_t* data, uint64_t limit, uint64_t offset, Value& value) noexcept
{
    if (offset > limit || sizeof(Value) > limit - offset) {
        return false;
    }
    std::memcpy(&value, data + offset, sizeof(Value));
    return true;
}

bool IsCopyAlignInstruction(ProbeInstrType type) noexcept
{
    switch (type) {
        case ProbeInstrType::CopyGmToUbufAlignB8:
        case ProbeInstrType::CopyGmToUbufAlignB16:
        case ProbeInstrType::CopyGmToUbufAlignB32:
        case ProbeInstrType::CopyUbufToGmAlignV2:
        case ProbeInstrType::CopyGmToUbufAlignV2B8:
        case ProbeInstrType::CopyGmToUbufAlignV2B16:
        case ProbeInstrType::CopyGmToUbufAlignV2B32:
            return true;
        default:
            return false;
    }
}

bool IsFlagInstruction(ProbeInstrType type) noexcept
{
    switch (type) {
        case ProbeInstrType::SetFlag:
        case ProbeInstrType::SetFlagI:
        case ProbeInstrType::WaitFlag:
        case ProbeInstrType::WaitFlagI:
        case ProbeInstrType::SetFlagV:
        case ProbeInstrType::SetFlagIV:
        case ProbeInstrType::WaitFlagV:
        case ProbeInstrType::WaitFlagIV:
            return true;
        default:
            return false;
    }
}

bool IsBufferInstruction(ProbeInstrType type) noexcept
{
    switch (type) {
        case ProbeInstrType::GetBuf:
        case ProbeInstrType::GetBufI:
        case ProbeInstrType::RlsBuf:
        case ProbeInstrType::RlsBufI:
        case ProbeInstrType::GetBufV:
        case ProbeInstrType::GetBufIV:
        case ProbeInstrType::RlsBufV:
        case ProbeInstrType::RlsBufIV:
            return true;
        default:
            return false;
    }
}

uint64_t ExtractBitRange(uint64_t value, uint32_t begin, uint32_t end) noexcept
{
    const uint32_t width = end - begin;
    const uint64_t mask = width == 64 ? std::numeric_limits<uint64_t>::max() : ((1ULL << width) - 1ULL);
    return (value >> begin) & mask;
}

bool MultiplyWithoutOverflow(uint64_t left, uint64_t right, uint64_t& product) noexcept
{
    if (left != 0 && right > std::numeric_limits<uint64_t>::max() / left) {
        return false;
    }
    product = left * right;
    return true;
}

void InitializeParsedRecord(
    const ProbeRecordCommon& common, const ProbeBlockHeader& header, uint64_t serialNo,
    ParsedProbeRecord& parsed) noexcept
{
    parsed = {};
    parsed.record.blockId = header.blockId;
    parsed.record.pc = static_cast<uint64_t>(common.pc);
    parsed.record.instrId = common.instrType;
    parsed.record.siteId = 0;
    parsed.serialNo = serialNo;
    parsed.coreId = header.phyCoreId;
}

bool ParseRecord(
    const uint8_t* blockData, uint64_t payloadOffset, uint64_t payloadSize, const ProbeBlockHeader& header,
    uint64_t serialNo, ParsedProbeRecord& parsed) noexcept
{
    ProbeRecordCommon common{};
    if (!ReadValue(blockData, payloadOffset + payloadSize, payloadOffset, common)) {
        return false;
    }
    const auto type = static_cast<ProbeInstrType>(common.instrType);
    if (ProbeInstructionName(type) == nullptr) {
        return false;
    }
    const auto instructionId = static_cast<CceInstructionId>(common.instrType);
    const AclsanDevicePipeline pipeline = GetCceInstructionPipeline(instructionId);
    if (pipeline == ACLSAN_DEVICE_PIPE_INVALID) {
        return false;
    }
    InitializeParsedRecord(common, header, serialNo, parsed);
    parsed.record.pipeline = static_cast<uint32_t>(pipeline);

    if (type == ProbeInstrType::CopyGmToUbuf) {
        CopyRecord record{};
        if (payloadSize != sizeof(record) ||
            !ReadValue(blockData, payloadOffset + payloadSize, payloadOffset, record)) {
            return false;
        }
        parsed.record.args[0] = record.dst;
        parsed.record.args[1] = record.src;
        parsed.record.args[2] = record.len;
        parsed.transferBytes = record.len;
        return true;
    }

    if (IsCopyAlignInstruction(type)) {
        CopyAlignRecord record{};
        if (payloadSize != sizeof(record) ||
            !ReadValue(blockData, payloadOffset + payloadSize, payloadOffset, record)) {
            return false;
        }
        parsed.record.args[0] = record.dst;
        parsed.record.args[1] = record.src;
        parsed.record.args[2] = record.config0;
        parsed.record.args[3] = record.config1;
        const uint64_t burstNum = ExtractBitRange(record.config0, 4, 24);
        const uint64_t burstLenBytes = ExtractBitRange(record.config0, 25, 45);
        return MultiplyWithoutOverflow(burstNum, burstLenBytes, parsed.transferBytes);
    }

    if (IsFlagInstruction(type)) {
        FlagRecord record{};
        if (payloadSize != sizeof(record) ||
            !ReadValue(blockData, payloadOffset + payloadSize, payloadOffset, record)) {
            return false;
        }
        parsed.record.args[0] = record.srcPipe;
        parsed.record.args[1] = record.dstPipe;
        parsed.record.args[2] = record.eventId;
        return true;
    }

    if (IsBufferInstruction(type)) {
        BufferRecord record{};
        if (payloadSize != sizeof(record) ||
            !ReadValue(blockData, payloadOffset + payloadSize, payloadOffset, record)) {
            return false;
        }
        parsed.record.args[0] = record.pipe;
        parsed.record.args[1] = record.bufId;
        parsed.record.args[2] = record.mode;
        return true;
    }
    return false;
}

} // namespace

const char* ProbeInstructionName(ProbeInstrType type) noexcept
{
    switch (type) {
        case ProbeInstrType::CopyGmToUbuf:
            return "COPY_GM_TO_UBUF";
        case ProbeInstrType::CopyGmToUbufAlignB8:
            return "COPY_GM_TO_UBUF_ALIGN_B8";
        case ProbeInstrType::CopyGmToUbufAlignB16:
            return "COPY_GM_TO_UBUF_ALIGN_B16";
        case ProbeInstrType::CopyGmToUbufAlignB32:
            return "COPY_GM_TO_UBUF_ALIGN_B32";
        case ProbeInstrType::CopyUbufToGmAlignV2:
            return "COPY_UBUF_TO_GM_ALIGN_V2";
        case ProbeInstrType::CopyGmToUbufAlignV2B8:
            return "COPY_GM_TO_UBUF_ALIGN_V2_B8";
        case ProbeInstrType::CopyGmToUbufAlignV2B16:
            return "COPY_GM_TO_UBUF_ALIGN_V2_B16";
        case ProbeInstrType::CopyGmToUbufAlignV2B32:
            return "COPY_GM_TO_UBUF_ALIGN_V2_B32";
        case ProbeInstrType::SetFlag:
            return "SET_FLAG";
        case ProbeInstrType::SetFlagI:
            return "SET_FLAGI";
        case ProbeInstrType::WaitFlag:
            return "WAIT_FLAG";
        case ProbeInstrType::WaitFlagI:
            return "WAIT_FLAGI";
        case ProbeInstrType::GetBuf:
            return "GET_BUF";
        case ProbeInstrType::GetBufI:
            return "GET_BUFI";
        case ProbeInstrType::RlsBuf:
            return "RLS_BUF";
        case ProbeInstrType::RlsBufI:
            return "RLS_BUFI";
        case ProbeInstrType::SetFlagV:
            return "SET_FLAG_V";
        case ProbeInstrType::SetFlagIV:
            return "SET_FLAGI_V";
        case ProbeInstrType::WaitFlagV:
            return "WAIT_FLAG_V";
        case ProbeInstrType::WaitFlagIV:
            return "WAIT_FLAGI_V";
        case ProbeInstrType::GetBufV:
            return "GET_BUF_V";
        case ProbeInstrType::GetBufIV:
            return "GET_BUFI_V";
        case ProbeInstrType::RlsBufV:
            return "RLS_BUF_V";
        case ProbeInstrType::RlsBufIV:
            return "RLS_BUFI_V";
    }
    return nullptr;
}

bool ParseProbeOutput(
    const uint8_t* data, size_t dataBytes, uint32_t blockCount, uint32_t aivOffset, std::ostream& output,
    ProbeParseResult& result) noexcept
{
    result = {};
    if ((blockCount != 0 && data == nullptr) || blockCount > dataBytes / kProbeBlockBytes || aivOffset > blockCount) {
        output << "[probe] status=invalid reason=output_bounds\n";
        return false;
    }

    ProbeParseResult parsed;
    uint64_t serialNo = 0;
    for (uint32_t block = 0; block < blockCount; ++block) {
        const uint8_t* blockData = data + static_cast<size_t>(block) * kProbeBlockBytes;
        ProbeBlockHeader header{};
        std::memcpy(&header, blockData, sizeof(header));
        const uint32_t expectedBlockId = block < aivOffset ? block : block - aivOffset;
        if (header.blockId != expectedBlockId || header.curAddr < kProbeRecordStartBytes ||
            header.curAddr > kProbeBlockBytes || (header.instrNum == 0 && header.curAddr != kProbeRecordStartBytes)) {
            output << "[probe] status=invalid reason=header block=" << block << '\n';
            return false;
        }
        if (header.instrNum == 0) {
            parsed.blockRecordCounts.push_back(0);
            continue;
        }

        uint64_t offset = kProbeRecordStartBytes;
        uint64_t blockRecordCount = 0;
        for (uint64_t recordIndex = 0; recordIndex < header.instrNum; ++recordIndex) {
            uint64_t payloadSize = 0;
            if (!ReadValue(blockData, header.curAddr, offset, payloadSize) ||
                payloadSize > header.curAddr - offset - sizeof(payloadSize)) {
                output << "[probe] status=invalid reason=payload_bounds block=" << block << '\n';
                return false;
            }
            const uint64_t payloadOffset = offset + sizeof(payloadSize);
            ParsedProbeRecord record{};
            if (!ParseRecord(blockData, payloadOffset, payloadSize, header, serialNo, record)) {
                output << "[probe] status=invalid reason=payload_format block=" << block << '\n';
                return false;
            }
            parsed.records.push_back(record);
            offset = payloadOffset + payloadSize;
            ++blockRecordCount;
            ++serialNo;
        }
        if (offset != header.curAddr) {
            output << "[probe] status=invalid reason=record_count block=" << block << '\n';
            return false;
        }
        parsed.blockRecordCounts.push_back(blockRecordCount);
    }
    result = std::move(parsed);
    return true;
}

} // namespace sanitizer
