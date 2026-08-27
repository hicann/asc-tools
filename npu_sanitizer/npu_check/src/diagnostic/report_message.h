// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#ifndef ACLSAN_REPORT_MESSAGE_H
#define ACLSAN_REPORT_MESSAGE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <variant>
#include <vector>

namespace aclsan::cann {

inline constexpr std::size_t kNpusanReportStackMax = 8;
inline constexpr std::size_t kNpusanReportFrameMax = 16;

enum class ReportTool {
    kMemcheck = 1,
    kInitcheck = 2,
    kRacecheck = 3,
    kSynccheck = 4,
    kSoccheck = 5,
};

enum class ReportSeverity {
    kInfo = 1,
    kWarning = 2,
    kError = 3,
    kFatal = 4,
};

enum class ReportStackRole {
    kNone = 0,
    kFaultDevice = 1,
    kHostLaunch = 2,
    kHostAlloc = 3,
    kHostFree = 4,
    kRelatedAccessA = 5,
    kRelatedAccessB = 6,
    kSyncProducer = 7,
    kSyncConsumer = 8,
    kStateProducer = 9,
    kStateConsumer = 10,
    kPeerDevice = 11,
    kSyncTrigger = 12,
    kSyncRelated = 13,
    kHostApiCall = 14,
};

enum class ReportStackFormat {
    kNone = 0,
    kRawText = 1,
    kFrames = 2,
    kBoth = 3,
};

struct ReportFrame {
    std::uint64_t pc = 0;
    std::uint64_t offset = 0;
    std::string function;
    std::string file;
    std::uint32_t line = 0;
    std::uint32_t column = 0;
    std::uint32_t inlineDepth = 0;
    std::uint32_t flags = 0;
};

struct ReportCallStack {
    ReportStackRole role = ReportStackRole::kFaultDevice;
    ReportStackFormat format = ReportStackFormat::kNone;
    std::string rawText;
    std::vector<ReportFrame> frames;
};

enum class NpusanReportMemorySpace {
    kUnknown = 0,
    kGm = 1,
    kUb = 2,
    kL1 = 3,
    kL0A = 4,
    kL0B = 5,
    kL0C = 6,
    kBt = 7,
    kPrivate = 8,
    kHost = 9,
};

enum class NpusanReportAccessMode {
    kRead = 1,
    kWrite = 2,
    kReadWrite = 3,
    kFree = 4,
};

enum class NpusanReportDistanceKind {
    kUnknown = 0,
    kInside = 1,
    kBefore = 2,
    kAfter = 3,
};

enum class NpusanMemcheckPattern {
    kInvalidAccess = 1,
    kMisalignedAccess = 2,
    kUseAfterFree = 3,
    kUseBeforeAlloc = 4,
    kInvalidFree = 5,
    kDoubleFree = 6,
    kLeak = 7,
    kApiError = 8,
};

enum class NpusanInitcheckPattern {
    kUninitializedRead = 1,
    kPartialUninitializedRead = 2,
    kUnusedMemory = 3,
    kApiReadUninitialized = 4,
};

enum class NpusanRacecheckPattern {
    kAnalysis = 1,
    kHazardRaw = 2,
    kHazardWar = 3,
    kHazardWaw = 4,
    kAtomicRace = 5,
    kCrossPipeRace = 6,
    kInterCoreRace = 7,
    kInvalidRemoteAccess = 8,
};

enum class NpusanSynccheckPattern {
    kIntraCoreDivergent = 1,
    kInterCoreDivergent = 2,
    kInvalidArgument = 3,
    kPairingMismatch = 4,
    kParticipantMismatch = 5,
    kDeadlock = 6,
    kObjectNotInitialized = 7,
    kInstructionSequenceMismatch = 8,
};

enum class NpusanSyncMismatchReason {
    kUnknown = 0,
    kDuplicateOpen = 1,
    kUnmatchedClose = 2,
    kUnconsumedOpen = 3,
};

enum class NpusanSyncPairKind {
    kUnknown = 0,
    kSetWaitFlag = 1,
    kGetRlsBuf = 2,
};

enum class NpusanSyncDetailKind {
    kBarrier = 1,
    kPairing = 2,
    kSequence = 3,
    kObject = 4,
};

enum class NpusanSyncPrimitiveKind {
    kUnknown = 0,
    kBarrier = 1,
    kSetWaitFlag = 2,
    kGetRlsBuf = 3,
    kInstructionSequence = 4,
    kSyncObject = 5,
};

enum class NpusanSoccheckPattern {
    kUninitializedStateRead = 1,
    kRegisterMismatch = 2,
    kIllegalStateTransition = 3,
    kStateNotRestored = 4,
    kCrossCoreStateInconsistent = 5,
    kScopeViolation = 6,
};

struct NpusanReportExecContext {
    std::uint64_t launchId = 0;
    std::uint64_t binaryId = 0;
    std::uint64_t functionId = 0;
    std::uint64_t instrExecId = 0;
    std::uint64_t serialNo = 0;
    std::uint64_t pc = 0;
    std::uint64_t offset = 0;

    std::uint32_t deviceId = 0;
    std::uint32_t phyCoreId = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t blockId = 0;
    std::uint32_t blockType = 0;
    std::uint32_t pipeId = 0;
    std::uint32_t siteId = 0;
    std::uint32_t line = 0;
    std::uint32_t column = 0;

    std::string function;
    std::string file;
    std::string pipeName;
    std::string kernelName;
};

inline constexpr std::uint32_t kNpusanReportCommonHasExecContext = 1U << 0;

struct NpusanReportCommon {
    std::uint64_t reportId = 0;
    std::uint64_t groupId = 0;
    std::uint64_t timestampNs = 0;

    ReportTool tool = ReportTool::kMemcheck;
    ReportSeverity severity = ReportSeverity::kError;
    std::uint32_t pattern = 0;
    std::uint32_t flags = 0;

    NpusanReportExecContext exec;
    std::uint32_t stackCount = 0;
    std::array<ReportCallStack, kNpusanReportStackMax> stacks{};
};

struct NpusanReportMemoryAccess {
    NpusanReportMemorySpace memorySpace = NpusanReportMemorySpace::kUnknown;
    NpusanReportAccessMode accessMode = NpusanReportAccessMode::kRead;
    std::uint32_t accessBytes = 0;
    std::uint32_t requiredAlign = 0;

    std::uint64_t address = 0;
    std::uint64_t rangeBegin = 0;
    std::uint64_t rangeEnd = 0;
};

struct NpusanReportAllocation {
    std::uint64_t allocId = 0;
    std::uint64_t base = 0;
    std::uint64_t bytes = 0;
    std::uint64_t allocSerialNo = 0;
    std::uint64_t freeSerialNo = 0;

    NpusanReportMemorySpace memorySpace = NpusanReportMemorySpace::kUnknown;
    std::uint32_t deviceId = 0;
    std::uint32_t state = 0;
    std::uint32_t flags = 0;
};

struct NpusanSyncPoint {
    std::string operation;
    bool hasExecContext = false;
    NpusanReportExecContext exec;
    ReportStackRole stackRole = ReportStackRole::kNone;
};

struct NpusanSyncPairKey {
    NpusanSyncPairKind pairKind = NpusanSyncPairKind::kUnknown;
    std::uint32_t srcPipe = 0;
    std::uint32_t dstPipe = 0;
    std::uint32_t mode = 0;
    std::uint64_t id = 0;
};

struct NpusanSyncBarrierError {
    std::string reason;
    std::string scope;
    std::uint64_t activeMask = 0;
    std::uint64_t expectedMask = 0;
    std::uint64_t objectId = 0;
};

struct NpusanSyncPairingError {
    NpusanSyncMismatchReason reason = NpusanSyncMismatchReason::kUnknown;
    NpusanSyncPairKey key;
};

struct NpusanSyncSequenceError {
    std::string reason;
    std::uint32_t sequenceIndex = 0;
    std::uint64_t activeMask = 0;
};

struct NpusanSyncObjectError {
    std::string reason;
    std::uint64_t objectId = 0;
    std::uint64_t address = 0;
    std::uint64_t waitingMask = 0;
    std::uint64_t timeoutNs = 0;
};

using NpusanSyncErrorDetail =
    std::variant<NpusanSyncBarrierError, NpusanSyncPairingError, NpusanSyncSequenceError, NpusanSyncObjectError>;

struct NpusanRaceAccessSite {
    NpusanReportExecContext exec;
    NpusanReportMemoryAccess access;

    std::uint64_t value = 0;
    bool hasValue = false;
    std::uint32_t syncEpoch = 0;
    std::uint32_t flags = 0;
};

struct NpusanSocStateRef {
    std::uint32_t stateKind = 0;
    std::uint32_t scope = 0;
    std::uint32_t registerId = 0;
    std::uint32_t ownerCoreId = std::numeric_limits<std::uint32_t>::max();

    std::uint64_t stateId = 0;
    std::uint64_t oldValue = 0;
    std::uint64_t newValue = 0;
    std::uint64_t expectedValue = 0;
    std::uint64_t observedValue = 0;
};

struct NpusanMemcheckReport {
    NpusanReportCommon common;
    NpusanReportMemoryAccess access;
    NpusanReportAllocation allocation;
    NpusanReportAllocation nearestAllocation;

    std::int64_t distanceBytes = 0;
    NpusanReportDistanceKind distanceKind = NpusanReportDistanceKind::kUnknown;
    std::uint32_t mallocFreeErrorKind = 0;
    std::uint32_t apiErrorCode = 0;

    std::string apiName;
    std::string apiErrorName;
    std::string apiErrorMessage;
};

struct NpusanInitcheckReport {
    NpusanReportCommon common;
    NpusanReportMemoryAccess access;
    NpusanReportAllocation allocation;

    std::uint64_t firstUninitAddress = 0;
    std::uint64_t firstUninitOffset = 0;
    std::uint64_t uninitBytes = 0;
    std::uint64_t initializedBytes = 0;

    std::uint64_t unusedBytes = 0;
    std::uint32_t unusedPercent = 0;
    std::uint32_t flags = 0;
};

struct NpusanRacecheckReport {
    NpusanReportCommon common;
    std::uint32_t hazardKind = 0;
    std::uint32_t reportMode = 0;
    std::uint32_t scope = 0;
    std::uint32_t flags = 0;

    std::uint64_t hazardCount = 0;
    NpusanRaceAccessSite first;
    NpusanRaceAccessSite second;

    std::uint64_t currentValue = 0;
    std::uint64_t incomingValue = 0;
};

struct NpusanSynccheckReport {
    NpusanReportCommon common;
    NpusanSyncPrimitiveKind primitiveKind = NpusanSyncPrimitiveKind::kUnknown;
    NpusanSyncDetailKind detailKind = NpusanSyncDetailKind::kBarrier;
    bool hasRelatedPoint = false;
    NpusanSyncPoint triggerPoint;
    NpusanSyncPoint relatedPoint;
    NpusanSyncErrorDetail detail = NpusanSyncBarrierError{};
};

struct NpusanSoccheckReport {
    NpusanReportCommon common;
    NpusanSocStateRef state;
    NpusanReportExecContext producer;
    NpusanReportExecContext consumer;

    std::uint64_t previousSerialNo = 0;
    std::uint64_t currentSerialNo = 0;
    std::uint64_t stateFlags = 0;
};

struct NpusanReportRecord {
    ReportTool tool = ReportTool::kMemcheck;
    std::uint32_t pattern = 0;

    using Payload = std::variant<
        std::monostate, const NpusanMemcheckReport*, const NpusanInitcheckReport*, const NpusanRacecheckReport*,
        const NpusanSynccheckReport*, const NpusanSoccheckReport*>;

    const Payload& GetPayload() const { return payload_; }

    static NpusanReportRecord From(const NpusanMemcheckReport& report)
    {
        NpusanReportRecord record{};
        record.tool = ReportTool::kMemcheck;
        record.pattern = report.common.pattern;
        record.payload_ = &report;
        return record;
    }

    static NpusanReportRecord From(const NpusanInitcheckReport& report)
    {
        NpusanReportRecord record{};
        record.tool = ReportTool::kInitcheck;
        record.pattern = report.common.pattern;
        record.payload_ = &report;
        return record;
    }

    static NpusanReportRecord From(const NpusanRacecheckReport& report)
    {
        NpusanReportRecord record{};
        record.tool = ReportTool::kRacecheck;
        record.pattern = report.common.pattern;
        record.payload_ = &report;
        return record;
    }

    static NpusanReportRecord From(const NpusanSynccheckReport& report)
    {
        NpusanReportRecord record{};
        record.tool = ReportTool::kSynccheck;
        record.pattern = report.common.pattern;
        record.payload_ = &report;
        return record;
    }

    static NpusanReportRecord From(const NpusanSoccheckReport& report)
    {
        NpusanReportRecord record{};
        record.tool = ReportTool::kSoccheck;
        record.pattern = report.common.pattern;
        record.payload_ = &report;
        return record;
    }

    static NpusanReportRecord From(NpusanMemcheckReport&&) = delete;
    static NpusanReportRecord From(NpusanInitcheckReport&&) = delete;
    static NpusanReportRecord From(NpusanRacecheckReport&&) = delete;
    static NpusanReportRecord From(NpusanSynccheckReport&&) = delete;
    static NpusanReportRecord From(NpusanSoccheckReport&&) = delete;
    static NpusanReportRecord From(const NpusanMemcheckReport&&) = delete;
    static NpusanReportRecord From(const NpusanInitcheckReport&&) = delete;
    static NpusanReportRecord From(const NpusanRacecheckReport&&) = delete;
    static NpusanReportRecord From(const NpusanSynccheckReport&&) = delete;
    static NpusanReportRecord From(const NpusanSoccheckReport&&) = delete;

private:
    Payload payload_;
};

} // namespace aclsan::cann

#endif
