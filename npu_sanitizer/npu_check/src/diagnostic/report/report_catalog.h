// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#ifndef ACLSAN_REPORT_CATALOG_H
#define ACLSAN_REPORT_CATALOG_H

#include "diagnostic/report_renderer.h"

#include <cstdint>
#include <map>

namespace aclsan::cann::detail {

struct PatternDescriptor {
    // Underlying value of the tool-specific pattern enum selected by ReportTemplateKey::tool.
    std::uint32_t value;
    ReportTemplate reportTemplate;
};

using PatternCatalog = std::map<ReportTemplateKey, PatternDescriptor>;

const PatternCatalog& GetPatternCatalog();
const PatternDescriptor* FindPatternDescriptor(ReportTool tool, std::uint32_t pattern);
const PatternDescriptor* FindPatternDescriptor(const ReportTemplateKey& key);
const char* PatternName(ReportTool tool, std::uint32_t pattern);

const char* ToolName(ReportTool tool);
bool ParseToolName(const std::string& text, ReportTool* tool);

} // namespace aclsan::cann::detail

#endif
