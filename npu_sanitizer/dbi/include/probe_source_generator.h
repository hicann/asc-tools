// Copyright (c) 2026 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#pragma once

#include "dbi_pipeline.h"

#include <string>
#include <string_view>
#include <vector>

namespace aclsan {

struct GeneratedProbeSource {
    bool success = false;
    std::string source;
    std::string sourceMap;
    std::string identity;
    std::vector<std::string> symbols;
    std::string diagnostic;
};

GeneratedProbeSource GenerateProbeSource(std::string_view arch, ProbeGroup group);
std::string ProbeGeneratorIdentity();

} // namespace aclsan
