// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "diagnostic/report_renderer.h"

#include "diagnostic/report/report_fields.h"

#include <cctype>
#include <fstream>
#include <ostream>
#include <sstream>

namespace aclsan::cann {
namespace {

std::string Trim(const std::string& value)
{
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::string UnescapeTemplateText(const std::string& value)
{
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '\\' || i + 1 >= value.size()) {
            out.push_back(value[i]);
            continue;
        }
        switch (value[++i]) {
            case 'n':
                out.push_back('\n');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case 't':
                out.push_back('\t');
                break;
            case '\\':
                out.push_back('\\');
                break;
            default:
                out.push_back(value[i]);
                break;
        }
    }
    return out;
}

ReportRenderStatus ParseTemplateKey(const std::string& text, ReportTemplateKey* key)
{
    if (key == nullptr) {
        return ReportRenderStatus::kInvalidArgument;
    }
    const std::size_t dot = text.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= text.size()) {
        return ReportRenderStatus::kMalformedTemplate;
    }

    ReportTool tool = ReportTool::kMemcheck;
    const std::string toolName = text.substr(0, dot);
    bool found = false;
    for (const ReportTool candidate :
         {ReportTool::kMemcheck, ReportTool::kInitcheck, ReportTool::kRacecheck, ReportTool::kSynccheck,
          ReportTool::kSoccheck}) {
        if (toolName == ReportToolName(candidate)) {
            tool = candidate;
            found = true;
            break;
        }
    }
    if (!found) {
        return ReportRenderStatus::kUnknownTemplate;
    }
    *key = ReportTemplateKey{tool, text.substr(dot + 1)};
    return ReportRenderStatus::kSuccess;
}

ReportRenderStatus AppendTemplate(const ReportTemplate& tpl, const ReportFields& fields, std::string* out)
{
    if (out == nullptr) {
        return ReportRenderStatus::kInvalidArgument;
    }
    std::size_t pos = 0;
    while (pos < tpl.text.size()) {
        const std::size_t open = tpl.text.find("{{", pos);
        if (open == std::string::npos) {
            out->append(tpl.text, pos, std::string::npos);
            return ReportRenderStatus::kSuccess;
        }
        out->append(tpl.text, pos, open - pos);
        const std::size_t close = tpl.text.find("}}", open + 2);
        if (close == std::string::npos) {
            return ReportRenderStatus::kMalformedTemplate;
        }
        const auto field = fields.find(tpl.text.substr(open + 2, close - open - 2));
        if (field == fields.end()) {
            return ReportRenderStatus::kMissingField;
        }
        out->append(field->second);
        pos = close + 2;
    }
    return ReportRenderStatus::kSuccess;
}

std::string FieldOr(const ReportFields& fields, const std::string& key, const char* fallback)
{
    const auto field = fields.find(key);
    return field == fields.end() || field->second.empty() ? fallback : field->second;
}

void PutDerivedLocation(const std::string& prefix, const std::string& locationKey, bool includeAt, ReportFields* fields)
{
    if (fields->find(locationKey) != fields->end()) {
        return;
    }
    const std::string functionKey = prefix.empty() ? "function" : prefix + "Function";
    const std::string offsetKey = prefix.empty() ? "offset" : prefix + "Offset";
    const std::string fileKey = prefix.empty() ? "file" : prefix + "File";
    const std::string lineKey = prefix.empty() ? "line" : prefix + "Line";
    const std::string pcKey = prefix.empty() ? "pc" : prefix + "Pc";
    const std::string kernelNameKey = prefix.empty() ? "kernelName" : prefix + "KernelName";
    const auto function = fields->find(functionKey);
    std::ostringstream os;
    if (includeAt) {
        os << "at ";
    }
    if (function != fields->end() && !function->second.empty()) {
        os << function->second << "+0x" << FieldOr(*fields, offsetKey, "0") << " in "
           << FieldOr(*fields, fileKey, "<unknown>") << ":" << FieldOr(*fields, lineKey, "0");
    } else {
        os << "pc 0x" << FieldOr(*fields, pcKey, "0") << " in " << FieldOr(*fields, kernelNameKey, "<unknown>");
    }
    (*fields)[locationKey] = os.str();
}

void PutDerivedLocations(ReportFields* fields)
{
    PutDerivedLocation("", "location", true, fields);
    PutDerivedLocation("first", "firstLocation", false, fields);
    PutDerivedLocation("second", "secondLocation", false, fields);
}

bool HasRawStack(ReportStackFormat format)
{
    return format == ReportStackFormat::kRawText || format == ReportStackFormat::kBoth;
}

bool HasFrameStack(ReportStackFormat format)
{
    return format == ReportStackFormat::kFrames || format == ReportStackFormat::kBoth;
}

void EnsureTrailingNewline(std::string* out)
{
    if (!out->empty() && out->back() != '\n') {
        out->push_back('\n');
    }
}

void AppendRawStackText(const std::string& rawText, std::string* out)
{
    std::istringstream input(rawText);
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        if (line.rfind("=========", 0) != 0) {
            out->append("=========     ");
        }
        out->append(line);
        out->push_back('\n');
    }
}

void AppendFrame(const ReportFrame& frame, std::size_t frameIndex, std::string* out)
{
    out->append("=========     #");
    out->append(std::to_string(frameIndex));
    out->append(" ");
    out->append(frame.function.empty() ? "<unknown>" : frame.function);
    out->append(" [0x");
    out->append(detail::Hex(frame.pc));
    out->append("]");
    if (!frame.file.empty()) {
        out->append(" in ");
        out->append(frame.file);
        if (frame.line != 0) {
            out->append(":");
            out->append(std::to_string(frame.line));
        }
        if (frame.column != 0) {
            out->append(":");
            out->append(std::to_string(frame.column));
        }
    }
    out->push_back('\n');
}

void AppendCallStacks(const std::vector<ReportCallStack>& stacks, std::string* out)
{
    for (const ReportCallStack& stack : stacks) {
        const bool renderRaw = HasRawStack(stack.format) && !stack.rawText.empty();
        const bool renderFrames = HasFrameStack(stack.format) && !stack.frames.empty();
        if (!renderRaw && !renderFrames) {
            continue;
        }
        EnsureTrailingNewline(out);
        out->append("=========  ");
        out->append(ReportStackRoleTitle(stack.role));
        out->push_back('\n');
        if (renderRaw) {
            AppendRawStackText(stack.rawText, out);
        }
        if (renderFrames) {
            for (std::size_t index = 0; index < stack.frames.size(); ++index) {
                AppendFrame(stack.frames[index], index, out);
            }
        }
    }
}

} // namespace

ReportRenderStatus RenderReportText(const ReportTemplate& tpl, const ReportFields& fields, std::string* out)
{
    if (out == nullptr) {
        return ReportRenderStatus::kInvalidArgument;
    }
    out->clear();
    return AppendTemplate(tpl, fields, out);
}

ReportRenderStatus RenderReportRecord(
    const ReportRecord& record, const ReportTemplateOverrides& overrides, std::string* out)
{
    if (out == nullptr) {
        return ReportRenderStatus::kInvalidArgument;
    }
    const auto override = overrides.find(record.key);
    const ReportTemplate* tpl = override == overrides.end() ? FindBuiltinReportTemplate(record.key) : &override->second;
    if (tpl == nullptr) {
        out->clear();
        return ReportRenderStatus::kUnknownTemplate;
    }

    ReportFields fields = record.fields;
    PutDerivedLocations(&fields);
    fields["Severity"] = ReportSeverityName(record.severity);
    fields["tool"] = ReportToolName(record.key.tool);
    fields["pattern"] = record.key.pattern;
    const ReportRenderStatus status = RenderReportText(*tpl, fields, out);
    if (status != ReportRenderStatus::kSuccess) {
        return status;
    }
    AppendCallStacks(record.stacks, out);
    return ReportRenderStatus::kSuccess;
}

ReportRenderStatus WriteReportTextToStream(const std::string& text, std::ostream* out)
{
    if (out == nullptr) {
        return ReportRenderStatus::kInvalidArgument;
    }
    (*out) << text;
    return out->good() ? ReportRenderStatus::kSuccess : ReportRenderStatus::kWriteFailed;
}

ReportRenderStatus WriteReportTextToFile(const std::string& text, const std::string& path)
{
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        return ReportRenderStatus::kOpenFailed;
    }
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    return out.good() ? ReportRenderStatus::kSuccess : ReportRenderStatus::kWriteFailed;
}

ReportRenderStatus LoadReportTemplateOverridesFromFile(const std::string& path, ReportTemplateOverrides* overrides)
{
    if (overrides == nullptr) {
        return ReportRenderStatus::kInvalidArgument;
    }
    std::ifstream input(path);
    if (!input.is_open()) {
        return ReportRenderStatus::kOpenFailed;
    }
    std::string line;
    while (std::getline(input, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos || eq == 0) {
            return ReportRenderStatus::kMalformedTemplate;
        }
        ReportTemplateKey key{};
        const ReportRenderStatus status = ParseTemplateKey(Trim(line.substr(0, eq)), &key);
        if (status != ReportRenderStatus::kSuccess) {
            return status;
        }
        (*overrides)[key] = ReportTemplate{UnescapeTemplateText(line.substr(eq + 1))};
    }
    return ReportRenderStatus::kSuccess;
}

const char* ReportStackRoleTitle(ReportStackRole role)
{
    switch (role) {
        case ReportStackRole::kHostLaunch:
        case ReportStackRole::kHostAlloc:
        case ReportStackRole::kHostFree:
        case ReportStackRole::kHostApiCall:
            return "Host Frames:";
        case ReportStackRole::kNone:
        case ReportStackRole::kFaultDevice:
        case ReportStackRole::kRelatedAccessA:
        case ReportStackRole::kRelatedAccessB:
        case ReportStackRole::kSyncProducer:
        case ReportStackRole::kSyncConsumer:
        case ReportStackRole::kStateProducer:
        case ReportStackRole::kStateConsumer:
        case ReportStackRole::kPeerDevice:
        case ReportStackRole::kSyncTrigger:
        case ReportStackRole::kSyncRelated:
            return "Device Frames:";
    }
    return "Device Frames:";
}

} // namespace aclsan::cann
