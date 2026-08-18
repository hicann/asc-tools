// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "report_renderer.h"

#include <cctype>
#include <array>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <utility>

namespace aclsan::cann {
namespace {

using TemplateEntry = std::pair<ReportTemplateKey, ReportTemplate>;

const std::vector<TemplateEntry>& BuiltinTemplates()
{
    static const std::vector<TemplateEntry> templates = {
        {{ReportTool::kMemcheck, "invalid_access"},
         {"========= {{Severity}}: Invalid {{space}} {{access}} of size {{accessBytes}} bytes\n"
          "=========     {{location}}\n"
          "=========     by aicore ({{coreId}}) block ({{blockId}}) pipe ({{pipeName}})\n"
          "=========     Address 0x{{address}} is out of bounds\n"
          "=========     and is {{distanceBytes}} bytes {{before|after}} the nearest allocation at 0x{{base}} of size "
          "{{bytes}} bytes\n"}},
        {{ReportTool::kMemcheck, "misaligned_access"},
         {"========= {{Severity}}: Invalid {{space}} {{access}} of size {{accessBytes}} bytes\n"
          "=========     {{location}}\n"
          "=========     by aicore ({{coreId}}) block ({{blockId}}) pipe ({{pipeName}})\n"
          "=========     Address 0x{{address}} is misaligned\n"
          "=========     required alignment is {{requiredAlign}} bytes\n"}},
        {{ReportTool::kMemcheck, "use_after_free"},
         {"========= {{Severity}}: Invalid {{space}} {{access}} of size {{accessBytes}} bytes\n"
          "=========     {{location}}\n"
          "=========     by aicore ({{coreId}}) block ({{blockId}}) pipe ({{pipeName}})\n"
          "=========     Address 0x{{address}} is used after free\n"
          "=========     and belongs to allocation at 0x{{base}} of size {{bytes}} bytes\n"}},
        {{ReportTool::kMemcheck, "use_before_alloc"},
         {"========= {{Severity}}: Invalid {{space}} {{access}} of size {{accessBytes}} bytes\n"
          "=========     {{location}}\n"
          "=========     by aicore ({{coreId}}) block ({{blockId}}) pipe ({{pipeName}})\n"
          "=========     Address 0x{{address}} is used before allocation is available\n"
          "=========     allocation 0x{{base}} of size {{bytes}} bytes was created at serial {{allocSerialNo}}\n"}},
        {{ReportTool::kMemcheck, "invalid_free"},
         {"========= {{Severity}}: Malloc/Free error encountered : Invalid pointer to free\n"
          "=========     at pc 0x{{pc}} in {{kernelName}}\n"
          "=========     by aicore ({{coreId}}) block ({{blockId}}) pipe ({{pipeName}})\n"
          "=========     Address 0x{{address}}\n"}},
        {{ReportTool::kMemcheck, "double_free"},
         {"========= {{Severity}}: Malloc/Free error encountered : Double free\n"
          "=========     at pc 0x{{pc}} in {{kernelName}}\n"
          "=========     by aicore ({{coreId}}) block ({{blockId}}) pipe ({{pipeName}})\n"
          "=========     Address 0x{{base}}\n"}},
        {{ReportTool::kMemcheck, "leak"},
         {"========= {{Severity}}: Leaked {{bytes}} bytes at 0x{{base}}\n"
          "=========     allocation id {{allocId}}, memory space {{space}}\n"}},
        {{ReportTool::kMemcheck, "api_error"},
         {"========= {{Severity}}: Program hit {{apiErrorName}} (error {{apiErrorCode}}) due to "
          "\"{{apiErrorMessage}}\" on ACL runtime API call to {{apiName}}.\n"}},

        {{ReportTool::kInitcheck, "uninitialized_read"},
         {"========= {{Severity}}: Uninitialized {{space}} memory read of size {{accessBytes}} bytes\n"
          "=========     {{location}}\n"
          "=========     by aicore ({{coreId}}) block ({{blockId}}) pipe ({{pipeName}})\n"
          "=========     Address 0x{{address}}\n"}},
        {{ReportTool::kInitcheck, "partial_uninitialized_read"},
         {"========= {{Severity}}: Partially uninitialized {{space}} memory read of size {{accessBytes}} bytes\n"
          "=========     {{location}}\n"
          "=========     by aicore ({{coreId}}) block ({{blockId}}) pipe ({{pipeName}})\n"
          "=========     First uninitialized byte at 0x{{firstUninitAddress}}, {{uninitBytes}} bytes are "
          "uninitialized\n"}},
        {{ReportTool::kInitcheck, "unused_memory"},
         {"========= {{Severity}}: Unused {{space}} memory in allocation 0x{{base}} of size {{bytes}} bytes\n"
          "=========     Not written {{unusedBytes}} bytes at offset 0x{{firstUninitOffset}} "
          "(0x{{firstUninitAddress}})\n"
          "=========     {{unusedPercent}}% of allocation were unused.\n"}},
        {{ReportTool::kInitcheck, "api_read_uninitialized"},
         {"========= {{Severity}}: Uninitialized {{space}} memory read by ACL runtime API\n"
          "=========     Address 0x{{address}}, size {{accessBytes}} bytes\n"}},

        {{ReportTool::kRacecheck, "analysis"},
         {"========= {{Severity}}: Race reported between {{firstAccess}} access at {{firstLocation}}\n"
          "=========     and {{secondAccess}} access at {{secondLocation}} [{{hazardCount}} hazards]\n"}},
        {{ReportTool::kRacecheck, "hazard_raw"},
         {"========= {{Severity}}: Potential RAW hazard detected at {{space}} 0x{{address}} in block ({{blockId}}) :\n"
          "=========     Write AICore ({{firstCoreId}}) pipe ({{firstPipe}}) at {{firstLocation}}\n"
          "=========     Read AICore ({{secondCoreId}}) pipe ({{secondPipe}}) at {{secondLocation}}\n"
          "=========     Current Value : {{currentValue}}\n"}},
        {{ReportTool::kRacecheck, "hazard_war"},
         {"========= {{Severity}}: Potential WAR hazard detected at {{space}} 0x{{address}} in block ({{blockId}}) :\n"
          "=========     Read AICore ({{firstCoreId}}) pipe ({{firstPipe}}) at {{firstLocation}}\n"
          "=========     Write AICore ({{secondCoreId}}) pipe ({{secondPipe}}) at {{secondLocation}}\n"
          "=========     Incoming Value : {{incomingValue}}\n"}},
        {{ReportTool::kRacecheck, "hazard_waw"},
         {"========= {{Severity}}: Potential WAW hazard detected at {{space}} 0x{{address}} in block ({{blockId}}) :\n"
          "=========     Write AICore ({{firstCoreId}}) pipe ({{firstPipe}}) at {{firstLocation}}\n"
          "=========     Write AICore ({{secondCoreId}}) pipe ({{secondPipe}}) at {{secondLocation}}\n"
          "=========     Current Value : {{currentValue}}, Incoming Value : {{incomingValue}}\n"}},
        {{ReportTool::kRacecheck, "atomic_race"},
         {"========= {{Severity}}: Potential atomic race detected at {{space}} 0x{{address}}\n"
          "=========     First access at {{firstLocation}}\n"
          "=========     Second access at {{secondLocation}}\n"
          "=========     Race scope {{scope}}\n"}},
        {{ReportTool::kRacecheck, "cross_pipe_race"},
         {"========= {{Severity}}: Potential cross-pipe race detected at {{space}} 0x{{address}}\n"
          "=========     First access by aicore ({{coreId}}) pipe ({{firstPipe}}) at {{firstLocation}}\n"
          "=========     Second access by aicore ({{coreId}}) pipe ({{secondPipe}}) at {{secondLocation}}\n"}},
        {{ReportTool::kRacecheck, "inter_core_race"},
         {"========= {{Severity}}: Potential inter-core race detected at {{space}} 0x{{address}}\n"
          "=========     First access by aicore ({{firstCoreId}}) block ({{firstBlock}}) pipe ({{firstPipe}})\n"
          "=========     Second access by aicore ({{secondCoreId}}) block ({{secondBlock}}) pipe ({{secondPipe}})\n"
          "=========     {{hazardCount}} hazards are associated with this address range\n"}},
        {{ReportTool::kRacecheck, "invalid_remote_access"},
         {"========= {{Severity}}: Potential invalid {{space}} {{access}} of size {{accessBytes}} bytes\n"
          "=========     {{location}}\n"
          "=========     by aicore ({{coreId}}) block ({{blockId}}) pipe ({{pipeName}})\n"
          "=========     Address 0x{{address}} is located in a remote execution entity that {{remoteState}}\n"}},

        {{ReportTool::kSynccheck, "intra_core_divergent"},
         {"========= {{Severity}}: Barrier error detected. {{reason}}.\n"
          "=========     trigger point: {{triggerOperation}} at {{triggerLocation}}\n"
          "=========     by aicore ({{triggerCoreId}}) block ({{triggerBlock}}) pipe ({{triggerPipe}})\n"
          "=========     scope {{scope}}, active mask 0x{{activeMask}}, expected mask 0x{{expectedMask}}\n"
          "{{objectLine}}"}},
        {{ReportTool::kSynccheck, "inter_core_divergent"},
         {"========= {{Severity}}: Barrier error detected. {{reason}}.\n"
          "=========     trigger point: {{triggerOperation}} by aicore ({{triggerCoreId}}) block ({{triggerBlock}}) at "
          "{{triggerLocation}}\n"
          "{{relatedPointLine}}"
          "=========     scope {{scope}}, active mask 0x{{activeMask}}, expected mask 0x{{expectedMask}}\n"
          "{{objectLine}}"}},
        {{ReportTool::kSynccheck, "invalid_argument"},
         {"========= {{Severity}}: Synchronization error detected. {{reason}}.\n"
          "=========     trigger point: {{triggerOperation}} by aicore ({{triggerCoreId}}) block ({{triggerBlock}}) "
          "pipe ({{triggerPipe}}) at {{triggerLocation}}\n"
          "{{objectLine}}"}},
        {{ReportTool::kSynccheck, "pairing_mismatch"},
         {"========= {{Severity}}: Synchronization pairing mismatch: {{reasonText}} {{triggerOperation}}.\n"
          "=========     trigger point: {{triggerOperation}} by aicore ({{triggerCoreId}}) block ({{triggerBlock}}) "
          "pipe ({{triggerPipe}}) at {{triggerLocation}}\n"
          "{{relatedPointLine}}"
          "{{expectedOperationLine}}"
          "=========     pair kind {{pairKind}}, key ({{pairKey}})\n"}},
        {{ReportTool::kSynccheck, "participant_mismatch"},
         {"========= {{Severity}}: Synchronization error detected. {{reason}}.\n"
          "=========     trigger point: {{triggerOperation}} by aicore ({{triggerCoreId}}) block ({{triggerBlock}}) "
          "pipe ({{triggerPipe}}) at {{triggerLocation}}\n"
          "{{relatedPointLine}}"
          "=========     scope {{scope}}, active mask 0x{{activeMask}}, expected mask 0x{{expectedMask}}\n"
          "{{objectLine}}"}},
        {{ReportTool::kSynccheck, "deadlock"},
         {"========= {{Severity}}: Deadlock detected. {{reason}}.\n"
          "=========     trigger point: {{triggerOperation}} by aicore ({{triggerCoreId}}) block ({{triggerBlock}}) "
          "pipe ({{triggerPipe}}) at {{triggerLocation}}\n"
          "{{relatedPointLine}}"
          "=========     waiting mask 0x{{waitingMask}}, timeout {{timeoutNs}} ns\n"
          "{{objectLine}}"}},
        {{ReportTool::kSynccheck, "object_not_initialized"},
         {"========= {{Severity}}: Synchronization error detected. {{reason}}.\n"
          "=========     trigger point: {{triggerOperation}} by aicore ({{triggerCoreId}}) block ({{triggerBlock}}) "
          "pipe ({{triggerPipe}}) at {{triggerLocation}}\n"
          "=========     primitive {{primitiveKind}}\n"
          "{{objectLine}}"}},
        {{ReportTool::kSynccheck, "instruction_sequence_mismatch"},
         {"========= {{Severity}}: Synchronization instruction sequence error detected. {{reason}}.\n"
          "=========     trigger point: {{triggerOperation}} by aicore ({{triggerCoreId}}) block ({{triggerBlock}}) "
          "pipe ({{triggerPipe}}) at {{triggerLocation}}\n"
          "{{relatedPointLine}}"
          "=========     sequence index {{sequenceIndex}}, active mask 0x{{activeMask}}\n"}},

        {{ReportTool::kSoccheck, "uninitialized_state_read"},
         {"========= {{Severity}}: SOC state error detected. Uninitialized state read.\n"
          "=========     {{location}}\n"
          "=========     by aicore ({{coreId}}) block ({{blockId}}) pipe ({{pipeName}})\n"
          "=========     state kind {{stateKind}}, state id {{stateId}}, observed value 0x{{observedValue}}\n"}},
        {{ReportTool::kSoccheck, "register_mismatch"},
         {"========= {{Severity}}: SOC register mismatch detected.\n"
          "=========     {{location}}\n"
          "=========     by aicore ({{coreId}}) block ({{blockId}}) pipe ({{pipeName}})\n"
          "=========     register {{registerId}}, expected 0x{{expectedValue}}, observed 0x{{observedValue}}\n"}},
        {{ReportTool::kSoccheck, "illegal_state_transition"},
         {"========= {{Severity}}: SOC state error detected. Illegal state transition.\n"
          "=========     {{location}}\n"
          "=========     by aicore ({{coreId}}) block ({{blockId}}) pipe ({{pipeName}})\n"
          "=========     state id {{stateId}}, old value 0x{{oldValue}}, new value 0x{{newValue}}\n"}},
        {{ReportTool::kSoccheck, "state_not_restored"},
         {"========= {{Severity}}: SOC state error detected. State was not restored.\n"
          "=========     at kernel exit for {{kernelName}}\n"
          "=========     owner aicore ({{ownerCoreId}}), state id {{stateId}}\n"
          "=========     expected 0x{{expectedValue}}, observed 0x{{observedValue}}\n"}},
        {{ReportTool::kSoccheck, "cross_core_state_inconsistent"},
         {"========= {{Severity}}: SOC state error detected. Cross-core state is inconsistent.\n"
          "=========     consumer aicore ({{consumerCoreId}}) observed 0x{{observedValue}}\n"
          "=========     producer aicore ({{producerCoreId}}) expected 0x{{expectedValue}}\n"
          "=========     state id {{stateId}}, scope {{scope}}\n"}},
        {{ReportTool::kSoccheck, "scope_violation"},
         {"========= {{Severity}}: SOC state error detected. State scope violation.\n"
          "=========     {{location}}\n"
          "=========     by aicore ({{coreId}}) block ({{blockId}}) pipe ({{pipeName}})\n"
          "=========     state id {{stateId}}, invalid scope {{scope}}\n"}},
    };
    return templates;
}

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

        const char escaped = value[++i];
        switch (escaped) {
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
                out.push_back(escaped);
                break;
        }
    }
    return out;
}

bool ParseReportToolName(const std::string& text, ReportTool* tool)
{
    if (tool == nullptr) {
        return false;
    }
    if (text == "memcheck") {
        *tool = ReportTool::kMemcheck;
        return true;
    }
    if (text == "initcheck") {
        *tool = ReportTool::kInitcheck;
        return true;
    }
    if (text == "racecheck") {
        *tool = ReportTool::kRacecheck;
        return true;
    }
    if (text == "synccheck") {
        *tool = ReportTool::kSynccheck;
        return true;
    }
    if (text == "soccheck") {
        *tool = ReportTool::kSoccheck;
        return true;
    }
    return false;
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
    if (!ParseReportToolName(text.substr(0, dot), &tool)) {
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

        const std::string key = tpl.text.substr(open + 2, close - (open + 2));
        const auto it = fields.find(key);
        if (it == fields.end()) {
            return ReportRenderStatus::kMissingField;
        }

        out->append(it->second);
        pos = close + 2;
    }

    return ReportRenderStatus::kSuccess;
}

std::string FieldOr(const ReportFields& fields, const std::string& key, const char* fallback)
{
    const auto it = fields.find(key);
    return it == fields.end() || it->second.empty() ? fallback : it->second;
}

void PutDerivedLocation(
    const std::string& fieldPrefix, const std::string& locationKey, bool includeAt, ReportFields* fields)
{
    if (fields->find(locationKey) != fields->end()) {
        return;
    }

    const std::string functionKey = fieldPrefix.empty() ? "function" : fieldPrefix + "Function";
    const std::string offsetKey = fieldPrefix.empty() ? "offset" : fieldPrefix + "Offset";
    const std::string fileKey = fieldPrefix.empty() ? "file" : fieldPrefix + "File";
    const std::string lineKey = fieldPrefix.empty() ? "line" : fieldPrefix + "Line";
    const std::string pcKey = fieldPrefix.empty() ? "pc" : fieldPrefix + "Pc";
    const std::string kernelNameKey = fieldPrefix.empty() ? "kernelName" : fieldPrefix + "KernelName";
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

std::string Hex(std::uint64_t value)
{
    std::ostringstream os;
    os << std::hex << std::nouppercase << value;
    return os.str();
}

std::string HexWithPrefix(std::uint64_t value) { return std::string("0x") + Hex(value); }

const char* MemorySpaceName(NpusanReportMemorySpace space)
{
    switch (space) {
        case NpusanReportMemorySpace::kGm:
            return "GM";
        case NpusanReportMemorySpace::kUb:
            return "UB";
        case NpusanReportMemorySpace::kL1:
            return "L1";
        case NpusanReportMemorySpace::kL0A:
            return "L0A";
        case NpusanReportMemorySpace::kL0B:
            return "L0B";
        case NpusanReportMemorySpace::kL0C:
            return "L0C";
        case NpusanReportMemorySpace::kBt:
            return "BT";
        case NpusanReportMemorySpace::kPrivate:
            return "private";
        case NpusanReportMemorySpace::kHost:
            return "host";
        case NpusanReportMemorySpace::kUnknown:
            return "unknown";
    }
    return "unknown";
}

const char* AccessModeName(NpusanReportAccessMode mode)
{
    switch (mode) {
        case NpusanReportAccessMode::kRead:
            return "read";
        case NpusanReportAccessMode::kWrite:
            return "write";
        case NpusanReportAccessMode::kReadWrite:
            return "read/write";
        case NpusanReportAccessMode::kFree:
            return "free";
    }
    return "access";
}

const char* DistanceDirectionName(NpusanReportDistanceKind kind)
{
    switch (kind) {
        case NpusanReportDistanceKind::kBefore:
            return "before";
        case NpusanReportDistanceKind::kAfter:
            return "after";
        case NpusanReportDistanceKind::kInside:
            return "inside";
        case NpusanReportDistanceKind::kUnknown:
            return "from";
    }
    return "from";
}

const char* MemcheckPatternName(std::uint32_t pattern)
{
    switch (static_cast<NpusanMemcheckPattern>(pattern)) {
        case NpusanMemcheckPattern::kInvalidAccess:
            return "invalid_access";
        case NpusanMemcheckPattern::kMisalignedAccess:
            return "misaligned_access";
        case NpusanMemcheckPattern::kUseAfterFree:
            return "use_after_free";
        case NpusanMemcheckPattern::kUseBeforeAlloc:
            return "use_before_alloc";
        case NpusanMemcheckPattern::kInvalidFree:
            return "invalid_free";
        case NpusanMemcheckPattern::kDoubleFree:
            return "double_free";
        case NpusanMemcheckPattern::kLeak:
            return "leak";
        case NpusanMemcheckPattern::kApiError:
            return "api_error";
    }
    return "";
}

const char* InitcheckPatternName(std::uint32_t pattern)
{
    switch (static_cast<NpusanInitcheckPattern>(pattern)) {
        case NpusanInitcheckPattern::kUninitializedRead:
            return "uninitialized_read";
        case NpusanInitcheckPattern::kPartialUninitializedRead:
            return "partial_uninitialized_read";
        case NpusanInitcheckPattern::kUnusedMemory:
            return "unused_memory";
        case NpusanInitcheckPattern::kApiReadUninitialized:
            return "api_read_uninitialized";
    }
    return "";
}

const char* RacecheckPatternName(std::uint32_t pattern)
{
    switch (static_cast<NpusanRacecheckPattern>(pattern)) {
        case NpusanRacecheckPattern::kAnalysis:
            return "analysis";
        case NpusanRacecheckPattern::kHazardRaw:
            return "hazard_raw";
        case NpusanRacecheckPattern::kHazardWar:
            return "hazard_war";
        case NpusanRacecheckPattern::kHazardWaw:
            return "hazard_waw";
        case NpusanRacecheckPattern::kAtomicRace:
            return "atomic_race";
        case NpusanRacecheckPattern::kCrossPipeRace:
            return "cross_pipe_race";
        case NpusanRacecheckPattern::kInterCoreRace:
            return "inter_core_race";
        case NpusanRacecheckPattern::kInvalidRemoteAccess:
            return "invalid_remote_access";
    }
    return "";
}

const char* SynccheckPatternName(std::uint32_t pattern)
{
    switch (static_cast<NpusanSynccheckPattern>(pattern)) {
        case NpusanSynccheckPattern::kIntraCoreDivergent:
            return "intra_core_divergent";
        case NpusanSynccheckPattern::kInterCoreDivergent:
            return "inter_core_divergent";
        case NpusanSynccheckPattern::kInvalidArgument:
            return "invalid_argument";
        case NpusanSynccheckPattern::kPairingMismatch:
            return "pairing_mismatch";
        case NpusanSynccheckPattern::kParticipantMismatch:
            return "participant_mismatch";
        case NpusanSynccheckPattern::kDeadlock:
            return "deadlock";
        case NpusanSynccheckPattern::kObjectNotInitialized:
            return "object_not_initialized";
        case NpusanSynccheckPattern::kInstructionSequenceMismatch:
            return "instruction_sequence_mismatch";
    }
    return "";
}

const char* SoccheckPatternName(std::uint32_t pattern)
{
    switch (static_cast<NpusanSoccheckPattern>(pattern)) {
        case NpusanSoccheckPattern::kUninitializedStateRead:
            return "uninitialized_state_read";
        case NpusanSoccheckPattern::kRegisterMismatch:
            return "register_mismatch";
        case NpusanSoccheckPattern::kIllegalStateTransition:
            return "illegal_state_transition";
        case NpusanSoccheckPattern::kStateNotRestored:
            return "state_not_restored";
        case NpusanSoccheckPattern::kCrossCoreStateInconsistent:
            return "cross_core_state_inconsistent";
        case NpusanSoccheckPattern::kScopeViolation:
            return "scope_violation";
    }
    return "";
}

std::string OrUnknown(const std::string& value) { return value.empty() ? "<unknown>" : value; }

std::string FormatCoreId(std::uint32_t coreId)
{
    return coreId == std::numeric_limits<std::uint32_t>::max() ? "<unknown>" : std::to_string(coreId);
}

std::string FormatLocation(const NpusanReportExecContext& exec, bool includeAt)
{
    std::ostringstream os;
    if (includeAt) {
        os << "at ";
    }
    if (!exec.function.empty() || !exec.file.empty()) {
        os << OrUnknown(exec.function) << "+0x" << Hex(exec.offset) << " in " << OrUnknown(exec.file) << ":"
           << exec.line;
    } else {
        os << "pc 0x" << Hex(exec.pc) << " in " << OrUnknown(exec.kernelName);
    }
    return os.str();
}

std::string FormatLocation(const ReportFrame& frame, bool includeAt)
{
    std::ostringstream os;
    if (includeAt) {
        os << "at ";
    }
    os << OrUnknown(frame.function) << "+0x" << Hex(frame.offset) << " in " << OrUnknown(frame.file) << ":"
       << frame.line;
    return os.str();
}

void PutExecFields(const NpusanReportExecContext& exec, ReportFields* fields)
{
    (*fields)["function"] = OrUnknown(exec.function);
    (*fields)["offset"] = Hex(exec.offset);
    (*fields)["file"] = OrUnknown(exec.file);
    (*fields)["line"] = std::to_string(exec.line);
    (*fields)["pc"] = Hex(exec.pc);
    (*fields)["kernelName"] = OrUnknown(exec.kernelName);
    (*fields)["coreId"] = FormatCoreId(exec.coreId);
    (*fields)["blockId"] = std::to_string(exec.blockId);
    (*fields)["pipeName"] = OrUnknown(exec.pipeName);
    (*fields)["launchId"] = std::to_string(exec.launchId);
    (*fields)["binaryId"] = std::to_string(exec.binaryId);
    (*fields)["functionId"] = std::to_string(exec.functionId);
    (*fields)["location"] = FormatLocation(exec, true);
}

void PutPrefixedExecFields(const NpusanReportExecContext& exec, const std::string& prefix, ReportFields* fields)
{
    (*fields)[prefix + "CoreId"] = FormatCoreId(exec.coreId);
    (*fields)[prefix + "Block"] = std::to_string(exec.blockId);
    (*fields)[prefix + "Pipe"] = OrUnknown(exec.pipeName);
    (*fields)[prefix + "Pc"] = Hex(exec.pc);
    (*fields)[prefix + "KernelName"] = OrUnknown(exec.kernelName);
    (*fields)[prefix + "Function"] = OrUnknown(exec.function);
    (*fields)[prefix + "Offset"] = Hex(exec.offset);
    (*fields)[prefix + "File"] = OrUnknown(exec.file);
    (*fields)[prefix + "Line"] = std::to_string(exec.line);
    (*fields)[prefix + "Location"] = FormatLocation(exec, false);
}

void PutAccessFields(const NpusanReportMemoryAccess& access, ReportFields* fields)
{
    (*fields)["space"] = MemorySpaceName(access.memorySpace);
    (*fields)["access"] = AccessModeName(access.accessMode);
    (*fields)["accessBytes"] = std::to_string(access.accessBytes);
    (*fields)["requiredAlign"] = std::to_string(access.requiredAlign);
    (*fields)["address"] = Hex(access.address);
}

void PutAllocationFields(const NpusanReportAllocation& allocation, ReportFields* fields)
{
    (*fields)["allocId"] = std::to_string(allocation.allocId);
    (*fields)["base"] = Hex(allocation.base);
    (*fields)["bytes"] = std::to_string(allocation.bytes);
    (*fields)["allocSerialNo"] = std::to_string(allocation.allocSerialNo);
    (*fields)["freeSerialNo"] = std::to_string(allocation.freeSerialNo);
}

void PutDefaultHostFields(ReportFields* fields)
{
    (*fields)["hostFunction"] = "<unknown>";
    (*fields)["hostPc"] = "0";
    (*fields)["hostBinary"] = "<unknown>";
    (*fields)["allocFunction"] = "<unknown>";
    (*fields)["allocPc"] = "0";
    (*fields)["freeFunction"] = "<unknown>";
    (*fields)["freePc"] = "0";
}

std::vector<ReportCallStack> ActiveCallStacks(const NpusanReportCommon& common)
{
    return std::vector<ReportCallStack>(common.stacks.begin(), common.stacks.begin() + common.stackCount);
}

const ReportCallStack* FindStackByRole(const NpusanReportCommon& common, ReportStackRole role)
{
    for (std::uint32_t i = 0; i < common.stackCount && i < common.stacks.size(); ++i) {
        if (common.stacks[i].role == role) {
            return &common.stacks[i];
        }
    }
    return nullptr;
}

const ReportFrame* FirstStructuredFrame(const NpusanReportCommon& common, ReportStackRole role)
{
    const ReportCallStack* stack = FindStackByRole(common, role);
    if (stack == nullptr ||
        (stack->format != ReportStackFormat::kFrames && stack->format != ReportStackFormat::kBoth) ||
        stack->frames.empty()) {
        return nullptr;
    }
    for (const ReportFrame& frame : stack->frames) {
        if (!frame.function.empty() || !frame.file.empty()) {
            return &frame;
        }
    }
    return nullptr;
}

void PutFrameLocationFields(const ReportFrame& frame, const std::string& prefix, ReportFields* fields)
{
    (*fields)[prefix + "Function"] = OrUnknown(frame.function);
    (*fields)[prefix + "Offset"] = Hex(frame.offset);
    (*fields)[prefix + "File"] = OrUnknown(frame.file);
    (*fields)[prefix + "Line"] = std::to_string(frame.line);
    (*fields)[prefix + "Location"] = FormatLocation(frame, false);
}

void PutSyncPointFields(
    const NpusanReportCommon& common, const NpusanSyncPoint& point, const std::string& prefix, ReportFields* fields)
{
    PutPrefixedExecFields(point.exec, prefix, fields);
    (*fields)[prefix + "Operation"] = OrUnknown(point.operation);
    if (point.stackRole != ReportStackRole::kNone) {
        if (const ReportFrame* frame = FirstStructuredFrame(common, point.stackRole)) {
            PutFrameLocationFields(*frame, prefix, fields);
        }
    }
}

void PutFaultLocationFields(const NpusanReportCommon& common, ReportFields* fields)
{
    const ReportFrame* frame = FirstStructuredFrame(common, ReportStackRole::kFaultDevice);
    if (frame == nullptr) {
        return;
    }
    (*fields)["function"] = OrUnknown(frame->function);
    (*fields)["offset"] = Hex(frame->offset);
    (*fields)["file"] = OrUnknown(frame->file);
    (*fields)["line"] = std::to_string(frame->line);
    (*fields)["location"] = FormatLocation(*frame, true);
}

bool ValidateReportCommon(const NpusanReportCommon& common, ReportTool expectedTool, std::uint32_t envelopePattern)
{
    if (common.tool != expectedTool || common.pattern != envelopePattern || common.stackCount > kNpusanReportStackMax) {
        return false;
    }

    for (std::uint32_t i = 0; i < common.stackCount; ++i) {
        const int role = static_cast<int>(common.stacks[i].role);
        const int format = static_cast<int>(common.stacks[i].format);
        if (role < static_cast<int>(ReportStackRole::kFaultDevice) ||
            role > static_cast<int>(ReportStackRole::kHostApiCall) ||
            format < static_cast<int>(ReportStackFormat::kNone) ||
            format > static_cast<int>(ReportStackFormat::kBoth) ||
            common.stacks[i].frames.size() > kNpusanReportFrameMax) {
            return false;
        }
        for (std::uint32_t j = i + 1; j < common.stackCount; ++j) {
            if (common.stacks[i].role == common.stacks[j].role) {
                return false;
            }
        }
    }
    return true;
}

bool ExecContextsEqual(const NpusanReportExecContext& lhs, const NpusanReportExecContext& rhs)
{
    return lhs.launchId == rhs.launchId && lhs.binaryId == rhs.binaryId && lhs.functionId == rhs.functionId &&
           lhs.instrExecId == rhs.instrExecId && lhs.serialNo == rhs.serialNo && lhs.pc == rhs.pc &&
           lhs.offset == rhs.offset && lhs.deviceId == rhs.deviceId && lhs.coreId == rhs.coreId &&
           lhs.blockId == rhs.blockId && lhs.blockType == rhs.blockType && lhs.pipeId == rhs.pipeId &&
           lhs.siteId == rhs.siteId && lhs.line == rhs.line && lhs.column == rhs.column &&
           lhs.function == rhs.function && lhs.file == rhs.file && lhs.pipeName == rhs.pipeName &&
           lhs.kernelName == rhs.kernelName;
}

bool HasStackRole(const NpusanReportCommon& common, ReportStackRole role)
{
    return FindStackByRole(common, role) != nullptr;
}

bool ValidatePointStackReference(
    const NpusanReportCommon& common, const NpusanSyncPoint& point, ReportStackRole expectedRole)
{
    const bool hasStack = HasStackRole(common, expectedRole);
    if (point.stackRole == ReportStackRole::kNone) {
        return !hasStack;
    }
    if (!point.hasExecContext || point.stackRole != expectedRole || !hasStack) {
        return false;
    }
    const ReportFrame* frame = FirstStructuredFrame(common, expectedRole);
    return frame == nullptr || frame->pc == 0 || point.exec.pc == 0 || frame->pc == point.exec.pc;
}

bool IsExpectedPoint(const NpusanSyncPoint& point)
{
    return !point.hasExecContext && point.stackRole == ReportStackRole::kNone && !point.operation.empty() &&
           ExecContextsEqual(point.exec, NpusanReportExecContext{});
}

bool IsActualPoint(const NpusanSyncPoint& point) { return point.hasExecContext && !point.operation.empty(); }

bool IsEmptyPoint(const NpusanSyncPoint& point)
{
    return !point.hasExecContext && point.operation.empty() && point.stackRole == ReportStackRole::kNone &&
           ExecContextsEqual(point.exec, NpusanReportExecContext{});
}

const char* PairKindName(NpusanSyncPairKind pairKind)
{
    switch (pairKind) {
        case NpusanSyncPairKind::kSetWaitFlag:
            return "SET_WAIT_FLAG";
        case NpusanSyncPairKind::kGetRlsBuf:
            return "GET_RLS_BUF";
        case NpusanSyncPairKind::kUnknown:
            return "UNKNOWN";
    }
    return "UNKNOWN";
}

const char* PrimitiveKindName(NpusanSyncPrimitiveKind primitiveKind)
{
    switch (primitiveKind) {
        case NpusanSyncPrimitiveKind::kBarrier:
            return "BARRIER";
        case NpusanSyncPrimitiveKind::kSetWaitFlag:
            return "SET_WAIT_FLAG";
        case NpusanSyncPrimitiveKind::kGetRlsBuf:
            return "GET_RLS_BUF";
        case NpusanSyncPrimitiveKind::kInstructionSequence:
            return "INSTRUCTION_SEQUENCE";
        case NpusanSyncPrimitiveKind::kSyncObject:
            return "SYNC_OBJECT";
        case NpusanSyncPrimitiveKind::kUnknown:
            return "UNKNOWN";
    }
    return "UNKNOWN";
}

bool IsValidPrimitiveKind(NpusanSyncPrimitiveKind primitiveKind)
{
    const int value = static_cast<int>(primitiveKind);
    return value >= static_cast<int>(NpusanSyncPrimitiveKind::kBarrier) &&
           value <= static_cast<int>(NpusanSyncPrimitiveKind::kSyncObject);
}

const char* OpenOperation(NpusanSyncPairKind pairKind)
{
    return pairKind == NpusanSyncPairKind::kSetWaitFlag ? "SET_FLAG" : "GET_BUF";
}

const char* CloseOperation(NpusanSyncPairKind pairKind)
{
    return pairKind == NpusanSyncPairKind::kSetWaitFlag ? "WAIT_FLAG" : "RLS_BUF";
}

bool ValidatePairingReport(const NpusanSynccheckReport& report, const NpusanSyncPairingError& detail)
{
    if (!report.hasRelatedPoint || detail.reason == NpusanSyncMismatchReason::kUnknown ||
        (detail.key.pairKind != NpusanSyncPairKind::kSetWaitFlag &&
         detail.key.pairKind != NpusanSyncPairKind::kGetRlsBuf)) {
        return false;
    }
    if ((detail.key.pairKind == NpusanSyncPairKind::kSetWaitFlag && detail.key.mode != 0) ||
        (detail.key.pairKind == NpusanSyncPairKind::kGetRlsBuf &&
         (detail.key.srcPipe != 0 || detail.key.dstPipe != 0))) {
        return false;
    }
    if ((detail.key.pairKind == NpusanSyncPairKind::kSetWaitFlag &&
         report.primitiveKind != NpusanSyncPrimitiveKind::kSetWaitFlag) ||
        (detail.key.pairKind == NpusanSyncPairKind::kGetRlsBuf &&
         report.primitiveKind != NpusanSyncPrimitiveKind::kGetRlsBuf)) {
        return false;
    }

    const std::string open = OpenOperation(detail.key.pairKind);
    const std::string close = CloseOperation(detail.key.pairKind);
    switch (detail.reason) {
        case NpusanSyncMismatchReason::kDuplicateOpen:
            return IsActualPoint(report.relatedPoint) && report.triggerPoint.operation == open &&
                   report.relatedPoint.operation == open;
        case NpusanSyncMismatchReason::kUnmatchedClose:
            return IsExpectedPoint(report.relatedPoint) && report.triggerPoint.operation == close &&
                   report.relatedPoint.operation == open;
        case NpusanSyncMismatchReason::kUnconsumedOpen:
            return IsExpectedPoint(report.relatedPoint) && report.triggerPoint.operation == open &&
                   report.relatedPoint.operation == close;
        case NpusanSyncMismatchReason::kUnknown:
            return false;
    }
    return false;
}

bool ValidateSynccheckReport(const NpusanSynccheckReport& report)
{
    if ((report.common.flags & kNpusanReportCommonHasExecContext) == 0 || !IsActualPoint(report.triggerPoint) ||
        !ExecContextsEqual(report.common.exec, report.triggerPoint.exec) ||
        !ValidatePointStackReference(report.common, report.triggerPoint, ReportStackRole::kSyncTrigger) ||
        !ValidatePointStackReference(report.common, report.relatedPoint, ReportStackRole::kSyncRelated)) {
        return false;
    }
    if ((!report.hasRelatedPoint && !IsEmptyPoint(report.relatedPoint)) ||
        (report.hasRelatedPoint && report.relatedPoint.operation.empty())) {
        return false;
    }

    for (std::uint32_t i = 0; i < report.common.stackCount; ++i) {
        const ReportStackRole role = report.common.stacks[i].role;
        if (role == ReportStackRole::kSyncProducer || role == ReportStackRole::kSyncConsumer) {
            return false;
        }
    }

    const auto pattern = static_cast<NpusanSynccheckPattern>(report.common.pattern);
    switch (pattern) {
        case NpusanSynccheckPattern::kIntraCoreDivergent:
            return report.detailKind == NpusanSyncDetailKind::kBarrier && !report.hasRelatedPoint &&
                   report.primitiveKind == NpusanSyncPrimitiveKind::kBarrier &&
                   std::holds_alternative<NpusanSyncBarrierError>(report.detail);
        case NpusanSynccheckPattern::kInterCoreDivergent:
        case NpusanSynccheckPattern::kParticipantMismatch:
            return report.detailKind == NpusanSyncDetailKind::kBarrier &&
                   report.primitiveKind == NpusanSyncPrimitiveKind::kBarrier &&
                   std::holds_alternative<NpusanSyncBarrierError>(report.detail) &&
                   (!report.hasRelatedPoint || IsActualPoint(report.relatedPoint) ||
                    IsExpectedPoint(report.relatedPoint));
        case NpusanSynccheckPattern::kInvalidArgument:
        case NpusanSynccheckPattern::kObjectNotInitialized:
            return report.detailKind == NpusanSyncDetailKind::kObject && !report.hasRelatedPoint &&
                   IsValidPrimitiveKind(report.primitiveKind) &&
                   std::holds_alternative<NpusanSyncObjectError>(report.detail);
        case NpusanSynccheckPattern::kPairingMismatch: {
            if (report.detailKind != NpusanSyncDetailKind::kPairing ||
                !std::holds_alternative<NpusanSyncPairingError>(report.detail)) {
                return false;
            }
            return ValidatePairingReport(report, std::get<NpusanSyncPairingError>(report.detail));
        }
        case NpusanSynccheckPattern::kDeadlock:
            return report.detailKind == NpusanSyncDetailKind::kObject && IsValidPrimitiveKind(report.primitiveKind) &&
                   std::holds_alternative<NpusanSyncObjectError>(report.detail) &&
                   (!report.hasRelatedPoint || IsActualPoint(report.relatedPoint) ||
                    IsExpectedPoint(report.relatedPoint));
        case NpusanSynccheckPattern::kInstructionSequenceMismatch:
            return report.detailKind == NpusanSyncDetailKind::kSequence && report.hasRelatedPoint &&
                   report.primitiveKind == NpusanSyncPrimitiveKind::kInstructionSequence &&
                   std::holds_alternative<NpusanSyncSequenceError>(report.detail) &&
                   (IsActualPoint(report.relatedPoint) || IsExpectedPoint(report.relatedPoint));
    }
    return false;
}

ReportRecord ToReportRecord(const NpusanMemcheckReport& report)
{
    ReportFields fields;
    PutExecFields(report.common.exec, &fields);
    PutFaultLocationFields(report.common, &fields);
    PutAccessFields(report.access, &fields);
    PutAllocationFields(report.allocation, &fields);
    PutDefaultHostFields(&fields);
    fields["distanceBytes"] = std::to_string(report.distanceBytes);
    fields["before|after"] = DistanceDirectionName(report.distanceKind);
    fields["apiName"] = report.apiName;
    fields["apiErrorName"] = report.apiErrorName;
    fields["apiErrorCode"] = std::to_string(report.apiErrorCode);
    fields["apiErrorMessage"] = report.apiErrorMessage;
    if (report.common.pattern == static_cast<std::uint32_t>(NpusanMemcheckPattern::kInvalidAccess)) {
        fields["base"] = Hex(report.nearestAllocation.base);
        fields["bytes"] = std::to_string(report.nearestAllocation.bytes);
    } else if (report.common.pattern == static_cast<std::uint32_t>(NpusanMemcheckPattern::kLeak)) {
        fields["space"] = MemorySpaceName(report.allocation.memorySpace);
    }
    return ReportRecord{
        {ReportTool::kMemcheck, MemcheckPatternName(report.common.pattern)},
        report.common.severity,
        fields,
        ActiveCallStacks(report.common)};
}

ReportRecord ToReportRecord(const NpusanInitcheckReport& report)
{
    ReportFields fields;
    PutExecFields(report.common.exec, &fields);
    PutFaultLocationFields(report.common, &fields);
    PutAccessFields(report.access, &fields);
    PutAllocationFields(report.allocation, &fields);
    PutDefaultHostFields(&fields);
    fields["firstUninitAddress"] = Hex(report.firstUninitAddress);
    fields["firstUninitOffset"] = Hex(report.firstUninitOffset);
    fields["uninitBytes"] = std::to_string(report.uninitBytes);
    fields["initializedBytes"] = std::to_string(report.initializedBytes);
    fields["unusedBytes"] = std::to_string(report.unusedBytes);
    fields["unusedPercent"] = std::to_string(report.unusedPercent);
    if (report.common.pattern == static_cast<std::uint32_t>(NpusanInitcheckPattern::kUnusedMemory)) {
        fields["space"] = MemorySpaceName(report.allocation.memorySpace);
    }
    return ReportRecord{
        {ReportTool::kInitcheck, InitcheckPatternName(report.common.pattern)},
        report.common.severity,
        fields,
        ActiveCallStacks(report.common)};
}

ReportRecord ToReportRecord(const NpusanRacecheckReport& report)
{
    ReportFields fields;
    PutExecFields(report.common.exec, &fields);
    PutFaultLocationFields(report.common, &fields);
    PutAccessFields(report.first.access, &fields);
    fields["blockId"] = std::to_string(report.first.exec.blockId);
    fields["firstAccess"] = AccessModeName(report.first.access.accessMode);
    fields["secondAccess"] = AccessModeName(report.second.access.accessMode);
    fields["firstCoreId"] = FormatCoreId(report.first.exec.coreId);
    fields["firstBlock"] = std::to_string(report.first.exec.blockId);
    fields["firstPipe"] = OrUnknown(report.first.exec.pipeName);
    fields["firstFunction"] = OrUnknown(report.first.exec.function);
    fields["firstOffset"] = Hex(report.first.exec.offset);
    fields["firstFile"] = OrUnknown(report.first.exec.file);
    fields["firstLine"] = std::to_string(report.first.exec.line);
    fields["firstLocation"] = FormatLocation(report.first.exec, false);
    fields["secondCoreId"] = FormatCoreId(report.second.exec.coreId);
    fields["secondBlock"] = std::to_string(report.second.exec.blockId);
    fields["secondPipe"] = OrUnknown(report.second.exec.pipeName);
    fields["secondFunction"] = OrUnknown(report.second.exec.function);
    fields["secondOffset"] = Hex(report.second.exec.offset);
    fields["secondFile"] = OrUnknown(report.second.exec.file);
    fields["secondLine"] = std::to_string(report.second.exec.line);
    fields["secondLocation"] = FormatLocation(report.second.exec, false);
    if (const ReportFrame* frame = FirstStructuredFrame(report.common, ReportStackRole::kRelatedAccessA)) {
        PutFrameLocationFields(*frame, "first", &fields);
    }
    if (const ReportFrame* frame = FirstStructuredFrame(report.common, ReportStackRole::kRelatedAccessB)) {
        PutFrameLocationFields(*frame, "second", &fields);
    }
    if (report.common.pattern == static_cast<std::uint32_t>(NpusanRacecheckPattern::kCrossPipeRace)) {
        fields["coreId"] = FormatCoreId(report.first.exec.coreId);
    } else if (report.common.pattern == static_cast<std::uint32_t>(NpusanRacecheckPattern::kInvalidRemoteAccess)) {
        fields["location"] = "at " + fields["firstLocation"];
        fields["coreId"] = FormatCoreId(report.first.exec.coreId);
        fields["blockId"] = std::to_string(report.first.exec.blockId);
        fields["pipeName"] = OrUnknown(report.first.exec.pipeName);
    }
    fields["hazardCount"] = std::to_string(report.hazardCount);
    fields["currentValue"] = HexWithPrefix(report.currentValue);
    fields["incomingValue"] = HexWithPrefix(report.incomingValue);
    fields["scope"] = std::to_string(report.scope);
    fields["remoteState"] = "is unavailable";
    return ReportRecord{
        {ReportTool::kRacecheck, RacecheckPatternName(report.common.pattern)},
        report.common.severity,
        fields,
        ActiveCallStacks(report.common)};
}

std::string FormatPairKey(const NpusanSyncPairKey& key)
{
    std::ostringstream os;
    if (key.pairKind == NpusanSyncPairKind::kSetWaitFlag) {
        os << "srcPipe=pipe(" << key.srcPipe << "), dstPipe=pipe(" << key.dstPipe << "), id=" << key.id;
    } else {
        os << "id=" << key.id << ", mode=" << key.mode;
    }
    return os.str();
}

std::string SyncObjectLine(std::uint64_t objectId, std::uint64_t address)
{
    if (objectId == 0 && address == 0) {
        return "";
    }
    std::ostringstream os;
    os << "=========     sync object";
    if (objectId != 0) {
        os << " 0x" << Hex(objectId);
    }
    if (address != 0) {
        os << (objectId == 0 ? " address" : ", address") << " 0x" << Hex(address);
    }
    os << '\n';
    return os.str();
}

std::string ActualRelatedPointLine(const NpusanSyncPoint& point, const ReportFields& fields)
{
    std::ostringstream os;
    os << "=========     related point: " << OrUnknown(point.operation) << " by aicore ("
       << FieldOr(fields, "relatedCoreId", "<unknown>") << ") block (" << FieldOr(fields, "relatedBlock", "0")
       << ") pipe (" << FieldOr(fields, "relatedPipe", "<unknown>") << ") at "
       << FieldOr(fields, "relatedLocation", "pc 0x0 in <unknown>") << '\n';
    return os.str();
}

ReportRecord ToReportRecord(const NpusanSynccheckReport& report)
{
    ReportFields fields;
    PutExecFields(report.common.exec, &fields);
    PutSyncPointFields(report.common, report.triggerPoint, "trigger", &fields);
    PutSyncPointFields(report.common, report.relatedPoint, "related", &fields);
    fields["primitiveKind"] = PrimitiveKindName(report.primitiveKind);
    fields["relatedPointLine"] = "";
    fields["expectedOperationLine"] = "";
    fields["objectLine"] = "";
    if (report.hasRelatedPoint && report.relatedPoint.hasExecContext) {
        fields["relatedPointLine"] = ActualRelatedPointLine(report.relatedPoint, fields);
    } else if (report.hasRelatedPoint) {
        fields["relatedPointLine"] = "=========     related point: expected " +
                                     OrUnknown(report.relatedPoint.operation) + ", but no runtime point was observed\n";
    }

    switch (report.detailKind) {
        case NpusanSyncDetailKind::kBarrier: {
            const auto& detail = std::get<NpusanSyncBarrierError>(report.detail);
            fields["reason"] = OrUnknown(detail.reason);
            fields["scope"] = OrUnknown(detail.scope);
            fields["activeMask"] = Hex(detail.activeMask);
            fields["expectedMask"] = Hex(detail.expectedMask);
            fields["objectLine"] = SyncObjectLine(detail.objectId, 0);
            break;
        }
        case NpusanSyncDetailKind::kPairing: {
            const auto& detail = std::get<NpusanSyncPairingError>(report.detail);
            fields["pairKind"] = PairKindName(detail.key.pairKind);
            fields["pairKey"] = FormatPairKey(detail.key);
            const char* open = OpenOperation(detail.key.pairKind);
            const char* close = CloseOperation(detail.key.pairKind);
            if (detail.reason == NpusanSyncMismatchReason::kDuplicateOpen) {
                fields["reasonText"] = "duplicate";
                fields["relatedPointLine"] = "=========     related point: previous " + report.relatedPoint.operation +
                                             " at " + fields["relatedLocation"] + " is still pending\n";
                fields["expectedOperationLine"] =
                    "=========     expected " + std::string(close) + " before another " + open + "\n";
            } else if (detail.reason == NpusanSyncMismatchReason::kUnmatchedClose) {
                fields["reasonText"] = "unmatched";
                fields["relatedPointLine"] = "=========     related point: expected " + std::string(open) +
                                             ", but no matching point exists for this pair key\n";
            } else {
                fields["reasonText"] = "redundant";
                fields["relatedPointLine"] = "=========     related point: expected " + std::string(close) +
                                             ", but no matching point was observed\n";
            }
            break;
        }
        case NpusanSyncDetailKind::kSequence: {
            const auto& detail = std::get<NpusanSyncSequenceError>(report.detail);
            fields["reason"] = OrUnknown(detail.reason);
            fields["sequenceIndex"] = std::to_string(detail.sequenceIndex);
            fields["activeMask"] = Hex(detail.activeMask);
            break;
        }
        case NpusanSyncDetailKind::kObject: {
            const auto& detail = std::get<NpusanSyncObjectError>(report.detail);
            fields["reason"] = OrUnknown(detail.reason);
            fields["waitingMask"] = Hex(detail.waitingMask);
            fields["timeoutNs"] = std::to_string(detail.timeoutNs);
            fields["objectLine"] = SyncObjectLine(detail.objectId, detail.address);
            break;
        }
    }
    return ReportRecord{
        {ReportTool::kSynccheck, SynccheckPatternName(report.common.pattern)},
        report.common.severity,
        fields,
        ActiveCallStacks(report.common)};
}

ReportRecord ToReportRecord(const NpusanSoccheckReport& report)
{
    ReportFields fields;
    PutExecFields(report.common.exec, &fields);
    PutFaultLocationFields(report.common, &fields);
    PutPrefixedExecFields(report.producer, "producer", &fields);
    PutPrefixedExecFields(report.consumer, "consumer", &fields);
    fields["stateKind"] = std::to_string(report.state.stateKind);
    fields["scope"] = std::to_string(report.state.scope);
    fields["registerId"] = std::to_string(report.state.registerId);
    fields["ownerCoreId"] = FormatCoreId(report.state.ownerCoreId);
    fields["stateId"] = std::to_string(report.state.stateId);
    fields["oldValue"] = Hex(report.state.oldValue);
    fields["newValue"] = Hex(report.state.newValue);
    fields["expectedValue"] = Hex(report.state.expectedValue);
    fields["observedValue"] = Hex(report.state.observedValue);
    return ReportRecord{
        {ReportTool::kSoccheck, SoccheckPatternName(report.common.pattern)},
        report.common.severity,
        fields,
        ActiveCallStacks(report.common)};
}

template <typename Report>
const Report* GetReportPayload(const NpusanReportRecord& record)
{
    const auto* payload = std::get_if<const Report*>(&record.GetPayload());
    return payload == nullptr ? nullptr : *payload;
}

ReportRenderStatus ToReportRecord(const NpusanReportRecord& record, ReportRecord* out)
{
    if (out == nullptr) {
        return ReportRenderStatus::kInvalidArgument;
    }
    switch (record.tool) {
        case ReportTool::kMemcheck: {
            const NpusanMemcheckReport* report = GetReportPayload<NpusanMemcheckReport>(record);
            if (report == nullptr || !ValidateReportCommon(report->common, ReportTool::kMemcheck, record.pattern)) {
                return ReportRenderStatus::kInvalidArgument;
            }
            *out = ToReportRecord(*report);
            return ReportRenderStatus::kSuccess;
        }
        case ReportTool::kInitcheck: {
            const NpusanInitcheckReport* report = GetReportPayload<NpusanInitcheckReport>(record);
            if (report == nullptr || !ValidateReportCommon(report->common, ReportTool::kInitcheck, record.pattern)) {
                return ReportRenderStatus::kInvalidArgument;
            }
            *out = ToReportRecord(*report);
            return ReportRenderStatus::kSuccess;
        }
        case ReportTool::kRacecheck: {
            const NpusanRacecheckReport* report = GetReportPayload<NpusanRacecheckReport>(record);
            if (report == nullptr || !ValidateReportCommon(report->common, ReportTool::kRacecheck, record.pattern) ||
                (report->common.pattern == static_cast<std::uint32_t>(NpusanRacecheckPattern::kCrossPipeRace) &&
                 report->first.exec.coreId != report->second.exec.coreId)) {
                return ReportRenderStatus::kInvalidArgument;
            }
            *out = ToReportRecord(*report);
            return ReportRenderStatus::kSuccess;
        }
        case ReportTool::kSynccheck: {
            const NpusanSynccheckReport* report = GetReportPayload<NpusanSynccheckReport>(record);
            if (report == nullptr || !ValidateReportCommon(report->common, ReportTool::kSynccheck, record.pattern) ||
                !ValidateSynccheckReport(*report)) {
                return ReportRenderStatus::kInvalidArgument;
            }
            *out = ToReportRecord(*report);
            return ReportRenderStatus::kSuccess;
        }
        case ReportTool::kSoccheck: {
            const NpusanSoccheckReport* report = GetReportPayload<NpusanSoccheckReport>(record);
            if (report == nullptr || !ValidateReportCommon(report->common, ReportTool::kSoccheck, record.pattern)) {
                return ReportRenderStatus::kInvalidArgument;
            }
            *out = ToReportRecord(*report);
            return ReportRenderStatus::kSuccess;
        }
    }
    return ReportRenderStatus::kUnknownTemplate;
}

bool StartsWithReportPrefix(const std::string& line) { return line.rfind("=========", 0) == 0; }

bool HasRawStack(ReportStackFormat format)
{
    return format == ReportStackFormat::kRawText || format == ReportStackFormat::kBoth;
}

bool HasFrameStack(ReportStackFormat format)
{
    return format == ReportStackFormat::kFrames || format == ReportStackFormat::kBoth;
}

bool IsHostStackRole(ReportStackRole role)
{
    return role == ReportStackRole::kHostLaunch || role == ReportStackRole::kHostAlloc ||
           role == ReportStackRole::kHostFree || role == ReportStackRole::kHostApiCall;
}

const char* FrameLabel(ReportStackRole role) { return IsHostStackRole(role) ? "Host Frame" : "Device Frame"; }

void EnsureTrailingNewline(std::string* out)
{
    if (out != nullptr && !out->empty() && out->back() != '\n') {
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
        if (StartsWithReportPrefix(line)) {
            out->append(line);
        } else {
            out->append("=========         ");
            out->append(line);
        }
        out->push_back('\n');
    }
}

void AppendFrame(const ReportFrame& frame, ReportStackRole role, std::string* out)
{
    out->append("=========         ");
    out->append(FrameLabel(role));
    out->append(": ");
    out->append(frame.function.empty() ? "<unknown>" : frame.function);
    out->append("+0x");
    out->append(Hex(frame.offset));
    out->append(" [0x");
    out->append(Hex(frame.pc));
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

void AppendCallStack(const ReportCallStack& stack, std::string* out)
{
    const bool shouldRenderRaw = HasRawStack(stack.format) && !stack.rawText.empty();
    const bool shouldRenderFrames = HasFrameStack(stack.format) && !stack.frames.empty();
    if (!shouldRenderRaw && !shouldRenderFrames) {
        return;
    }

    EnsureTrailingNewline(out);
    out->append("=========     ");
    out->append(ReportStackRoleTitle(stack.role));
    out->push_back('\n');

    if (shouldRenderRaw) {
        AppendRawStackText(stack.rawText, out);
    }
    if (shouldRenderFrames) {
        for (const ReportFrame& frame : stack.frames) {
            AppendFrame(frame, stack.role, out);
        }
    }
}

void AppendCallStacks(const std::vector<ReportCallStack>& stacks, std::string* out)
{
    for (const ReportCallStack& stack : stacks) {
        AppendCallStack(stack, out);
    }
}

bool IsErrorSeverity(ReportSeverity severity)
{
    return severity == ReportSeverity::kError || severity == ReportSeverity::kFatal;
}

bool IsPattern(const ReportRecord& record, ReportTool tool, const char* pattern)
{
    return record.key.tool == tool && record.key.pattern == pattern;
}

struct ToolSummary {
    std::uint64_t errors = 0;
    std::uint64_t warnings = 0;
    std::uint64_t infos = 0;
    std::uint64_t leaks = 0;
    std::uint64_t unused = 0;
    std::uint64_t hazards = 0;
    std::uint64_t deadlocks = 0;
};

std::size_t ToolSummaryIndex(ReportTool tool)
{
    const int value = static_cast<int>(tool);
    if (value < static_cast<int>(ReportTool::kMemcheck) || value > static_cast<int>(ReportTool::kSoccheck)) {
        return 0;
    }
    return static_cast<std::size_t>(value);
}

void AccumulateSummary(const ReportRecord& record, std::array<ToolSummary, 6>* summaries)
{
    ToolSummary& summary = (*summaries)[ToolSummaryIndex(record.key.tool)];
    if (IsErrorSeverity(record.severity)) {
        ++summary.errors;
    } else if (record.severity == ReportSeverity::kWarning) {
        ++summary.warnings;
    } else if (record.severity == ReportSeverity::kInfo) {
        ++summary.infos;
    }

    if (IsPattern(record, ReportTool::kMemcheck, "leak")) {
        ++summary.leaks;
    }
    if (IsPattern(record, ReportTool::kInitcheck, "unused_memory")) {
        ++summary.unused;
    }
    if (record.key.tool == ReportTool::kRacecheck && record.key.pattern != "analysis") {
        ++summary.hazards;
    }
    if (IsPattern(record, ReportTool::kSynccheck, "deadlock")) {
        ++summary.deadlocks;
    }
}

void AppendToolSummaries(const std::array<ToolSummary, 6>& summaries, std::string* out)
{
    const ToolSummary& memcheck = summaries[ToolSummaryIndex(ReportTool::kMemcheck)];
    out->append("========= MEMCHECK SUMMARY: ");
    out->append(std::to_string(memcheck.errors));
    out->append(" errors, ");
    out->append(std::to_string(memcheck.warnings));
    out->append(" warnings, ");
    out->append(std::to_string(memcheck.infos));
    out->append(" infos, ");
    out->append(std::to_string(memcheck.leaks));
    out->append(" leaks\n");

    const ToolSummary& initcheck = summaries[ToolSummaryIndex(ReportTool::kInitcheck)];
    out->append("========= INITCHECK SUMMARY: ");
    out->append(std::to_string(initcheck.errors));
    out->append(" errors, ");
    out->append(std::to_string(initcheck.warnings));
    out->append(" warnings, ");
    out->append(std::to_string(initcheck.infos));
    out->append(" infos, ");
    out->append(std::to_string(initcheck.unused));
    out->append(" unused memory reports\n");

    const ToolSummary& racecheck = summaries[ToolSummaryIndex(ReportTool::kRacecheck)];
    out->append("========= RACECHECK SUMMARY: ");
    out->append(std::to_string(racecheck.hazards));
    out->append(" hazard displayed (");
    out->append(std::to_string(racecheck.errors));
    out->append(" errors, ");
    out->append(std::to_string(racecheck.warnings));
    out->append(" warnings, ");
    out->append(std::to_string(racecheck.infos));
    out->append(" infos)\n");

    const ToolSummary& synccheck = summaries[ToolSummaryIndex(ReportTool::kSynccheck)];
    out->append("========= SYNCCHECK SUMMARY: ");
    out->append(std::to_string(synccheck.errors));
    out->append(" errors, ");
    out->append(std::to_string(synccheck.warnings));
    out->append(" warnings, ");
    out->append(std::to_string(synccheck.infos));
    out->append(" infos, ");
    out->append(std::to_string(synccheck.deadlocks));
    out->append(" deadlocks\n");

    const ToolSummary& soccheck = summaries[ToolSummaryIndex(ReportTool::kSoccheck)];
    out->append("========= SOCCHECK SUMMARY: ");
    out->append(std::to_string(soccheck.errors));
    out->append(" errors, ");
    out->append(std::to_string(soccheck.warnings));
    out->append(" warnings, ");
    out->append(std::to_string(soccheck.infos));
    out->append(" infos\n");
}

void AppendGlobalSummary(const std::array<ToolSummary, 6>& summaries, std::uint64_t fatalCount, std::string* out)
{
    const ToolSummary& memcheck = summaries[ToolSummaryIndex(ReportTool::kMemcheck)];
    const ToolSummary& initcheck = summaries[ToolSummaryIndex(ReportTool::kInitcheck)];
    const ToolSummary& racecheck = summaries[ToolSummaryIndex(ReportTool::kRacecheck)];
    const ToolSummary& synccheck = summaries[ToolSummaryIndex(ReportTool::kSynccheck)];
    const ToolSummary& soccheck = summaries[ToolSummaryIndex(ReportTool::kSoccheck)];
    const std::uint64_t totalErrorCount =
        memcheck.errors + initcheck.errors + racecheck.errors + synccheck.errors + soccheck.errors;

    out->append("========= ERROR SUMMARY: ");
    out->append(std::to_string(totalErrorCount));
    out->append(" errors\n");
    out->append("=========     MEMCHECK: ");
    out->append(std::to_string(memcheck.errors));
    out->append(" errors\n");
    out->append("=========     INITCHECK: ");
    out->append(std::to_string(initcheck.errors));
    out->append(" errors\n");
    out->append("=========     RACECHECK: ");
    out->append(std::to_string(racecheck.errors));
    out->append(" errors\n");
    out->append("=========     SYNCCHECK: ");
    out->append(std::to_string(synccheck.errors));
    out->append(" errors\n");
    out->append("=========     SOCCHECK: ");
    out->append(std::to_string(soccheck.errors));
    out->append(" errors\n");
    out->append("=========     FATAL: ");
    out->append(std::to_string(fatalCount));
    out->append(" fatal errors\n");
}

} // namespace

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

    const ReportTemplate* tpl = nullptr;
    const auto overrideIt = overrides.find(record.key);
    if (overrideIt != overrides.end()) {
        tpl = &overrideIt->second;
    } else {
        tpl = FindBuiltinReportTemplate(record.key);
    }
    if (tpl == nullptr) {
        out->clear();
        return ReportRenderStatus::kUnknownTemplate;
    }

    ReportFields fields = record.fields;
    PutDerivedLocations(&fields);
    fields["Severity"] = ReportSeverityName(record.severity);
    fields["tool"] = ReportToolName(record.key.tool);
    fields["pattern"] = record.key.pattern;

    ReportRenderStatus status = RenderReportText(*tpl, fields, out);
    if (status != ReportRenderStatus::kSuccess) {
        return status;
    }
    AppendCallStacks(record.stacks, out);
    return ReportRenderStatus::kSuccess;
}

ReportRenderStatus RenderReportBundle(
    const std::vector<ReportRecord>& records, const ReportTemplateOverrides& overrides, std::string* out)
{
    if (out == nullptr) {
        return ReportRenderStatus::kInvalidArgument;
    }

    std::array<ToolSummary, 6> summaries{};
    std::uint64_t fatalCount = 0;

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
        AccumulateSummary(record, &summaries);
        if (record.severity == ReportSeverity::kFatal) {
            ++fatalCount;
        }
    }

    AppendToolSummaries(summaries, out);
    AppendGlobalSummary(summaries, fatalCount, out);
    return ReportRenderStatus::kSuccess;
}

ReportRenderStatus RenderNpusanReportRecord(
    const NpusanReportRecord& record, const ReportTemplateOverrides& overrides, std::string* out)
{
    ReportRecord templateRecord{};
    const ReportRenderStatus status = ToReportRecord(record, &templateRecord);
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
        const ReportRenderStatus status = ToReportRecord(record, &templateRecord);
        if (status != ReportRenderStatus::kSuccess) {
            out->clear();
            return status;
        }
        templateRecords.push_back(std::move(templateRecord));
    }
    return RenderReportBundle(templateRecords, overrides, out);
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
        ReportRenderStatus status = ParseTemplateKey(Trim(line.substr(0, eq)), &key);
        if (status != ReportRenderStatus::kSuccess) {
            return status;
        }

        (*overrides)[key] = ReportTemplate{UnescapeTemplateText(line.substr(eq + 1))};
    }
    return ReportRenderStatus::kSuccess;
}

const ReportTemplate* FindBuiltinReportTemplate(const ReportTemplateKey& key)
{
    for (const auto& entry : BuiltinTemplates()) {
        if (entry.first == key) {
            return &entry.second;
        }
    }
    return nullptr;
}

std::vector<ReportTemplateKey> ListBuiltinReportTemplates()
{
    std::vector<ReportTemplateKey> keys;
    keys.reserve(BuiltinTemplates().size());
    for (const auto& entry : BuiltinTemplates()) {
        keys.push_back(entry.first);
    }
    return keys;
}

ReportSeverity DefaultReportSeverity(ReportTool tool, const std::string& pattern)
{
    switch (tool) {
        case ReportTool::kMemcheck:
            return ReportSeverity::kError;
        case ReportTool::kInitcheck:
            if (pattern == "unused_memory") {
                return ReportSeverity::kWarning;
            }
            return ReportSeverity::kError;
        case ReportTool::kRacecheck:
            if (pattern == "cross_pipe_race" || pattern == "inter_core_race") {
                return ReportSeverity::kError;
            }
            if (pattern == "analysis" || pattern == "hazard_raw" || pattern == "hazard_war" ||
                pattern == "hazard_waw" || pattern == "atomic_race" || pattern == "invalid_remote_access") {
                return ReportSeverity::kWarning;
            }
            return ReportSeverity::kError;
        case ReportTool::kSynccheck:
            if (pattern == "participant_mismatch") {
                return ReportSeverity::kWarning;
            }
            return ReportSeverity::kError;
        case ReportTool::kSoccheck:
            if (pattern == "state_not_restored") {
                return ReportSeverity::kWarning;
            }
            return ReportSeverity::kError;
    }
    return ReportSeverity::kError;
}

const char* ReportToolName(ReportTool tool)
{
    switch (tool) {
        case ReportTool::kMemcheck:
            return "memcheck";
        case ReportTool::kInitcheck:
            return "initcheck";
        case ReportTool::kRacecheck:
            return "racecheck";
        case ReportTool::kSynccheck:
            return "synccheck";
        case ReportTool::kSoccheck:
            return "soccheck";
    }
    return "unknown";
}

const char* ReportSeverityName(ReportSeverity severity)
{
    switch (severity) {
        case ReportSeverity::kInfo:
            return "INFO";
        case ReportSeverity::kWarning:
            return "WARNING";
        case ReportSeverity::kError:
            return "ERROR";
        case ReportSeverity::kFatal:
            return "FATAL";
    }
    return "ERROR";
}

const char* ReportStackRoleTitle(ReportStackRole role)
{
    switch (role) {
        case ReportStackRole::kNone:
            return "Device Frame";
        case ReportStackRole::kFaultDevice:
            return "Device Frame";
        case ReportStackRole::kHostLaunch:
            return "Saved host backtrace up to runtime launch entry point";
        case ReportStackRole::kHostAlloc:
            return "Saved host backtrace up to allocation time";
        case ReportStackRole::kHostFree:
            return "Saved host backtrace up to free time";
        case ReportStackRole::kRelatedAccessA:
            return "First Access Device Frame";
        case ReportStackRole::kRelatedAccessB:
            return "Second Access Device Frame";
        case ReportStackRole::kSyncProducer:
            return "Producer Device Frame";
        case ReportStackRole::kSyncConsumer:
            return "Consumer Device Frame";
        case ReportStackRole::kStateProducer:
            return "State Producer Device Frame";
        case ReportStackRole::kStateConsumer:
            return "State Consumer Device Frame";
        case ReportStackRole::kPeerDevice:
            return "Peer Device Frame";
        case ReportStackRole::kSyncTrigger:
            return "Trigger Point Device Backtrace:";
        case ReportStackRole::kSyncRelated:
            return "Related Point Device Backtrace:";
        case ReportStackRole::kHostApiCall:
            return "Saved host backtrace up to current runtime API call";
    }
    return "Device Frame";
}

} // namespace aclsan::cann
