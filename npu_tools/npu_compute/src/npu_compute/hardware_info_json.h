/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_SRC_NPU_COMPUTE_HARDWARE_INFO_JSON_H_
#define NPU_COMPUTE_SRC_NPU_COMPUTE_HARDWARE_INFO_JSON_H_

#include "hardware_info_types.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace npu_compute {

struct HardwareInfoFrequencies {
    std::uint32_t aiCubeCount = 0;
    std::uint32_t aiVectorCount = 0;
    std::uint32_t aiCubeFrequencyMhz = 0;
    std::uint32_t aiVectorFrequencyMhz = 0;
};

bool SerializeHardwareInfoJsonl(const HardwareInfoSnapshot& snapshot, std::string* jsonl, std::string* error);
bool ParseHardwareInfoFrequenciesJsonl(
    std::string_view jsonl, HardwareInfoFrequencies* frequencies, std::string* error);
bool ParseHardwareInfoSocNameJsonl(std::string_view jsonl, std::string* socName, std::string* error);

} // namespace npu_compute

#endif // NPU_COMPUTE_SRC_NPU_COMPUTE_HARDWARE_INFO_JSON_H_
