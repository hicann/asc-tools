/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cassert>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <vector>

#include "aclsan/aclsan_cbdata_device.h"
#include "probe_parser.h"

namespace {

template <typename Record>
void AppendRecord(std::vector<uint8_t>& block, uint64_t& offset, const Record& record)
{
    const uint64_t payloadBytes = sizeof(record);
    std::memcpy(block.data() + offset, &payloadBytes, sizeof(payloadBytes));
    offset += sizeof(payloadBytes);
    std::memcpy(block.data() + offset, &record, sizeof(record));
    offset += sizeof(record);
}

void TestParsesProbeRecords()
{
    std::vector<uint8_t> block(sanitizer::kProbeBlockBytes, 0);
    uint64_t offset = sanitizer::kProbeRecordStartBytes;
    AppendRecord(
        block, offset,
        sanitizer::CopyAlignRecord{
            {static_cast<uint32_t>(sanitizer::ProbeInstrType::CopyGmToUbufAlignV2B16), 0x120},
            0x2000,
            0x1000,
            (2ULL << 4) | (64ULL << 25),
            0x5678});
    AppendRecord(
        block, offset,
        sanitizer::CopyAlignRecord{
            {static_cast<uint32_t>(sanitizer::ProbeInstrType::CopyUbufToGmAlignV2), 0x130},
            0x3000,
            0x4000,
            (3ULL << 4) | (32ULL << 25),
            0x6789});
    AppendRecord(
        block, offset,
        sanitizer::FlagRecord{{static_cast<uint32_t>(sanitizer::ProbeInstrType::SetFlag), 0x140}, 2, 3, 9});
    AppendRecord(
        block, offset,
        sanitizer::BufferRecord{{static_cast<uint32_t>(sanitizer::ProbeInstrType::GetBuf), 0x150}, 7, 4, 2, {0, 0, 0}});

    const sanitizer::ProbeBlockHeader header{4, offset, 0, 7};
    std::memcpy(block.data(), &header, sizeof(header));

    sanitizer::ProbeParseResult result;
    std::ostringstream output;
    assert(sanitizer::ParseProbeOutput(block.data(), block.size(), 1, 0, output, result));
    assert(result.records.size() == 4);
    assert(result.blockRecordCounts.size() == 1);
    assert(result.blockRecordCounts[0] == 4);

    const sanitizer::ParsedProbeRecord& copy = result.records[0];
    assert(copy.record.blockId == 0);
    assert(copy.record.pc == 0x120);
    assert(copy.record.instrId == 85);
    assert(copy.record.args[0] == 0x2000);
    assert(copy.record.args[1] == 0x1000);
    assert(copy.record.args[2] == ((2ULL << 4) | (64ULL << 25)));
    assert(copy.record.args[3] == 0x5678);
    assert(copy.record.siteId == 0);
    assert(copy.record.pipeline == ACLSAN_DEVICE_PIPE_MTE2);
    assert(copy.transferBytes == 128);
    assert(copy.coreId == 7);
    assert(copy.serialNo == 0);

    const sanitizer::ParsedProbeRecord& ubToGm = result.records[1];
    assert(ubToGm.record.pc == 0x130);
    assert(ubToGm.record.instrId == 83);
    assert(ubToGm.record.args[0] == 0x3000);
    assert(ubToGm.record.args[1] == 0x4000);
    assert(ubToGm.record.args[2] == ((3ULL << 4) | (32ULL << 25)));
    assert(ubToGm.record.args[3] == 0x6789);
    assert(ubToGm.record.pipeline == ACLSAN_DEVICE_PIPE_MTE3);
    assert(ubToGm.transferBytes == 96);
    assert(ubToGm.serialNo == 1);

    const sanitizer::ParsedProbeRecord& flag = result.records[2];
    assert(flag.record.instrId == 440);
    assert(flag.record.args[0] == 2);
    assert(flag.record.args[1] == 3);
    assert(flag.record.args[2] == 9);
    assert(flag.record.pipeline == ACLSAN_DEVICE_PIPE_SCALAR);
    assert(flag.transferBytes == 0);
    assert(flag.serialNo == 2);

    const sanitizer::ParsedProbeRecord& buffer = result.records[3];
    assert(buffer.record.instrId == 448);
    assert(buffer.record.args[0] == 4);
    assert(buffer.record.args[1] == 7);
    assert(buffer.record.args[2] == 2);
    assert(buffer.record.pipeline == ACLSAN_DEVICE_PIPE_SCALAR);
    assert(buffer.transferBytes == 0);
    assert(buffer.serialNo == 3);
}

void TestRejectsInvalidBlocks()
{
    std::vector<uint8_t> block(sanitizer::kProbeBlockBytes, 0);
    sanitizer::ProbeBlockHeader header{0, sanitizer::kProbeRecordStartBytes, 0, 0};
    std::memcpy(block.data(), &header, sizeof(header));

    sanitizer::ProbeParseResult result;
    std::ostringstream output;
    assert(sanitizer::ParseProbeOutput(block.data(), block.size(), 1, 0, output, result));
    assert(result.records.empty());
    assert(result.blockRecordCounts.size() == 1);
    assert(result.blockRecordCounts[0] == 0);

    header.curAddr = sanitizer::kProbeRecordStartBytes + 1;
    std::memcpy(block.data(), &header, sizeof(header));
    assert(!sanitizer::ParseProbeOutput(block.data(), block.size(), 1, 0, output, result));

    header.instrNum = 1;
    header.curAddr = sanitizer::kProbeRecordStartBytes + sizeof(uint64_t);
    std::memcpy(block.data(), &header, sizeof(header));
    const uint64_t oversizedPayload = sizeof(sanitizer::CopyAlignRecord);
    std::memcpy(block.data() + sanitizer::kProbeRecordStartBytes, &oversizedPayload, sizeof(oversizedPayload));
    assert(!sanitizer::ParseProbeOutput(block.data(), block.size(), 1, 0, output, result));
}

} // namespace

int main()
{
    TestParsesProbeRecords();
    TestRejectsInvalidBlocks();
    return 0;
}
