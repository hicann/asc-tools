// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "diagnostic/report_renderer.h"
#include "diagnostic/report/report_catalog.h"
#include "diagnostic/report/report_fields.h"

#include "aclsan/aclsan_cbdata_device.h"
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

using aclsan::cann::NpusanInitcheckPattern;
using aclsan::cann::NpusanInitcheckReport;
using aclsan::cann::NpusanMemcheckPattern;
using aclsan::cann::NpusanMemcheckReport;
using aclsan::cann::NpusanRaceAccessSite;
using aclsan::cann::NpusanRacecheckPattern;
using aclsan::cann::NpusanRacecheckReport;
using aclsan::cann::NpusanReportAccessMode;
using aclsan::cann::NpusanReportAllocation;
using aclsan::cann::NpusanReportCommon;
using aclsan::cann::NpusanReportDistanceKind;
using aclsan::cann::NpusanReportMemoryAccess;
using aclsan::cann::NpusanReportMemorySpace;
using aclsan::cann::NpusanReportRecord;
using aclsan::cann::NpusanSoccheckPattern;
using aclsan::cann::NpusanSoccheckReport;
using aclsan::cann::NpusanSyncBarrierError;
using aclsan::cann::NpusanSynccheckPattern;
using aclsan::cann::NpusanSynccheckReport;
using aclsan::cann::NpusanSyncDetailKind;
using aclsan::cann::NpusanSyncMismatchReason;
using aclsan::cann::NpusanSyncObjectError;
using aclsan::cann::NpusanSyncPairingError;
using aclsan::cann::NpusanSyncPairKind;
using aclsan::cann::NpusanSyncPoint;
using aclsan::cann::NpusanSyncPrimitiveKind;
using aclsan::cann::NpusanSyncSequenceError;
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

template <typename Report, typename = void>
struct CanCreateNpusanRecordFrom : std::false_type {};

template <typename Report>
struct CanCreateNpusanRecordFrom<Report, std::void_t<decltype(NpusanReportRecord::From(std::declval<Report>()))>>
    : std::true_type {};

template <typename Descriptor, typename = void>
struct HasDefaultSeverity : std::false_type {};

template <typename Descriptor>
struct HasDefaultSeverity<Descriptor, std::void_t<decltype(std::declval<Descriptor>().defaultSeverity)>>
    : std::true_type {};

template <typename Descriptor, typename = void>
struct HasSummaryTag : std::false_type {};

template <typename Descriptor>
struct HasSummaryTag<Descriptor, std::void_t<decltype(std::declval<Descriptor>().summaryTag)>> : std::true_type {};

template <typename Report, typename = void>
struct HasPattern : std::false_type {};

template <typename Report>
struct HasPattern<Report, std::void_t<decltype(std::declval<Report>().pattern)>> : std::true_type {};

static_assert(
    !CanCreateNpusanRecordFrom<NpusanMemcheckReport&&>::value, "NpusanReportRecord must not borrow a temporary report");
static_assert(
    !CanCreateNpusanRecordFrom<const NpusanMemcheckReport&&>::value,
    "NpusanReportRecord must not borrow a const temporary report");
static_assert(static_cast<int>(NpusanSynccheckPattern::PAIRING_MISMATCH) == 4);
static_assert(static_cast<int>(NpusanSynccheckPattern::PARTICIPANT_MISMATCH) == 5);
static_assert(static_cast<int>(NpusanSynccheckPattern::DEADLOCK) == 6);
static_assert(static_cast<int>(NpusanSynccheckPattern::OBJECT_NOT_INITIALIZED) == 7);
static_assert(static_cast<int>(NpusanSynccheckPattern::INSTRUCTION_SEQUENCE_MISMATCH) == 8);
static_assert(std::is_same_v<aclsan::cann::detail::PatternCatalog::key_type, ReportTemplateKey>);
static_assert(!HasDefaultSeverity<aclsan::cann::detail::PatternDescriptor>::value);
static_assert(!HasSummaryTag<aclsan::cann::detail::PatternDescriptor>::value);
static_assert(std::is_same_v<decltype(NpusanReportCommon{}.pattern), std::uint32_t>);
static_assert(!HasPattern<NpusanMemcheckReport>::value);
static_assert(!HasPattern<NpusanInitcheckReport>::value);
static_assert(!HasPattern<NpusanRacecheckReport>::value);
static_assert(!HasPattern<NpusanSynccheckReport>::value);
static_assert(!HasPattern<NpusanSoccheckReport>::value);
TEST(ReportFieldsTest, FormatsBlockTypes)
{
    using aclsan::cann::detail::FormatBlockType;
    static_assert(std::is_same_v<decltype(FormatBlockType(ACLSAN_DEVICE_BLOCK_TYPE_AICORE)), std::string>);

    EXPECT_EQ(FormatBlockType(ACLSAN_DEVICE_BLOCK_TYPE_AICORE), "AICORE");
    EXPECT_EQ(FormatBlockType(ACLSAN_DEVICE_BLOCK_TYPE_AICORE_VECTOR), "AIV");
    EXPECT_EQ(FormatBlockType(ACLSAN_DEVICE_BLOCK_TYPE_AICORE_CUBE), "AIC");
    EXPECT_EQ(FormatBlockType(std::numeric_limits<std::uint32_t>::max()), "<unknown>");
}

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
        {ReportTool::MEMCHECK, "invalid_access"},
        ReportSeverity::ERROR,
        {
            {"space", "GM"},
            {"access", "read"},
            {"accessBytes", "16"},
            {"function", "kernel"},
            {"offset", "10"},
            {"file", "kernel.cpp"},
            {"line", "42"},
            {"coreId", "3"},
            {"blockType", "AIC"},
            {"blockId", "7"},
            {"pipeName", "MTE2"},
            {"launchId", "41"},
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
        {ReportTool::INITCHECK, "uninitialized_read"},
        ReportSeverity::ERROR,
        {
            {"space", "GM"},
            {"accessBytes", "32"},
            {"function", "init_kernel"},
            {"offset", "18"},
            {"file", "init.cpp"},
            {"line", "21"},
            {"coreId", "4"},
            {"blockType", "AIV"},
            {"blockId", "2"},
            {"pipeName", "MTE2"},
            {"launchId", "42"},
            {"address", "3000"},
        }};
}

ReportRecord MakeRaceRecord()
{
    return ReportRecord{
        {ReportTool::RACECHECK, "hazard_raw"},
        ReportSeverity::WARNING,
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
            {"firstLaunchId", "41"},
            {"secondCoreId", "1"},
            {"secondPipe", "MTE2"},
            {"secondFunction", "reader"},
            {"secondOffset", "30"},
            {"secondFile", "race.cpp"},
            {"secondLine", "19"},
            {"secondLaunchId", "42"},
            {"currentValue", "0xab"},
        }};
}

ReportRecord MakeSyncRecord()
{
    return ReportRecord{
        {ReportTool::SYNCCHECK, "pairing_mismatch"},
        ReportSeverity::ERROR,
        {
            {"reasonText", "unmatched"},
            {"triggerOperation", "WAIT_FLAG"},
            {"triggerCoreId", "2"},
            {"triggerType", "AIC"},
            {"triggerBlock", "5"},
            {"triggerPipe", "V"},
            {"triggerLaunchId", "42"},
            {"triggerLocation", "sync_kernel+0x44 in sync.cpp:88"},
            {"relatedPointLine", "=========     related point: expected SET_FLAG\n"},
            {"expectedOperationLine", ""},
            {"pairKind", "SET_WAIT_FLAG"},
            {"pairKey", "srcPipe=PIPE_V, dstPipe=PIPE_MTE2, id=7"},
        }};
}

NpusanSynccheckReport MakePairingReport(NpusanSyncMismatchReason reason, NpusanSyncPairKind pairKind)
{
    NpusanSynccheckReport report{};
    report.common.tool = ReportTool::SYNCCHECK;
    report.common.pattern = static_cast<std::uint32_t>(NpusanSynccheckPattern::PAIRING_MISMATCH);
    report.common.severity = ReportSeverity::ERROR;
    report.common.flags = aclsan::cann::kNpusanReportCommonHasExecContext;
    report.primitiveKind = pairKind == NpusanSyncPairKind::SET_WAIT_FLAG ? NpusanSyncPrimitiveKind::SET_WAIT_FLAG :
                                                                           NpusanSyncPrimitiveKind::GET_RLS_BUF;
    report.detailKind = NpusanSyncDetailKind::PAIRING;
    report.hasRelatedPoint = true;
    report.triggerPoint.hasExecContext = true;
    report.triggerPoint.exec.phyCoreId = 1;
    report.triggerPoint.exec.blockId = 4;
    report.triggerPoint.exec.pipeName = "MTE2";
    report.triggerPoint.exec.function = "SyncOperation";
    report.triggerPoint.exec.offset = 0x20;
    report.triggerPoint.exec.file = "sync.cpp";
    report.triggerPoint.exec.line = 30;
    report.common.exec = report.triggerPoint.exec;

    const bool setWait = pairKind == NpusanSyncPairKind::SET_WAIT_FLAG;
    const char* open = setWait ? "SET_FLAG" : "GET_BUF";
    const char* close = setWait ? "WAIT_FLAG" : "RLS_BUF";
    if (reason == NpusanSyncMismatchReason::DUPLICATE_OPEN) {
        report.triggerPoint.operation = open;
        report.relatedPoint.operation = open;
        report.relatedPoint.hasExecContext = true;
        report.relatedPoint.exec = report.triggerPoint.exec;
        report.relatedPoint.exec.pc = 0x1010;
        report.relatedPoint.exec.offset = 0x10;
        report.relatedPoint.exec.line = 24;
    } else if (reason == NpusanSyncMismatchReason::UNMATCHED_CLOSE) {
        report.triggerPoint.operation = close;
        report.relatedPoint.operation = open;
    } else {
        report.triggerPoint.operation = open;
        report.relatedPoint.operation = close;
    }
    report.detail =
        setWait ?
            NpusanSyncPairingError{reason, {pairKind, ACLSAN_DEVICE_PIPE_VECTOR, ACLSAN_DEVICE_PIPE_MTE2, 0, 42}} :
            NpusanSyncPairingError{reason, {pairKind, ACLSAN_DEVICE_PIPE_SCALAR, ACLSAN_DEVICE_PIPE_MTE2, 3, 42}};
    return report;
}

NpusanSynccheckReport MakeSynccheckReport(NpusanSynccheckPattern pattern)
{
    if (pattern == NpusanSynccheckPattern::PAIRING_MISMATCH) {
        return MakePairingReport(NpusanSyncMismatchReason::UNMATCHED_CLOSE, NpusanSyncPairKind::SET_WAIT_FLAG);
    }

    NpusanSynccheckReport report{};
    report.common.tool = ReportTool::SYNCCHECK;
    report.common.pattern = static_cast<std::uint32_t>(pattern);
    report.common.severity = ReportSeverity::ERROR;
    report.common.flags = aclsan::cann::kNpusanReportCommonHasExecContext;
    report.triggerPoint.operation = "SYNC_OPERATION";
    report.triggerPoint.hasExecContext = true;
    report.triggerPoint.exec.phyCoreId = 0;
    report.triggerPoint.exec.blockId = 1;
    report.triggerPoint.exec.pipeName = "S";
    report.triggerPoint.exec.pc = 0x100;
    report.triggerPoint.exec.kernelName = "sync_kernel";
    report.common.exec = report.triggerPoint.exec;

    switch (pattern) {
        case NpusanSynccheckPattern::INTRA_CORE_DIVERGENT:
        case NpusanSynccheckPattern::INTER_CORE_DIVERGENT:
        case NpusanSynccheckPattern::PARTICIPANT_MISMATCH:
            report.primitiveKind = NpusanSyncPrimitiveKind::BARRIER;
            report.detailKind = NpusanSyncDetailKind::BARRIER;
            report.detail = NpusanSyncBarrierError{"participant set differs", "AICore", 0x3, 0xf, 1};
            break;
        case NpusanSynccheckPattern::INVALID_ARGUMENT:
        case NpusanSynccheckPattern::OBJECT_NOT_INITIALIZED:
        case NpusanSynccheckPattern::DEADLOCK:
            report.primitiveKind = NpusanSyncPrimitiveKind::SYNC_OBJECT;
            report.detailKind = NpusanSyncDetailKind::OBJECT;
            report.detail = NpusanSyncObjectError{"sync object error", 1, 0x1000, 0x3, 1000};
            break;
        case NpusanSynccheckPattern::INSTRUCTION_SEQUENCE_MISMATCH:
            report.primitiveKind = NpusanSyncPrimitiveKind::INSTRUCTION_SEQUENCE;
            report.detailKind = NpusanSyncDetailKind::SEQUENCE;
            report.hasRelatedPoint = true;
            report.relatedPoint.operation = "EXPECTED_OPERATION";
            report.detail = NpusanSyncSequenceError{"instruction order differs", 2, 0x3};
            break;
        case NpusanSynccheckPattern::PAIRING_MISMATCH:
            break;
    }
    return report;
}

ReportRecord MakeSocRecord()
{
    return ReportRecord{
        {ReportTool::SOCCHECK, "register_mismatch"},
        ReportSeverity::FATAL,
        {
            {"function", "soc_kernel"},
            {"offset", "4"},
            {"file", "soc.cpp"},
            {"line", "9"},
            {"coreId", "6"},
            {"blockType", "AIC"},
            {"blockId", "1"},
            {"pipeName", "S"},
            {"launchId", "43"},
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

    EXPECT_EQ(
        aclsan::cann::RenderReportText(ReportTemplate{"{{missing}}"}, fields, &rendered),
        ReportRenderStatus::kMissingField);
    EXPECT_EQ(
        aclsan::cann::RenderReportText(ReportTemplate{"{{missing"}, fields, &rendered),
        ReportRenderStatus::kMalformedTemplate);
    EXPECT_EQ(aclsan::cann::RenderReportText(tpl, fields, nullptr), ReportRenderStatus::kInvalidArgument);
    EXPECT_EQ(aclsan::cann::WriteReportTextToStream(rendered, nullptr), ReportRenderStatus::kInvalidArgument);
    EXPECT_EQ(aclsan::cann::WriteReportTextToFile(rendered, ""), ReportRenderStatus::kOpenFailed);
}

TEST(ReportRendererTest, ListsAndRendersBuiltinTemplates)
{
    const auto builtinKeys = aclsan::cann::ListBuiltinReportTemplates();
    EXPECT_GE(builtinKeys.size(), 34U);
    EXPECT_NE(
        std::find(builtinKeys.begin(), builtinKeys.end(), ReportTemplateKey{ReportTool::MEMCHECK, "invalid_access"}),
        builtinKeys.end());
    EXPECT_NE(
        std::find(builtinKeys.begin(), builtinKeys.end(), ReportTemplateKey{ReportTool::SYNCCHECK, "pairing_mismatch"}),
        builtinKeys.end());

    std::string rendered;
    EXPECT_EQ(
        aclsan::cann::RenderReportRecord(MakeMemcheckInvalidAccessRecord(), {}, &rendered),
        ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("========= ERROR: Invalid GM read of size 16 bytes"), std::string::npos);
    EXPECT_NE(rendered.find("by aicore (3) type (AIC) block (7) pipe (MTE2)"), std::string::npos);

    EXPECT_EQ(aclsan::cann::RenderReportRecord(MakeInitcheckRecord(), {}, &rendered), ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("========= ERROR: Uninitialized GM memory read of size 32 bytes"), std::string::npos);

    EXPECT_EQ(aclsan::cann::RenderReportRecord(MakeRaceRecord(), {}, &rendered), ReportRenderStatus::kSuccess);
    EXPECT_NE(
        rendered.find("========= WARNING: Potential RAW hazard detected at UB 0x2000 in block (8) :"),
        std::string::npos);

    EXPECT_EQ(aclsan::cann::RenderReportRecord(MakeSyncRecord(), {}, &rendered), ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("Synchronization pairing mismatch: unmatched WAIT_FLAG"), std::string::npos);

    EXPECT_EQ(aclsan::cann::RenderReportRecord(MakeSocRecord(), {}, &rendered), ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("========= FATAL: SOC register mismatch detected."), std::string::npos);
}

TEST(ReportRendererTest, CatalogOwnsCompletePatternMetadata)
{
    using aclsan::cann::detail::FindPatternDescriptor;
    using aclsan::cann::detail::GetPatternCatalog;

    const auto* invalidAccess =
        FindPatternDescriptor(ReportTool::MEMCHECK, static_cast<std::uint32_t>(NpusanMemcheckPattern::INVALID_ACCESS));
    ASSERT_NE(invalidAccess, nullptr);
    EXPECT_FALSE(invalidAccess->reportTemplate.text.empty());
    EXPECT_EQ(invalidAccess, FindPatternDescriptor(ReportTemplateKey{ReportTool::MEMCHECK, "invalid_access"}));

    const auto* unused = FindPatternDescriptor(ReportTemplateKey{ReportTool::INITCHECK, "unused_memory"});
    ASSERT_NE(unused, nullptr);
    EXPECT_FALSE(unused->reportTemplate.text.empty());

    const auto& catalog = GetPatternCatalog();
    EXPECT_EQ(catalog.size(), 34U);
    for (const auto& [key, descriptor] : catalog) {
        EXPECT_FALSE(key.pattern.empty());
        EXPECT_FALSE(descriptor.reportTemplate.text.empty()) << key.pattern;
        EXPECT_EQ(&descriptor, FindPatternDescriptor(key.tool, descriptor.value)) << key.pattern;
    }
}

TEST(ReportRendererTest, AppendsStructuredCallStacks)
{
    ReportRecord record = MakeMemcheckInvalidAccessRecord();

    ReportCallStack rawStack{};
    rawStack.role = ReportStackRole::HOST_LAUNCH;
    rawStack.format = ReportStackFormat::RAW_TEXT;
    rawStack.rawText = "aclrtLaunchKernel [0x400123] in libacl.so\n";
    record.stacks.push_back(rawStack);

    ReportCallStack frameStack{};
    frameStack.role = ReportStackRole::FAULT_DEVICE;
    frameStack.format = ReportStackFormat::FRAMES;
    ReportFrame deviceFrame{};
    deviceFrame.pc = 0x100;
    deviceFrame.offset = 0x10;
    deviceFrame.function = "kernel";
    deviceFrame.file = "kernel.cpp";
    deviceFrame.line = 42;
    frameStack.frames.push_back(deviceFrame);
    ReportFrame callerFrame{};
    callerFrame.pc = 0x80;
    callerFrame.function = "launch_kernel";
    callerFrame.file = "launch.cpp";
    callerFrame.line = 18;
    frameStack.frames.push_back(callerFrame);
    record.stacks.push_back(frameStack);

    std::string rendered;
    EXPECT_EQ(aclsan::cann::RenderReportRecord(record, {}, &rendered), ReportRenderStatus::kSuccess);
    EXPECT_NE(
        rendered.find("=========  Host Frames:\n"
                      "=========     aclrtLaunchKernel [0x400123] in libacl.so\n"),
        std::string::npos);
    EXPECT_NE(
        rendered.find("=========  Device Frames:\n"
                      "=========     #0 kernel [0x100] in kernel.cpp:42\n"
                      "=========     #1 launch_kernel [0x80] in launch.cpp:18\n"),
        std::string::npos);
    EXPECT_EQ(rendered.find("Device Frame:"), std::string::npos);
    EXPECT_EQ(rendered.find("Host Frame:"), std::string::npos);
}

TEST(ReportRendererTest, PrefersRawCallStackWhenTypedReportHasBothRepresentations)
{
    NpusanSynccheckReport report =
        MakePairingReport(NpusanSyncMismatchReason::UNCONSUMED_OPEN, NpusanSyncPairKind::GET_RLS_BUF);
    report.triggerPoint.exec.pc = 0x164;
    report.common.exec = report.triggerPoint.exec;
    report.triggerPoint.stackRole = ReportStackRole::SYNC_TRIGGER;
    report.common.stackCount = 1;
    ReportCallStack& stack = report.common.stacks[0];
    stack.role = ReportStackRole::SYNC_TRIGGER;
    stack.format = ReportStackFormat::BOTH;
    stack.rawText = "[CALL-STACK] pc=0x164 status=available binary_id=1\n"
                    "  #0 GetBufInternal at kernel_event.h:718:13\n";
    stack.frames.push_back(ReportFrame{0x164, 0, "GetBufInternal", "kernel_event.h", 718, 13});

    std::string rendered;
    ASSERT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("=========     [CALL-STACK] pc=0x164 status=available binary_id=1"), std::string::npos);
    EXPECT_NE(rendered.find("=========       #0 GetBufInternal at kernel_event.h:718:13"), std::string::npos);
    EXPECT_EQ(rendered.find("=========     #0 GetBufInternal"), std::string::npos);
    EXPECT_NE(rendered.find("GetBufInternal+0x0 in kernel_event.h:718"), std::string::npos);
}

TEST(ReportRendererTest, UsesOnlyDeviceOrHostCallStackHeadings)
{
    const std::array<ReportStackRole, 4> hostRoles = {
        ReportStackRole::HOST_LAUNCH,
        ReportStackRole::HOST_ALLOC,
        ReportStackRole::HOST_FREE,
        ReportStackRole::HOST_API_CALL,
    };
    for (const ReportStackRole role : hostRoles) {
        EXPECT_STREQ(aclsan::cann::ReportStackRoleTitle(role), "Host Frames:");
    }

    const std::array<ReportStackRole, 11> deviceRoles = {
        ReportStackRole::NONE,
        ReportStackRole::FAULT_DEVICE,
        ReportStackRole::RELATED_ACCESS_A,
        ReportStackRole::RELATED_ACCESS_B,
        ReportStackRole::SYNC_PRODUCER,
        ReportStackRole::SYNC_CONSUMER,
        ReportStackRole::STATE_PRODUCER,
        ReportStackRole::STATE_CONSUMER,
        ReportStackRole::PEER_DEVICE,
        ReportStackRole::SYNC_TRIGGER,
        ReportStackRole::SYNC_RELATED,
    };
    for (const ReportStackRole role : deviceRoles) {
        EXPECT_STREQ(aclsan::cann::ReportStackRoleTitle(role), "Device Frames:");
    }
}

TEST(ReportRendererTest, RendersOnlyActiveCommonCallStackPrefix)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::MEMCHECK;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::API_ERROR);
    report.common.stackCount = 1;
    report.apiName = "aclrtLaunchKernel";
    report.apiErrorName = "ACL_ERROR_FAILURE";
    report.apiErrorMessage = "launch failed";

    report.common.stacks[0].role = ReportStackRole::HOST_LAUNCH;
    report.common.stacks[0].format = ReportStackFormat::RAW_TEXT;
    report.common.stacks[0].rawText = "active host frame";
    report.common.stacks[1].role = ReportStackRole::HOST_LAUNCH;
    report.common.stacks[1].format = ReportStackFormat::RAW_TEXT;
    report.common.stacks[1].rawText = "inactive host frame";

    std::string rendered;
    EXPECT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("runtime API call to aclrtLaunchKernel"), std::string::npos);
    EXPECT_NE(rendered.find("active host frame"), std::string::npos);
    EXPECT_EQ(rendered.find("inactive host frame"), std::string::npos);
}

TEST(ReportRendererTest, UsesExplicitUnknownPhysicalCoreSentinel)
{
    EXPECT_EQ(aclsan::cann::NpusanReportExecContext{}.phyCoreId, std::numeric_limits<std::uint32_t>::max());
    EXPECT_EQ(aclsan::cann::NpusanSyncPoint{}.exec.phyCoreId, std::numeric_limits<std::uint32_t>::max());
    EXPECT_EQ(aclsan::cann::NpusanSocStateRef{}.ownerCoreId, std::numeric_limits<std::uint32_t>::max());
}

TEST(ReportRendererTest, PrefersStructuredFaultFrameForSourceLocation)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::MEMCHECK;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::INVALID_ACCESS);
    report.common.exec.function = "fallback_function";
    report.common.exec.offset = 0x10;
    report.common.exec.file = "fallback.cpp";
    report.common.exec.line = 7;
    report.common.stackCount = 2;
    report.common.stacks[0].role = ReportStackRole::FAULT_DEVICE;
    report.common.stacks[0].format = ReportStackFormat::FRAMES;
    report.common.stacks[0].frames.push_back(ReportFrame{});
    report.common.stacks[0].frames.push_back(
        ReportFrame{0x100, 0x20, "symbolized_function", "symbolized.cpp", 42, 0, 0, 0});
    report.common.stacks[1].role = ReportStackRole::HOST_LAUNCH;
    report.common.stacks[1].format = ReportStackFormat::RAW_TEXT;
    report.common.stacks[1].rawText = "real host frame";
    report.access.accessBytes = 4;

    std::string rendered;
    EXPECT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("at symbolized_function+0x20 in symbolized.cpp:42"), std::string::npos);
    EXPECT_EQ(CountOccurrences(rendered, "=========  Host Frames:"), 1U);
    EXPECT_EQ(rendered.find("Host Frame: <unknown>"), std::string::npos);
    EXPECT_NE(rendered.find("real host frame"), std::string::npos);
}

TEST(ReportRendererTest, UsesAllocationMemorySpaceForAllocationPatterns)
{
    NpusanMemcheckReport leak{};
    leak.common.tool = ReportTool::MEMCHECK;
    leak.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::LEAK);
    leak.access.memorySpace = NpusanReportMemorySpace::UB;
    leak.allocation.memorySpace = NpusanReportMemorySpace::GM;

    std::string rendered;
    EXPECT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(leak), {}, &rendered),
        ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("memory space GM"), std::string::npos);

    NpusanInitcheckReport unused{};
    unused.common.tool = ReportTool::INITCHECK;
    unused.common.pattern = static_cast<std::uint32_t>(NpusanInitcheckPattern::UNUSED_MEMORY);
    unused.access.memorySpace = NpusanReportMemorySpace::UB;
    unused.allocation.memorySpace = NpusanReportMemorySpace::L1;

    EXPECT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(unused), {}, &rendered),
        ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("Unused L1 memory"), std::string::npos);
}

TEST(ReportRendererTest, UsesRaceAccessCoreForCrossPipeTemplate)
{
    NpusanRacecheckReport report{};
    report.common.tool = ReportTool::RACECHECK;
    report.common.pattern = static_cast<std::uint32_t>(NpusanRacecheckPattern::CROSS_PIPE_RACE);
    report.first.exec.phyCoreId = 7;
    report.second.exec.phyCoreId = 7;
    report.first.exec.pipeName = "MTE2";
    report.second.exec.pipeName = "V";
    report.first.exec.launchId = 41;
    report.second.exec.launchId = 42;

    std::string rendered;
    EXPECT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("First access by aicore (7) pipe (MTE2) in launch (41)"), std::string::npos);
    EXPECT_NE(rendered.find("Second access by aicore (7) pipe (V) in launch (42)"), std::string::npos);
}

TEST(ReportRendererTest, IncludesLaunchIdForCommonDeviceExecutionPoint)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::MEMCHECK;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::MISALIGNED_ACCESS);
    report.common.exec.phyCoreId = 18;
    report.common.exec.blockType = ACLSAN_DEVICE_BLOCK_TYPE_AICORE_VECTOR;
    report.common.exec.blockId = 0;
    report.common.exec.pipeName = "MTE2";
    report.common.exec.launchId = 41;

    std::string rendered;
    ASSERT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("by aicore (18) type (AIV) block (0) pipe (MTE2) in launch (41)"), std::string::npos);

    report.common.exec.launchId = 0;
    ASSERT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("pipe (MTE2) in launch (<unknown>)"), std::string::npos);
}

TEST(ReportRendererTest, FallsBackToProgramCounterWhenFaultIsNotSymbolized)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::MEMCHECK;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::MISALIGNED_ACCESS);
    report.common.exec.pc = 0xabc;
    report.common.exec.kernelName = "fallback_kernel";

    std::string rendered;
    EXPECT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("at pc 0xabc in fallback_kernel"), std::string::npos);
    EXPECT_EQ(rendered.find("<unknown>+0x0 in <unknown>:0"), std::string::npos);
    EXPECT_NE(rendered.find("by aicore (<unknown>)"), std::string::npos);
    EXPECT_EQ(rendered.find("4294967295"), std::string::npos);
}

TEST(ReportRendererTest, FallsBackToEachRaceSiteProgramCounter)
{
    NpusanRacecheckReport report{};
    report.common.tool = ReportTool::RACECHECK;
    report.common.pattern = static_cast<std::uint32_t>(NpusanRacecheckPattern::HAZARD_RAW);
    report.first.exec.pc = 0x111;
    report.first.exec.kernelName = "writer_kernel";
    report.second.exec.pc = 0x222;
    report.second.exec.kernelName = "reader_kernel";

    std::string rendered;
    EXPECT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("at pc 0x111 in writer_kernel"), std::string::npos);
    EXPECT_NE(rendered.find("at pc 0x222 in reader_kernel"), std::string::npos);
}

TEST(ReportRendererTest, UsesFirstRaceSiteAsInvalidRemoteAccessLocation)
{
    NpusanRacecheckReport report{};
    report.common.tool = ReportTool::RACECHECK;
    report.common.pattern = static_cast<std::uint32_t>(NpusanRacecheckPattern::INVALID_REMOTE_ACCESS);
    report.common.exec.phyCoreId = 1;
    report.common.exec.launchId = 99;
    report.first.exec.phyCoreId = 6;
    report.first.exec.blockType = ACLSAN_DEVICE_BLOCK_TYPE_AICORE_CUBE;
    report.first.exec.blockId = 7;
    report.first.exec.pipeName = "MTE2";
    report.first.exec.pc = 0x345;
    report.first.exec.kernelName = "remote_caller";
    report.first.exec.launchId = 41;

    std::string rendered;
    EXPECT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("at pc 0x345 in remote_caller"), std::string::npos);
    EXPECT_NE(rendered.find("by aicore (6) type (AIC) block (7) pipe (MTE2) in launch (41)"), std::string::npos);
    EXPECT_EQ(rendered.find("in launch (99)"), std::string::npos);
}

TEST(ReportRendererTest, RejectsCrossPipeRaceAcrossDifferentPhysicalCores)
{
    NpusanRacecheckReport report{};
    report.common.tool = ReportTool::RACECHECK;
    report.common.pattern = static_cast<std::uint32_t>(NpusanRacecheckPattern::CROSS_PIPE_RACE);
    report.first.exec.phyCoreId = 2;
    report.second.exec.phyCoreId = 3;

    std::string rendered = "stale";
    EXPECT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kInvalidArgument);
    EXPECT_TRUE(rendered.empty());
}

TEST(ReportRendererTest, RejectsCommonCallStackCountBeyondFixedCapacity)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::MEMCHECK;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::API_ERROR);
    report.common.stackCount = 9;

    std::string rendered = "stale";
    EXPECT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kInvalidArgument);
    EXPECT_TRUE(rendered.empty());
}

TEST(ReportRendererTest, RejectsRecordToolMismatch)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::INITCHECK;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::API_ERROR);

    std::string rendered = "stale";
    EXPECT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kInvalidArgument);
    EXPECT_TRUE(rendered.empty());
}

TEST(ReportRendererTest, RejectsOuterToolAndPayloadMismatchSafely)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::MEMCHECK;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::API_ERROR);
    NpusanReportRecord record = NpusanReportRecord::From(report);
    ASSERT_TRUE(std::holds_alternative<const NpusanMemcheckReport*>(record.GetPayload()));
    record.tool = ReportTool::INITCHECK;

    std::string rendered = "stale";
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(record, {}, &rendered), ReportRenderStatus::kInvalidArgument);
    EXPECT_TRUE(rendered.empty());
}

TEST(ReportRendererTest, RejectsRecordPatternMismatch)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::MEMCHECK;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::API_ERROR);
    NpusanReportRecord record = NpusanReportRecord::From(report);
    record.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::LEAK);

    std::string rendered = "stale";
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(record, {}, &rendered), ReportRenderStatus::kInvalidArgument);
    EXPECT_TRUE(rendered.empty());
}

TEST(ReportRendererTest, RejectsNullSelectedReportPointer)
{
    NpusanReportRecord record{};
    record.tool = ReportTool::MEMCHECK;
    record.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::API_ERROR);

    std::string rendered = "stale";
    EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(record, {}, &rendered), ReportRenderStatus::kInvalidArgument);
    EXPECT_TRUE(rendered.empty());
}

TEST(ReportRendererTest, RejectsDuplicateActiveCallStackRoles)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::MEMCHECK;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::API_ERROR);
    report.common.stackCount = 2;
    report.common.stacks[0].role = ReportStackRole::HOST_LAUNCH;
    report.common.stacks[1].role = ReportStackRole::HOST_LAUNCH;

    std::string rendered = "stale";
    EXPECT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kInvalidArgument);
    EXPECT_TRUE(rendered.empty());
}

TEST(ReportRendererTest, ValidatesActiveCallStackMetadataAndBoundaries)
{
    NpusanMemcheckReport report{};
    report.common.tool = ReportTool::MEMCHECK;
    report.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::API_ERROR);
    report.common.stackCount = aclsan::cann::kNpusanReportStackMax;
    for (std::uint32_t i = 0; i < report.common.stackCount; ++i) {
        report.common.stacks[i].role = static_cast<ReportStackRole>(i + 1);
        report.common.stacks[i].format = ReportStackFormat::NONE;
    }

    std::string rendered;
    EXPECT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kSuccess);

    report.common.stackCount = 1;
    report.common.stacks[0].role = static_cast<ReportStackRole>(0);
    EXPECT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kInvalidArgument);

    report.common.stacks[0].role = ReportStackRole::FAULT_DEVICE;
    report.common.stacks[0].format = static_cast<ReportStackFormat>(99);
    EXPECT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kInvalidArgument);

    report.common.stacks[0].format = ReportStackFormat::FRAMES;
    report.common.stacks[0].frames.resize(17);
    EXPECT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
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
    EXPECT_EQ(
        aclsan::cann::LoadReportTemplateOverridesFromFile(overridePath, &overrides), ReportRenderStatus::kSuccess);
    EXPECT_EQ(
        aclsan::cann::RenderReportRecord(MakeMemcheckInvalidAccessRecord(), overrides, &rendered),
        ReportRenderStatus::kSuccess);
    EXPECT_EQ(rendered, "USER ERROR 1000\n");

    const ReportRecord unknownRecord{{ReportTool::MEMCHECK, "unknown"}, ReportSeverity::ERROR, {}};
    EXPECT_EQ(aclsan::cann::RenderReportRecord(unknownRecord, {}, &rendered), ReportRenderStatus::kUnknownTemplate);
}

TEST(ReportRendererTest, RendersBundleSummaries)
{
    const ReportRecord leakRecord{
        {ReportTool::MEMCHECK, "leak"},
        ReportSeverity::WARNING,
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
        {ReportTool::INITCHECK, "unused_memory"},
        ReportSeverity::INFO,
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
        {ReportTool::SYNCCHECK, "deadlock"},
        ReportSeverity::ERROR,
        {
            {"reason", "wait cannot complete"},
            {"triggerOperation", "WAIT_FLAG"},
            {"triggerCoreId", "2"},
            {"triggerType", "AIC"},
            {"triggerBlock", "5"},
            {"triggerPipe", "V"},
            {"triggerLaunchId", "42"},
            {"triggerLocation", "pc 0x5000 in sync_kernel"},
            {"relatedPointLine", ""},
            {"waitingMask", "f"},
            {"timeoutNs", "1000"},
            {"objectLine", "=========     sync object 0x99\n"},
        }};
    const std::vector<ReportRecord> records{
        MakeMemcheckInvalidAccessRecord(), MakeRaceRecord(), MakeSocRecord(), leakRecord, unusedRecord, deadlockRecord};

    std::string rendered;
    EXPECT_EQ(aclsan::cann::RenderReportBundle(records, {}, &rendered), ReportRenderStatus::kSuccess);
    EXPECT_EQ(rendered.rfind("========= NPUSAN\n", 0), 0U);
    EXPECT_NE(rendered.find("========= MEMCHECK SUMMARY: 1 errors, 1 warnings, 0 infos, 1 leaks"), std::string::npos);
    EXPECT_NE(
        rendered.find("========= INITCHECK SUMMARY: 0 errors, 0 warnings, 1 infos, 1 unused memory reports"),
        std::string::npos);
    EXPECT_NE(
        rendered.find("========= RACECHECK SUMMARY: 1 hazard displayed (0 errors, 1 warnings, 0 infos)"),
        std::string::npos);
    EXPECT_NE(
        rendered.find("========= SYNCCHECK SUMMARY: 1 errors, 0 warnings, 0 infos, 1 deadlocks"), std::string::npos);
    EXPECT_NE(rendered.find("========= SOCCHECK SUMMARY: 1 errors, 0 warnings, 0 infos"), std::string::npos);
    EXPECT_NE(
        rendered.find("========= ERROR SUMMARY: 3 errors\n"
                      "=========     MEMCHECK: 1 errors\n"
                      "=========     INITCHECK: 0 errors\n"
                      "=========     RACECHECK: 0 errors\n"
                      "=========     SYNCCHECK: 1 errors\n"
                      "=========     SOCCHECK: 1 errors\n"
                      "=========     FATAL: 1 fatal errors\n"),
        std::string::npos);
}

TEST(ReportRendererTest, SummarizesOnlyToolsPresentInBundle)
{
    const std::vector<ReportRecord> records{MakeMemcheckInvalidAccessRecord()};

    std::string rendered;
    ASSERT_EQ(aclsan::cann::RenderReportBundle(records, {}, &rendered), ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("========= MEMCHECK SUMMARY:"), std::string::npos);
    EXPECT_NE(rendered.find("=========     MEMCHECK:"), std::string::npos);
    EXPECT_EQ(rendered.find("INITCHECK SUMMARY"), std::string::npos);
    EXPECT_EQ(rendered.find("RACECHECK SUMMARY"), std::string::npos);
    EXPECT_EQ(rendered.find("SYNCCHECK SUMMARY"), std::string::npos);
    EXPECT_EQ(rendered.find("SOCCHECK SUMMARY"), std::string::npos);
    EXPECT_EQ(rendered.find("=========     INITCHECK:"), std::string::npos);
    EXPECT_EQ(rendered.find("=========     RACECHECK:"), std::string::npos);
    EXPECT_EQ(rendered.find("=========     SYNCCHECK:"), std::string::npos);
    EXPECT_EQ(rendered.find("=========     SOCCHECK:"), std::string::npos);
}

TEST(ReportRendererTest, EmptyBundleHasOnlyGlobalSummary)
{
    std::string rendered;
    ASSERT_EQ(aclsan::cann::RenderReportBundle({}, {}, &rendered), ReportRenderStatus::kSuccess);
    EXPECT_EQ(rendered.find("MEMCHECK SUMMARY"), std::string::npos);
    EXPECT_EQ(rendered.find("INITCHECK SUMMARY"), std::string::npos);
    EXPECT_EQ(rendered.find("RACECHECK SUMMARY"), std::string::npos);
    EXPECT_EQ(rendered.find("SYNCCHECK SUMMARY"), std::string::npos);
    EXPECT_EQ(rendered.find("SOCCHECK SUMMARY"), std::string::npos);
    EXPECT_NE(rendered.find("========= ERROR SUMMARY: 0 errors\n"), std::string::npos);
    EXPECT_NE(rendered.find("=========     FATAL: 0 fatal errors\n"), std::string::npos);
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
        {NpusanSyncPairKind::SET_WAIT_FLAG, NpusanSyncMismatchReason::DUPLICATE_OPEN,
         "Synchronization pairing mismatch: duplicate SET_FLAG.",
         "related point: previous SET_FLAG in launch (<unknown>) at SyncOperation+0x10 in sync.cpp:24 is still "
         "pending"},
        {NpusanSyncPairKind::SET_WAIT_FLAG, NpusanSyncMismatchReason::UNMATCHED_CLOSE,
         "Synchronization pairing mismatch: unmatched WAIT_FLAG.",
         "related point: expected SET_FLAG, but no matching point exists for this pair key"},
        {NpusanSyncPairKind::GET_RLS_BUF, NpusanSyncMismatchReason::UNCONSUMED_OPEN,
         "Synchronization pairing mismatch: redundant GET_BUF.",
         "related point: expected RLS_BUF, but no matching point was observed"},
    };

    for (const Case& testCase : cases) {
        NpusanSynccheckReport report = MakePairingReport(testCase.reason, testCase.pairKind);

        std::string rendered;
        ASSERT_EQ(
            aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
            ReportRenderStatus::kSuccess);
        EXPECT_NE(rendered.find(testCase.expectedHeadline), std::string::npos);
        EXPECT_NE(rendered.find(testCase.expectedRelated), std::string::npos);
        EXPECT_EQ(rendered.find("observed sequence"), std::string::npos);
    }
}

TEST(ReportRendererTest, SeparatesBundleRecordsWithOneEmptyLine)
{
    const ReportTemplateKey key{ReportTool::MEMCHECK, "invalid_access"};
    const ReportTemplateOverrides overrides{{key, {"{{recordName}}\n"}}};
    const std::vector<ReportRecord> records{
        {key, ReportSeverity::ERROR, {{"recordName", "first record"}}},
        {key, ReportSeverity::ERROR, {{"recordName", "second record"}}},
    };

    std::string rendered;
    ASSERT_EQ(aclsan::cann::RenderReportBundle(records, overrides, &rendered), ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("first record\n\nsecond record\n"), std::string::npos);
    EXPECT_EQ(rendered.find("first record\n\n\nsecond record\n"), std::string::npos);
}

TEST(ReportRendererTest, RendersStructuredPairingMismatchEvidence)
{
    NpusanSynccheckReport report =
        MakePairingReport(NpusanSyncMismatchReason::DUPLICATE_OPEN, NpusanSyncPairKind::SET_WAIT_FLAG);
    report.triggerPoint.exec.launchId = 42;
    report.common.exec.launchId = 42;
    report.relatedPoint.exec.launchId = 41;
    report.common.stackCount = 2;
    report.common.stacks[0].role = ReportStackRole::SYNC_TRIGGER;
    report.common.stacks[0].format = ReportStackFormat::FRAMES;
    report.common.stacks[0].frames.push_back(ReportFrame{0, 0x20, "SecondSet", "sync.cpp", 30});
    report.common.stacks[1].role = ReportStackRole::SYNC_RELATED;
    report.common.stacks[1].format = ReportStackFormat::FRAMES;
    report.common.stacks[1].frames.push_back(ReportFrame{0x1010, 0x10, "FirstSet", "sync.cpp", 24});
    report.triggerPoint.stackRole = ReportStackRole::SYNC_TRIGGER;
    report.relatedPoint.stackRole = ReportStackRole::SYNC_RELATED;

    std::string rendered;
    ASSERT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("Synchronization pairing mismatch: duplicate SET_FLAG."), std::string::npos);
    EXPECT_NE(
        rendered.find(
            "related point: previous SET_FLAG in launch (41) at FirstSet+0x10 in sync.cpp:24 is still pending"),
        std::string::npos);
    EXPECT_NE(rendered.find("pipe (MTE2) in launch (42) at SecondSet+0x20 in sync.cpp:30"), std::string::npos);
    EXPECT_NE(rendered.find("expected WAIT_FLAG before another SET_FLAG"), std::string::npos);
    EXPECT_NE(
        rendered.find("pair kind SET_WAIT_FLAG, key (srcPipe=PIPE_V, dstPipe=PIPE_MTE2, id=42)"), std::string::npos);
    EXPECT_EQ(CountOccurrences(rendered, "=========  Device Frames:"), 2U);
    EXPECT_EQ(CountOccurrences(rendered, "=========     #0 "), 2U);
    EXPECT_EQ(CountOccurrences(rendered, "=========     #1 "), 0U);
    EXPECT_EQ(rendered.find("Trigger Point Device Backtrace:"), std::string::npos);
    EXPECT_EQ(rendered.find("Related Point Device Backtrace:"), std::string::npos);
}

TEST(ReportRendererTest, IncludesLaunchIdForSoccheckProducerAndConsumer)
{
    NpusanSoccheckReport report{};
    report.common.tool = ReportTool::SOCCHECK;
    report.common.pattern = static_cast<std::uint32_t>(NpusanSoccheckPattern::CROSS_CORE_STATE_INCONSISTENT);
    report.producer.phyCoreId = 3;
    report.producer.launchId = 41;
    report.consumer.phyCoreId = 4;
    report.consumer.launchId = 42;

    std::string rendered;
    ASSERT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("consumer aicore (4) in launch (42) observed"), std::string::npos);
    EXPECT_NE(rendered.find("producer aicore (3) in launch (41) expected"), std::string::npos);
}

TEST(ReportRendererTest, IncludesLaunchIdForSynccheckActualRelatedPoint)
{
    NpusanSynccheckReport report = MakeSynccheckReport(NpusanSynccheckPattern::PARTICIPANT_MISMATCH);
    report.triggerPoint.exec.launchId = 42;
    report.common.exec.launchId = 42;
    report.hasRelatedPoint = true;
    report.relatedPoint.operation = "BARRIER";
    report.relatedPoint.hasExecContext = true;
    report.relatedPoint.exec.phyCoreId = 3;
    report.relatedPoint.exec.blockType = ACLSAN_DEVICE_BLOCK_TYPE_AICORE_VECTOR;
    report.relatedPoint.exec.blockId = 1;
    report.relatedPoint.exec.pipeName = "MTE2";
    report.relatedPoint.exec.launchId = 41;

    std::string rendered;
    ASSERT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("pipe (S) in launch (42) at pc 0x100 in sync_kernel"), std::string::npos);
    EXPECT_NE(
        rendered.find("related point: BARRIER by aicore (3) type (AIV) block (1) pipe (MTE2) in launch (41)"),
        std::string::npos);
}

TEST(ReportRendererTest, RendersUnconsumedGetBufferWithExpectedRelatedPoint)
{
    NpusanSynccheckReport report{};
    report.common.tool = ReportTool::SYNCCHECK;
    report.common.pattern = static_cast<std::uint32_t>(NpusanSynccheckPattern::PAIRING_MISMATCH);
    report.common.severity = ReportSeverity::ERROR;
    report.common.flags = aclsan::cann::kNpusanReportCommonHasExecContext;
    report.primitiveKind = NpusanSyncPrimitiveKind::GET_RLS_BUF;
    report.detailKind = NpusanSyncDetailKind::PAIRING;
    report.triggerPoint.operation = "GET_BUF";
    report.triggerPoint.hasExecContext = true;
    report.triggerPoint.exec.phyCoreId = 1;
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
        NpusanSyncMismatchReason::UNCONSUMED_OPEN,
        {NpusanSyncPairKind::GET_RLS_BUF, ACLSAN_DEVICE_PIPE_SCALAR, ACLSAN_DEVICE_PIPE_MTE2, 3, 42},
    };

    std::string rendered;
    ASSERT_EQ(
        aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
        ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("Synchronization pairing mismatch: redundant GET_BUF."), std::string::npos);
    EXPECT_NE(rendered.find("related point: expected RLS_BUF, but no matching point was observed"), std::string::npos);
    EXPECT_NE(rendered.find("pair kind GET_RLS_BUF, key (pipe=PIPE_MTE2, id=42, mode=3)"), std::string::npos);
}

TEST(ReportRendererTest, RejectsInvalidPairingMismatchMetadata)
{
    std::string rendered;
    {
        NpusanSynccheckReport report =
            MakePairingReport(NpusanSyncMismatchReason::UNMATCHED_CLOSE, NpusanSyncPairKind::SET_WAIT_FLAG);
        report.detailKind = NpusanSyncDetailKind::BARRIER;
        EXPECT_EQ(
            aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
            ReportRenderStatus::kInvalidArgument);
    }
    {
        NpusanSynccheckReport report =
            MakePairingReport(NpusanSyncMismatchReason::UNMATCHED_CLOSE, NpusanSyncPairKind::SET_WAIT_FLAG);
        std::get<NpusanSyncPairingError>(report.detail).reason = NpusanSyncMismatchReason::UNKNOWN;
        EXPECT_EQ(
            aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
            ReportRenderStatus::kInvalidArgument);
    }
    {
        NpusanSynccheckReport report =
            MakePairingReport(NpusanSyncMismatchReason::UNCONSUMED_OPEN, NpusanSyncPairKind::GET_RLS_BUF);
        report.primitiveKind = NpusanSyncPrimitiveKind::SET_WAIT_FLAG;
        EXPECT_EQ(
            aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
            ReportRenderStatus::kInvalidArgument);
    }
    {
        NpusanSynccheckReport report =
            MakePairingReport(NpusanSyncMismatchReason::UNCONSUMED_OPEN, NpusanSyncPairKind::GET_RLS_BUF);
        std::get<NpusanSyncPairingError>(report.detail).key.srcPipe = ACLSAN_DEVICE_PIPE_VECTOR;
        EXPECT_EQ(
            aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
            ReportRenderStatus::kInvalidArgument);
    }
    {
        NpusanSynccheckReport report =
            MakePairingReport(NpusanSyncMismatchReason::UNCONSUMED_OPEN, NpusanSyncPairKind::GET_RLS_BUF);
        report.relatedPoint.hasExecContext = true;
        EXPECT_EQ(
            aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
            ReportRenderStatus::kInvalidArgument);
    }
    {
        NpusanSynccheckReport report =
            MakePairingReport(NpusanSyncMismatchReason::DUPLICATE_OPEN, NpusanSyncPairKind::SET_WAIT_FLAG);
        report.common.exec.pc = 0xdead;
        EXPECT_EQ(
            aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
            ReportRenderStatus::kInvalidArgument);
    }
}

TEST(ReportRendererTest, RejectsOrphanRelatedPointAndMismatchedPointStackPc)
{
    std::string rendered;
    {
        NpusanSynccheckReport report = MakeSynccheckReport(NpusanSynccheckPattern::INTRA_CORE_DIVERGENT);
        report.relatedPoint.operation = "UNREFERENCED_OPERATION";
        EXPECT_EQ(
            aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
            ReportRenderStatus::kInvalidArgument);
    }
    {
        NpusanSynccheckReport report =
            MakePairingReport(NpusanSyncMismatchReason::DUPLICATE_OPEN, NpusanSyncPairKind::SET_WAIT_FLAG);
        report.triggerPoint.exec.pc = 0x1000;
        report.common.exec = report.triggerPoint.exec;
        report.triggerPoint.stackRole = ReportStackRole::SYNC_TRIGGER;
        report.common.stackCount = 1;
        report.common.stacks[0].role = ReportStackRole::SYNC_TRIGGER;
        report.common.stacks[0].format = ReportStackFormat::FRAMES;
        report.common.stacks[0].frames.push_back(ReportFrame{0x2000, 0x20, "DifferentInstruction", "sync.cpp", 30});
        EXPECT_EQ(
            aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
            ReportRenderStatus::kInvalidArgument);
    }
    {
        NpusanSynccheckReport report =
            MakePairingReport(NpusanSyncMismatchReason::UNMATCHED_CLOSE, NpusanSyncPairKind::SET_WAIT_FLAG);
        report.common.flags = 0;
        EXPECT_EQ(
            aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
            ReportRenderStatus::kInvalidArgument);
    }
    {
        NpusanSynccheckReport report = MakeSynccheckReport(NpusanSynccheckPattern::INVALID_ARGUMENT);
        report.primitiveKind = static_cast<NpusanSyncPrimitiveKind>(99);
        EXPECT_EQ(
            aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
            ReportRenderStatus::kInvalidArgument);
    }
}

TEST(ReportRendererTest, RendersStructuredReportsFromEachCheckerStruct)
{
    NpusanMemcheckReport memcheck{};
    memcheck.common.tool = ReportTool::MEMCHECK;
    memcheck.common.pattern = static_cast<std::uint32_t>(NpusanMemcheckPattern::INVALID_ACCESS);
    memcheck.common.severity = ReportSeverity::ERROR;
    memcheck.common.exec.function = "kernel";
    memcheck.common.exec.offset = 0x10;
    memcheck.common.exec.file = "kernel.cpp";
    memcheck.common.exec.line = 42;
    memcheck.common.exec.phyCoreId = 3;
    memcheck.common.exec.blockId = 7;
    memcheck.common.exec.pipeName = "MTE2";
    memcheck.access.memorySpace = NpusanReportMemorySpace::GM;
    memcheck.access.accessMode = NpusanReportAccessMode::READ;
    memcheck.access.accessBytes = 16;
    memcheck.access.address = 0x1000;
    memcheck.nearestAllocation.base = 0x0fc0;
    memcheck.nearestAllocation.bytes = 128;
    memcheck.distanceKind = NpusanReportDistanceKind::AFTER;
    memcheck.distanceBytes = 64;

    NpusanInitcheckReport initcheck{};
    initcheck.common.tool = ReportTool::INITCHECK;
    initcheck.common.pattern = static_cast<std::uint32_t>(NpusanInitcheckPattern::UNINITIALIZED_READ);
    initcheck.common.severity = ReportSeverity::ERROR;
    initcheck.common.exec.function = "init_kernel";
    initcheck.common.exec.offset = 0x18;
    initcheck.common.exec.file = "init.cpp";
    initcheck.common.exec.line = 21;
    initcheck.common.exec.phyCoreId = 4;
    initcheck.common.exec.blockId = 2;
    initcheck.common.exec.pipeName = "MTE2";
    initcheck.access.memorySpace = NpusanReportMemorySpace::GM;
    initcheck.access.accessBytes = 32;
    initcheck.access.address = 0x3000;

    NpusanRacecheckReport racecheck{};
    racecheck.common.tool = ReportTool::RACECHECK;
    racecheck.common.pattern = static_cast<std::uint32_t>(NpusanRacecheckPattern::HAZARD_RAW);
    racecheck.common.severity = ReportSeverity::WARNING;
    racecheck.first.access.memorySpace = NpusanReportMemorySpace::UB;
    racecheck.first.access.address = 0x2000;
    racecheck.first.exec.blockId = 8;
    racecheck.first.exec.phyCoreId = 0;
    racecheck.first.exec.pipeName = "MTE3";
    racecheck.first.exec.function = "writer";
    racecheck.first.exec.offset = 0x20;
    racecheck.first.exec.file = "race.cpp";
    racecheck.first.exec.line = 11;
    racecheck.second.exec.phyCoreId = 1;
    racecheck.second.exec.pipeName = "MTE2";
    racecheck.second.exec.function = "reader";
    racecheck.second.exec.offset = 0x30;
    racecheck.second.exec.file = "race.cpp";
    racecheck.second.exec.line = 19;
    racecheck.currentValue = 0xab;

    NpusanSynccheckReport synccheck =
        MakePairingReport(NpusanSyncMismatchReason::UNMATCHED_CLOSE, NpusanSyncPairKind::SET_WAIT_FLAG);

    NpusanSoccheckReport soccheck{};
    soccheck.common.tool = ReportTool::SOCCHECK;
    soccheck.common.pattern = static_cast<std::uint32_t>(NpusanSoccheckPattern::REGISTER_MISMATCH);
    soccheck.common.severity = ReportSeverity::FATAL;
    soccheck.common.exec.function = "soc_kernel";
    soccheck.common.exec.offset = 0x4;
    soccheck.common.exec.file = "soc.cpp";
    soccheck.common.exec.line = 9;
    soccheck.common.exec.phyCoreId = 6;
    soccheck.common.exec.blockId = 1;
    soccheck.common.exec.pipeName = "S";
    soccheck.state.registerId = 17;
    soccheck.state.expectedValue = 0x10;
    soccheck.state.observedValue = 0x11;

    std::vector<NpusanReportRecord> records{
        NpusanReportRecord::From(memcheck),  NpusanReportRecord::From(initcheck), NpusanReportRecord::From(racecheck),
        NpusanReportRecord::From(synccheck), NpusanReportRecord::From(soccheck),
    };

    std::string rendered;
    for (const NpusanReportRecord& record : records) {
        EXPECT_EQ(aclsan::cann::RenderNpusanReportRecord(record, {}, &rendered), ReportRenderStatus::kSuccess);
    }
    EXPECT_EQ(aclsan::cann::RenderNpusanReportBundle(records, {}, &rendered), ReportRenderStatus::kSuccess);
    EXPECT_NE(rendered.find("========= ERROR: Invalid GM read of size 16 bytes"), std::string::npos);
    EXPECT_NE(rendered.find("========= ERROR: Uninitialized GM memory read of size 32 bytes"), std::string::npos);
    EXPECT_NE(
        rendered.find("========= WARNING: Potential RAW hazard detected at UB 0x2000 in block (8) :"),
        std::string::npos);
    EXPECT_NE(
        rendered.find("========= ERROR: Synchronization pairing mismatch: unmatched WAIT_FLAG."), std::string::npos);
    EXPECT_NE(rendered.find("========= FATAL: SOC register mismatch detected."), std::string::npos);
    EXPECT_NE(rendered.find("========= ERROR SUMMARY: 4 errors"), std::string::npos);
}

TEST(ReportRendererTest, RendersAllStructuredPatternTemplates)
{
    std::string rendered;

    for (const auto pattern :
         {NpusanMemcheckPattern::INVALID_ACCESS, NpusanMemcheckPattern::MISALIGNED_ACCESS,
          NpusanMemcheckPattern::USE_AFTER_FREE, NpusanMemcheckPattern::USE_BEFORE_ALLOC,
          NpusanMemcheckPattern::INVALID_FREE, NpusanMemcheckPattern::DOUBLE_FREE, NpusanMemcheckPattern::LEAK,
          NpusanMemcheckPattern::API_ERROR}) {
        NpusanMemcheckReport report{};
        report.common.tool = ReportTool::MEMCHECK;
        report.common.pattern = static_cast<std::uint32_t>(pattern);
        report.common.severity = ReportSeverity::ERROR;
        EXPECT_EQ(
            aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
            ReportRenderStatus::kSuccess);
    }

    for (const auto pattern :
         {NpusanInitcheckPattern::UNINITIALIZED_READ, NpusanInitcheckPattern::PARTIAL_UNINITIALIZED_READ,
          NpusanInitcheckPattern::UNUSED_MEMORY, NpusanInitcheckPattern::API_READ_UNINITIALIZED}) {
        NpusanInitcheckReport report{};
        report.common.tool = ReportTool::INITCHECK;
        report.common.pattern = static_cast<std::uint32_t>(pattern);
        report.common.severity = ReportSeverity::ERROR;
        EXPECT_EQ(
            aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
            ReportRenderStatus::kSuccess);
    }

    for (const auto pattern :
         {NpusanRacecheckPattern::ANALYSIS, NpusanRacecheckPattern::HAZARD_RAW, NpusanRacecheckPattern::HAZARD_WAR,
          NpusanRacecheckPattern::HAZARD_WAW, NpusanRacecheckPattern::ATOMIC_RACE,
          NpusanRacecheckPattern::CROSS_PIPE_RACE, NpusanRacecheckPattern::INTER_CORE_RACE,
          NpusanRacecheckPattern::INVALID_REMOTE_ACCESS}) {
        NpusanRacecheckReport report{};
        report.common.tool = ReportTool::RACECHECK;
        report.common.pattern = static_cast<std::uint32_t>(pattern);
        report.common.severity = ReportSeverity::WARNING;
        EXPECT_EQ(
            aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
            ReportRenderStatus::kSuccess);
    }

    for (const auto pattern :
         {NpusanSynccheckPattern::INTRA_CORE_DIVERGENT, NpusanSynccheckPattern::INTER_CORE_DIVERGENT,
          NpusanSynccheckPattern::INVALID_ARGUMENT, NpusanSynccheckPattern::PAIRING_MISMATCH,
          NpusanSynccheckPattern::PARTICIPANT_MISMATCH, NpusanSynccheckPattern::DEADLOCK,
          NpusanSynccheckPattern::OBJECT_NOT_INITIALIZED, NpusanSynccheckPattern::INSTRUCTION_SEQUENCE_MISMATCH}) {
        NpusanSynccheckReport report = MakeSynccheckReport(pattern);
        EXPECT_EQ(
            aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
            ReportRenderStatus::kSuccess);
    }

    for (const auto pattern :
         {NpusanSoccheckPattern::UNINITIALIZED_STATE_READ, NpusanSoccheckPattern::REGISTER_MISMATCH,
          NpusanSoccheckPattern::ILLEGAL_STATE_TRANSITION, NpusanSoccheckPattern::STATE_NOT_RESTORED,
          NpusanSoccheckPattern::CROSS_CORE_STATE_INCONSISTENT, NpusanSoccheckPattern::SCOPE_VIOLATION}) {
        NpusanSoccheckReport report{};
        report.common.tool = ReportTool::SOCCHECK;
        report.common.pattern = static_cast<std::uint32_t>(pattern);
        report.common.severity = ReportSeverity::ERROR;
        EXPECT_EQ(
            aclsan::cann::RenderNpusanReportRecord(NpusanReportRecord::From(report), {}, &rendered),
            ReportRenderStatus::kSuccess);
    }
}

} // namespace

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
