#include "report_renderer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

using aclsan::cann::ReportCallStack;
using aclsan::cann::ReportFields;
using aclsan::cann::ReportFrame;
using aclsan::cann::ReportRecord;
using aclsan::cann::ReportRenderStatus;
using aclsan::cann::ReportSeverity;
using aclsan::cann::ReportStackFormat;
using aclsan::cann::ReportStackRole;
using aclsan::cann::ReportTemplate;
using aclsan::cann::ReportTemplateKey;
using aclsan::cann::ReportTemplateOverrides;
using aclsan::cann::ReportTool;
using aclsan::cann::NpusanInitcheckPattern;
using aclsan::cann::NpusanInitcheckReport;
using aclsan::cann::NpusanMemcheckPattern;
using aclsan::cann::NpusanMemcheckReport;
using aclsan::cann::NpusanRaceAccessSite;
using aclsan::cann::NpusanRacecheckPattern;
using aclsan::cann::NpusanRacecheckReport;
using aclsan::cann::NpusanReportAccessMode;
using aclsan::cann::NpusanReportAllocation;
using aclsan::cann::NpusanReportDistanceKind;
using aclsan::cann::NpusanReportMemoryAccess;
using aclsan::cann::NpusanReportMemorySpace;
using aclsan::cann::NpusanReportRecord;
using aclsan::cann::NpusanSoccheckPattern;
using aclsan::cann::NpusanSoccheckReport;
using aclsan::cann::NpusanSyncDetailKind;
using aclsan::cann::NpusanSyncBarrierError;
using aclsan::cann::NpusanSyncMismatchReason;
using aclsan::cann::NpusanSyncObjectError;
using aclsan::cann::NpusanSyncPairingError;
using aclsan::cann::NpusanSyncPairKind;
using aclsan::cann::NpusanSyncPoint;
using aclsan::cann::NpusanSyncPrimitiveKind;
using aclsan::cann::NpusanSyncSequenceError;
using aclsan::cann::NpusanSynccheckPattern;
using aclsan::cann::NpusanSynccheckReport;

template <typename Report, typename = void>
struct CanCreateNpusanRecordFrom : std::false_type {};

template <typename Report>
struct CanCreateNpusanRecordFrom<
    Report, std::void_t<decltype(NpusanReportRecord::From(std::declval<Report>()))>> : std::true_type {};

static_assert(!CanCreateNpusanRecordFrom<NpusanMemcheckReport&&>::value,
              "NpusanReportRecord must not borrow a temporary report");
static_assert(!CanCreateNpusanRecordFrom<const NpusanMemcheckReport&&>::value,
              "NpusanReportRecord must not borrow a const temporary report");
static_assert(static_cast<int>(NpusanSynccheckPattern::kPairingMismatch) == 4);
static_assert(static_cast<int>(NpusanSynccheckPattern::kParticipantMismatch) == 5);
static_assert(static_cast<int>(NpusanSynccheckPattern::kDeadlock) == 6);
static_assert(static_cast<int>(NpusanSynccheckPattern::kObjectNotInitialized) == 7);
static_assert(static_cast<int>(NpusanSynccheckPattern::kInstructionSequenceMismatch) == 8);

std::string ReadFile(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::size_t CountOccurrences(const std::string& text, const std::string& needle)
{
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

ReportRecord MakeMemcheckInvalidAccessRecord()
{
    return ReportRecord{
        {ReportTool::kMemcheck, "invalid_access"},
        ReportSeverity::kError,
        {
            {"space", "GM"},
            {"access", "read"},
            {"accessBytes", "16"},
            {"function", "kernel"},
            {"offset", "10"},
            {"file", "kernel.cpp"},
            {"line", "42"},
            {"coreId", "3"},
            {"blockId", "7"},
            {"pipeName", "MTE2"},
            {"address", "1000"},
            {"distanceBytes", "64"},
            {"before|after", "after"},
            {"base", "0fc0"},
            {"bytes", "128"},
            {"hostFunction", "aclrtLaunchKernel"},
            {"hostPc", "400123"},
            {"hostBinary", "libacl.so"},
        }};
}

ReportRecord MakeInitcheckRecord()
{
    return ReportRecord{
        {ReportTool::kInitcheck, "uninitialized_read"},
        ReportSeverity::kError,
        {
            {"space", "GM"},
            {"accessBytes", "32"},
            {"function", "init_kernel"},
            {"offset", "18"},
            {"file", "init.cpp"},
            {"line", "21"},
            {"coreId", "4"},
            {"blockId", "2"},
            {"pipeName", "MTE2"},
            {"address", "3000"},
        }};
}

ReportRecord MakeRaceRecord()
{
    return ReportRecord{
        {ReportTool::kRacecheck, "hazard_raw"},
        ReportSeverity::kWarning,
        {
            {"space", "UB"},
            {"address", "2000"},
            {"blockId", "8"},
            {"firstCoreId", "0"},
            {"firstPipe", "MTE3"},
            {"firstFunction", "writer"},
            {"firstOffset", "20"},
            {"firstFile", "race.cpp"},
            {"firstLine", "11"},
            {"secondCoreId", "1"},
            {"secondPipe", "MTE2"},
            {"secondFunction", "reader"},
            {"secondOffset", "30"},
            {"secondFile", "race.cpp"},
            {"secondLine", "19"},
            {"currentValue", "0xab"},
        }};
}

ReportRecord MakeSyncRecord()
{
    return ReportRecord{
        {ReportTool::kSynccheck, "pairing_mismatch"},
        ReportSeverity::kError,
        {
            {"reasonText", "unmatched"},
            {"triggerOperation", "WAIT_FLAG"},
            {"triggerCoreId", "2"},
            {"triggerBlock", "5"},
            {"triggerPipe", "V"},
            {"triggerLocation", "sync_kernel+0x44 in sync.cpp:88"},
            {"relatedPointLine", "=========     related point: expected SET_FLAG\n"},
            {"expectedOperationLine", ""},
            {"pairKind", "SET_WAIT_FLAG"},
            {"pairKey", "srcPipe=pipe(1), dstPipe=pipe(4), id=7"},
        }};
}

NpusanSynccheckReport MakePairingReport(
    NpusanSyncMismatchReason reason, NpusanSyncPairKind pairKind)
{
    NpusanSynccheckReport report{};
    report.common.tool = ReportTool::kSynccheck;
    report.common.pattern = static_cast<std::uint32_t>(NpusanSynccheckPattern::kPairingMismatch);
    report.common.severity = ReportSeverity::kError;
    report.common.flags = aclsan::cann::kNpusanReportCommonHasExecContext;
    report.primitiveKind = pairKind == NpusanSyncPairKind::kSetWaitFlag
                               ? NpusanSyncPrimitiveKind::kSetWaitFlag
                               : NpusanSyncPrimitiveKind::kGetRlsBuf;
    report.detailKind = NpusanSyncDetailKind::kPairing;
    report.hasRelatedPoint = true;
    report.triggerPoint.hasExecContext = true;
    report.triggerPoint.exec.coreId = 1;
    report.triggerPoint.exec.blockId = 4;
    report.triggerPoint.exec.pipeName = "MTE2";
    report.triggerPoint.exec.function = "SyncOperation";
    report.triggerPoint.exec.offset = 0x20;
    report.triggerPoint.exec.file = "sync.cpp";
    report.triggerPoint.exec.line = 30;
    report.common.exec = report.triggerPoint.exec;

    const bool setWait = pairKind == NpusanSyncPairKind::kSetWaitFlag;
    const char* open = setWait ? "SET_FLAG" : "GET_BUF";
    const char* close = setWait ? "WAIT_FLAG" : "RLS_BUF";
    if (reason == NpusanSyncMismatchReason::kDuplicateOpen) {
        report.triggerPoint.operation = open;
        report.relatedPoint.operation = open;
        report.relatedPoint.hasExecContext = true;
        report.relatedPoint.exec = report.triggerPoint.exec;
        report.relatedPoint.exec.pc = 0x1010;
        report.relatedPoint.exec.offset = 0x10;
        report.relatedPoint.exec.line = 24;
    } else if (reason == NpusanSyncMismatchReason::kUnmatchedClose) {
        report.triggerPoint.operation = close;
        report.relatedPoint.operation = open;
    } else {
        report.triggerPoint.operation = open;
        report.relatedPoint.operation = close;
    }
    report.detail = setWait
                        ? NpusanSyncPairingError{reason, {pairKind, 1, 4, 0, 42}}
                        : NpusanSyncPairingError{reason, {pairKind, 0, 0, 3, 42}};
    return report;
}

NpusanSynccheckReport MakeSynccheckReport(NpusanSynccheckPattern pattern)
{
    if (pattern == NpusanSynccheckPattern::kPairingMismatch) {
        return MakePairingReport(
            NpusanSyncMismatchReason::kUnmatchedClose, NpusanSyncPairKind::kSetWaitFlag);
    }

    NpusanSynccheckReport report{};
    report.common.tool = ReportTool::kSynccheck;
    report.common.pattern = static_cast<std::uint32_t>(pattern);
    report.common.severity = ReportSeverity::kError;
    report.common.flags = aclsan::cann::kNpusanReportCommonHasExecContext;
    report.triggerPoint.operation = "SYNC_OPERATION";
    report.triggerPoint.hasExecContext = true;
    report.triggerPoint.exec.coreId = 0;
    report.triggerPoint.exec.blockId = 1;
    report.triggerPoint.exec.pipeName = "S";
    report.triggerPoint.exec.pc = 0x100;
    report.triggerPoint.exec.kernelName = "sync_kernel";
    report.common.exec = report.triggerPoint.exec;

    switch (pattern) {
        case NpusanSynccheckPattern::kIntraCoreDivergent:
        case NpusanSynccheckPattern::kInterCoreDivergent:
        case NpusanSynccheckPattern::kParticipantMismatch:
            report.primitiveKind = NpusanSyncPrimitiveKind::kBarrier;
            report.detailKind = NpusanSyncDetailKind::kBarrier;
            report.detail = NpusanSyncBarrierError{"participant set differs", "AICore", 0x3, 0xf, 1};
            break;
        case NpusanSynccheckPattern::kInvalidArgument:
        case NpusanSynccheckPattern::kObjectNotInitialized:
        case NpusanSynccheckPattern::kDeadlock:
            report.primitiveKind = NpusanSyncPrimitiveKind::kSyncObject;
            report.detailKind = NpusanSyncDetailKind::kObject;
            report.detail = NpusanSyncObjectError{"sync object error", 1, 0x1000, 0x3, 1000};
            break;
        case NpusanSynccheckPattern::kInstructionSequenceMismatch:
            report.primitiveKind = NpusanSyncPrimitiveKind::kInstructionSequence;
            report.detailKind = NpusanSyncDetailKind::kSequence;
            report.hasRelatedPoint = true;
            report.relatedPoint.operation = "EXPECTED_OPERATION";
            report.detail = NpusanSyncSequenceError{"instruction order differs", 2, 0x3};
            break;
        case NpusanSynccheckPattern::kPairingMismatch:
            break;
    }
    return report;
}

ReportRecord MakeSocRecord()
{
    return ReportRecord{
        {ReportTool::kSoccheck, "register_mismatch"},
        ReportSeverity::kFatal,
        {
            {"function", "soc_kernel"},
            {"offset", "4"},
            {"file", "soc.cpp"},
            {"line", "9"},
            {"coreId", "6"},
            {"blockId", "1"},
            {"pipeName", "S"},
            {"registerId", "17"},
            {"expectedValue", "10"},
            {"observedValue", "11"},
        }};
}

TEST(ReportRendererTest, RendersTemplateAndWritesText)
{
    const ReportTemplate tpl{"========= {{severity}}: {{title}}\n{{body}}\n"};
    const ReportFields fields{
        {"severity", "ERROR"},
        {"title", "Invalid access"},
        {"body", "=========     at foo+0x10 in bar.cpp:27"},
    };

    std::string rendered;
    EXPECT_EQ(aclsan::cann::RenderReportText(tpl, fields, &rendered), ReportRenderStatus::kSuccess);
    EXPECT_EQ(rendered, "========= ERROR: Invalid access\n=========     at foo+0x10 in bar.cpp:27\n");

    std::ostringstream stream;
    EXPECT_EQ(aclsan::cann::WriteReportTextToStream(rendered, &stream), ReportRenderStatus::kSuccess);
    EXPECT_EQ(stream.str(), rendered);

    const std::string path = "/tmp/aclsan_report_renderer_smoke.txt";
    EXPECT_EQ(aclsan::cann::WriteReportTextToFile(rendered, path), ReportRenderStatus::kSuccess);
    EXPECT_EQ(ReadFile(path), rendered);
}

TEST(ReportRendererTest, ReportsTemplateErrors)
{
    const ReportTemplate tpl{"========= {{severity}}: {{title}}\n"};
    const ReportFields fields{{"severity", "ERROR"}, {"title", "Invalid access"}};
    std::string rendered;

    EXPECT_EQ(aclsan::cann::RenderReportText(ReportTemplate{"{{missing}}"}, fields, &rendered),
              ReportRenderStatus::kMissingField);
    EXPECT_EQ(aclsan::cann::RenderReportText(ReportTemplate{"{{missing"}, fields, &rendered),
              ReportRenderStatus::kMalformedTemplate);
    EXPECT_EQ(aclsan::cann::RenderReportText(tpl, fields, nullptr), ReportRenderStatus::kInvalidArgument);
    EXPECT_EQ(aclsan::cann::WriteReportTextToStream(rendered, nullptr), ReportRenderStatus::kInvalidArgument);
    EXPECT_EQ(aclsan::cann::WriteReportTextToFile(rendered, ""), ReportRenderStatus::kOpenFailed);
}

TEST(ReportRendererTest, ListsAndRendersBuiltinTemplates)
{
    const auto builtinKeys = aclsan::cann::ListBuiltinReportTemplates();
    EXPECT_GE(builtinKeys.size(), 34U);
    EXPECT_NE(std::find(builtinKeys.begin(), builtinKeys.end(),
                        ReportTemplateKey{ReportTool::kMemcheck, "invalid_access"}),
              builtinKeys.end());
    EXPECT_NE(std::find(builtinKeys.begin(), builtinKeys.end(),
                        ReportTemplateKey{ReportTool::kSynccheck, "pairing_mismatch"}),
              builtinKeys.end());

    std::string rendered;
    EXPECT_EQ(aclsan::cann::RenderReportRecord(MakeMemcheckInvalidAccessRecord(), {}, &rendered),
              ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("========= ERROR: Invalid GM read of size 16 bytes"), std::string::npos);
    EXPECT_NE(rendered.find("by aicore (3) block (7) pipe (MTE2)"), std::string::npos);

    EXPECT_EQ(aclsan::cann::RenderReportRecord(MakeInitcheckRecord(), {}, &rendered),
              ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("========= ERROR: Uninitialized GM memory read of size 32 bytes"),
              std::string::npos);

    EXPECT_EQ(aclsan::cann::RenderReportRecord(MakeRaceRecord(), {}, &rendered), ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("========= WARNING: Potential RAW hazard detected at UB 0x2000 in block (8) :"),
              std::string::npos);

    EXPECT_EQ(aclsan::cann::RenderReportRecord(MakeSyncRecord(), {}, &rendered), ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("Synchronization pairing mismatch: unmatched WAIT_FLAG"), std::string::npos);

    EXPECT_EQ(aclsan::cann::RenderReportRecord(MakeSocRecord(), {}, &rendered), ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("========= FATAL: SOC register mismatch detected."), std::string::npos);
}

TEST(ReportRendererTest, AppendsStructuredCallStacks)
{
    ReportRecord record = MakeMemcheckInvalidAccessRecord();

    ReportCallStack rawStack{};
    rawStack.role = ReportStackRole::kHostLaunch;
    rawStack.format = ReportStackFormat::kRawText;
    rawStack.rawText = "=========         Host Frame: aclrtLaunchKernel [0x400123] in libacl.so\n";
    record.stacks.push_back(rawStack);

    ReportCallStack frameStack{};
    frameStack.role = ReportStackRole::kFaultDevice;
    frameStack.format = ReportStackFormat::kFrames;
    frameStack.frames.push_back(ReportFrame{0x100, 0x10, "kernel", "kernel.cpp", 42, 0, 0, 0});
    record.stacks.push_back(frameStack);

    std::string rendered;
    EXPECT_EQ(aclsan::cann::RenderReportRecord(record, {}, &rendered), ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("=========     Saved host backtrace up to runtime launch entry point\n"
                            "=========         Host Frame: aclrtLaunchKernel [0x400123] in libacl.so\n"),
              std::string::npos);
    EXPECT_NE(rendered.find("=========     Device Frame\n"
                            "=========         Device Frame: kernel+0x10 [0x100] in kernel.cpp:42\n"),
              std::string::npos);
}

TEST(ReportRendererTest, RendersOnlyActiveCommonCallStackPrefix)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::kMemcheck;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::kApiError);
    report.common.stackCount = 1;
    report.apiName = "aclrtLaunchKernel";
    report.apiErrorName = "ACL_ERROR_FAILURE";
    report.apiErrorMessage = "launch failed";

    report.common.stacks[0].role = ReportStackRole::kHostLaunch;
    report.common.stacks[0].format = ReportStackFormat::kRawText;
    report.common.stacks[0].rawText = "active host frame";
    report.common.stacks[1].role = ReportStackRole::kHostLaunch;
    report.common.stacks[1].format = ReportStackFormat::kRawText;
    report.common.stacks[1].rawText = "inactive host frame";

    std::string rendered;
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
              ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("runtime API call to aclrtLaunchKernel"), std::string::npos);
    EXPECT_NE(rendered.find("active host frame"), std::string::npos);
    EXPECT_EQ(rendered.find("inactive host frame"), std::string::npos);
}

TEST(ReportRendererTest, UsesExplicitUnknownPhysicalCoreSentinel)
{
    EXPECT_EQ(aclsan::cann::NpusanReportExecContext{}.coreId,
              std::numeric_limits<std::uint32_t>::max());
    EXPECT_EQ(aclsan::cann::NpusanSyncPoint{}.exec.coreId,
              std::numeric_limits<std::uint32_t>::max());
    EXPECT_EQ(aclsan::cann::NpusanSocStateRef{}.ownerCoreId,
              std::numeric_limits<std::uint32_t>::max());
}

TEST(ReportRendererTest, PrefersStructuredFaultFrameForSourceLocation)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::kMemcheck;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::kInvalidAccess);
    report.common.exec.function = "fallback_function";
    report.common.exec.offset = 0x10;
    report.common.exec.file = "fallback.cpp";
    report.common.exec.line = 7;
    report.common.stackCount = 2;
    report.common.stacks[0].role = ReportStackRole::kFaultDevice;
    report.common.stacks[0].format = ReportStackFormat::kFrames;
    report.common.stacks[0].frames.push_back(ReportFrame{});
    report.common.stacks[0].frames.push_back(
        ReportFrame{0x100, 0x20, "symbolized_function", "symbolized.cpp", 42, 0, 0, 0});
    report.common.stacks[1].role = ReportStackRole::kHostLaunch;
    report.common.stacks[1].format = ReportStackFormat::kRawText;
    report.common.stacks[1].rawText = "real host frame";
    report.access.accessBytes = 4;

    std::string rendered;
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
              ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("at symbolized_function+0x20 in symbolized.cpp:42"), std::string::npos);
    EXPECT_EQ(CountOccurrences(rendered, "Saved host backtrace up to runtime launch entry point"), 1U);
    EXPECT_EQ(rendered.find("Host Frame: <unknown>"), std::string::npos);
    EXPECT_NE(rendered.find("real host frame"), std::string::npos);
}

TEST(ReportRendererTest, UsesAllocationMemorySpaceForAllocationPatterns)
{
    NpusanMemcheckReport leak{};
    leak.common.tool = ReportTool::kMemcheck;
    leak.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::kLeak);
    leak.access.memorySpace = NpusanReportMemorySpace::kUb;
    leak.allocation.memorySpace = NpusanReportMemorySpace::kGm;

    std::string rendered;
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(leak), {}, &rendered),
              ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("memory space GM"), std::string::npos);

    NpusanInitcheckReport unused{};
    unused.common.tool = ReportTool::kInitcheck;
    unused.common.pattern = static_cast<std::uint32_t>(NpusanInitcheckPattern::kUnusedMemory);
    unused.access.memorySpace = NpusanReportMemorySpace::kUb;
    unused.allocation.memorySpace = NpusanReportMemorySpace::kL1;

    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(unused), {}, &rendered),
              ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("Unused L1 memory"), std::string::npos);
}

TEST(ReportRendererTest, UsesRaceAccessCoreForCrossPipeTemplate)
{
    NpusanRacecheckReport report{};
    report.common.tool = ReportTool::kRacecheck;
    report.common.pattern = static_cast<std::uint32_t>(NpusanRacecheckPattern::kCrossPipeRace);
    report.first.exec.coreId = 7;
    report.second.exec.coreId = 7;
    report.first.exec.pipeName = "MTE2";
    report.second.exec.pipeName = "V";

    std::string rendered;
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
              ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("First access by aicore (7)"), std::string::npos);
    EXPECT_NE(rendered.find("Second access by aicore (7)"), std::string::npos);
}

TEST(ReportRendererTest, FallsBackToProgramCounterWhenFaultIsNotSymbolized)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::kMemcheck;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::kMisalignedAccess);
    report.common.exec.pc = 0xabc;
    report.common.exec.kernelName = "fallback_kernel";

    std::string rendered;
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
              ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("at pc 0xabc in fallback_kernel"), std::string::npos);
    EXPECT_EQ(rendered.find("<unknown>+0x0 in <unknown>:0"), std::string::npos);
    EXPECT_NE(rendered.find("by aicore (<unknown>)"), std::string::npos);
    EXPECT_EQ(rendered.find("4294967295"), std::string::npos);
}

TEST(ReportRendererTest, FallsBackToEachRaceSiteProgramCounter)
{
    NpusanRacecheckReport report{};
    report.common.tool = ReportTool::kRacecheck;
    report.common.pattern = static_cast<std::uint32_t>(NpusanRacecheckPattern::kHazardRaw);
    report.first.exec.pc = 0x111;
    report.first.exec.kernelName = "writer_kernel";
    report.second.exec.pc = 0x222;
    report.second.exec.kernelName = "reader_kernel";

    std::string rendered;
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
              ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("at pc 0x111 in writer_kernel"), std::string::npos);
    EXPECT_NE(rendered.find("at pc 0x222 in reader_kernel"), std::string::npos);
}

TEST(ReportRendererTest, UsesFirstRaceSiteAsInvalidRemoteAccessLocation)
{
    NpusanRacecheckReport report{};
    report.common.tool = ReportTool::kRacecheck;
    report.common.pattern = static_cast<std::uint32_t>(NpusanRacecheckPattern::kInvalidRemoteAccess);
    report.common.exec.coreId = 1;
    report.first.exec.coreId = 6;
    report.first.exec.blockId = 7;
    report.first.exec.pipeName = "MTE2";
    report.first.exec.pc = 0x345;
    report.first.exec.kernelName = "remote_caller";

    std::string rendered;
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
              ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("at pc 0x345 in remote_caller"), std::string::npos);
    EXPECT_NE(rendered.find("by aicore (6) block (7) pipe (MTE2)"), std::string::npos);
}

TEST(ReportRendererTest, RejectsCrossPipeRaceAcrossDifferentPhysicalCores)
{
    NpusanRacecheckReport report{};
    report.common.tool = ReportTool::kRacecheck;
    report.common.pattern = static_cast<std::uint32_t>(NpusanRacecheckPattern::kCrossPipeRace);
    report.first.exec.coreId = 2;
    report.second.exec.coreId = 3;

    std::string rendered = "stale";
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
              ReportRenderStatus::kInvalidArgument);
    EXPECT_TRUE(rendered.empty());
}

TEST(ReportRendererTest, RejectsCommonCallStackCountBeyondFixedCapacity)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::kMemcheck;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::kApiError);
    report.common.stackCount = 9;

    std::string rendered = "stale";
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
              ReportRenderStatus::kInvalidArgument);
    EXPECT_TRUE(rendered.empty());
}

TEST(ReportRendererTest, RejectsRecordToolMismatch)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::kInitcheck;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::kApiError);

    std::string rendered = "stale";
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
              ReportRenderStatus::kInvalidArgument);
    EXPECT_TRUE(rendered.empty());
}

TEST(ReportRendererTest, RejectsOuterToolAndPayloadMismatchSafely)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::kMemcheck;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::kApiError);
    NpusanReportRecord record = NpusanReportRecord::From(report);
    ASSERT_TRUE(std::holds_alternative<const NpusanMemcheckReport*>(record.GetPayload()));
    record.tool = ReportTool::kInitcheck;

    std::string rendered = "stale";
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(record, {}, &rendered),
              ReportRenderStatus::kInvalidArgument);
    EXPECT_TRUE(rendered.empty());
}

TEST(ReportRendererTest, RejectsRecordPatternMismatch)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::kMemcheck;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::kApiError);
    NpusanReportRecord record = NpusanReportRecord::From(report);
    record.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::kLeak);

    std::string rendered = "stale";
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(record, {}, &rendered),
              ReportRenderStatus::kInvalidArgument);
    EXPECT_TRUE(rendered.empty());
}

TEST(ReportRendererTest, RejectsNullSelectedReportPointer)
{
    NpusanReportRecord record{};
    record.tool = ReportTool::kMemcheck;
    record.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::kApiError);

    std::string rendered = "stale";
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(record, {}, &rendered),
              ReportRenderStatus::kInvalidArgument);
    EXPECT_TRUE(rendered.empty());
}

TEST(ReportRendererTest, RejectsDuplicateActiveCallStackRoles)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::kMemcheck;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::kApiError);
    report.common.stackCount = 2;
    report.common.stacks[0].role = ReportStackRole::kHostLaunch;
    report.common.stacks[1].role = ReportStackRole::kHostLaunch;

    std::string rendered = "stale";
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
              ReportRenderStatus::kInvalidArgument);
    EXPECT_TRUE(rendered.empty());
}

TEST(ReportRendererTest, ValidatesActiveCallStackMetadataAndBoundaries)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::kMemcheck;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::kApiError);
    report.common.stackCount = aclsan::cann::kNpusanReportStackMax;
    for (std::uint32_t i = 0; i < report.common.stackCount; ++i) {
        report.common.stacks[i].role = static_cast<ReportStackRole>(i + 1);
        report.common.stacks[i].format = ReportStackFormat::kNone;
    }

    std::string rendered;
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
              ReportRenderStatus::kSuccess);

    report.common.stackCount = 1;
    report.common.stacks[0].role = static_cast<ReportStackRole>(0);
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
              ReportRenderStatus::kInvalidArgument);

    report.common.stacks[0].role = ReportStackRole::kFaultDevice;
    report.common.stacks[0].format = static_cast<ReportStackFormat>(99);
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
              ReportRenderStatus::kInvalidArgument);

    report.common.stacks[0].format = ReportStackFormat::kFrames;
    report.common.stacks[0].frames.resize(17);
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
              ReportRenderStatus::kInvalidArgument);
}

TEST(ReportRendererTest, LoadsTemplateOverrides)
{
    const std::string overridePath = "/tmp/aclsan_report_renderer_templates.conf";
    {
        std::ofstream config(overridePath, std::ios::binary);
        config << "memcheck.invalid_access=USER {{Severity}} {{address}}\\n";
    }

    ReportTemplateOverrides overrides;
    std::string rendered;
    EXPECT_EQ(aclsan::cann::LoadReportTemplateOverridesFromFile(overridePath, &overrides),
              ReportRenderStatus::kSuccess);
    EXPECT_EQ(aclsan::cann::RenderReportRecord(MakeMemcheckInvalidAccessRecord(), overrides, &rendered),
              ReportRenderStatus::kSuccess);
    EXPECT_EQ(rendered, "USER ERROR 1000\n");

    const ReportRecord unknownRecord{{ReportTool::kMemcheck, "unknown"}, ReportSeverity::kError, {}};
    EXPECT_EQ(aclsan::cann::RenderReportRecord(unknownRecord, {}, &rendered),
              ReportRenderStatus::kUnknownTemplate);
}

TEST(ReportRendererTest, RendersBundleSummaries)
{
    const ReportRecord leakRecord{
        {ReportTool::kMemcheck, "leak"},
        ReportSeverity::kWarning,
        {
            {"bytes", "128"},
            {"base", "0fc0"},
            {"allocId", "12"},
            {"space", "GM"},
            {"allocFunction", "aclrtMalloc"},
            {"allocPc", "400456"},
            {"hostBinary", "libacl.so"},
        }};
    const ReportRecord unusedRecord{
        {ReportTool::kInitcheck, "unused_memory"},
        ReportSeverity::kInfo,
        {
            {"space", "GM"},
            {"base", "0fc0"},
            {"bytes", "128"},
            {"unusedBytes", "64"},
            {"firstUninitOffset", "40"},
            {"firstUninitAddress", "1000"},
            {"unusedPercent", "50"},
        }};
    const ReportRecord deadlockRecord{
        {ReportTool::kSynccheck, "deadlock"},
        ReportSeverity::kError,
        {
            {"reason", "wait cannot complete"},
            {"triggerOperation", "WAIT_FLAG"},
            {"triggerCoreId", "2"},
            {"triggerBlock", "5"},
            {"triggerPipe", "V"},
            {"triggerLocation", "pc 0x5000 in sync_kernel"},
            {"relatedPointLine", ""},
            {"waitingMask", "f"},
            {"timeoutNs", "1000"},
            {"objectLine", "=========     sync object 0x99\n"},
        }};
    const std::vector<ReportRecord> records{
        MakeMemcheckInvalidAccessRecord(), MakeRaceRecord(), MakeSocRecord(), leakRecord, unusedRecord,
        deadlockRecord};

    std::string rendered;
    EXPECT_EQ(aclsan::cann::RenderReportBundle(records, {}, &rendered), ReportRenderStatus::kSuccess);
    EXPECT_EQ(rendered.rfind("========= NPUSAN\n", 0), 0U);
    EXPECT_NE(rendered.find("========= MEMCHECK SUMMARY: 1 errors, 1 warnings, 0 infos, 1 leaks"),
              std::string::npos);
    EXPECT_NE(rendered.find("========= INITCHECK SUMMARY: 0 errors, 0 warnings, 1 infos, 1 unused memory reports"),
              std::string::npos);
    EXPECT_NE(rendered.find("========= RACECHECK SUMMARY: 1 hazard displayed (0 errors, 1 warnings, 0 infos)"),
              std::string::npos);
    EXPECT_NE(rendered.find("========= SYNCCHECK SUMMARY: 1 errors, 0 warnings, 0 infos, 1 deadlocks"),
              std::string::npos);
    EXPECT_NE(rendered.find("========= SOCCHECK SUMMARY: 1 errors, 0 warnings, 0 infos"), std::string::npos);
    EXPECT_NE(rendered.find("========= ERROR SUMMARY: 3 errors\n"
                            "=========     MEMCHECK: 1 errors\n"
                            "=========     INITCHECK: 0 errors\n"
                            "=========     RACECHECK: 0 errors\n"
                            "=========     SYNCCHECK: 1 errors\n"
                            "=========     SOCCHECK: 1 errors\n"
                            "=========     FATAL: 1 fatal errors\n"),
              std::string::npos);
}

TEST(ReportRendererTest, ProvidesDefaultSeverities)
{
    EXPECT_EQ(aclsan::cann::DefaultReportSeverity(ReportTool::kInitcheck, "unused_memory"),
              ReportSeverity::kWarning);
    EXPECT_EQ(aclsan::cann::DefaultReportSeverity(ReportTool::kSynccheck, "participant_mismatch"),
              ReportSeverity::kWarning);
    EXPECT_EQ(aclsan::cann::DefaultReportSeverity(ReportTool::kSoccheck, "state_not_restored"),
              ReportSeverity::kWarning);
    EXPECT_EQ(aclsan::cann::DefaultReportSeverity(ReportTool::kMemcheck, "invalid_access"),
              ReportSeverity::kError);
}

TEST(ReportRendererTest, RendersPairingMismatchReasonForDifferentOperationKinds)
{
    struct Case {
        NpusanSyncPairKind pairKind;
        NpusanSyncMismatchReason reason;
        const char* expectedHeadline;
        const char* expectedRelated;
    };
    const Case cases[] = {
        {NpusanSyncPairKind::kSetWaitFlag, NpusanSyncMismatchReason::kDuplicateOpen,
         "Synchronization pairing mismatch: duplicate SET_FLAG.",
         "related point: previous SET_FLAG at SyncOperation+0x10 in sync.cpp:24 is still pending"},
        {NpusanSyncPairKind::kSetWaitFlag, NpusanSyncMismatchReason::kUnmatchedClose,
         "Synchronization pairing mismatch: unmatched WAIT_FLAG.",
         "related point: expected SET_FLAG, but no matching point exists for this pair key"},
        {NpusanSyncPairKind::kGetRlsBuf, NpusanSyncMismatchReason::kUnconsumedOpen,
         "Synchronization pairing mismatch: redundant GET_BUF.",
         "related point: expected RLS_BUF, but no matching point was observed"},
    };

    for (const Case& testCase : cases) {
        NpusanSynccheckReport report = MakePairingReport(testCase.reason, testCase.pairKind);

        std::string rendered;
        ASSERT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
                  ReportRenderStatus::kSuccess);
        EXPECT_NE(rendered.find(testCase.expectedHeadline), std::string::npos);
        EXPECT_NE(rendered.find(testCase.expectedRelated), std::string::npos);
        EXPECT_EQ(rendered.find("observed sequence"), std::string::npos);
    }
}

TEST(ReportRendererTest, SeparatesBundleRecordsWithOneEmptyLine)
{
    const ReportTemplateKey key{ReportTool::kMemcheck, "invalid_access"};
    const ReportTemplateOverrides overrides{{key, {"{{recordName}}\n"}}};
    const std::vector<ReportRecord> records{
        {key, ReportSeverity::kError, {{"recordName", "first record"}}},
        {key, ReportSeverity::kError, {{"recordName", "second record"}}},
    };

    std::string rendered;
    ASSERT_EQ(aclsan::cann::RenderReportBundle(records, overrides, &rendered), ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("first record\n\nsecond record\n"), std::string::npos);
    EXPECT_EQ(rendered.find("first record\n\n\nsecond record\n"), std::string::npos);
}

TEST(ReportRendererTest, RendersStructuredPairingMismatchEvidence)
{
    NpusanSynccheckReport report = MakePairingReport(
        NpusanSyncMismatchReason::kDuplicateOpen, NpusanSyncPairKind::kSetWaitFlag);
    report.common.stackCount = 2;
    report.common.stacks[0].role = ReportStackRole::kSyncTrigger;
    report.common.stacks[0].format = ReportStackFormat::kFrames;
    report.common.stacks[0].frames.push_back(
        ReportFrame{0, 0x20, "SecondSet", "sync.cpp", 30});
    report.common.stacks[1].role = ReportStackRole::kSyncRelated;
    report.common.stacks[1].format = ReportStackFormat::kFrames;
    report.common.stacks[1].frames.push_back(
        ReportFrame{0x1010, 0x10, "FirstSet", "sync.cpp", 24});
    report.triggerPoint.stackRole = ReportStackRole::kSyncTrigger;
    report.relatedPoint.stackRole = ReportStackRole::kSyncRelated;

    std::string rendered;
    ASSERT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
              ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("Synchronization pairing mismatch: duplicate SET_FLAG."), std::string::npos);
    EXPECT_NE(rendered.find("related point: previous SET_FLAG at FirstSet+0x10 in sync.cpp:24 is still pending"),
              std::string::npos);
    EXPECT_NE(rendered.find("expected WAIT_FLAG before another SET_FLAG"), std::string::npos);
    EXPECT_NE(rendered.find("Trigger Point Device Backtrace:"), std::string::npos);
    EXPECT_NE(rendered.find("Related Point Device Backtrace:"), std::string::npos);
}

TEST(ReportRendererTest, RendersUnconsumedGetBufferWithExpectedRelatedPoint)
{
    NpusanSynccheckReport report{};
    report.common.tool = ReportTool::kSynccheck;
    report.common.pattern = static_cast<std::uint32_t>(NpusanSynccheckPattern::kPairingMismatch);
    report.common.severity = ReportSeverity::kError;
    report.common.flags = aclsan::cann::kNpusanReportCommonHasExecContext;
    report.primitiveKind = NpusanSyncPrimitiveKind::kGetRlsBuf;
    report.detailKind = NpusanSyncDetailKind::kPairing;
    report.triggerPoint.operation = "GET_BUF";
    report.triggerPoint.hasExecContext = true;
    report.triggerPoint.exec.coreId = 1;
    report.triggerPoint.exec.blockId = 4;
    report.triggerPoint.exec.pipeName = "MTE2";
    report.triggerPoint.exec.function = "AcquireBuf";
    report.triggerPoint.exec.offset = 0x10;
    report.triggerPoint.exec.file = "sync.cpp";
    report.triggerPoint.exec.line = 20;
    report.common.exec = report.triggerPoint.exec;
    report.hasRelatedPoint = true;
    report.relatedPoint.operation = "RLS_BUF";
    report.detail = NpusanSyncPairingError{
        NpusanSyncMismatchReason::kUnconsumedOpen,
        {NpusanSyncPairKind::kGetRlsBuf, 0, 0, 3, 42},
    };

    std::string rendered;
    ASSERT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
              ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("Synchronization pairing mismatch: redundant GET_BUF."), std::string::npos);
    EXPECT_NE(rendered.find("related point: expected RLS_BUF, but no matching point was observed"),
              std::string::npos);
    EXPECT_NE(rendered.find("pair kind GET_RLS_BUF, key (id=42, mode=3)"), std::string::npos);
}

TEST(ReportRendererTest, RejectsInvalidPairingMismatchMetadata)
{
    std::string rendered;
    {
        NpusanSynccheckReport report = MakePairingReport(
            NpusanSyncMismatchReason::kUnmatchedClose, NpusanSyncPairKind::kSetWaitFlag);
        report.detailKind = NpusanSyncDetailKind::kBarrier;
        EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
                  ReportRenderStatus::kInvalidArgument);
    }
    {
        NpusanSynccheckReport report = MakePairingReport(
            NpusanSyncMismatchReason::kUnmatchedClose, NpusanSyncPairKind::kSetWaitFlag);
        std::get<NpusanSyncPairingError>(report.detail).reason = NpusanSyncMismatchReason::kUnknown;
        EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
                  ReportRenderStatus::kInvalidArgument);
    }
    {
        NpusanSynccheckReport report = MakePairingReport(
            NpusanSyncMismatchReason::kUnconsumedOpen, NpusanSyncPairKind::kGetRlsBuf);
        report.primitiveKind = NpusanSyncPrimitiveKind::kSetWaitFlag;
        EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
                  ReportRenderStatus::kInvalidArgument);
    }
    {
        NpusanSynccheckReport report = MakePairingReport(
            NpusanSyncMismatchReason::kUnconsumedOpen, NpusanSyncPairKind::kGetRlsBuf);
        std::get<NpusanSyncPairingError>(report.detail).key.srcPipe = 1;
        EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
                  ReportRenderStatus::kInvalidArgument);
    }
    {
        NpusanSynccheckReport report = MakePairingReport(
            NpusanSyncMismatchReason::kUnconsumedOpen, NpusanSyncPairKind::kGetRlsBuf);
        report.relatedPoint.hasExecContext = true;
        EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
                  ReportRenderStatus::kInvalidArgument);
    }
    {
        NpusanSynccheckReport report = MakePairingReport(
            NpusanSyncMismatchReason::kDuplicateOpen, NpusanSyncPairKind::kSetWaitFlag);
        report.common.exec.pc = 0xdead;
        EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
                  ReportRenderStatus::kInvalidArgument);
    }
}

TEST(ReportRendererTest, RejectsOrphanRelatedPointAndMismatchedPointStackPc)
{
    std::string rendered;
    {
        NpusanSynccheckReport report = MakeSynccheckReport(NpusanSynccheckPattern::kIntraCoreDivergent);
        report.relatedPoint.operation = "UNREFERENCED_OPERATION";
        EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
                  ReportRenderStatus::kInvalidArgument);
    }
    {
        NpusanSynccheckReport report = MakePairingReport(
            NpusanSyncMismatchReason::kDuplicateOpen, NpusanSyncPairKind::kSetWaitFlag);
        report.triggerPoint.exec.pc = 0x1000;
        report.common.exec = report.triggerPoint.exec;
        report.triggerPoint.stackRole = ReportStackRole::kSyncTrigger;
        report.common.stackCount = 1;
        report.common.stacks[0].role = ReportStackRole::kSyncTrigger;
        report.common.stacks[0].format = ReportStackFormat::kFrames;
        report.common.stacks[0].frames.push_back(
            ReportFrame{0x2000, 0x20, "DifferentInstruction", "sync.cpp", 30});
        EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
                  ReportRenderStatus::kInvalidArgument);
    }
    {
        NpusanSynccheckReport report = MakePairingReport(
            NpusanSyncMismatchReason::kUnmatchedClose, NpusanSyncPairKind::kSetWaitFlag);
        report.common.flags = 0;
        EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
                  ReportRenderStatus::kInvalidArgument);
    }
    {
        NpusanSynccheckReport report = MakeSynccheckReport(NpusanSynccheckPattern::kInvalidArgument);
        report.primitiveKind = static_cast<NpusanSyncPrimitiveKind>(99);
        EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
                  ReportRenderStatus::kInvalidArgument);
    }
}

TEST(ReportRendererTest, RendersStructuredReportsFromEachCheckerStruct)
{
    NpusanMemcheckReport memcheck{};
    memcheck.common.tool = ReportTool::kMemcheck;
    memcheck.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::kInvalidAccess);
    memcheck.common.severity = ReportSeverity::kError;
    memcheck.common.exec.function = "kernel";
    memcheck.common.exec.offset = 0x10;
    memcheck.common.exec.file = "kernel.cpp";
    memcheck.common.exec.line = 42;
    memcheck.common.exec.coreId = 3;
    memcheck.common.exec.blockId = 7;
    memcheck.common.exec.pipeName = "MTE2";
    memcheck.access.memorySpace = NpusanReportMemorySpace::kGm;
    memcheck.access.accessMode = NpusanReportAccessMode::kRead;
    memcheck.access.accessBytes = 16;
    memcheck.access.address = 0x1000;
    memcheck.nearestAllocation.base = 0x0fc0;
    memcheck.nearestAllocation.bytes = 128;
    memcheck.distanceKind = NpusanReportDistanceKind::kAfter;
    memcheck.distanceBytes = 64;

    NpusanInitcheckReport initcheck{};
    initcheck.common.tool = ReportTool::kInitcheck;
    initcheck.common.pattern = static_cast<std::uint32_t>(NpusanInitcheckPattern::kUninitializedRead);
    initcheck.common.severity = ReportSeverity::kError;
    initcheck.common.exec.function = "init_kernel";
    initcheck.common.exec.offset = 0x18;
    initcheck.common.exec.file = "init.cpp";
    initcheck.common.exec.line = 21;
    initcheck.common.exec.coreId = 4;
    initcheck.common.exec.blockId = 2;
    initcheck.common.exec.pipeName = "MTE2";
    initcheck.access.memorySpace = NpusanReportMemorySpace::kGm;
    initcheck.access.accessBytes = 32;
    initcheck.access.address = 0x3000;

    NpusanRacecheckReport racecheck{};
    racecheck.common.tool = ReportTool::kRacecheck;
    racecheck.common.pattern = static_cast<std::uint32_t>(NpusanRacecheckPattern::kHazardRaw);
    racecheck.common.severity = ReportSeverity::kWarning;
    racecheck.first.access.memorySpace = NpusanReportMemorySpace::kUb;
    racecheck.first.access.address = 0x2000;
    racecheck.first.exec.blockId = 8;
    racecheck.first.exec.coreId = 0;
    racecheck.first.exec.pipeName = "MTE3";
    racecheck.first.exec.function = "writer";
    racecheck.first.exec.offset = 0x20;
    racecheck.first.exec.file = "race.cpp";
    racecheck.first.exec.line = 11;
    racecheck.second.exec.coreId = 1;
    racecheck.second.exec.pipeName = "MTE2";
    racecheck.second.exec.function = "reader";
    racecheck.second.exec.offset = 0x30;
    racecheck.second.exec.file = "race.cpp";
    racecheck.second.exec.line = 19;
    racecheck.currentValue = 0xab;

    NpusanSynccheckReport synccheck = MakePairingReport(
        NpusanSyncMismatchReason::kUnmatchedClose, NpusanSyncPairKind::kSetWaitFlag);

    NpusanSoccheckReport soccheck{};
    soccheck.common.tool = ReportTool::kSoccheck;
    soccheck.common.pattern = static_cast<std::uint32_t>(NpusanSoccheckPattern::kRegisterMismatch);
    soccheck.common.severity = ReportSeverity::kFatal;
    soccheck.common.exec.function = "soc_kernel";
    soccheck.common.exec.offset = 0x4;
    soccheck.common.exec.file = "soc.cpp";
    soccheck.common.exec.line = 9;
    soccheck.common.exec.coreId = 6;
    soccheck.common.exec.blockId = 1;
    soccheck.common.exec.pipeName = "S";
    soccheck.state.registerId = 17;
    soccheck.state.expectedValue = 0x10;
    soccheck.state.observedValue = 0x11;

    std::vector<NpusanReportRecord> records{
        NpusanReportRecord::From(memcheck),
        NpusanReportRecord::From(initcheck),
        NpusanReportRecord::From(racecheck),
        NpusanReportRecord::From(synccheck),
        NpusanReportRecord::From(soccheck),
    };

    std::string rendered;
    for (const NpusanReportRecord& record : records) {
        EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(record, {}, &rendered), ReportRenderStatus::kSuccess);
    }
    EXPECT_EQ(aclsan::cann::RenderNpusanReportBundle(records, {}, &rendered), ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("========= ERROR: Invalid GM read of size 16 bytes"), std::string::npos);
    EXPECT_NE(rendered.find("========= ERROR: Uninitialized GM memory read of size 32 bytes"), std::string::npos);
    EXPECT_NE(rendered.find("========= WARNING: Potential RAW hazard detected at UB 0x2000 in block (8) :"),
              std::string::npos);
    EXPECT_NE(rendered.find("========= ERROR: Synchronization pairing mismatch: unmatched WAIT_FLAG."),
              std::string::npos);
    EXPECT_NE(rendered.find("========= FATAL: SOC register mismatch detected."), std::string::npos);
    EXPECT_NE(rendered.find("========= ERROR SUMMARY: 4 errors"), std::string::npos);
}

TEST(ReportRendererTest, RendersAllStructuredPatternTemplates)
{
    std::string rendered;

    for (const auto pattern : {NpusanMemcheckPattern::kInvalidAccess, NpusanMemcheckPattern::kMisalignedAccess,
                              NpusanMemcheckPattern::kUseAfterFree, NpusanMemcheckPattern::kUseBeforeAlloc,
                              NpusanMemcheckPattern::kInvalidFree, NpusanMemcheckPattern::kDoubleFree,
                              NpusanMemcheckPattern::kLeak, NpusanMemcheckPattern::kApiError}) {
        NpusanMemcheckReport report{};
        report.common.tool = ReportTool::kMemcheck;
        report.common.pattern = static_cast<std::uint32_t>(pattern);
        report.common.severity = ReportSeverity::kError;
        EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
                  ReportRenderStatus::kSuccess);
    }

    for (const auto pattern : {NpusanInitcheckPattern::kUninitializedRead,
                              NpusanInitcheckPattern::kPartialUninitializedRead,
                              NpusanInitcheckPattern::kUnusedMemory,
                              NpusanInitcheckPattern::kApiReadUninitialized}) {
        NpusanInitcheckReport report{};
        report.common.tool = ReportTool::kInitcheck;
        report.common.pattern = static_cast<std::uint32_t>(pattern);
        report.common.severity = ReportSeverity::kError;
        EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
                  ReportRenderStatus::kSuccess);
    }

    for (const auto pattern : {NpusanRacecheckPattern::kAnalysis, NpusanRacecheckPattern::kHazardRaw,
                              NpusanRacecheckPattern::kHazardWar, NpusanRacecheckPattern::kHazardWaw,
                              NpusanRacecheckPattern::kAtomicRace, NpusanRacecheckPattern::kCrossPipeRace,
                              NpusanRacecheckPattern::kInterCoreRace,
                              NpusanRacecheckPattern::kInvalidRemoteAccess}) {
        NpusanRacecheckReport report{};
        report.common.tool = ReportTool::kRacecheck;
        report.common.pattern = static_cast<std::uint32_t>(pattern);
        report.common.severity = ReportSeverity::kWarning;
        EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
                  ReportRenderStatus::kSuccess);
    }

    for (const auto pattern : {NpusanSynccheckPattern::kIntraCoreDivergent,
                              NpusanSynccheckPattern::kInterCoreDivergent,
                              NpusanSynccheckPattern::kInvalidArgument,
                              NpusanSynccheckPattern::kPairingMismatch,
                              NpusanSynccheckPattern::kParticipantMismatch,
                              NpusanSynccheckPattern::kDeadlock,
                              NpusanSynccheckPattern::kObjectNotInitialized,
                              NpusanSynccheckPattern::kInstructionSequenceMismatch}) {
        NpusanSynccheckReport report = MakeSynccheckReport(pattern);
        EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
                  ReportRenderStatus::kSuccess);
    }

    for (const auto pattern : {NpusanSoccheckPattern::kUninitializedStateRead,
                              NpusanSoccheckPattern::kRegisterMismatch,
                              NpusanSoccheckPattern::kIllegalStateTransition,
                              NpusanSoccheckPattern::kStateNotRestored,
                              NpusanSoccheckPattern::kCrossCoreStateInconsistent,
                              NpusanSoccheckPattern::kScopeViolation}) {
        NpusanSoccheckReport report{};
        report.common.tool = ReportTool::kSoccheck;
        report.common.pattern = static_cast<std::uint32_t>(pattern);
        report.common.severity = ReportSeverity::kError;
        EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
                  ReportRenderStatus::kSuccess);
    }
}

} // namespace

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
