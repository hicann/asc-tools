/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_SRC_NPU_COMPUTE_SECTION_CONFIG_H_
#define NPU_COMPUTE_SRC_NPU_COMPUTE_SECTION_CONFIG_H_

#include "aclpti/aclpti.h"

#include <string>
#include <vector>

namespace npu_compute {

class SectionConfig {
public:
    SectionConfig() = default;

    SectionConfig(const SectionConfig&) = delete;
    SectionConfig& operator=(const SectionConfig&) = delete;

    bool LoadFromEnvironment(const char* name, std::string* error);
    aclptiRangeProfilerSetConfigParams* Params();
    std::string JoinedSections() const;
    const std::vector<std::string>& Sections() const;

private:
    void Reset();

    std::vector<std::string> sections_;
    std::vector<const char*> section_pointers_;
    aclptiRangeProfilerSetConfigParams params_{};
};

} // namespace npu_compute

#endif // NPU_COMPUTE_SRC_NPU_COMPUTE_SECTION_CONFIG_H_
