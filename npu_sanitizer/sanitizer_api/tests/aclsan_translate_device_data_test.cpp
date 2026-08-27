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
constexpr uint64_t kCopyGmToUbufAlignV2B32Id = 86;
constexpr uint64_t kCopyUbufToGmAlignV2Id = 83;
constexpr uint64_t kCopyGmToCbufMultiNd2NzB16Id = 78;
constexpr uint64_t kFixL0cToOutF32Id = 91;
constexpr uint64_t kSetFlagId = 440;
constexpr uint64_t kSetFlagIId = 441;
constexpr uint64_t kWaitFlagId = 442;
constexpr uint64_t kWaitFlagIId = 443;
constexpr uint64_t kSetFlagVId = 456;
constexpr uint64_t kSetFlagIVId = 457;
constexpr uint64_t kWaitFlagVId = 458;
constexpr uint64_t kWaitFlagIVId = 459;

void TestDecodeRawTraceTransferBytes()
{
    sanitizer::AscsanRawTraceRecord record{};
    record.args[2] = (1ULL << 4) | (8256ULL << 25);

    for (uint64_t instructionId : {74ULL, 75ULL, 76ULL, 84ULL, 85ULL, 86ULL, 83ULL}) {
        record.instrId = instructionId;
        assert(aclsan::DecodeRawTraceTransferBytes(record) == 8256);
    }

    record.instrId = kSetFlagId;
    assert(aclsan::DecodeRawTraceTransferBytes(record) == 0);

    record.instrId = kCopyGmToUbufAlignV2B32Id;
    record.args[2] = 8256ULL << 25;
    assert(aclsan::DecodeRawTraceTransferBytes(record) == 0);

    record.args[2] = 1ULL << 4;
    assert(aclsan::DecodeRawTraceTransferBytes(record) == 0);

    constexpr uint64_t maximum = (1ULL << 20) - 1;
    record.args[2] = (maximum << 4) | (maximum << 25);
    assert(aclsan::DecodeRawTraceTransferBytes(record) == maximum * maximum);
}

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
    record.args[2] = (1ULL << 4U) | (128ULL << 25U);
    const aclsan::TraceCallbackContext callbackContext{128, 1002, 2, 0};

    const auto memoryCallback = aclsan::TranslateRawTraceToCallbackData(record, callbackContext);
    assert(memoryCallback.has_value());
    const auto* accesses = std::get_if<aclsan::MemoryCbdata>(&*memoryCallback);
    assert(accesses != nullptr);
    assert(accesses->size() == 1);
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
    assert(source.accessCount == 1);
    assert(source.layout.range.bytes == callbackContext.transferBytes);

    record.instrId = kCopyUbufToGmAlignV2Id;
    record.pipeline = ACLSAN_DEVICE_PIPE_MTE3;
    record.args[0] = 0x300040;
    record.args[1] = 0x400040;
    const auto ubToGmCallback = aclsan::TranslateRawTraceToCallbackData(record, callbackContext);
    assert(ubToGmCallback.has_value());
    const auto* ubToGmAccesses = std::get_if<aclsan::MemoryCbdata>(&*ubToGmCallback);
    assert(ubToGmAccesses != nullptr);
    assert(ubToGmAccesses->size() == 1);
    const AclsanDeviceMemoryAccessData& gmDestination = (*ubToGmAccesses)[0];
    assert(gmDestination.address == record.args[0]);
    assert(gmDestination.memorySpace == ACLSAN_DEVICE_MEMORY_SPACE_GM);
    assert(gmDestination.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_WRITE);
    assert(gmDestination.accessIndex == 0);
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

void TestTranslateMultiAndFixpipeToGmCbdata()
{
    sanitizer::AscsanRawTraceRecord multi{};
    multi.instrId = kCopyGmToCbufMultiNd2NzB16Id;
    multi.pipeline = ACLSAN_DEVICE_PIPE_MTE2;
    multi.args[1] = 0x5000;
    multi.args[2] = (64ULL << 4U) | (4ULL << 48U);
    multi.args[3] = 8ULL | (512ULL << 21U);
    multi.args[4] = 2;
    const aclsan::TraceCallbackContext context{128, 20, 19, 3};

    const auto multiCallback = aclsan::TranslateRawTraceToCallbackData(multi, context);
    assert(multiCallback.has_value());
    const auto* multiAccesses = std::get_if<aclsan::MemoryCbdata>(&*multiCallback);
    assert(multiAccesses != nullptr);
    assert(multiAccesses->size() == 1);
    const auto& multiAccess = multiAccesses->front();
    assert(multiAccess.address == multi.args[1]);
    assert(multiAccess.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_READ);
    assert(multiAccess.layoutKind == ACLSAN_MEM_LAYOUT_ND_AFFINE);
    assert(multiAccess.layout.ndAffine.rank == 2);
    assert(multiAccess.layout.ndAffine.elementBytes == 16);
    assert(multiAccess.layout.ndAffine.dims[0] == 4);
    assert(multiAccess.layout.ndAffine.dims[1] == 2);
    assert(multiAccess.layout.ndAffine.strides[0] == 64);
    assert(multiAccess.layout.ndAffine.strides[1] == 512);
    assert(aclsan::DecodeRawTraceTransferBytes(multi) == 128);

    sanitizer::AscsanRawTraceRecord fixpipe{};
    fixpipe.instrId = kFixL0cToOutF32Id;
    fixpipe.pipeline = ACLSAN_DEVICE_PIPE_FIXPIPE;
    fixpipe.args[0] = 0x8000;
    fixpipe.args[2] = (18ULL << 4U) | (4ULL << 16U) | (64ULL << 32U);

    const auto fixpipeCallback = aclsan::TranslateRawTraceToCallbackData(fixpipe, context);
    assert(fixpipeCallback.has_value());
    const auto* fixpipeAccesses = std::get_if<aclsan::MemoryCbdata>(&*fixpipeCallback);
    assert(fixpipeAccesses != nullptr);
    assert(fixpipeAccesses->size() == 2);
    assert((*fixpipeAccesses)[0].address == 0x8000);
    assert((*fixpipeAccesses)[0].layout.range.bytes == 256);
    assert((*fixpipeAccesses)[1].address == 0x8100);
    assert((*fixpipeAccesses)[1].layout.range.bytes == 32);
    assert(aclsan::DecodeRawTraceTransferBytes(fixpipe) == 288);
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
        logs.find("[cbdata] type=AclsanDeviceMemoryAccessData index=0 address=0x3000 memorySpace=1 accessMode=2") !=
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
    TestDecodeRawTraceTransferBytes();
    TestTranslateMovOutToL1AlignV2();
    TestTranslateSetAndWaitFlag();
    TestTranslateAllFlagVariantsToCorrectActions();
    TestTranslateCopyUbufToGmAlignV2();
    TestTranslateRawTraceToCallbackData();
    TestTranslateMultiAndFixpipeToGmCbdata();
    TestTranslateDebugLogsShowSyncConversion();
    TestTranslateDebugLogsShowUbufToGmConversion();
    TestRejectUnsupportedInstruction();
    return 0;
}
