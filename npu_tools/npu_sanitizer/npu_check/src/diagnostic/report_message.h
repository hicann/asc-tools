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
    MEMCHECK = 1,
    INITCHECK = 2,
    RACECHECK = 3,
    SYNCCHECK = 4,
    SOCCHECK = 5,
};

enum class ReportSeverity {
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    FATAL = 4,
};

enum class ReportStackRole {
    NONE = 0,
    FAULT_DEVICE = 1,
    HOST_LAUNCH = 2,
    HOST_ALLOC = 3,
    HOST_FREE = 4,
    RELATED_ACCESS_A = 5,
    RELATED_ACCESS_B = 6,
    SYNC_PRODUCER = 7,
    SYNC_CONSUMER = 8,
    STATE_PRODUCER = 9,
    STATE_CONSUMER = 10,
    PEER_DEVICE = 11,
    SYNC_TRIGGER = 12,
    SYNC_RELATED = 13,
    HOST_API_CALL = 14,
};

enum class ReportStackFormat {
    NONE = 0,
    RAW_TEXT = 1,
    FRAMES = 2,
    BOTH = 3,
};

struct ReportFrame {
    std::uint64_t pc = 0;
    std::uint64_t offset = 0;
    std::string function;
    std::string file;
    std::uint32_t line = 0;        // One-based source line; 0 means unknown.
    std::uint32_t column = 0;      // One-based source column; 0 means unknown.
    std::uint32_t inlineDepth = 0; // Inline call depth; 0 is the physical frame.
    std::uint32_t flags = 0;       // Frame metadata bit mask; no bits are defined in P0.
};

struct ReportCallStack {
    ReportStackRole role = ReportStackRole::FAULT_DEVICE;
    ReportStackFormat format = ReportStackFormat::NONE;
    std::string rawText;
    std::vector<ReportFrame> frames;
};

enum class NpusanReportMemorySpace {
    UNKNOWN = 0,
    GM = 1,
    UB = 2,
    L1 = 3,
    L0_A = 4,
    L0_B = 5,
    L0_C = 6,
    BT = 7,
    PRIVATE = 8,
    HOST = 9,
};

enum class NpusanReportAccessMode {
    READ = 1,
    WRITE = 2,
    READ_WRITE = 3,
    FREE = 4,
};

enum class NpusanReportDistanceKind {
    UNKNOWN = 0,
    INSIDE = 1,
    BEFORE = 2,
    AFTER = 3,
};

enum class NpusanReportPattern : std::uint32_t {
    UNKNOWN = 0,

    MEMCHECK_INVALID_ACCESS = 0x0101,
    MEMCHECK_MISALIGNED_ACCESS = 0x0102,
    MEMCHECK_USE_AFTER_FREE = 0x0103,
    MEMCHECK_USE_BEFORE_ALLOC = 0x0104,
    MEMCHECK_INVALID_FREE = 0x0105,
    MEMCHECK_DOUBLE_FREE = 0x0106,
    MEMCHECK_LEAK = 0x0107,
    MEMCHECK_API_ERROR = 0x0108,

    INITCHECK_UNINITIALIZED_READ = 0x0201,
    INITCHECK_PARTIAL_UNINITIALIZED_READ = 0x0202,
    INITCHECK_UNUSED_MEMORY = 0x0203,
    INITCHECK_API_READ_UNINITIALIZED = 0x0204,

    RACECHECK_ANALYSIS = 0x0301,
    RACECHECK_HAZARD_RAW = 0x0302,
    RACECHECK_HAZARD_WAR = 0x0303,
    RACECHECK_HAZARD_WAW = 0x0304,
    RACECHECK_ATOMIC_RACE = 0x0305,
    RACECHECK_CROSS_PIPE_RACE = 0x0306,
    RACECHECK_INTER_CORE_RACE = 0x0307,
    RACECHECK_INVALID_REMOTE_ACCESS = 0x0308,

    SYNCCHECK_INTRA_CORE_DIVERGENT = 0x0401,
    SYNCCHECK_INTER_CORE_DIVERGENT = 0x0402,
    SYNCCHECK_INVALID_ARGUMENT = 0x0403,
    SYNCCHECK_PAIRING_MISMATCH = 0x0404,
    SYNCCHECK_PARTICIPANT_MISMATCH = 0x0405,
    SYNCCHECK_DEADLOCK = 0x0406,
    SYNCCHECK_OBJECT_NOT_INITIALIZED = 0x0407,
    SYNCCHECK_INSTRUCTION_SEQUENCE_MISMATCH = 0x0408,

    SOCCHECK_UNINITIALIZED_STATE_READ = 0x0501,
    SOCCHECK_REGISTER_MISMATCH = 0x0502,
    SOCCHECK_ILLEGAL_STATE_TRANSITION = 0x0503,
    SOCCHECK_STATE_NOT_RESTORED = 0x0504,
    SOCCHECK_CROSS_CORE_STATE_INCONSISTENT = 0x0505,
    SOCCHECK_SCOPE_VIOLATION = 0x0506,
};

enum class NpusanSyncMismatchReason {
    UNKNOWN = 0,
    DUPLICATE_OPEN = 1,
    UNMATCHED_CLOSE = 2,
    UNCONSUMED_OPEN = 3,
};

enum class NpusanSyncPairKind {
    UNKNOWN = 0,
    SET_WAIT_FLAG = 1,
    GET_RLS_BUF = 2,
};

enum class NpusanSyncDetailKind {
    BARRIER = 1,
    PAIRING = 2,
    SEQUENCE = 3,
    OBJECT = 4,
};

enum class NpusanSyncPrimitiveKind {
    UNKNOWN = 0,
    BARRIER = 1,
    SET_WAIT_FLAG = 2,
    GET_RLS_BUF = 3,
    INSTRUCTION_SEQUENCE = 4,
    SYNC_OBJECT = 5,
};

struct NpusanReportExecContext {
    std::uint64_t launchId = 0;
    std::uint64_t binaryId = 0;
    std::uint64_t functionId = 0;
    std::uint64_t instrExecId = 0;
    std::uint64_t serialNo = 0;
    std::uint64_t pc = 0;
    std::uint64_t offset = 0;

    std::uint32_t deviceId = 0; // Runtime logical device identifier.
    std::uint32_t phyCoreId =
        std::numeric_limits<std::uint32_t>::max(); // Physical AI Core index; UINT32_MAX means unknown.
    std::uint32_t blockId = 0;                     // Logical block identifier within the kernel launch.
    std::uint32_t blockType = 0;                   // AclsanDeviceBlockType value from the device callback ABI.
    std::uint32_t pipeId = 0;                      // AclsanDevicePipeline value from the device callback ABI.
    std::uint32_t siteId = 0;                      // Instrumentation site identifier; 0 means unavailable.
    std::uint32_t line = 0;                        // One-based source line; 0 means unknown.
    std::uint32_t column = 0;                      // One-based source column; 0 means unknown.

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

    ReportTool tool = ReportTool::MEMCHECK;
    ReportSeverity severity = ReportSeverity::ERROR;
    NpusanReportPattern pattern = NpusanReportPattern::UNKNOWN;
    std::uint32_t flags = 0; // NpusanReportCommon flags bit mask.

    NpusanReportExecContext exec;
    std::uint32_t stackCount = 0; // Number of active entries in the compact stacks prefix.
    std::array<ReportCallStack, kNpusanReportStackMax> stacks{};
};

struct NpusanReportMemoryAccess {
    NpusanReportMemorySpace memorySpace = NpusanReportMemorySpace::UNKNOWN;
    NpusanReportAccessMode accessMode = NpusanReportAccessMode::READ;
    std::uint32_t accessBytes = 0;   // Number of bytes accessed by the reported operation.
    std::uint32_t requiredAlign = 0; // Required byte alignment; 0 means no known requirement.

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

    NpusanReportMemorySpace memorySpace = NpusanReportMemorySpace::UNKNOWN;
    std::uint32_t deviceId = 0; // Runtime logical device that owns the allocation.
    std::uint32_t state = 0;    // Checker-defined allocation lifecycle state; 0 means unknown.
    std::uint32_t flags = 0;    // Checker-defined allocation metadata bit mask.
};

struct NpusanSyncPoint {
    std::string operation;
    bool hasExecContext = false;
    NpusanReportExecContext exec;
    ReportStackRole stackRole = ReportStackRole::NONE;
};

struct NpusanSyncPairKey {
    NpusanSyncPairKind pairKind = NpusanSyncPairKind::UNKNOWN;
    std::uint32_t srcPipe = 0; // AclsanDevicePipeline source value for SET_FLAG/WAIT_FLAG pairs.
    std::uint32_t dstPipe = 0; // AclsanDevicePipeline destination or buffer-pipeline value.
    std::uint32_t mode = 0;    // GET_BUF/RLS_BUF mode; 0 for SET_FLAG/WAIT_FLAG pairs.
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
    NpusanSyncMismatchReason reason = NpusanSyncMismatchReason::UNKNOWN;
    NpusanSyncPairKey key;
};

struct NpusanSyncSequenceError {
    std::string reason;
    std::uint32_t sequenceIndex = 0; // Zero-based position in the observed synchronization sequence.
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
    std::uint32_t syncEpoch = 0; // Checker-defined happens-before epoch; 0 means unavailable.
    std::uint32_t flags = 0;     // Checker-defined access-site metadata bit mask.
};

struct NpusanSocStateRef {
    std::uint32_t stateKind = 0;  // Checker-defined SOC state category; 0 means unknown.
    std::uint32_t scope = 0;      // Checker-defined state visibility scope; 0 means unknown.
    std::uint32_t registerId = 0; // Register identifier when stateKind denotes a register.
    std::uint32_t ownerCoreId =
        std::numeric_limits<std::uint32_t>::max(); // Owning physical AI Core; UINT32_MAX means unknown.

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
    NpusanReportDistanceKind distanceKind = NpusanReportDistanceKind::UNKNOWN;
    std::uint32_t mallocFreeErrorKind = 0; // Checker-defined allocation API error category; 0 means unknown.
    std::uint32_t apiErrorCode = 0;        // Runtime API status code reported by the failing call.

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
    std::uint32_t unusedPercent = 0; // Integer percentage of allocation bytes never initialized.
    std::uint32_t flags = 0;         // Checker-defined initcheck metadata bit mask.
};

struct NpusanRacecheckReport {
    NpusanReportCommon common;
    std::uint32_t hazardKind = 0; // Checker-defined RAW/WAR/WAW/atomic hazard category; 0 means unknown.
    std::uint32_t reportMode = 0; // Checker-defined race reporting mode; 0 means the default mode.
    std::uint32_t scope = 0;      // Checker-defined synchronization scope; 0 means unknown.
    std::uint32_t flags = 0;      // Checker-defined race metadata bit mask.

    std::uint64_t hazardCount = 0;
    NpusanRaceAccessSite first;
    NpusanRaceAccessSite second;

    std::uint64_t currentValue = 0;
    std::uint64_t incomingValue = 0;
};

struct NpusanSynccheckReport {
    NpusanReportCommon common;
    NpusanSyncPrimitiveKind primitiveKind = NpusanSyncPrimitiveKind::UNKNOWN;
    NpusanSyncDetailKind detailKind = NpusanSyncDetailKind::BARRIER;
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

class NpusanReportRecord {
public:
    ReportTool tool = ReportTool::MEMCHECK;
    NpusanReportPattern pattern = NpusanReportPattern::UNKNOWN;

    using Payload = std::variant<
        std::monostate, const NpusanMemcheckReport*, const NpusanInitcheckReport*, const NpusanRacecheckReport*,
        const NpusanSynccheckReport*, const NpusanSoccheckReport*>;

    const Payload& GetPayload() const { return payload_; }

    static NpusanReportRecord From(const NpusanMemcheckReport& report)
    {
        NpusanReportRecord record{};
        record.tool = ReportTool::MEMCHECK;
        record.pattern = report.common.pattern;
        record.payload_ = &report;
        return record;
    }

    static NpusanReportRecord From(const NpusanInitcheckReport& report)
    {
        NpusanReportRecord record{};
        record.tool = ReportTool::INITCHECK;
        record.pattern = report.common.pattern;
        record.payload_ = &report;
        return record;
    }

    static NpusanReportRecord From(const NpusanRacecheckReport& report)
    {
        NpusanReportRecord record{};
        record.tool = ReportTool::RACECHECK;
        record.pattern = report.common.pattern;
        record.payload_ = &report;
        return record;
    }

    static NpusanReportRecord From(const NpusanSynccheckReport& report)
    {
        NpusanReportRecord record{};
        record.tool = ReportTool::SYNCCHECK;
        record.pattern = report.common.pattern;
        record.payload_ = &report;
        return record;
    }

    static NpusanReportRecord From(const NpusanSoccheckReport& report)
    {
        NpusanReportRecord record{};
        record.tool = ReportTool::SOCCHECK;
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
