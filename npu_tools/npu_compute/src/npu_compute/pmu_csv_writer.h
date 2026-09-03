/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_SRC_NPU_COMPUTE_PMU_CSV_WRITER_H_
#define NPU_COMPUTE_SRC_NPU_COMPUTE_PMU_CSV_WRITER_H_

#include "aclpti/aclpti_data.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace npu_compute {

struct PmuCsvConfig {
    std::string outputDirectory;
    std::string mirrorOutputDirectory;
    double frequencyMhz = 1000.0;
    double aicFrequencyMhz = 0.0;
    double aivFrequencyMhz = 0.0;
    std::uint32_t aicCoreCount = 0;
    std::uint32_t aivCoreCount = 0;
    std::uint32_t aicBlockCount = 0; // 真实启动 AIC block 数，0=不可用(退回核数)
    std::uint32_t aivBlockCount = 0; // 真实启动 AIV block 数，0=不可用(退回核数)
    std::string socName = "950X";
};

class PmuCsvWriter final {
public:
    static aclptiResult Write(
        const aclptiProfilingDataResult& result, const std::vector<std::string>& sections, const PmuCsvConfig& config);
};

} // namespace npu_compute

#endif // NPU_COMPUTE_SRC_NPU_COMPUTE_PMU_CSV_WRITER_H_
