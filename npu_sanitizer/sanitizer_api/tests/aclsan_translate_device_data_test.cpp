/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <variant>
#include <unistd.h>

#include "internal/aclsan_internal.h"

namespace {

constexpr uint64_t kCopyGmToCbufAlignV2B16Id = 75;
constexpr uint64_t kCopyGmToCbufV2Id = 73;
constexpr uint64_t kCopyUbufToGmAlignV2Id = 83;
constexpr uint64_t kSetFlagId = 440;
constexpr uint64_t kSetFlagIId = 441;
constexpr uint64_t kWaitFlagId = 442;
constexpr uint64_t kWaitFlagIId = 443;
constexpr uint64_t kSetFlagVId = 456;
constexpr uint64_t kSetFlagIVId = 457;
constexpr uint64_t kWaitFlagVId = 458;
constexpr uint64_t kWaitFlagIVId = 459;

void TestTranslateMovOutToL1AlignV2()
{
    sanitizer::AscsanRawTraceRecord record{};
    record.instrId = kCopyGmToCbufAlignV2B16Id;
    record.args[0] = 0x1000;
    record.args[1] = 0x2000;
    record.args[2] = (5ULL << 0) | (0x12345ULL << 4) | (0x23456ULL << 25) | (0x1AULL << 46) | (0x15ULL << 52) |
                     (1ULL << 58) | (5ULL << 60);
    record.args[3] = 0x123456789ULL | (0x34567ULL << 40);

    const auto translated = aclsan::TranslateRawTraceRecord(record);

    assert(translated.has_value());
    const auto* params = std::get_if<sanitizer::CopyGmToCbufAlignV2ParamField>(&*translated);
    assert(params != nullptr);
    assert(params->instr_id == record.instrId);
    assert(params->dstAddr == record.args[0]);
    assert(params->srcAddr == record.args[1]);
    assert(params->sid == 5);
    assert(params->burstNum == 0x12345);
    assert(params->burstLen == 0x23456);
    assert(params->leftPaddingCount == 0x1A);
    assert(params->rightPaddingCount == 0x15);
    assert(params->dataSelectBit);
    assert(params->l2CacheControl == 5);
    assert(params->burstSrcStride == 0x123456789ULL);
    assert(params->burstDstStride == 0x34567);
}

void TestTranslateSetAndWaitFlag()
{
    sanitizer::AscsanRawTraceRecord record{};
    record.instrId = kSetFlagId;
    record.args[0] = 2;
    record.args[1] = 3;
    record.args[2] = 7;
    const auto setFlag = aclsan::TranslateRawTraceRecord(record);
    assert(setFlag.has_value());
    const auto* setParams = std::get_if<sanitizer::FlagParamField>(&*setFlag);
    assert(setParams != nullptr);
    assert(setParams->srcPipe == 2);
    assert(setParams->dstPipe == 3);
    assert(setParams->eventId == 7);

    record.instrId = kWaitFlagId;
    const auto waitFlag = aclsan::TranslateRawTraceRecord(record);
    assert(waitFlag.has_value());
    const auto* waitParams = std::get_if<sanitizer::FlagParamField>(&*waitFlag);
    assert(waitParams != nullptr);
    assert(waitParams->instr_id == kWaitFlagId);
}

void TestTranslateAllFlagVariantsToCorrectActions()
{
    sanitizer::AscsanRawTraceRecord record{};
    record.pipeline = ACLSAN_DEVICE_PIPE_SCALAR;
    record.args[0] = 4;
    record.args[1] = 1;
    record.args[2] = 5;
    const aclsan::TraceCallbackContext context{0, 1, 0, 2};

    for (uint64_t instructionId : {kSetFlagId, kSetFlagIId, kSetFlagVId, kSetFlagIVId}) {
        record.instrId = instructionId;
        const auto callback = aclsan::TranslateRawTraceToCallbackData(record, context);
        assert(callback.has_value());
        const auto* sync = std::get_if<AclsanDeviceSyncData>(&*callback);
        assert(sync != nullptr);
        assert(sync->action == ACLSAN_DEVICE_SYNC_ACTION_SET);
        assert(sync->srcPipe == 4);
        assert(sync->dstPipe == 1);
        assert(sync->objectId == 5);
    }

    for (uint64_t instructionId : {kWaitFlagId, kWaitFlagIId, kWaitFlagVId, kWaitFlagIVId}) {
        record.instrId = instructionId;
        const auto callback = aclsan::TranslateRawTraceToCallbackData(record, context);
        assert(callback.has_value());
        const auto* sync = std::get_if<AclsanDeviceSyncData>(&*callback);
        assert(sync != nullptr);
        assert(sync->action == ACLSAN_DEVICE_SYNC_ACTION_WAIT);
    }
}

void TestTranslateCopyUbufToGmAlignV2()
{
    sanitizer::AscsanRawTraceRecord record{};
    record.instrId = kCopyUbufToGmAlignV2Id;
    record.args[0] = 0x3000;
    record.args[1] = 0x4000;
    record.args[2] = (5ULL << 0) | (0x12345ULL << 4) | (0x23456ULL << 25) | (5ULL << 60);
    record.args[3] = 0x123456789ULL | (0x34567ULL << 40);

    const auto translated = aclsan::TranslateRawTraceRecord(record);

    assert(translated.has_value());
    const auto* params = std::get_if<sanitizer::CopyUbufToGmAlignV2ParamField>(&*translated);
    assert(params != nullptr);
    assert(params->instr_id == record.instrId);
    assert(params->dstAddr == record.args[0]);
    assert(params->srcAddr == record.args[1]);
    assert(params->sid == 5);
    assert(params->burstNum == 0x12345);
    assert(params->burstLen == 0x23456);
    assert(params->l2CacheControl == 5);
    assert(params->dstStride == 0x123456789ULL);
    assert(params->srcStride == 0x34567);
}

void TestTranslateRawTraceToCallbackData()
{
    sanitizer::AscsanRawTraceRecord record{};
    record.instrId = kCopyGmToCbufAlignV2B16Id;
    record.pc = 0x100;
    record.blockId = 1;
    record.siteId = 2;
    record.args[0] = 0x200040;
    record.args[1] = 0x100040;
    const aclsan::TraceCallbackContext callbackContext{128, 1002, 2, 0};

    const auto memoryCallback = aclsan::TranslateRawTraceToCallbackData(record, callbackContext);
    assert(memoryCallback.has_value());
    const auto* accesses = std::get_if<aclsan::DeviceMemoryAccessDataArray>(&*memoryCallback);
    assert(accesses != nullptr);
    const AclsanDeviceMemoryAccessData& source = (*accesses)[0];
    assert(source.header.pc == record.pc);
    assert(source.header.siteId == record.siteId);
    assert(source.header.blockId == record.blockId);
    assert(source.header.launchId == 0);
    assert(source.header.instrExecId == callbackContext.instrExecId);
    assert(source.header.serialNo == callbackContext.serialNo);
    assert(source.header.coreId == callbackContext.coreId);
    assert(source.address == record.args[1]);
    assert(source.memorySpace == ACLSAN_DEVICE_MEMORY_SPACE_GM);
    assert(source.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_READ);
    assert(source.accessIndex == 0);
    assert(source.accessCount == 2);
    assert(source.layout.range.bytes == callbackContext.transferBytes);

    const AclsanDeviceMemoryAccessData& destination = (*accesses)[1];
    assert(destination.address == record.args[0]);
    assert(destination.memorySpace == ACLSAN_DEVICE_MEMORY_SPACE_L1);
    assert(destination.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_WRITE);
    assert(destination.accessIndex == 1);
    assert(destination.accessCount == 2);
    assert(destination.layout.range.bytes == callbackContext.transferBytes);

    record.instrId = kCopyUbufToGmAlignV2Id;
    record.pipeline = ACLSAN_DEVICE_PIPE_MTE3;
    record.args[0] = 0x300040;
    record.args[1] = 0x400040;
    const auto ubToGmCallback = aclsan::TranslateRawTraceToCallbackData(record, callbackContext);
    assert(ubToGmCallback.has_value());
    const auto* ubToGmAccesses = std::get_if<aclsan::DeviceMemoryAccessDataArray>(&*ubToGmCallback);
    assert(ubToGmAccesses != nullptr);
    const AclsanDeviceMemoryAccessData& ubSource = (*ubToGmAccesses)[0];
    assert(ubSource.address == record.args[1]);
    assert(ubSource.memorySpace == ACLSAN_DEVICE_MEMORY_SPACE_UB);
    assert(ubSource.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_READ);
    assert(ubSource.accessIndex == 0);
    assert(ubSource.header.pipeline == ACLSAN_DEVICE_PIPE_MTE3);
    assert(ubSource.layout.range.bytes == callbackContext.transferBytes);

    const AclsanDeviceMemoryAccessData& gmDestination = (*ubToGmAccesses)[1];
    assert(gmDestination.address == record.args[0]);
    assert(gmDestination.memorySpace == ACLSAN_DEVICE_MEMORY_SPACE_GM);
    assert(gmDestination.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_WRITE);
    assert(gmDestination.accessIndex == 1);
    assert(gmDestination.header.pc == record.pc);
    assert(gmDestination.header.instrExecId == callbackContext.instrExecId);

    record.instrId = kSetFlagId;
    record.pipeline = ACLSAN_DEVICE_PIPE_SCALAR;
    record.args[0] = 2;
    record.args[1] = 3;
    record.args[2] = 7;
    const auto setCallback = aclsan::TranslateRawTraceToCallbackData(record, callbackContext);
    assert(setCallback.has_value());
    const auto* setSync = std::get_if<AclsanDeviceSyncData>(&*setCallback);
    assert(setSync != nullptr);
    assert(setSync->header.version == ACLSAN_API_VERSION);
    assert(setSync->header.size == sizeof(AclsanDeviceSyncData));
    assert(setSync->header.launchId == 0);
    assert(setSync->header.sourceKind == ACLSAN_DEVICE_SOURCE_SET_WAIT_FLAG);
    assert(setSync->action == ACLSAN_DEVICE_SYNC_ACTION_SET);
    assert(setSync->srcPipe == 2);
    assert(setSync->dstPipe == 3);
    assert(setSync->objectId == 7);
    assert(setSync->header.instrExecId == callbackContext.instrExecId);
    assert(setSync->header.coreId == callbackContext.coreId);

    record.instrId = kWaitFlagId;
    const auto waitCallback = aclsan::TranslateRawTraceToCallbackData(record, callbackContext);
    assert(waitCallback.has_value());
    const auto* waitSync = std::get_if<AclsanDeviceSyncData>(&*waitCallback);
    assert(waitSync != nullptr);
    assert(waitSync->action == ACLSAN_DEVICE_SYNC_ACTION_WAIT);
    assert(waitSync->header.sourceKind == ACLSAN_DEVICE_SOURCE_SET_WAIT_FLAG);
}

std::string CaptureTranslateDebugLogs(
    const sanitizer::AscsanRawTraceRecord& record, const aclsan::TraceCallbackContext& context)
{
    assert(setenv("ASCEND_GLOBAL_LOG_LEVEL", "0", 1) == 0);
    assert(setenv("NPU_SAN_DEBUG", "1", 1) == 0);

    int pipeFds[2] = {-1, -1};
    assert(pipe(pipeFds) == 0);
    const int savedStdout = dup(STDOUT_FILENO);
    assert(savedStdout >= 0);
    assert(dup2(pipeFds[1], STDOUT_FILENO) >= 0);
    assert(close(pipeFds[1]) == 0);

    const auto translated = aclsan::TranslateRawTraceToCallbackData(record, context);
    assert(translated.has_value());
    assert(std::fflush(stdout) == 0);
    assert(dup2(savedStdout, STDOUT_FILENO) >= 0);
    assert(close(savedStdout) == 0);

    std::string logs;
    char buffer[256] = {};
    ssize_t bytesRead = 0;
    while ((bytesRead = read(pipeFds[0], buffer, sizeof(buffer))) > 0) {
        logs.append(buffer, static_cast<size_t>(bytesRead));
    }
    assert(bytesRead == 0);
    assert(close(pipeFds[0]) == 0);
    assert(unsetenv("ASCEND_GLOBAL_LOG_LEVEL") == 0);
    assert(unsetenv("NPU_SAN_DEBUG") == 0);
    return logs;
}

void TestTranslateDebugLogsShowSyncConversion()
{
    sanitizer::AscsanRawTraceRecord record{};
    record.blockId = 4;
    record.pc = 0x1234;
    record.instrId = kSetFlagId;
    record.siteId = 5;
    record.pipeline = ACLSAN_DEVICE_PIPE_SCALAR;
    record.args[0] = 2;
    record.args[1] = 3;
    record.args[2] = 7;
    const aclsan::TraceCallbackContext context{0, 9, 8, 6};

    const std::string logs = CaptureTranslateDebugLogs(record, context);
    assert(logs.find("[raw]") != std::string::npos);
    assert(logs.find("type=AscsanRawTraceRecord") != std::string::npos);
    assert(logs.find("blockId=4 pc=0x1234") != std::string::npos);
    assert(logs.find("instrId=440") != std::string::npos);
    assert(logs.find("siteId=5 pipeline=" + std::to_string(ACLSAN_DEVICE_PIPE_SCALAR)) != std::string::npos);
    assert(logs.find("args=[0x2,0x3,0x7,0x0,0x0,0x0]") != std::string::npos);
    assert(logs.find("transferBytes=0 instrExecId=9 serialNo=8 coreId=6") != std::string::npos);
    assert(logs.find("[param]") != std::string::npos);
    assert(logs.find("type=FlagParamField") != std::string::npos);
    assert(logs.find("srcPipe=2 dstPipe=3 eventId=7") != std::string::npos);
    assert(logs.find("[cbdata]") != std::string::npos);
    assert(logs.find("type=AclsanDeviceSyncData") != std::string::npos);
    assert(logs.find("pc=0x1234 instrExecId=9 launchId=0 blockId=4 coreId=6") != std::string::npos);
    assert(logs.find("action=1") != std::string::npos);
    assert(logs.find("objectId=7") != std::string::npos);
}

void TestTranslateDebugLogsShowUbufToGmConversion()
{
    sanitizer::AscsanRawTraceRecord record{};
    record.blockId = 3;
    record.pc = 0x5678;
    record.instrId = kCopyUbufToGmAlignV2Id;
    record.siteId = 4;
    record.pipeline = ACLSAN_DEVICE_PIPE_MTE3;
    record.args[0] = 0x3000;
    record.args[1] = 0x4000;
    record.args[2] = (2ULL << 4) | (64ULL << 25);
    record.args[3] = 0x1234;
    const aclsan::TraceCallbackContext context{128, 11, 10, 5};

    const std::string logs = CaptureTranslateDebugLogs(record, context);
    assert(logs.find("[raw] type=AscsanRawTraceRecord") != std::string::npos);
    assert(logs.find("instrId=83 siteId=4 pipeline=" + std::to_string(ACLSAN_DEVICE_PIPE_MTE3)) != std::string::npos);
    assert(logs.find("[param] type=CopyUbufToGmAlignV2ParamField") != std::string::npos);
    assert(logs.find("burstNum=2 burstLen=64") != std::string::npos);
    assert(
        logs.find("[cbdata] type=AclsanDeviceMemoryAccessData index=0 address=0x4000 memorySpace=2 accessMode=1") !=
        std::string::npos);
    assert(
        logs.find("[cbdata] type=AclsanDeviceMemoryAccessData index=1 address=0x3000 memorySpace=1 accessMode=2") !=
        std::string::npos);
}

void TestRejectUnsupportedInstruction()
{
    sanitizer::AscsanRawTraceRecord record{};
    record.instrId = kCopyGmToCbufV2Id;
    assert(!aclsan::TranslateRawTraceRecord(record).has_value());

    record.instrId = UINT64_C(0x100000000);
    assert(!aclsan::TranslateRawTraceRecord(record).has_value());
}

} // namespace

int main()
{
    TestTranslateMovOutToL1AlignV2();
    TestTranslateSetAndWaitFlag();
    TestTranslateAllFlagVariantsToCorrectActions();
    TestTranslateCopyUbufToGmAlignV2();
    TestTranslateRawTraceToCallbackData();
    TestTranslateDebugLogsShowSyncConversion();
    TestTranslateDebugLogsShowUbufToGmConversion();
    TestRejectUnsupportedInstruction();
    return 0;
}
