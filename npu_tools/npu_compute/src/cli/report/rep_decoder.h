/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_TOOLS_NPU_COMPUTE_SRC_CLI_REPORT_REP_DECODER_H
#define NPU_TOOLS_NPU_COMPUTE_SRC_CLI_REPORT_REP_DECODER_H

#include "report/rep_format.h"

#include <cstdint>
#include <string>
#include <vector>

namespace npucompute::cli {

struct DecodedRepEntry {
    std::string file_name;
    NpuRepFileType file_type = NpuRepFileType::NpuRep;
    std::vector<uint8_t> payload;
};

struct DecodedRep {
    uint32_t version = 0;
    uint16_t origin = 0;
    std::vector<DecodedRepEntry> entries;
};

bool DecodeRep(const std::vector<uint8_t>& encoded, DecodedRep* decoded, std::string* error);

} // namespace npucompute::cli

#endif // NPU_TOOLS_NPU_COMPUTE_SRC_CLI_REPORT_REP_DECODER_H
