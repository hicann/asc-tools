// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "diagnostic/report_renderer.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace aclsan::cann;

ReportFrame MakeFrame(
    std::uint64_t pc, std::uint64_t offset, const char* function, const char* file, std::uint32_t line)
{
    return ReportFrame{pc, offset, function, file, line, 0, 0, 0};
}

void AddFrameStack(NpusanReportCommon* common, std::uint32_t index, ReportStackRole role, const ReportFrame& frame)
{
    common->stacks[index].role = role;
    common->stacks[index].format = ReportStackFormat::kFrames;
    common->stacks[index].frames.push_back(frame);
    common->stackCount = index + 1;
}

void AddRawStack(NpusanReportCommon* common, std::uint32_t index, ReportStackRole role, const char* rawText)
{
    common->stacks[index].role = role;
    common->stacks[index].format = ReportStackFormat::kRawText;
    common->stacks[index].rawText = rawText;
    common->stackCount = index + 1;
}

NpusanMemcheckReport MakeInvalidAccessReport()
{
    NpusanMemcheckReport report{};
    report.common.reportId = 1001;
    report.common.groupId = 9001;
    report.common.timestampNs = 1786759200123456789ULL;
    report.common.tool = ReportTool::kMemcheck;
    report.common.severity = ReportSeverity::kError;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::kInvalidAccess);
    report.common.exec.launchId = 41;
    report.common.exec.binaryId = 7;
    report.common.exec.functionId = 3;
    report.common.exec.pc = 0x4012;
    report.common.exec.deviceId = 0;
    report.common.exec.coreId = 2;
    report.common.exec.blockId = 17;
    report.common.exec.pipeId = 1;
    report.common.exec.pipeName = "MTE2";
    report.common.exec.kernelName = "VectorAddKernel";
    AddFrameStack(
        &report.common, 0, ReportStackRole::kFaultDevice,
        MakeFrame(0x4012, 0x12, "VectorAddKernel", "/workspace/vector_add.cpp", 87));
    AddRawStack(
        &report.common, 1, ReportStackRole::kHostLaunch,
        "Host Frame: aclrtLaunchKernel [0x7f012345] in libascendcl.so\n"
        "Host Frame: RunVectorAdd [0x4012ab] in npusan_demo\n");

    report.access.memorySpace = NpusanReportMemorySpace::kGm;
    report.access.accessMode = NpusanReportAccessMode::kRead;
    report.access.accessBytes = 16;
    report.access.address = 0x10090;
    report.nearestAllocation.allocId = 501;
    report.nearestAllocation.base = 0x10000;
    report.nearestAllocation.bytes = 128;
    report.nearestAllocation.memorySpace = NpusanReportMemorySpace::kGm;
    report.distanceBytes = 16;
    report.distanceKind = NpusanReportDistanceKind::kAfter;
    return report;
}

NpusanMemcheckReport MakeApiErrorReport()
{
    NpusanMemcheckReport report{};
    report.common.reportId = 1002;
    report.common.groupId = 9001;
    report.common.timestampNs = 1786759200123456890ULL;
    report.common.tool = ReportTool::kMemcheck;
    report.common.severity = ReportSeverity::kError;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::kApiError);
    report.apiName = "aclrtMemcpyAsync";
    report.apiErrorName = "ACL_ERROR_RT_PARAM_INVALID";
    report.apiErrorCode = 107002;
    report.apiErrorMessage = "destination pointer is not device-accessible";
    AddRawStack(
        &report.common, 0, ReportStackRole::kHostLaunch,
        "Host Frame: aclrtMemcpyAsync [0x7f0188a0] in libascendcl.so\n"
        "Host Frame: CopyOutput [0x401420] in npusan_demo\n");
    return report;
}

NpusanInitcheckReport MakeInitcheckReport()
{
    NpusanInitcheckReport report{};
    report.common.reportId = 2001;
    report.common.groupId = 9002;
    report.common.timestampNs = 1786759200123457000ULL;
    report.common.tool = ReportTool::kInitcheck;
    report.common.severity = ReportSeverity::kWarning;
    report.common.pattern = static_cast<std::uint32_t>(NpusanInitcheckPattern::kPartialUninitializedRead);
    report.common.exec.pc = 0x5128;
    report.common.exec.deviceId = 0;
    report.common.exec.coreId = 3;
    report.common.exec.blockId = 9;
    report.common.exec.pipeName = "MTE2";
    report.common.exec.kernelName = "ReduceKernel";
    AddFrameStack(
        &report.common, 0, ReportStackRole::kFaultDevice,
        MakeFrame(0x5128, 0x28, "ReduceKernel", "/workspace/reduce.cpp", 132));

    report.access.memorySpace = NpusanReportMemorySpace::kGm;
    report.access.accessMode = NpusanReportAccessMode::kRead;
    report.access.accessBytes = 64;
    report.access.address = 0x22000;
    report.firstUninitAddress = 0x22020;
    report.firstUninitOffset = 0x20;
    report.uninitBytes = 32;
    report.initializedBytes = 32;
    return report;
}

NpusanRacecheckReport MakeRacecheckReport()
{
    NpusanRacecheckReport report{};
    report.common.reportId = 3001;
    report.common.groupId = 9003;
    report.common.timestampNs = 1786759200123457100ULL;
    report.common.tool = ReportTool::kRacecheck;
    report.common.severity = ReportSeverity::kWarning;
    report.common.pattern = static_cast<std::uint32_t>(NpusanRacecheckPattern::kHazardRaw);
    report.hazardCount = 1;

    report.first.exec.coreId = 2;
    report.first.exec.blockId = 17;
    report.first.exec.pipeName = "MTE3";
    report.first.exec.pc = 0x6040;
    report.first.exec.kernelName = "VectorAddKernel";
    report.first.access.memorySpace = NpusanReportMemorySpace::kUb;
    report.first.access.accessMode = NpusanReportAccessMode::kWrite;
    report.first.access.accessBytes = 4;
    report.first.access.address = 0x2000;

    report.second.exec.coreId = 2;
    report.second.exec.blockId = 17;
    report.second.exec.pipeName = "V";
    report.second.exec.pc = 0x6088;
    report.second.exec.kernelName = "VectorAddKernel";
    report.second.access.memorySpace = NpusanReportMemorySpace::kUb;
    report.second.access.accessMode = NpusanReportAccessMode::kRead;
    report.second.access.accessBytes = 4;
    report.second.access.address = 0x2000;
    report.currentValue = 0x3f800000;

    AddFrameStack(
        &report.common, 0, ReportStackRole::kRelatedAccessA,
        MakeFrame(0x6040, 0x40, "WriteTile", "/workspace/vector_add.cpp", 103));
    AddFrameStack(
        &report.common, 1, ReportStackRole::kRelatedAccessB,
        MakeFrame(0x6088, 0x88, "ReadTile", "/workspace/vector_add.cpp", 118));
    return report;
}

NpusanReportExecContext MakeSyncExec(
    std::uint32_t coreId, std::uint32_t blockId, std::uint32_t pipeId, const char* pipeName, std::uint64_t pc,
    const char* function, std::uint64_t offset, const char* file, std::uint32_t line)
{
    NpusanReportExecContext exec{};
    exec.pc = pc;
    exec.offset = offset;
    exec.deviceId = 0;
    exec.coreId = coreId;
    exec.blockId = blockId;
    exec.pipeId = pipeId;
    exec.line = line;
    exec.function = function;
    exec.file = file;
    exec.pipeName = pipeName;
    exec.kernelName = "SynccheckDemoKernel";
    return exec;
}

NpusanSynccheckReport MakeSynccheckBase(
    std::uint64_t reportId, ReportSeverity severity, NpusanSynccheckPattern pattern,
    NpusanSyncPrimitiveKind primitiveKind, const char* triggerOperation, const NpusanReportExecContext& triggerExec)
{
    NpusanSynccheckReport report{};
    report.common.reportId = reportId;
    report.common.groupId = 9004;
    report.common.timestampNs = 1786759200123457200ULL + reportId;
    report.common.tool = ReportTool::kSynccheck;
    report.common.severity = severity;
    report.common.pattern = static_cast<std::uint32_t>(pattern);
    report.common.flags = kNpusanReportCommonHasExecContext;
    report.common.exec = triggerExec;
    report.primitiveKind = primitiveKind;
    report.triggerPoint.operation = triggerOperation;
    report.triggerPoint.hasExecContext = true;
    report.triggerPoint.exec = triggerExec;
    report.triggerPoint.stackRole = ReportStackRole::kSyncTrigger;
    AddFrameStack(
        &report.common, 0, ReportStackRole::kSyncTrigger,
        MakeFrame(
            triggerExec.pc, triggerExec.offset, triggerExec.function.c_str(), triggerExec.file.c_str(),
            triggerExec.line));
    return report;
}

NpusanSynccheckReport MakeBarrierDivergenceReport()
{
    const NpusanReportExecContext trigger =
        MakeSyncExec(2, 17, 0, "SIMT", 0x4010, "BlockReduce", 0x40, "/workspace/reduce.cpp", 91);
    NpusanSynccheckReport report = MakeSynccheckBase(
        4001, ReportSeverity::kError, NpusanSynccheckPattern::kIntraCoreDivergent, NpusanSyncPrimitiveKind::kBarrier,
        "SYNC_THREADS", trigger);
    report.detailKind = NpusanSyncDetailKind::kBarrier;
    report.detail = NpusanSyncBarrierError{"Divergent execution entities in block", "block", 0x0000ffff, 0xffffffff, 0};
    report.common.stacks[0].frames.push_back(MakeFrame(0x3f20, 0x120, "ReduceKernel", "/workspace/reduce.cpp", 137));
    return report;
}

NpusanSynccheckReport MakeDuplicateSetReport()
{
    const NpusanReportExecContext trigger =
        MakeSyncExec(1, 4, 1, "PIPE_S", 0x5040, "SignalMte", 0x40, "/workspace/pipeline.cpp", 55);
    const NpusanReportExecContext related =
        MakeSyncExec(1, 4, 1, "PIPE_S", 0x5010, "SignalMte", 0x10, "/workspace/pipeline.cpp", 48);
    NpusanSynccheckReport report = MakeSynccheckBase(
        4002, ReportSeverity::kError, NpusanSynccheckPattern::kPairingMismatch, NpusanSyncPrimitiveKind::kSetWaitFlag,
        "SET_FLAG", trigger);
    report.detailKind = NpusanSyncDetailKind::kPairing;
    report.hasRelatedPoint = true;
    report.relatedPoint.operation = "SET_FLAG";
    report.relatedPoint.hasExecContext = true;
    report.relatedPoint.exec = related;
    report.relatedPoint.stackRole = ReportStackRole::kSyncRelated;
    report.detail = NpusanSyncPairingError{
        NpusanSyncMismatchReason::kDuplicateOpen,
        {NpusanSyncPairKind::kSetWaitFlag, 1, 3, 0, 7},
    };
    report.common.stacks[0].frames.push_back(MakeFrame(0x4f10, 0x90, "PipelineStage", "/workspace/pipeline.cpp", 103));
    AddFrameStack(
        &report.common, 1, ReportStackRole::kSyncRelated,
        MakeFrame(related.pc, related.offset, related.function.c_str(), related.file.c_str(), related.line));
    report.common.stacks[1].frames.push_back(MakeFrame(0x4f10, 0x90, "PipelineStage", "/workspace/pipeline.cpp", 103));
    return report;
}

NpusanSynccheckReport MakeUnmatchedRlsReport()
{
    const NpusanReportExecContext trigger =
        MakeSyncExec(3, 8, 3, "PIPE_MTE3", 0x6080, "ReleaseBuffer", 0x80, "/workspace/buffer.cpp", 80);
    NpusanSynccheckReport report = MakeSynccheckBase(
        4003, ReportSeverity::kError, NpusanSynccheckPattern::kPairingMismatch, NpusanSyncPrimitiveKind::kGetRlsBuf,
        "RLS_BUF", trigger);
    report.detailKind = NpusanSyncDetailKind::kPairing;
    report.hasRelatedPoint = true;
    report.relatedPoint.operation = "GET_BUF";
    report.detail = NpusanSyncPairingError{
        NpusanSyncMismatchReason::kUnmatchedClose,
        {NpusanSyncPairKind::kGetRlsBuf, 0, 0, 2, 12},
    };
    report.common.stacks[0].frames.push_back(MakeFrame(0x5f20, 0x60, "BufferStage", "/workspace/buffer.cpp", 104));
    return report;
}

NpusanSynccheckReport MakeUnconsumedGetReport()
{
    const NpusanReportExecContext trigger =
        MakeSyncExec(3, 8, 3, "PIPE_MTE3", 0x6040, "AcquireBuffer", 0x20, "/workspace/buffer.cpp", 70);
    NpusanSynccheckReport report = MakeSynccheckBase(
        4004, ReportSeverity::kWarning, NpusanSynccheckPattern::kPairingMismatch, NpusanSyncPrimitiveKind::kGetRlsBuf,
        "GET_BUF", trigger);
    report.detailKind = NpusanSyncDetailKind::kPairing;
    report.hasRelatedPoint = true;
    report.relatedPoint.operation = "RLS_BUF";
    report.detail = NpusanSyncPairingError{
        NpusanSyncMismatchReason::kUnconsumedOpen,
        {NpusanSyncPairKind::kGetRlsBuf, 0, 0, 1, 13},
    };
    return report;
}

NpusanSynccheckReport MakeSequenceMismatchReport()
{
    const NpusanReportExecContext trigger =
        MakeSyncExec(5, 2, 5, "MMA", 0x7080, "MmaStage", 0x80, "/workspace/gemm.cpp", 122);
    NpusanSynccheckReport report = MakeSynccheckBase(
        4005, ReportSeverity::kError, NpusanSynccheckPattern::kInstructionSequenceMismatch,
        NpusanSyncPrimitiveKind::kInstructionSequence, "wgmma.mma_async.m64n64k16", trigger);
    report.detailKind = NpusanSyncDetailKind::kSequence;
    report.hasRelatedPoint = true;
    report.relatedPoint.operation = "wgmma.commit_group";
    report.detail = NpusanSyncSequenceError{"Instruction order differs between warps", 3, 0xffffffff};
    report.common.stacks[0].frames.push_back(MakeFrame(0x6f00, 0x200, "GemmKernel", "/workspace/gemm.cpp", 188));
    return report;
}

NpusanSynccheckReport MakeDeadlockReport()
{
    const NpusanReportExecContext trigger =
        MakeSyncExec(6, 1, 0, "SIMT", 0x8040, "BarrierWait", 0x40, "/workspace/barrier.cpp", 64);
    NpusanSynccheckReport report = MakeSynccheckBase(
        4006, ReportSeverity::kError, NpusanSynccheckPattern::kDeadlock, NpusanSyncPrimitiveKind::kSyncObject,
        "BARRIER_WAIT", trigger);
    report.detailKind = NpusanSyncDetailKind::kObject;
    report.hasRelatedPoint = true;
    report.relatedPoint.operation = "BARRIER_ARRIVE";
    report.detail = NpusanSyncObjectError{"Wait cannot complete", 0x21, 0x8000, 0x0f, 5000000};
    report.common.stacks[0].frames.push_back(MakeFrame(0x7f10, 0x70, "ConsumerStage", "/workspace/barrier.cpp", 112));
    return report;
}

NpusanSoccheckReport MakeSoccheckReport()
{
    NpusanSoccheckReport report{};
    report.common.reportId = 5001;
    report.common.groupId = 9005;
    report.common.timestampNs = 1786759200123457300ULL;
    report.common.tool = ReportTool::kSoccheck;
    report.common.severity = ReportSeverity::kFatal;
    report.common.pattern = static_cast<std::uint32_t>(NpusanSoccheckPattern::kRegisterMismatch);
    report.common.exec.function = "RestoreControlState";
    report.common.exec.offset = 0x18;
    report.common.exec.file = "/workspace/control_state.cpp";
    report.common.exec.line = 54;
    report.common.exec.pc = 0x8018;
    report.common.exec.deviceId = 0;
    report.common.exec.coreId = 1;
    report.common.exec.blockId = 3;
    report.common.exec.pipeName = "S";

    report.state.stateKind = 2;
    report.state.scope = 1;
    report.state.registerId = 17;
    report.state.ownerCoreId = 1;
    report.state.stateId = 7001;
    report.state.expectedValue = 0x10;
    report.state.observedValue = 0x11;
    return report;
}

} // namespace

int main()
{
    NpusanMemcheckReport invalidAccess = MakeInvalidAccessReport();
    NpusanMemcheckReport apiError = MakeApiErrorReport();
    NpusanInitcheckReport initcheck = MakeInitcheckReport();
    NpusanRacecheckReport racecheck = MakeRacecheckReport();
    NpusanSynccheckReport barrierDivergence = MakeBarrierDivergenceReport();
    NpusanSynccheckReport duplicateSet = MakeDuplicateSetReport();
    NpusanSynccheckReport unmatchedRls = MakeUnmatchedRlsReport();
    NpusanSynccheckReport unconsumedGet = MakeUnconsumedGetReport();
    NpusanSynccheckReport sequenceMismatch = MakeSequenceMismatchReport();
    NpusanSynccheckReport deadlock = MakeDeadlockReport();
    NpusanSoccheckReport soccheck = MakeSoccheckReport();

    const std::vector<NpusanReportRecord> records{
        NpusanReportRecord::From(invalidAccess),     NpusanReportRecord::From(apiError),
        NpusanReportRecord::From(initcheck),         NpusanReportRecord::From(racecheck),
        NpusanReportRecord::From(barrierDivergence), NpusanReportRecord::From(duplicateSet),
        NpusanReportRecord::From(unmatchedRls),      NpusanReportRecord::From(unconsumedGet),
        NpusanReportRecord::From(sequenceMismatch),  NpusanReportRecord::From(deadlock),
        NpusanReportRecord::From(soccheck),
    };

    std::string output;
    const ReportRenderStatus status = RenderNpusanReportBundle(records, {}, &output);
    if (status != ReportRenderStatus::kSuccess) {
        std::cerr << "Failed to render NPUSAN demo bundle, status=" << static_cast<int>(status) << '\n';
        return 1;
    }

    std::cout << output;
    return 0;
}
