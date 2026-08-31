// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#ifndef ACLSAN_REPORT_RENDERER_H
#define ACLSAN_REPORT_RENDERER_H

#include "diagnostic/report_message.h"

#include <iosfwd>
#include <map>
#include <string>
#include <vector>

namespace aclsan::cann {

enum class ReportRenderStatus {
    kSuccess = 0,
    kInvalidArgument = 1,
    kMalformedTemplate = 2,
    kMissingField = 3,
    kOpenFailed = 4,
    kWriteFailed = 5,
    kUnknownTemplate = 6,
};
struct ReportTemplate {
    std::string text;
};

using ReportFields = std::map<std::string, std::string>;

struct ReportTemplateKey {
    ReportTool tool;
    std::string pattern;

    bool operator<(const ReportTemplateKey& other) const;
    bool operator==(const ReportTemplateKey& other) const;
};

struct ReportRecord {
    ReportTemplateKey key;
    ReportSeverity severity;
    ReportFields fields;
    std::vector<ReportCallStack> stacks;
};

using ReportTemplateOverrides = std::map<ReportTemplateKey, ReportTemplate>;

ReportRenderStatus RenderReportText(const ReportTemplate& tpl, const ReportFields& fields, std::string* out);
ReportRenderStatus RenderReportRecord(
    const ReportRecord& record, const ReportTemplateOverrides& overrides, std::string* out);
ReportRenderStatus RenderReportBundle(
    const std::vector<ReportRecord>& records, const ReportTemplateOverrides& overrides, std::string* out);
ReportRenderStatus RenderNpusanReportRecord(
    const NpusanReportRecord& record, const ReportTemplateOverrides& overrides, std::string* out);
ReportRenderStatus RenderNpusanReportBundle(
    const std::vector<NpusanReportRecord>& records, const ReportTemplateOverrides& overrides, std::string* out);
ReportRenderStatus WriteReportTextToStream(const std::string& text, std::ostream* out);
ReportRenderStatus WriteReportTextToFile(const std::string& text, const std::string& path);
ReportRenderStatus LoadReportTemplateOverridesFromFile(const std::string& path, ReportTemplateOverrides* overrides);

const ReportTemplate* FindBuiltinReportTemplate(const ReportTemplateKey& key);
std::vector<ReportTemplateKey> ListBuiltinReportTemplates();
const char* ReportToolName(ReportTool tool);
const char* ReportSeverityName(ReportSeverity severity);
const char* ReportStackRoleTitle(ReportStackRole role);

} // namespace aclsan::cann

#endif
