// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "diagnostic/report/report_catalog.h"

namespace aclsan::cann::detail {
namespace {

template <typename Pattern>
constexpr std::uint32_t PatternValue(Pattern pattern)
{
    return static_cast<std::uint32_t>(pattern);
}

template <typename Pattern>
PatternCatalog::value_type MakePattern(ReportTool tool, Pattern value, const char* name, const char* reportTemplate)
{
    return {{tool, name}, {PatternValue(value), ReportTemplate{reportTemplate}}};
}

const PatternCatalog& Catalog()
{
    static const PatternCatalog catalog = {
        MakePattern(
            ReportTool::MEMCHECK, NpusanMemcheckPattern::INVALID_ACCESS, "invalid_access",
            "========= {{Severity}}: Invalid {{space}} {{access}} of size {{accessBytes}} bytes\n"
            "=========     {{location}}\n"
            "=========     by aicore ({{coreId}}) type ({{blockType}}) block ({{blockId}}) pipe ({{pipeName}})\n"
            "=========     Address 0x{{address}} is out of bounds\n"
            "=========     and is {{distanceBytes}} bytes {{before|after}} the nearest allocation at 0x{{base}} of "
            "size "
            "{{bytes}} bytes\n"),
        MakePattern(
            ReportTool::MEMCHECK, NpusanMemcheckPattern::MISALIGNED_ACCESS, "misaligned_access",
            "========= {{Severity}}: Invalid {{space}} {{access}} of size {{accessBytes}} bytes\n"
            "=========     {{location}}\n"
            "=========     by aicore ({{coreId}}) type ({{blockType}}) block ({{blockId}}) pipe ({{pipeName}})\n"
            "=========     Address 0x{{address}} is misaligned\n"
            "=========     required alignment is {{requiredAlign}} bytes\n"),
        MakePattern(
            ReportTool::MEMCHECK, NpusanMemcheckPattern::USE_AFTER_FREE, "use_after_free",
            "========= {{Severity}}: Invalid {{space}} {{access}} of size {{accessBytes}} bytes\n"
            "=========     {{location}}\n"
            "=========     by aicore ({{coreId}}) type ({{blockType}}) block ({{blockId}}) pipe ({{pipeName}})\n"
            "=========     Address 0x{{address}} is used after free\n"
            "=========     and belongs to allocation at 0x{{base}} of size {{bytes}} bytes\n"),
        MakePattern(
            ReportTool::MEMCHECK, NpusanMemcheckPattern::USE_BEFORE_ALLOC, "use_before_alloc",
            "========= {{Severity}}: Invalid {{space}} {{access}} of size {{accessBytes}} bytes\n"
            "=========     {{location}}\n"
            "=========     by aicore ({{coreId}}) type ({{blockType}}) block ({{blockId}}) pipe ({{pipeName}})\n"
            "=========     Address 0x{{address}} is used before allocation is available\n"
            "=========     allocation 0x{{base}} of size {{bytes}} bytes was created at serial {{allocSerialNo}}\n"),
        MakePattern(
            ReportTool::MEMCHECK, NpusanMemcheckPattern::INVALID_FREE, "invalid_free",
            "========= {{Severity}}: Malloc/Free error encountered : Invalid pointer to free\n"
            "=========     at pc 0x{{pc}} in {{kernelName}}\n"
            "=========     by aicore ({{coreId}}) type ({{blockType}}) block ({{blockId}}) pipe ({{pipeName}})\n"
            "=========     Address 0x{{address}}\n"),
        MakePattern(
            ReportTool::MEMCHECK, NpusanMemcheckPattern::DOUBLE_FREE, "double_free",
            "========= {{Severity}}: Malloc/Free error encountered : Double free\n"
            "=========     at pc 0x{{pc}} in {{kernelName}}\n"
            "=========     by aicore ({{coreId}}) type ({{blockType}}) block ({{blockId}}) pipe ({{pipeName}})\n"
            "=========     Address 0x{{base}}\n"),
        MakePattern(
            ReportTool::MEMCHECK, NpusanMemcheckPattern::LEAK, "leak",
            "========= {{Severity}}: Leaked {{bytes}} bytes at 0x{{base}}\n"
            "=========     allocation id {{allocId}}, memory space {{space}}\n"),
        MakePattern(
            ReportTool::MEMCHECK, NpusanMemcheckPattern::API_ERROR, "api_error",
            "========= {{Severity}}: Program hit {{apiErrorName}} (error {{apiErrorCode}}) due to "
            "\"{{apiErrorMessage}}\" on ACL runtime API call to {{apiName}}.\n"),

        MakePattern(
            ReportTool::INITCHECK, NpusanInitcheckPattern::UNINITIALIZED_READ, "uninitialized_read",
            "========= {{Severity}}: Uninitialized {{space}} memory read of size {{accessBytes}} bytes\n"
            "=========     {{location}}\n"
            "=========     by aicore ({{coreId}}) type ({{blockType}}) block ({{blockId}}) pipe ({{pipeName}})\n"
            "=========     Address 0x{{address}}\n"),
        MakePattern(
            ReportTool::INITCHECK, NpusanInitcheckPattern::PARTIAL_UNINITIALIZED_READ, "partial_uninitialized_read",
            "========= {{Severity}}: Partially uninitialized {{space}} memory read of size {{accessBytes}} bytes\n"
            "=========     {{location}}\n"
            "=========     by aicore ({{coreId}}) type ({{blockType}}) block ({{blockId}}) pipe ({{pipeName}})\n"
            "=========     First uninitialized byte at 0x{{firstUninitAddress}}, {{uninitBytes}} bytes are "
            "uninitialized\n"),
        MakePattern(
            ReportTool::INITCHECK, NpusanInitcheckPattern::UNUSED_MEMORY, "unused_memory",
            "========= {{Severity}}: Unused {{space}} memory in allocation 0x{{base}} of size {{bytes}} bytes\n"
            "=========     Not written {{unusedBytes}} bytes at offset 0x{{firstUninitOffset}} "
            "(0x{{firstUninitAddress}})\n"
            "=========     {{unusedPercent}}% of allocation were unused.\n"),
        MakePattern(
            ReportTool::INITCHECK, NpusanInitcheckPattern::API_READ_UNINITIALIZED, "api_read_uninitialized",
            "========= {{Severity}}: Uninitialized {{space}} memory read by ACL runtime API\n"
            "=========     Address 0x{{address}}, size {{accessBytes}} bytes\n"),

        MakePattern(
            ReportTool::RACECHECK, NpusanRacecheckPattern::ANALYSIS, "analysis",
            "========= {{Severity}}: Race reported between {{firstAccess}} access at {{firstLocation}}\n"
            "=========     and {{secondAccess}} access at {{secondLocation}} [{{hazardCount}} hazards]\n"),
        MakePattern(
            ReportTool::RACECHECK, NpusanRacecheckPattern::HAZARD_RAW, "hazard_raw",
            "========= {{Severity}}: Potential RAW hazard detected at {{space}} 0x{{address}} in block ({{blockId}}) "
            ":\n"
            "=========     Write AICore ({{firstCoreId}}) pipe ({{firstPipe}}) at {{firstLocation}}\n"
            "=========     Read AICore ({{secondCoreId}}) pipe ({{secondPipe}}) at {{secondLocation}}\n"
            "=========     Current Value : {{currentValue}}\n"),
        MakePattern(
            ReportTool::RACECHECK, NpusanRacecheckPattern::HAZARD_WAR, "hazard_war",
            "========= {{Severity}}: Potential WAR hazard detected at {{space}} 0x{{address}} in block ({{blockId}}) "
            ":\n"
            "=========     Read AICore ({{firstCoreId}}) pipe ({{firstPipe}}) at {{firstLocation}}\n"
            "=========     Write AICore ({{secondCoreId}}) pipe ({{secondPipe}}) at {{secondLocation}}\n"
            "=========     Incoming Value : {{incomingValue}}\n"),
        MakePattern(
            ReportTool::RACECHECK, NpusanRacecheckPattern::HAZARD_WAW, "hazard_waw",
            "========= {{Severity}}: Potential WAW hazard detected at {{space}} 0x{{address}} in block ({{blockId}}) "
            ":\n"
            "=========     Write AICore ({{firstCoreId}}) pipe ({{firstPipe}}) at {{firstLocation}}\n"
            "=========     Write AICore ({{secondCoreId}}) pipe ({{secondPipe}}) at {{secondLocation}}\n"
            "=========     Current Value : {{currentValue}}, Incoming Value : {{incomingValue}}\n"),
        MakePattern(
            ReportTool::RACECHECK, NpusanRacecheckPattern::ATOMIC_RACE, "atomic_race",
            "========= {{Severity}}: Potential atomic race detected at {{space}} 0x{{address}}\n"
            "=========     First access at {{firstLocation}}\n"
            "=========     Second access at {{secondLocation}}\n"
            "=========     Race scope {{scope}}\n"),
        MakePattern(
            ReportTool::RACECHECK, NpusanRacecheckPattern::CROSS_PIPE_RACE, "cross_pipe_race",
            "========= {{Severity}}: Potential cross-pipe race detected at {{space}} 0x{{address}}\n"
            "=========     First access by aicore ({{coreId}}) pipe ({{firstPipe}}) at {{firstLocation}}\n"
            "=========     Second access by aicore ({{coreId}}) pipe ({{secondPipe}}) at {{secondLocation}}\n"),
        MakePattern(
            ReportTool::RACECHECK, NpusanRacecheckPattern::INTER_CORE_RACE, "inter_core_race",
            "========= {{Severity}}: Potential inter-core race detected at {{space}} 0x{{address}}\n"
            "=========     First access by aicore ({{firstCoreId}}) type ({{firstType}}) block ({{firstBlock}}) "
            "pipe ({{firstPipe}})\n"
            "=========     Second access by aicore ({{secondCoreId}}) type ({{secondType}}) block ({{secondBlock}}) "
            "pipe ({{secondPipe}})\n"
            "=========     {{hazardCount}} hazards are associated with this address range\n"),
        MakePattern(
            ReportTool::RACECHECK, NpusanRacecheckPattern::INVALID_REMOTE_ACCESS, "invalid_remote_access",
            "========= {{Severity}}: Potential invalid {{space}} {{access}} of size {{accessBytes}} bytes\n"
            "=========     {{location}}\n"
            "=========     by aicore ({{coreId}}) type ({{blockType}}) block ({{blockId}}) pipe ({{pipeName}})\n"
            "=========     Address 0x{{address}} is located in a remote execution entity that {{remoteState}}\n"),

        MakePattern(
            ReportTool::SYNCCHECK, NpusanSynccheckPattern::INTRA_CORE_DIVERGENT, "intra_core_divergent",
            "========= {{Severity}}: Barrier error detected. {{reason}}.\n"
            "=========     trigger point: {{triggerOperation}} at {{triggerLocation}}\n"
            "=========     by aicore ({{triggerCoreId}}) type ({{triggerType}}) block ({{triggerBlock}}) "
            "pipe ({{triggerPipe}})\n"
            "=========     scope {{scope}}, active mask 0x{{activeMask}}, expected mask 0x{{expectedMask}}\n"
            "{{objectLine}}"),
        MakePattern(
            ReportTool::SYNCCHECK, NpusanSynccheckPattern::INTER_CORE_DIVERGENT, "inter_core_divergent",
            "========= {{Severity}}: Barrier error detected. {{reason}}.\n"
            "=========     trigger point: {{triggerOperation}} by aicore ({{triggerCoreId}}) type ({{triggerType}}) "
            "block ({{triggerBlock}}) "
            "at "
            "{{triggerLocation}}\n"
            "{{relatedPointLine}}"
            "=========     scope {{scope}}, active mask 0x{{activeMask}}, expected mask 0x{{expectedMask}}\n"
            "{{objectLine}}"),
        MakePattern(
            ReportTool::SYNCCHECK, NpusanSynccheckPattern::INVALID_ARGUMENT, "invalid_argument",
            "========= {{Severity}}: Synchronization error detected. {{reason}}.\n"
            "=========     trigger point: {{triggerOperation}} by aicore ({{triggerCoreId}}) type ({{triggerType}}) "
            "block ({{triggerBlock}}) "
            "pipe ({{triggerPipe}}) at {{triggerLocation}}\n"
            "{{objectLine}}"),
        MakePattern(
            ReportTool::SYNCCHECK, NpusanSynccheckPattern::PAIRING_MISMATCH, "pairing_mismatch",
            "========= {{Severity}}: Synchronization pairing mismatch: {{reasonText}} {{triggerOperation}}.\n"
            "=========     trigger point: {{triggerOperation}} by aicore ({{triggerCoreId}}) type ({{triggerType}}) "
            "block ({{triggerBlock}}) "
            "pipe ({{triggerPipe}}) at {{triggerLocation}}\n"
            "{{relatedPointLine}}"
            "{{expectedOperationLine}}"
            "=========     pair kind {{pairKind}}, key ({{pairKey}})\n"),
        MakePattern(
            ReportTool::SYNCCHECK, NpusanSynccheckPattern::PARTICIPANT_MISMATCH, "participant_mismatch",
            "========= {{Severity}}: Synchronization error detected. {{reason}}.\n"
            "=========     trigger point: {{triggerOperation}} by aicore ({{triggerCoreId}}) type ({{triggerType}}) "
            "block ({{triggerBlock}}) "
            "pipe ({{triggerPipe}}) at {{triggerLocation}}\n"
            "{{relatedPointLine}}"
            "=========     scope {{scope}}, active mask 0x{{activeMask}}, expected mask 0x{{expectedMask}}\n"
            "{{objectLine}}"),
        MakePattern(
            ReportTool::SYNCCHECK, NpusanSynccheckPattern::DEADLOCK, "deadlock",
            "========= {{Severity}}: Deadlock detected. {{reason}}.\n"
            "=========     trigger point: {{triggerOperation}} by aicore ({{triggerCoreId}}) type ({{triggerType}}) "
            "block ({{triggerBlock}}) "
            "pipe ({{triggerPipe}}) at {{triggerLocation}}\n"
            "{{relatedPointLine}}"
            "=========     waiting mask 0x{{waitingMask}}, timeout {{timeoutNs}} ns\n"
            "{{objectLine}}"),
        MakePattern(
            ReportTool::SYNCCHECK, NpusanSynccheckPattern::OBJECT_NOT_INITIALIZED, "object_not_initialized",
            "========= {{Severity}}: Synchronization error detected. {{reason}}.\n"
            "=========     trigger point: {{triggerOperation}} by aicore ({{triggerCoreId}}) type ({{triggerType}}) "
            "block ({{triggerBlock}}) "
            "pipe ({{triggerPipe}}) at {{triggerLocation}}\n"
            "=========     primitive {{primitiveKind}}\n"
            "{{objectLine}}"),
        MakePattern(
            ReportTool::SYNCCHECK, NpusanSynccheckPattern::INSTRUCTION_SEQUENCE_MISMATCH,
            "instruction_sequence_mismatch",
            "========= {{Severity}}: Synchronization instruction sequence error detected. {{reason}}.\n"
            "=========     trigger point: {{triggerOperation}} by aicore ({{triggerCoreId}}) type ({{triggerType}}) "
            "block ({{triggerBlock}}) "
            "pipe ({{triggerPipe}}) at {{triggerLocation}}\n"
            "{{relatedPointLine}}"
            "=========     sequence index {{sequenceIndex}}, active mask 0x{{activeMask}}\n"),

        MakePattern(
            ReportTool::SOCCHECK, NpusanSoccheckPattern::UNINITIALIZED_STATE_READ, "uninitialized_state_read",
            "========= {{Severity}}: SOC state error detected. Uninitialized state read.\n"
            "=========     {{location}}\n"
            "=========     by aicore ({{coreId}}) type ({{blockType}}) block ({{blockId}}) pipe ({{pipeName}})\n"
            "=========     state kind {{stateKind}}, state id {{stateId}}, observed value 0x{{observedValue}}\n"),
        MakePattern(
            ReportTool::SOCCHECK, NpusanSoccheckPattern::REGISTER_MISMATCH, "register_mismatch",
            "========= {{Severity}}: SOC register mismatch detected.\n"
            "=========     {{location}}\n"
            "=========     by aicore ({{coreId}}) type ({{blockType}}) block ({{blockId}}) pipe ({{pipeName}})\n"
            "=========     register {{registerId}}, expected 0x{{expectedValue}}, observed 0x{{observedValue}}\n"),
        MakePattern(
            ReportTool::SOCCHECK, NpusanSoccheckPattern::ILLEGAL_STATE_TRANSITION, "illegal_state_transition",
            "========= {{Severity}}: SOC state error detected. Illegal state transition.\n"
            "=========     {{location}}\n"
            "=========     by aicore ({{coreId}}) type ({{blockType}}) block ({{blockId}}) pipe ({{pipeName}})\n"
            "=========     state id {{stateId}}, old value 0x{{oldValue}}, new value 0x{{newValue}}\n"),
        MakePattern(
            ReportTool::SOCCHECK, NpusanSoccheckPattern::STATE_NOT_RESTORED, "state_not_restored",
            "========= {{Severity}}: SOC state error detected. State was not restored.\n"
            "=========     at kernel exit for {{kernelName}}\n"
            "=========     owner aicore ({{ownerCoreId}}), state id {{stateId}}\n"
            "=========     expected 0x{{expectedValue}}, observed 0x{{observedValue}}\n"),
        MakePattern(
            ReportTool::SOCCHECK, NpusanSoccheckPattern::CROSS_CORE_STATE_INCONSISTENT, "cross_core_state_inconsistent",
            "========= {{Severity}}: SOC state error detected. Cross-core state is inconsistent.\n"
            "=========     consumer aicore ({{consumerCoreId}}) observed 0x{{observedValue}}\n"
            "=========     producer aicore ({{producerCoreId}}) expected 0x{{expectedValue}}\n"
            "=========     state id {{stateId}}, scope {{scope}}\n"),
        MakePattern(
            ReportTool::SOCCHECK, NpusanSoccheckPattern::SCOPE_VIOLATION, "scope_violation",
            "========= {{Severity}}: SOC state error detected. State scope violation.\n"
            "=========     {{location}}\n"
            "=========     by aicore ({{coreId}}) type ({{blockType}}) block ({{blockId}}) pipe ({{pipeName}})\n"
            "=========     state id {{stateId}}, invalid scope {{scope}}\n"),
    };
    return catalog;
}

} // namespace

const PatternCatalog& GetPatternCatalog() { return Catalog(); }

const PatternDescriptor* FindPatternDescriptor(ReportTool tool, std::uint32_t pattern)
{
    for (const auto& [key, descriptor] : Catalog()) {
        if (key.tool == tool && descriptor.value == pattern) {
            return &descriptor;
        }
    }
    return nullptr;
}

const PatternDescriptor* FindPatternDescriptor(const ReportTemplateKey& key)
{
    const auto found = Catalog().find(key);
    return found == Catalog().end() ? nullptr : &found->second;
}

const char* PatternName(ReportTool tool, std::uint32_t pattern)
{
    for (const auto& [key, descriptor] : Catalog()) {
        if (key.tool == tool && descriptor.value == pattern) {
            return key.pattern.c_str();
        }
    }
    return "";
}

const char* ToolName(ReportTool tool)
{
    switch (tool) {
        case ReportTool::MEMCHECK:
            return "memcheck";
        case ReportTool::INITCHECK:
            return "initcheck";
        case ReportTool::RACECHECK:
            return "racecheck";
        case ReportTool::SYNCCHECK:
            return "synccheck";
        case ReportTool::SOCCHECK:
            return "soccheck";
    }
    return "unknown";
}

bool ParseToolName(const std::string& text, ReportTool* tool)
{
    if (tool == nullptr) {
        return false;
    }
    for (const ReportTool candidate :
         {ReportTool::MEMCHECK, ReportTool::INITCHECK, ReportTool::RACECHECK, ReportTool::SYNCCHECK,
          ReportTool::SOCCHECK}) {
        if (text == ToolName(candidate)) {
            *tool = candidate;
            return true;
        }
    }
    return false;
}

} // namespace aclsan::cann::detail

namespace aclsan::cann {

bool ReportTemplateKey::operator<(const ReportTemplateKey& other) const
{
    if (tool != other.tool) {
        return static_cast<int>(tool) < static_cast<int>(other.tool);
    }
    return pattern < other.pattern;
}

bool ReportTemplateKey::operator==(const ReportTemplateKey& other) const
{
    return tool == other.tool && pattern == other.pattern;
}

const ReportTemplate* FindBuiltinReportTemplate(const ReportTemplateKey& key)
{
    const detail::PatternDescriptor* descriptor = detail::FindPatternDescriptor(key);
    return descriptor == nullptr ? nullptr : &descriptor->reportTemplate;
}

std::vector<ReportTemplateKey> ListBuiltinReportTemplates()
{
    std::vector<ReportTemplateKey> keys;
    keys.reserve(detail::GetPatternCatalog().size());
    for (const auto& [key, descriptor] : detail::GetPatternCatalog()) {
        (void)descriptor;
        keys.push_back(key);
    }
    return keys;
}

const char* ReportToolName(ReportTool tool) { return detail::ToolName(tool); }

const char* ReportSeverityName(ReportSeverity severity)
{
    switch (severity) {
        case ReportSeverity::INFO:
            return "INFO";
        case ReportSeverity::WARNING:
            return "WARNING";
        case ReportSeverity::ERROR:
            return "ERROR";
        case ReportSeverity::FATAL:
            return "FATAL";
    }
    return "ERROR";
}

} // namespace aclsan::cann
