// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "checker/synccheck.h"

#include "logging/logger.h"

#include <functional>
#include <sstream>

namespace npu::sanitizer {
namespace {

using namespace aclsan::cann;

void HashCombine(size_t& seed, uint64_t value) noexcept
{
    seed ^= std::hash<uint64_t>{}(value) + static_cast<size_t>(0x9e3779b9U) + (seed << 6U) + (seed >> 2U);
}

const char* OperationName(uint32_t syncKind, uint32_t action)
{
    if (syncKind == ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG) {
        if (action == ACLSAN_DEVICE_SYNC_ACTION_SET) {
            return "SET_FLAG";
        }
        if (action == ACLSAN_DEVICE_SYNC_ACTION_WAIT) {
            return "WAIT_FLAG";
        }
    } else if (syncKind == ACLSAN_DEVICE_SYNC_KIND_GET_RLS_BUF) {
        if (action == ACLSAN_DEVICE_SYNC_ACTION_GET) {
            return "GET_BUF";
        }
        if (action == ACLSAN_DEVICE_SYNC_ACTION_RELEASE) {
            return "RLS_BUF";
        }
    }
    return "";
}

const char* ExpectedOperation(uint32_t kind, uint32_t reason)
{
    const bool setWait = kind == ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG;
    if (reason == static_cast<uint32_t>(NpusanSyncMismatchReason::kUnmatchedClose)) {
        return setWait ? "SET_FLAG" : "GET_BUF";
    }
    return setWait ? "WAIT_FLAG" : "RLS_BUF";
}

NpusanReportExecContext ExecContext(const AclsanDeviceSyncData& data)
{
    NpusanReportExecContext exec{};
    exec.launchId = data.header.launchId;
    exec.instrExecId = data.header.instrExecId;
    exec.pc = data.header.pc;
    exec.coreId = data.header.coreId;
    exec.blockId = data.header.blockId;
    // Sync instructions execute on PIPE_S independently of their srcPipe/dstPipe pairing arguments.
    exec.pipeId = 0;
    exec.pipeName = "PIPE_S";
    return exec;
}

} // namespace

size_t Synccheck::SyncPairKeyHash::operator()(const SyncPairKey& key) const noexcept
{
    size_t seed = 0;
    HashCombine(seed, key.launchId);
    HashCombine(seed, key.objectId);
    HashCombine(seed, key.coreId);
    HashCombine(seed, key.blockId);
    HashCombine(seed, key.syncKind);
    HashCombine(seed, key.scope);
    HashCombine(seed, key.srcPipe);
    HashCombine(seed, key.dstPipe);
    HashCombine(seed, key.mode);
    return seed;
}

bool Synccheck::BufferOccupancyKey::operator==(const BufferOccupancyKey& other) const noexcept
{
    return objectId == other.objectId && coreId == other.coreId;
}

size_t Synccheck::BufferOccupancyKeyHash::operator()(const BufferOccupancyKey& key) const noexcept
{
    size_t seed = 0;
    HashCombine(seed, key.objectId);
    HashCombine(seed, key.coreId);
    return seed;
}

Synccheck::SyncPairKey Synccheck::BuildExactKey(const AclsanDeviceSyncData& data)
{
    SyncPairKey key{};
    key.launchId = data.header.launchId;
    key.objectId = data.objectId;
    key.coreId = data.header.coreId;
    key.blockId = data.header.blockId;
    key.syncKind = data.syncKind;
    key.scope = data.scope;
    key.dstPipe = data.dstPipe;
    if (data.syncKind == ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG) {
        key.srcPipe = data.srcPipe;
    } else {
        key.mode = data.mode;
    }
    return key;
}

Synccheck::BufferOccupancyKey Synccheck::BuildBufferOccupancyKey(const AclsanDeviceSyncData& data)
{
    return {data.objectId, data.header.coreId};
}

bool Synccheck::IsClose(uint32_t action)
{
    return action == ACLSAN_DEVICE_SYNC_ACTION_WAIT || action == ACLSAN_DEVICE_SYNC_ACTION_RELEASE;
}

bool Synccheck::IsValidEvent(const AclsanDeviceSyncData& data)
{
    if (data.syncKind == ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG) {
        return data.action == ACLSAN_DEVICE_SYNC_ACTION_SET || data.action == ACLSAN_DEVICE_SYNC_ACTION_WAIT;
    }
    if (data.syncKind == ACLSAN_DEVICE_SYNC_KIND_GET_RLS_BUF) {
        return data.action == ACLSAN_DEVICE_SYNC_ACTION_GET || data.action == ACLSAN_DEVICE_SYNC_ACTION_RELEASE;
    }
    return false;
}

std::vector<Synccheck::Report> Synccheck::OnDeviceSync(const AclsanDeviceSyncData& data)
{
    ++stats_.syncEvents;

    if (!IsValidEvent(data)) {
        ++stats_.invalidEvents;
        if (logger_ != nullptr) {
            std::ostringstream message;
            message << "invalid device sync data"
                    << " sync_kind=" << data.syncKind << " action=" << data.action << " launch=" << data.header.launchId
                    << " instr_exec=" << data.header.instrExecId << " pc=0x" << std::hex << data.header.pc << std::dec
                    << " core=" << data.header.coreId << " block=" << data.header.blockId;
            logger_->Error(message.str());
        }
        return {};
    }

    auto& state = launchStates_[data.header.launchId];
    const SyncPairKey key = BuildExactKey(data);
    Reports reports;
    if (data.action == ACLSAN_DEVICE_SYNC_ACTION_SET) {
        const auto [open, inserted] = state.pending.emplace(key, data);
        if (!inserted) {
            // Duplicate flag open, for example:
            // set_flag(V, MTE2, 3) -> set_flag(V, MTE2, 3).
            ++stats_.duplicateOpens;
            reports.push_back(
                BuildMismatch(data, &open->second, static_cast<uint32_t>(NpusanSyncMismatchReason::kDuplicateOpen)));
        }
    } else if (data.action == ACLSAN_DEVICE_SYNC_ACTION_GET) {
        const BufferOccupancyKey occupancyKey = BuildBufferOccupancyKey(data);
        const auto active = state.activeBuffers.find(occupancyKey);
        if (active != state.activeBuffers.end()) {
            // Within one execution domain, duplicate buffer open is keyed by bufId, for example:
            // get_buf(MTE2, 2, 0) -> get_buf(MTE2, 2, 1), or
            // get_buf(MTE2, 2, 1) -> get_buf(MTE3, 2, 1).
            ++stats_.duplicateOpens;
            const auto& open = state.pending.at(active->second);
            reports.push_back(
                BuildMismatch(data, &open, static_cast<uint32_t>(NpusanSyncMismatchReason::kDuplicateOpen)));
        } else {
            state.pending.emplace(key, data);
            state.activeBuffers.emplace(occupancyKey, key);
        }
    } else if (IsClose(data.action)) {
        const auto open = state.pending.find(key);
        if (open == state.pending.end()) {
            // Close without an exact open, for example:
            // wait_flag(V, MTE2, 3), or
            // set_flag(V, MTE3, 3) -> wait_flag(V, MTE2, 3), or
            // get_buf(MTE2, 2, 0) -> rls_buf(MTE2, 2, 1).
            ++stats_.unmatchedCloses;
            reports.push_back(
                BuildMismatch(data, nullptr, static_cast<uint32_t>(NpusanSyncMismatchReason::kUnmatchedClose)));
        } else {
            if (data.action == ACLSAN_DEVICE_SYNC_ACTION_RELEASE) {
                state.activeBuffers.erase(BuildBufferOccupancyKey(data));
            }
            state.pending.erase(open);
            ++stats_.matchedPairs;
        }
    }
    stats_.pendingOpens = 0;
    for (const auto& launch : launchStates_) {
        stats_.pendingOpens += launch.second.pending.size();
    }
    return reports;
}

std::vector<Synccheck::Report> Synccheck::OnSynchronization()
{
    ++stats_.synchronizationEvents;
    Reports reports;
    const auto state = launchStates_.find(0);
    if (state == launchStates_.end()) {
        return reports;
    }
    for (const auto& entry : state->second.pending) {
        // Open left at synchronization, for example:
        // set_flag(V, MTE2, 3) -> synchronize, or
        // get_buf(MTE2, 2, 1) -> synchronize.
        ++stats_.unconsumedOpens;
        reports.push_back(
            BuildMismatch(entry.second, nullptr, static_cast<uint32_t>(NpusanSyncMismatchReason::kUnconsumedOpen)));
    }
    launchStates_.erase(state);
    stats_.pendingOpens = 0;
    for (const auto& launch : launchStates_) {
        stats_.pendingOpens += launch.second.pending.size();
    }
    return reports;
}

SynccheckStats Synccheck::Stats() const { return stats_; }

NpusanSyncPoint Synccheck::ActualPoint(const AclsanDeviceSyncData& data)
{
    NpusanSyncPoint point{};
    point.operation = OperationName(data.syncKind, data.action);
    point.hasExecContext = true;
    point.exec = ExecContext(data);
    return point;
}

NpusanSyncPoint Synccheck::ExpectedPoint(const AclsanDeviceSyncData& data, uint32_t reason)
{
    NpusanSyncPoint point{};
    point.operation = ExpectedOperation(data.syncKind, reason);
    return point;
}

Synccheck::Report Synccheck::BuildMismatch(
    const AclsanDeviceSyncData& trigger, const AclsanDeviceSyncData* related, uint32_t reason)
{
    Report report{};
    report.common.tool = ReportTool::kSynccheck;
    report.common.severity = ReportSeverity::kError;
    report.common.pattern = static_cast<uint32_t>(NpusanSynccheckPattern::kPairingMismatch);
    report.common.flags = kNpusanReportCommonHasExecContext;
    report.primitiveKind = trigger.syncKind == ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG ?
                               NpusanSyncPrimitiveKind::kSetWaitFlag :
                               NpusanSyncPrimitiveKind::kGetRlsBuf;
    report.detailKind = NpusanSyncDetailKind::kPairing;
    report.hasRelatedPoint = true;
    report.triggerPoint = ActualPoint(trigger);
    report.common.exec = report.triggerPoint.exec;
    report.relatedPoint = related == nullptr ? ExpectedPoint(trigger, reason) : ActualPoint(*related);

    NpusanSyncPairingError detail{};
    detail.reason = static_cast<NpusanSyncMismatchReason>(reason);
    const SyncPairKey key = BuildExactKey(trigger);
    detail.key.pairKind = trigger.syncKind == ACLSAN_DEVICE_SYNC_KIND_SET_WAIT_FLAG ? NpusanSyncPairKind::kSetWaitFlag :
                                                                                      NpusanSyncPairKind::kGetRlsBuf;
    detail.key.srcPipe = key.srcPipe;
    detail.key.dstPipe = key.dstPipe;
    detail.key.mode = key.mode;
    detail.key.id = key.objectId;
    report.detail = detail;
    return report;
}

} // namespace npu::sanitizer
