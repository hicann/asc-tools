// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "diagnostic/report/report_summary.h"

#include <array>
#include <cctype>
#include <map>

namespace aclsan::cann::detail {
namespace {

struct ToolSummary {
    std::uint64_t errors = 0;
    std::uint64_t warnings = 0;
    std::uint64_t infos = 0;
    std::uint64_t leaks = 0;
    std::uint64_t unused = 0;
    std::uint64_t hazards = 0;
    std::uint64_t deadlocks = 0;
};

using Summaries = std::map<ReportTool, ToolSummary>;
using SummaryCounter = std::uint64_t ToolSummary::*;

const std::map<ReportTemplateKey, SummaryCounter>& SummaryCounters()
{
    static const std::map<ReportTemplateKey, SummaryCounter> counters = {
        {{ReportTool::kMemcheck, "leak"}, &ToolSummary::leaks},
        {{ReportTool::kInitcheck, "unused_memory"}, &ToolSummary::unused},
        {{ReportTool::kRacecheck, "hazard_raw"}, &ToolSummary::hazards},
        {{ReportTool::kRacecheck, "hazard_war"}, &ToolSummary::hazards},
        {{ReportTool::kRacecheck, "hazard_waw"}, &ToolSummary::hazards},
        {{ReportTool::kRacecheck, "atomic_race"}, &ToolSummary::hazards},
        {{ReportTool::kRacecheck, "cross_pipe_race"}, &ToolSummary::hazards},
        {{ReportTool::kRacecheck, "inter_core_race"}, &ToolSummary::hazards},
        {{ReportTool::kRacecheck, "invalid_remote_access"}, &ToolSummary::hazards},
        {{ReportTool::kSynccheck, "deadlock"}, &ToolSummary::deadlocks},
    };
    return counters;
}

constexpr std::array<ReportTool, 5> kReportTools = {
    ReportTool::kMemcheck,  ReportTool::kInitcheck, ReportTool::kRacecheck,
    ReportTool::kSynccheck, ReportTool::kSoccheck,
};

void IncrementPatternMetric(const ReportTemplateKey& key, ToolSummary* summary)
{
    const auto found = SummaryCounters().find(key);
    if (found != SummaryCounters().end()) {
        ++(summary->*(found->second));
    }
}

void AccumulateSummary(const ReportRecord& record, Summaries* summaries, std::uint64_t* fatalCount)
{
    ToolSummary& summary = (*summaries)[record.key.tool];
    if (record.severity == ReportSeverity::kError || record.severity == ReportSeverity::kFatal) {
        ++summary.errors;
    } else if (record.severity == ReportSeverity::kWarning) {
        ++summary.warnings;
    } else if (record.severity == ReportSeverity::kInfo) {
        ++summary.infos;
    }
    if (record.severity == ReportSeverity::kFatal) {
        ++*fatalCount;
    }
    IncrementPatternMetric(record.key, &summary);
}

void AppendSeverityCounts(const ToolSummary& summary, std::string* out)
{
    out->append(std::to_string(summary.errors));
    out->append(" errors, ");
    out->append(std::to_string(summary.warnings));
    out->append(" warnings, ");
    out->append(std::to_string(summary.infos));
    out->append(" infos");
}

void AppendToolSummaries(const Summaries& summaries, std::string* out)
{
    const ToolSummary& memcheck = summaries.at(ReportTool::kMemcheck);
    out->append("========= MEMCHECK SUMMARY: ");
    AppendSeverityCounts(memcheck, out);
    out->append(", ");
    out->append(std::to_string(memcheck.leaks));
    out->append(" leaks\n");

    const ToolSummary& initcheck = summaries.at(ReportTool::kInitcheck);
    out->append("========= INITCHECK SUMMARY: ");
    AppendSeverityCounts(initcheck, out);
    out->append(", ");
    out->append(std::to_string(initcheck.unused));
    out->append(" unused memory reports\n");

    const ToolSummary& racecheck = summaries.at(ReportTool::kRacecheck);
    out->append("========= RACECHECK SUMMARY: ");
    out->append(std::to_string(racecheck.hazards));
    out->append(" hazard displayed (");
    AppendSeverityCounts(racecheck, out);
    out->append(")\n");

    const ToolSummary& synccheck = summaries.at(ReportTool::kSynccheck);
    out->append("========= SYNCCHECK SUMMARY: ");
    AppendSeverityCounts(synccheck, out);
    out->append(", ");
    out->append(std::to_string(synccheck.deadlocks));
    out->append(" deadlocks\n");

    const ToolSummary& soccheck = summaries.at(ReportTool::kSoccheck);
    out->append("========= SOCCHECK SUMMARY: ");
    AppendSeverityCounts(soccheck, out);
    out->push_back('\n');
}

void AppendGlobalSummary(const Summaries& summaries, std::uint64_t fatalCount, std::string* out)
{
    std::uint64_t totalErrorCount = 0;
    for (const ReportTool tool : kReportTools) {
        totalErrorCount += summaries.at(tool).errors;
    }
    out->append("========= ERROR SUMMARY: ");
    out->append(std::to_string(totalErrorCount));
    out->append(" errors\n");
    for (const ReportTool tool : kReportTools) {
        std::string toolName = ReportToolName(tool);
        for (char& ch : toolName) {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        }
        out->append("=========     ");
        out->append(toolName);
        out->append(": ");
        out->append(std::to_string(summaries.at(tool).errors));
        out->append(" errors\n");
    }
    out->append("=========     FATAL: ");
    out->append(std::to_string(fatalCount));
    out->append(" fatal errors\n");
}

} // namespace

void AppendReportSummaries(const std::vector<ReportRecord>& records, std::string* out)
{
    Summaries summaries;
    for (const ReportTool tool : kReportTools) {
        summaries.emplace(tool, ToolSummary{});
    }
    std::uint64_t fatalCount = 0;
    for (const ReportRecord& record : records) {
        AccumulateSummary(record, &summaries, &fatalCount);
    }
    AppendToolSummaries(summaries, out);
    AppendGlobalSummary(summaries, fatalCount, out);
}

} // namespace aclsan::cann::detail
