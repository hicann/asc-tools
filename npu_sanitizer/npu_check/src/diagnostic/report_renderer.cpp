// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "report_renderer.h"

#include "diagnostic/report/report_normalizer.h"
#include "diagnostic/report/report_summary.h"

#include <utility>

namespace aclsan::cann {

ReportRenderStatus RenderReportBundle(
    const std::vector<ReportRecord>& records, const ReportTemplateOverrides& overrides, std::string* out)
{
    if (out == nullptr) {
        return ReportRenderStatus::kInvalidArgument;
    }

    out->clear();
    out->append("========= NPUSAN\n");
    for (const ReportRecord& record : records) {
        std::string rendered;
        const ReportRenderStatus status = RenderReportRecord(record, overrides, &rendered);
        if (status != ReportRenderStatus::kSuccess) {
            out->clear();
            return status;
        }
        while (!rendered.empty() && rendered.back() == '\n') {
            rendered.pop_back();
        }
        out->append(rendered);
        out->append("\n\n");
    }

    detail::AppendReportSummaries(records, out);
    return ReportRenderStatus::kSuccess;
}

ReportRenderStatus RenderNpusanReportRecord(
    const NpusanReportRecord& record, const ReportTemplateOverrides& overrides, std::string* out)
{
    ReportRecord templateRecord{};
    const ReportRenderStatus status = detail::NormalizeReport(record, &templateRecord);
    if (status != ReportRenderStatus::kSuccess) {
        if (out != nullptr) {
            out->clear();
        }
        return status;
    }
    return RenderReportRecord(templateRecord, overrides, out);
}

ReportRenderStatus RenderNpusanReportBundle(
    const std::vector<NpusanReportRecord>& records, const ReportTemplateOverrides& overrides, std::string* out)
{
    if (out == nullptr) {
        return ReportRenderStatus::kInvalidArgument;
    }

    std::vector<ReportRecord> templateRecords;
    templateRecords.reserve(records.size());
    for (const NpusanReportRecord& record : records) {
        ReportRecord templateRecord{};
        const ReportRenderStatus status = detail::NormalizeReport(record, &templateRecord);
        if (status != ReportRenderStatus::kSuccess) {
            out->clear();
            return status;
        }
        templateRecords.push_back(std::move(templateRecord));
    }
    return RenderReportBundle(templateRecords, overrides, out);
}

} // namespace aclsan::cann
