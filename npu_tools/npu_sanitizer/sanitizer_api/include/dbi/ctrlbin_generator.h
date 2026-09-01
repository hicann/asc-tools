// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#pragma once

#include "dbi_pipeline.h"

#include <string>
#include <vector>

namespace aclsan {

std::vector<ProbeGroup> AllProbeGroups();
std::vector<std::string> BindingSymbols(const std::vector<ProbeGroup>& groups);
std::string CtrlBinGeneratorIdentity();
bool GenerateCtrlBin(const std::string& outputPath, const std::vector<ProbeGroup>& groups, std::string& diagnostic);

} // namespace aclsan
