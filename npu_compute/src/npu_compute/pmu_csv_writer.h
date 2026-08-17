/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#pragma once

#include "aclpti/aclpti_data.h"

#include <optional>
#include <string>
#include <vector>

namespace npu_compute {

struct PmuCsvConfig {
    std::string outputDirectory = "/tmp/npu_compute_csv";
    double frequencyMhz = 1000.0;
    std::string socName = "950X";
};

class PmuCsvWriter final {
public:
    static aclptiResult Write(
        const aclptiPmuDataResult& result, const std::vector<std::string>& sections, const PmuCsvConfig& config);
};

} // namespace npu_compute
