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

using namespace npucheck;

ReportFrame MakeFrame(
    std::uint64_t pc, std::uint64_t offset, const char* function, const char* file, std::uint32_t line)
{
    return ReportFrame{pc, offset, function, file, line, 0, 0, 0};
}

void AddFrameStack(NpuCheckReportCommon* common, std::uint32_t index, ReportStackRole role, const ReportFrame& frame)
{
    common->stacks[index].role = role;
    common->stacks[index].format = ReportStackFormat::FRAMES;
    common->stacks[index].frames.push_back(frame);
    common->stackCount = index + 1;
}

NpuCheckMemcheckReport MakeInvalidAccessReport()
{
    NpuCheckMemcheckReport report{};
    report.common.reportId = 1001;
    report.common.groupId = 9001;
    report.common.timestampNs = 1786759200123456789ULL;
    report.common.tool = ReportTool::MEMCHECK;
    report.common.severity = ReportSeverity::ERROR;
    report.common.pattern = NpuCheckReportPattern::MEMCHECK_INVALID_ACCESS;
    report.common.exec.launchId = 41;
    report.common.exec.binaryId = 7;
    report.common.exec.functionId = 3;
    report.common.exec.pc = 0x4012;
    report.common.exec.deviceId = 0;
    report.common.exec.phyCoreId = 2;
    report.common.exec.blockId = 17;
    report.common.exec.pipeId = 1;
    report.common.exec.pipeName = "MTE2";
    report.common.exec.kernelName = "VectorAddKernel";
    AddFrameStack(
        &report.common, 0, ReportStackRole::FAULT_DEVICE,
        MakeFrame(0x4012, 0x12, "VectorAddKernel", "/workspace/vector_add.cpp", 87));
    AddFrameStack(
        &report.common, 1, ReportStackRole::HOST_LAUNCH, MakeFrame(0x7f012345, 0, "aclrtLaunchKernel", "", 0));
    report.common.stacks[1].frames.push_back(MakeFrame(0x4012ab, 0, "RunVectorAdd", "", 0));

    report.access.memorySpace = NpuCheckReportMemorySpace::GM;
    report.access.accessMode = NpuCheckReportAccessMode::READ;
    report.access.accessBytes = 16;
    report.access.address = 0x10090;
    report.nearestAllocation.allocId = 501;
    report.nearestAllocation.base = 0x10000;
    report.nearestAllocation.bytes = 128;
    report.nearestAllocation.memorySpace = NpuCheckReportMemorySpace::GM;
    report.distanceBytes = 16;
    report.distanceKind = NpuCheckReportDistanceKind::AFTER;
    return report;
}

NpuCheckMemcheckReport MakeApiErrorReport()
{
    NpuCheckMemcheckReport report{};
    report.common.reportId = 1002;
    report.common.groupId = 9001;
    report.common.timestampNs = 1786759200123456890ULL;
    report.common.tool = ReportTool::MEMCHECK;
    report.common.severity = ReportSeverity::ERROR;
    report.common.pattern = NpuCheckReportPattern::MEMCHECK_API_ERROR;
    report.apiName = "aclrtMemcpyAsync";
    report.apiErrorName = "ACL_ERROR_RT_PARAM_INVALID";
    report.apiErrorCode = 107002;
    report.apiErrorMessage = "destination pointer is not device-accessible";
    AddFrameStack(
        &report.common, 0, ReportStackRole::HOST_API_CALL, MakeFrame(0x7f0188a0, 0, "aclrtMemcpyAsync", "", 0));
    report.common.stacks[0].frames.push_back(MakeFrame(0x401420, 0, "CopyOutput", "", 0));
    return report;
}

NpuCheckInitcheckReport MakeInitcheckReport()
{
    NpuCheckInitcheckReport report{};
    report.common.reportId = 2001;
    report.common.groupId = 9002;
    report.common.timestampNs = 1786759200123457000ULL;
    report.common.tool = ReportTool::INITCHECK;
    report.common.severity = ReportSeverity::WARNING;
    report.common.pattern = NpuCheckReportPattern::INITCHECK_PARTIAL_UNINITIALIZED_READ;
    report.common.exec.pc = 0x5128;
    report.common.exec.deviceId = 0;
    report.common.exec.phyCoreId = 3;
    report.common.exec.blockId = 9;
    report.common.exec.pipeName = "MTE2";
    report.common.exec.kernelName = "ReduceKernel";
    AddFrameStack(
        &report.common, 0, ReportStackRole::FAULT_DEVICE,
        MakeFrame(0x5128, 0x28, "ReduceKernel", "/workspace/reduce.cpp", 132));

    report.access.memorySpace = NpuCheckReportMemorySpace::GM;
    report.access.accessMode = NpuCheckReportAccessMode::READ;
    report.access.accessBytes = 64;
    report.access.address = 0x22000;
    report.firstUninitAddress = 0x22020;
    report.firstUninitOffset = 0x20;
    report.uninitBytes = 32;
    report.initializedBytes = 32;
    return report;
}

NpuCheckRacecheckReport MakeRacecheckReport()
{
    NpuCheckRacecheckReport report{};
    report.common.reportId = 3001;
    report.common.groupId = 9003;
    report.common.timestampNs = 1786759200123457100ULL;
    report.common.tool = ReportTool::RACECHECK;
    report.common.severity = ReportSeverity::WARNING;
    report.common.pattern = NpuCheckReportPattern::RACECHECK_HAZARD_RAW;
    report.hazardCount = 1;

    report.first.exec.phyCoreId = 2;
    report.first.exec.blockId = 17;
    report.first.exec.pipeName = "MTE3";
    report.first.exec.pc = 0x6040;
    report.first.exec.kernelName = "VectorAddKernel";
    report.first.access.memorySpace = NpuCheckReportMemorySpace::UB;
    report.first.access.accessMode = NpuCheckReportAccessMode::WRITE;
    report.first.access.accessBytes = 4;
    report.first.access.address = 0x2000;

    report.second.exec.phyCoreId = 2;
    report.second.exec.blockId = 17;
    report.second.exec.pipeName = "V";
    report.second.exec.pc = 0x6088;
    report.second.exec.kernelName = "VectorAddKernel";
    report.second.access.memorySpace = NpuCheckReportMemorySpace::UB;
    report.second.access.accessMode = NpuCheckReportAccessMode::READ;
    report.second.access.accessBytes = 4;
    report.second.access.address = 0x2000;
    report.currentValue = 0x3f800000;

    AddFrameStack(
        &report.common, 0, ReportStackRole::RELATED_ACCESS_A,
        MakeFrame(0x6040, 0x40, "WriteTile", "/workspace/vector_add.cpp", 103));
    AddFrameStack(
        &report.common, 1, ReportStackRole::RELATED_ACCESS_B,
        MakeFrame(0x6088, 0x88, "ReadTile", "/workspace/vector_add.cpp", 118));
    return report;
}

NpuCheckReportExecContext MakeSyncExec(
    std::uint32_t phyCoreId, std::uint32_t blockId, std::uint32_t pipeId, const char* pipeName, std::uint64_t pc,
    const char* function, std::uint64_t offset, const char* file, std::uint32_t line)
{
    NpuCheckReportExecContext exec{};
    exec.pc = pc;
    exec.offset = offset;
    exec.deviceId = 0;
    exec.phyCoreId = phyCoreId;
    exec.blockId = blockId;
    exec.pipeId = pipeId;
    exec.line = line;
    exec.function = function;
    exec.file = file;
    exec.pipeName = pipeName;
    exec.kernelName = "SynccheckDemoKernel";
    return exec;
}

NpuCheckSynccheckReport MakeSynccheckBase(
    std::uint64_t reportId, ReportSeverity severity, NpuCheckReportPattern pattern,
    NpuCheckSyncPrimitiveKind primitiveKind, const char* triggerOperation, const NpuCheckReportExecContext& triggerExec)
{
    NpuCheckSynccheckReport report{};
    report.common.reportId = reportId;
    report.common.groupId = 9004;
    report.common.timestampNs = 1786759200123457200ULL + reportId;
    report.common.tool = ReportTool::SYNCCHECK;
    report.common.severity = severity;
    report.common.pattern = pattern;
    report.common.flags = kNpuCheckReportCommonHasExecContext;
    report.common.exec = triggerExec;
    report.primitiveKind = primitiveKind;
    report.triggerPoint.operation = triggerOperation;
    report.triggerPoint.hasExecContext = true;
    report.triggerPoint.exec = triggerExec;
    report.triggerPoint.stackRole = ReportStackRole::SYNC_TRIGGER;
    AddFrameStack(
        &report.common, 0, ReportStackRole::SYNC_TRIGGER,
        MakeFrame(
            triggerExec.pc, triggerExec.offset, triggerExec.function.c_str(), triggerExec.file.c_str(),
            triggerExec.line));
    return report;
}

NpuCheckSynccheckReport MakeBarrierDivergenceReport()
{
    const NpuCheckReportExecContext trigger =
        MakeSyncExec(2, 17, 0, "SIMT", 0x4010, "BlockReduce", 0x40, "/workspace/reduce.cpp", 91);
    NpuCheckSynccheckReport report = MakeSynccheckBase(
        4001, ReportSeverity::ERROR, NpuCheckReportPattern::SYNCCHECK_INTRA_CORE_DIVERGENT,
        NpuCheckSyncPrimitiveKind::BARRIER, "SYNC_THREADS", trigger);
    report.detailKind = NpuCheckSyncDetailKind::BARRIER;
    report.detail =
        NpuCheckSyncBarrierError{"Divergent execution entities in block", "block", 0x0000ffff, 0xffffffff, 0};
    report.common.stacks[0].frames.push_back(MakeFrame(0x3f20, 0x120, "ReduceKernel", "/workspace/reduce.cpp", 137));
    return report;
}

NpuCheckSynccheckReport MakeDuplicateSetReport()
{
    const NpuCheckReportExecContext trigger =
        MakeSyncExec(1, 4, 1, "PIPE_S", 0x5040, "SignalMte", 0x40, "/workspace/pipeline.cpp", 55);
    const NpuCheckReportExecContext related =
        MakeSyncExec(1, 4, 1, "PIPE_S", 0x5010, "SignalMte", 0x10, "/workspace/pipeline.cpp", 48);
    NpuCheckSynccheckReport report = MakeSynccheckBase(
        4002, ReportSeverity::ERROR, NpuCheckReportPattern::SYNCCHECK_PAIRING_MISMATCH,
        NpuCheckSyncPrimitiveKind::SET_WAIT_FLAG, "SET_FLAG", trigger);
    report.detailKind = NpuCheckSyncDetailKind::PAIRING;
    report.hasRelatedPoint = true;
    report.relatedPoint.operation = "SET_FLAG";
    report.relatedPoint.hasExecContext = true;
    report.relatedPoint.exec = related;
    report.relatedPoint.stackRole = ReportStackRole::SYNC_RELATED;
    report.detail = NpuCheckSyncPairingError{
        NpuCheckSyncMismatchReason::DUPLICATE_OPEN,
        {NpuCheckSyncPairKind::SET_WAIT_FLAG, 1, 3, 0, 7},
    };
    report.common.stacks[0].frames.push_back(MakeFrame(0x4f10, 0x90, "PipelineStage", "/workspace/pipeline.cpp", 103));
    AddFrameStack(
        &report.common, 1, ReportStackRole::SYNC_RELATED,
        MakeFrame(related.pc, related.offset, related.function.c_str(), related.file.c_str(), related.line));
    report.common.stacks[1].frames.push_back(MakeFrame(0x4f10, 0x90, "PipelineStage", "/workspace/pipeline.cpp", 103));
    return report;
}

NpuCheckSynccheckReport MakeUnmatchedRlsReport()
{
    const NpuCheckReportExecContext trigger =
        MakeSyncExec(3, 8, 3, "PIPE_MTE3", 0x6080, "ReleaseBuffer", 0x80, "/workspace/buffer.cpp", 80);
    NpuCheckSynccheckReport report = MakeSynccheckBase(
        4003, ReportSeverity::ERROR, NpuCheckReportPattern::SYNCCHECK_PAIRING_MISMATCH,
        NpuCheckSyncPrimitiveKind::GET_RLS_BUF, "RLS_BUF", trigger);
    report.detailKind = NpuCheckSyncDetailKind::PAIRING;
    report.hasRelatedPoint = true;
    report.relatedPoint.operation = "GET_BUF";
    report.detail = NpuCheckSyncPairingError{
        NpuCheckSyncMismatchReason::UNMATCHED_CLOSE,
        {NpuCheckSyncPairKind::GET_RLS_BUF, 0, 0, 2, 12},
    };
    report.common.stacks[0].frames.push_back(MakeFrame(0x5f20, 0x60, "BufferStage", "/workspace/buffer.cpp", 104));
    return report;
}

NpuCheckSynccheckReport MakeUnconsumedGetReport()
{
    const NpuCheckReportExecContext trigger =
        MakeSyncExec(3, 8, 3, "PIPE_MTE3", 0x6040, "AcquireBuffer", 0x20, "/workspace/buffer.cpp", 70);
    NpuCheckSynccheckReport report = MakeSynccheckBase(
        4004, ReportSeverity::WARNING, NpuCheckReportPattern::SYNCCHECK_PAIRING_MISMATCH,
        NpuCheckSyncPrimitiveKind::GET_RLS_BUF, "GET_BUF", trigger);
    report.detailKind = NpuCheckSyncDetailKind::PAIRING;
    report.hasRelatedPoint = true;
    report.relatedPoint.operation = "RLS_BUF";
    report.detail = NpuCheckSyncPairingError{
        NpuCheckSyncMismatchReason::UNCONSUMED_OPEN,
        {NpuCheckSyncPairKind::GET_RLS_BUF, 0, 0, 1, 13},
    };
    return report;
}

NpuCheckSynccheckReport MakeSequenceMismatchReport()
{
    const NpuCheckReportExecContext trigger =
        MakeSyncExec(5, 2, 5, "MMA", 0x7080, "MmaStage", 0x80, "/workspace/gemm.cpp", 122);
    NpuCheckSynccheckReport report = MakeSynccheckBase(
        4005, ReportSeverity::ERROR, NpuCheckReportPattern::SYNCCHECK_INSTRUCTION_SEQUENCE_MISMATCH,
        NpuCheckSyncPrimitiveKind::INSTRUCTION_SEQUENCE, "wgmma.mma_async.m64n64k16", trigger);
    report.detailKind = NpuCheckSyncDetailKind::SEQUENCE;
    report.hasRelatedPoint = true;
    report.relatedPoint.operation = "wgmma.commit_group";
    report.detail = NpuCheckSyncSequenceError{"Instruction order differs between warps", 3, 0xffffffff};
    report.common.stacks[0].frames.push_back(MakeFrame(0x6f00, 0x200, "GemmKernel", "/workspace/gemm.cpp", 188));
    return report;
}

NpuCheckSynccheckReport MakeDeadlockReport()
{
    const NpuCheckReportExecContext trigger =
        MakeSyncExec(6, 1, 0, "SIMT", 0x8040, "BarrierWait", 0x40, "/workspace/barrier.cpp", 64);
    NpuCheckSynccheckReport report = MakeSynccheckBase(
        4006, ReportSeverity::ERROR, NpuCheckReportPattern::SYNCCHECK_DEADLOCK, NpuCheckSyncPrimitiveKind::SYNC_OBJECT,
        "BARRIER_WAIT", trigger);
    report.detailKind = NpuCheckSyncDetailKind::OBJECT;
    report.hasRelatedPoint = true;
    report.relatedPoint.operation = "BARRIER_ARRIVE";
    report.detail = NpuCheckSyncObjectError{"Wait cannot complete", 0x21, 0x8000, 0x0f, 5000000};
    report.common.stacks[0].frames.push_back(MakeFrame(0x7f10, 0x70, "ConsumerStage", "/workspace/barrier.cpp", 112));
    return report;
}

NpuCheckSoccheckReport MakeSoccheckReport()
{
    NpuCheckSoccheckReport report{};
    report.common.reportId = 5001;
    report.common.groupId = 9005;
    report.common.timestampNs = 1786759200123457300ULL;
    report.common.tool = ReportTool::SOCCHECK;
    report.common.severity = ReportSeverity::FATAL;
    report.common.pattern = NpuCheckReportPattern::SOCCHECK_REGISTER_MISMATCH;
    report.common.exec.function = "RestoreControlState";
    report.common.exec.offset = 0x18;
    report.common.exec.file = "/workspace/control_state.cpp";
    report.common.exec.line = 54;
    report.common.exec.pc = 0x8018;
    report.common.exec.deviceId = 0;
    report.common.exec.phyCoreId = 1;
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
    NpuCheckMemcheckReport invalidAccess = MakeInvalidAccessReport();
    NpuCheckMemcheckReport apiError = MakeApiErrorReport();
    NpuCheckInitcheckReport initcheck = MakeInitcheckReport();
    NpuCheckRacecheckReport racecheck = MakeRacecheckReport();
    NpuCheckSynccheckReport barrierDivergence = MakeBarrierDivergenceReport();
    NpuCheckSynccheckReport duplicateSet = MakeDuplicateSetReport();
    NpuCheckSynccheckReport unmatchedRls = MakeUnmatchedRlsReport();
    NpuCheckSynccheckReport unconsumedGet = MakeUnconsumedGetReport();
    NpuCheckSynccheckReport sequenceMismatch = MakeSequenceMismatchReport();
    NpuCheckSynccheckReport deadlock = MakeDeadlockReport();
    NpuCheckSoccheckReport soccheck = MakeSoccheckReport();

    const std::vector<NpuCheckReportRecord> records{
        NpuCheckReportRecord::From(invalidAccess),     NpuCheckReportRecord::From(apiError),
        NpuCheckReportRecord::From(initcheck),         NpuCheckReportRecord::From(racecheck),
        NpuCheckReportRecord::From(barrierDivergence), NpuCheckReportRecord::From(duplicateSet),
        NpuCheckReportRecord::From(unmatchedRls),      NpuCheckReportRecord::From(unconsumedGet),
        NpuCheckReportRecord::From(sequenceMismatch),  NpuCheckReportRecord::From(deadlock),
        NpuCheckReportRecord::From(soccheck),
    };

    std::string output;
    const ReportRenderStatus status = RenderNpuCheckReportBundle(records, {}, &output);
    if (status != ReportRenderStatus::kSuccess) {
        std::cerr << "Failed to render NPU-CHECK demo bundle, status=" << static_cast<int>(status) << '\n';
        return 1;
    }

    std::cout << output;
    return 0;
}
