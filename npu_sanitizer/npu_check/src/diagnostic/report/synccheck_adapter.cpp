// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "diagnostic/report/synccheck_adapter.h"

#include "diagnostic/report/report_catalog.h"
#include "diagnostic/report/report_fields.h"

#include <sstream>

namespace aclsan::cann::detail {
namespace {
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

struct PairingReasonRule {
    NpusanSyncMismatchReason reason;
    bool triggerIsOpen;
    bool relatedIsOpen;
    bool relatedIsActual;
    const char* reasonText;
    const char* missingPointText;
    bool emitExpectedCloseBeforeOpen;
};

const PairingReasonRule* FindPairingReasonRule(NpusanSyncMismatchReason reason)
{
    static constexpr PairingReasonRule rules[] = {
        {NpusanSyncMismatchReason::kDuplicateOpen, true, true, true, "duplicate", "", true},
        {NpusanSyncMismatchReason::kUnmatchedClose, false, true, false, "unmatched",
         "but no matching point exists for this pair key", false},
        {NpusanSyncMismatchReason::kUnconsumedOpen, true, false, false, "redundant",
         "but no matching point was observed", false},
    };
    for (const PairingReasonRule& rule : rules) {
        if (rule.reason == reason) {
            return &rule;
        }
    }
    return nullptr;
}

bool ValidatePairingReport(const NpusanSynccheckReport& report, const NpusanSyncPairingError& detail)
{
    const PairingReasonRule* rule = FindPairingReasonRule(detail.reason);
    if (!report.hasRelatedPoint || rule == nullptr ||
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

    const char* open = OpenOperation(detail.key.pairKind);
    const char* close = CloseOperation(detail.key.pairKind);
    const char* triggerOperation = rule->triggerIsOpen ? open : close;
    const char* relatedOperation = rule->relatedIsOpen ? open : close;
    const bool relatedKindMatches =
        rule->relatedIsActual ? IsActualPoint(report.relatedPoint) : IsExpectedPoint(report.relatedPoint);
    return relatedKindMatches && report.triggerPoint.operation == triggerOperation &&
           report.relatedPoint.operation == relatedOperation;
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
            const PairingReasonRule& rule = *FindPairingReasonRule(detail.reason);
            fields["pairKind"] = PairKindName(detail.key.pairKind);
            fields["pairKey"] = FormatPairKey(detail.key);
            const char* open = OpenOperation(detail.key.pairKind);
            const char* close = CloseOperation(detail.key.pairKind);
            fields["reasonText"] = rule.reasonText;
            if (rule.relatedIsActual) {
                fields["relatedPointLine"] = "=========     related point: previous " + report.relatedPoint.operation +
                                             " at " + fields["relatedLocation"] + " is still pending\n";
            } else {
                fields["relatedPointLine"] = "=========     related point: expected " + report.relatedPoint.operation +
                                             ", " + rule.missingPointText + "\n";
            }
            if (rule.emitExpectedCloseBeforeOpen) {
                fields["expectedOperationLine"] =
                    "=========     expected " + std::string(close) + " before another " + open + "\n";
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
        {ReportTool::kSynccheck, PatternName(ReportTool::kSynccheck, report.common.pattern)},
        report.common.severity,
        fields,
        ActiveCallStacks(report.common)};
}

} // namespace

ReportRenderStatus NormalizeSynccheckReport(const NpusanSynccheckReport& report, ReportRecord* out)
{
    if (out == nullptr || !ValidateSynccheckReport(report)) {
        return ReportRenderStatus::kInvalidArgument;
    }
    *out = ToReportRecord(report);
    return ReportRenderStatus::kSuccess;
}

} // namespace aclsan::cann::detail
