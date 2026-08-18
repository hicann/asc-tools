/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_TESTS_REP_TEST_DECODER_H_
#define NPU_COMPUTE_TESTS_REP_TEST_DECODER_H_

#include "rep_format.h"

#include <cstdint>
#include <string>
#include <vector>

namespace npu_compute::compute_launcher::test {

struct DecodedRepEntry {
    std::string file_name;
    NpuRepFileType file_type = NpuRepFileType::NpuRep;
    std::uint64_t file_length = 0;
    std::uint64_t file_offset = 0;
    std::vector<std::uint8_t> payload;
};

struct DecodedRep {
    std::uint32_t version = 0;
    std::uint16_t origin = 0;
    std::uint16_t head_length = 0;
    std::uint32_t file_info_count = 0;
    std::uint32_t file_info_length = 0;
    std::uint64_t rep_length = 0;
    std::vector<DecodedRepEntry> entries;
};

bool DecodeRep(const std::vector<std::uint8_t>& encoded, DecodedRep* decoded, std::string* error);

} // namespace npu_compute::compute_launcher::test

#endif // NPU_COMPUTE_TESTS_REP_TEST_DECODER_H_
