/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_PROBE_PARSER_H_
#define ACLSAN_PROBE_PARSER_H_

#include "cce_instr/raw_data_struct.h"
#include "probe_record.h"

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <vector>

namespace sanitizer {

struct ParsedProbeRecord {
    AscsanRawTraceRecord record{};
    uint64_t transferBytes = 0;
    uint64_t serialNo = 0;
    uint32_t coreId = 0;
};

struct ProbeParseResult {
    std::vector<uint64_t> blockRecordCounts;
    std::vector<ParsedProbeRecord> records;
};

const char* ProbeInstructionName(ProbeInstrType type) noexcept;

bool ParseProbeOutput(
    const uint8_t* data, size_t dataBytes, uint32_t blockCount, uint32_t aivOffset, std::ostream& output,
    ProbeParseResult& result) noexcept;

} // namespace sanitizer

#endif // ACLSAN_PROBE_PARSER_H_
