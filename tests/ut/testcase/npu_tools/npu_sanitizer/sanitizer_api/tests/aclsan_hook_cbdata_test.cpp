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
#include <array>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <sys/wait.h>
#include <type_traits>
#include <unistd.h>
#include <utility>

#include "aclsan/aclsan_api.h"
#include "aclsan/aclsan_cbdata.h"
#include "device_instr/common/instruction_id.h"
#include "device_instr/decoder_registry.h"
#include "internal/aclsan_trace_buffer.h"
#include "internal/aclsan_trace_runtime.h"
#include "../src/aclsan/aclsan_dispatch.cpp"
#include "../src/aclsan/aclsan_hook_aclrt.cpp"

namespace {

template <typename T, typename = void>
struct CanDispatchDeviceMemoryAccess : std::false_type {};

template <typename T>
struct CanDispatchDeviceMemoryAccess<
    T, std::void_t<decltype(aclsan::AclsanCallbackDispatcher::DispatchDeviceMemoryAccess(std::declval<const T&>()))>>
    : std::true_type {};

static_assert(!CanDispatchDeviceMemoryAccess<aclsan::DeviceMemoryAccessDataList>::value);

struct CallbackCapture {
    AclsanCallbackDomain domain = ACLSAN_CB_DOMAIN_INVALID;
    AclsanCallbackId callbackId{};
    AclsanResourceData resource{};
    AclsanSynchronizeData synchronize{};
    uint32_t calls = 0;
};

CallbackCapture g_callbackCapture{};
bool g_callbackEnabled = true;
uint32_t g_deviceMemoryCallbackCount = 0;
uint32_t g_deviceSyncCallbackCount = 0;
uint32_t g_decoderCallCount = 0;
std::array<AclsanDeviceMemoryAccessData, 4> g_deviceMemoryCallbacks{};
std::array<AclsanDeviceSyncData, 3> g_deviceSyncCallbacks{};
void* g_lastFreedAddress = nullptr;
int32_t g_lastSynchronizeTimeout = 0;
bool g_mallocOriginalAvailable = true;
bool g_freeOriginalAvailable = true;
int32_t g_currentDeviceId = 3;
aclError g_getDeviceResult = ACL_SUCCESS;
int32_t g_clearCallbackResult = 0;
int32_t g_registerMallocResult = 0;

void ResetCapture()
{
    g_callbackCapture = {};
    g_callbackEnabled = true;
    g_deviceMemoryCallbackCount = 0;
    g_deviceSyncCallbackCount = 0;
    g_deviceMemoryCallbacks = {};
    g_deviceSyncCallbacks = {};
    g_lastFreedAddress = nullptr;
    g_lastSynchronizeTimeout = 0;
    g_currentDeviceId = 3;
    g_getDeviceResult = ACL_SUCCESS;
    g_clearCallbackResult = 0;
    g_registerMallocResult = 0;
}

void* Address(uintptr_t value) { return reinterpret_cast<void*>(value); }

std::optional<aclsan::DecodedInstruction> CountDecoderCalls(const aclsan::AclsanRawTraceRecord&) noexcept
{
    ++g_decoderCallCount;
    return std::nullopt;
}

template <typename Action>
std::string CaptureDebugLogs(Action action)
{
    assert(setenv("ASCEND_GLOBAL_LOG_LEVEL", "0", 1) == 0);
    assert(setenv("NPU_SAN_DEBUG", "1", 1) == 0);

    int pipeFds[2] = {-1, -1};
    assert(pipe(pipeFds) == 0);
    const int savedStdout = dup(STDOUT_FILENO);
    assert(savedStdout >= 0);
    assert(dup2(pipeFds[1], STDOUT_FILENO) >= 0);
    assert(close(pipeFds[1]) == 0);

    action();
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

aclError FakeAclrtMalloc(void** deviceAddress, size_t size, aclrtMemMallocPolicy policy)
{
    (void)size;
    (void)policy;
    if (deviceAddress == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *deviceAddress = Address(0x12340000U);
    return ACL_SUCCESS;
}

aclError FakeAclrtFree(void* deviceAddress)
{
    g_lastFreedAddress = deviceAddress;
    return ACL_SUCCESS;
}

aclError FakeAclrtSynchronizeStream(aclrtStream stream)
{
    return stream == Address(0x45670000U) ? ACL_SUCCESS : ACL_ERROR_INVALID_PARAM;
}

aclError FakeAclrtSynchronizeStreamWithTimeout(aclrtStream stream, int32_t timeout)
{
    g_lastSynchronizeTimeout = timeout;
    return stream == Address(0x45670000U) ? ACL_SUCCESS : ACL_ERROR_INVALID_PARAM;
}

aclError FakeAclrtGetDevice(int32_t* deviceId)
{
    if (g_getDeviceResult != ACL_SUCCESS) {
        return g_getDeviceResult;
    }
    if (deviceId == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *deviceId = g_currentDeviceId;
    return ACL_SUCCESS;
}

aclError FakeAclrtBinaryGetGlobal(aclrtBinHandle, const char*, void** address, size_t* bytes)
{
    static uint64_t global = 0;
    if (address == nullptr || bytes == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *address = &global;
    *bytes = sizeof(global);
    return ACL_SUCCESS;
}

aclError FakeAclrtGetFunctionAttribute(aclrtFuncHandle, aclrtFuncAttribute, int64_t* attrValue)
{
    if (attrValue == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *attrValue = 0;
    return ACL_SUCCESS;
}

const char* FakeAclrtGetSocName() { return "Ascend950PR_9589"; }

aclError FakeAclrtGetDeviceInfo(uint32_t, aclrtDevAttr attr, int64_t* value)
{
    if (value == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *value = attr == ACL_DEV_ATTR_CUBE_CORE_NUM ? 36 : 72;
    return ACL_SUCCESS;
}

void CheckCommonData(const AclsanCallbackCommonData& common, size_t expectedSize, const char* apiName)
{
    assert(common.version == ACLSAN_API_VERSION);
    assert(common.size == expectedSize);
    assert(std::strcmp(common.apiName, apiName) == 0);
    assert(common.result == ACL_SUCCESS);
    assert(common.correlationId == 0);
}

void TestMallocCallbackData()
{
    ResetCapture();
    void* deviceAddress = nullptr;

    assert(aclrtMallocHook(&deviceAddress, 64, ACL_MEM_MALLOC_HUGE_FIRST) == ACL_SUCCESS);
    assert(g_callbackCapture.calls == 1);
    assert(g_callbackCapture.domain == ACLSAN_CB_DOMAIN_RESOURCE);
    assert(g_callbackCapture.callbackId == ACLSAN_CBID_RESOURCE_MEMORY_ALLOC);
    CheckCommonData(g_callbackCapture.resource.common, sizeof(AclsanResourceData), "aclrtMalloc");
    assert(g_callbackCapture.resource.ptr == deviceAddress);
    assert(g_callbackCapture.resource.bytes == 64);
    assert(g_callbackCapture.resource.memorySpace == ACLSAN_MEMORY_SPACE_DEVICE);
    assert(g_callbackCapture.resource.deviceId == 3);
    assert(g_callbackCapture.resource.resourceId == reinterpret_cast<uintptr_t>(deviceAddress));
}

void TestMallocPreservesOriginalRuntimeError()
{
    ResetCapture();
    assert(aclrtMallocHook(nullptr, 64, ACL_MEM_MALLOC_HUGE_FIRST) == ACL_ERROR_INVALID_PARAM);
}

void TestMallocSkipsCallbackWhenGetDeviceFails()
{
    ResetCapture();
    g_getDeviceResult = ACL_ERROR_RT_INTERNAL_ERROR;
    void* deviceAddress = nullptr;

    assert(aclrtMallocHook(&deviceAddress, 64, ACL_MEM_MALLOC_HUGE_FIRST) == ACL_SUCCESS);
    assert(g_callbackCapture.calls == 0);
}

void TestMissingOriginalMallocAborts()
{
    ResetCapture();
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        g_mallocOriginalAvailable = false;
        void* deviceAddress = nullptr;
        (void)aclrtMallocHook(&deviceAddress, 64, ACL_MEM_MALLOC_HUGE_FIRST);
        _exit(0);
    }

    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
    assert(WTERMSIG(status) == SIGABRT);
}

void TestFreeCallbackData()
{
    ResetCapture();
    void* const deviceAddress = Address(0x12340000U);

    assert(aclrtFreeHook(deviceAddress) == ACL_SUCCESS);
    assert(g_lastFreedAddress == deviceAddress);
    assert(g_callbackCapture.calls == 1);
    assert(g_callbackCapture.domain == ACLSAN_CB_DOMAIN_RESOURCE);
    assert(g_callbackCapture.callbackId == ACLSAN_CBID_RESOURCE_MEMORY_FREE);
    CheckCommonData(g_callbackCapture.resource.common, sizeof(AclsanResourceData), "aclrtFree");
    assert(g_callbackCapture.resource.ptr == deviceAddress);
    assert(g_callbackCapture.resource.bytes == 0);
    assert(g_callbackCapture.resource.memorySpace == ACLSAN_MEMORY_SPACE_DEVICE);
    assert(g_callbackCapture.resource.deviceId == 3);
    assert(g_callbackCapture.resource.resourceId == reinterpret_cast<uintptr_t>(deviceAddress));
}

void TestMissingOriginalFreeAborts()
{
    ResetCapture();
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        g_freeOriginalAvailable = false;
        (void)aclrtFreeHook(Address(0x12340000U));
        _exit(0);
    }

    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
    assert(WTERMSIG(status) == SIGABRT);
}

void TestRuntimeHookRegistrationFailureAborts()
{
    ResetCapture();
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        g_registerMallocResult = 1;
        aclsan::ApplyRuntimeHooks({ACL_RT_API_aclrtMalloc});
        _exit(0);
    }

    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
    assert(WTERMSIG(status) == SIGABRT);
}

void TestRuntimeHookClearingFailureAborts()
{
    ResetCapture();
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        g_clearCallbackResult = 1;
        aclsan::ApplyRuntimeHooks({});
        _exit(0);
    }

    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
    assert(WTERMSIG(status) == SIGABRT);
}

void TestSynchronizeStreamCallbackData()
{
    ResetCapture();
    void* const stream = Address(0x45670000U);

    assert(aclrtSynchronizeStreamHook(stream) == ACL_SUCCESS);
    assert(g_deviceMemoryCallbackCount == 0);
    assert(g_deviceSyncCallbackCount == 0);
    assert(g_callbackCapture.calls == 1);
    assert(g_callbackCapture.domain == ACLSAN_CB_DOMAIN_SYNCHRONIZE);
    assert(g_callbackCapture.callbackId == ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END);
    CheckCommonData(g_callbackCapture.synchronize.common, sizeof(AclsanSynchronizeData), "aclrtSynchronizeStream");
    assert(g_callbackCapture.synchronize.stream == stream);
}

void TestSynchronizeStreamWithTimeoutCallbackData()
{
    ResetCapture();
    void* const stream = Address(0x45670000U);

    assert(aclrtSynchronizeStreamWithTimeoutHook(stream, 1234) == ACL_SUCCESS);
    assert(g_lastSynchronizeTimeout == 1234);
    assert(g_deviceMemoryCallbackCount == 0);
    assert(g_deviceSyncCallbackCount == 0);
    assert(g_callbackCapture.calls == 1);
    assert(g_callbackCapture.domain == ACLSAN_CB_DOMAIN_SYNCHRONIZE);
    assert(g_callbackCapture.callbackId == ACLSAN_CBID_SYNCHRONIZE_STREAM_SYNC_END);
    CheckCommonData(
        g_callbackCapture.synchronize.common, sizeof(AclsanSynchronizeData), "aclrtSynchronizeStreamWithTimeout");
    assert(g_callbackCapture.synchronize.stream == stream);
}

void TestSetPaddingRecordsUpdateLaunchStateWithoutCallback()
{
    ResetCapture();
    const aclsan::DeviceInstructionDecoder* decoder =
        aclsan::FindDeviceInstructionDecoder(aclsan::SocVersion::DAV_3510);
    assert(decoder != nullptr);

    aclsan::ParsedTraceRecord first{};
    first.record.instrId = 392;
    first.record.category = aclsan::DeviceInstructionCategory::RegisterState;
    first.record.args[0] = UINT64_C(0x1111222233334444);
    first.blockType = ACLSAN_DEVICE_BLOCK_TYPE_AICORE_CUBE;
    first.blockId = 3;
    first.phyCoreId = 5;
    first.instrExecId = 1;
    first.launchId = 27;
    first.deviceId = 3;
    aclsan::ParsedTraceRecord second = first;
    second.record.args[0] = UINT64_C(0xfedcba9876543210);
    second.instrExecId = 2;

    const std::string logs = CaptureDebugLogs([&] { aclsan::DispatchTraceRecords({first, second}, *decoder); });

    assert(g_deviceMemoryCallbackCount == 0);
    assert(g_deviceSyncCallbackCount == 0);
    assert(
        logs.find("[raw] deviceId=3 phyCoreId=5 blockId=3 blockType=AIC  instrExecId=1 launchId=27  "
                  "type=AclsanRawTraceRecord pc=0x0 instrId=392 siteId=0 category=3 pipeline=0 "
                  "args=[0x1111222233334444,0x0,0x0,0x0,0x0]") != std::string::npos);
    assert(
        logs.find("[raw] deviceId=3 phyCoreId=5 blockId=3 blockType=AIC  instrExecId=2 launchId=27  "
                  "type=AclsanRawTraceRecord pc=0x0 instrId=392 siteId=0 category=3 pipeline=0 "
                  "args=[0xfedcba9876543210,0x0,0x0,0x0,0x0]") != std::string::npos);
    assert(logs.find("[param] type=SetPaddingParamField value=0x1111222233334444") != std::string::npos);
    assert(logs.find("[param] type=SetPaddingParamField value=0xfedcba9876543210") != std::string::npos);
    assert(
        logs.find("[register] action=update register=set_padding launchId=27 blockType=2 blockId=3 "
                  "value=0x1111222233334444") != std::string::npos);
    assert(
        logs.find("[register] action=update register=set_padding launchId=27 blockType=2 blockId=3 "
                  "value=0xfedcba9876543210") != std::string::npos);
}

void TestUndefinedInstructionIdsSkipDecoder()
{
    g_decoderCallCount = 0;
    const aclsan::DeviceInstructionDecoder decoder{"test", CountDecoderCalls};
    aclsan::ParsedTraceRecord loadCbufToCa{};
    loadCbufToCa.record.instrId = 142;
    aclsan::ParsedTraceRecord loadCbufToCb{};
    loadCbufToCb.record.instrId = 145;

    aclsan::DispatchTraceRecords({loadCbufToCa, loadCbufToCb}, decoder);

    assert(g_decoderCallCount == 0);
}

void TestDefinedInstructionIdUsesDecoder()
{
    g_decoderCallCount = 0;
    const aclsan::DeviceInstructionDecoder decoder{"test", CountDecoderCalls};
    const std::array newlyDefinedIds{
        aclsan::InstructionId::LoopSizeUbufToGm,    aclsan::InstructionId::Loop1StrideUbufToGm,
        aclsan::InstructionId::Loop2StrideUbufToGm, aclsan::InstructionId::LoopSizeGmToUbuf,
        aclsan::InstructionId::Loop1StrideGmToUbuf, aclsan::InstructionId::Loop2StrideGmToUbuf,
        aclsan::InstructionId::NdDmaPadCount,       aclsan::InstructionId::Loop3Param,
        aclsan::InstructionId::CopyCbufToFbuf,      aclsan::InstructionId::FixL0cToCbufF32,
        aclsan::InstructionId::FixL0cToCbufS32,     aclsan::InstructionId::FixL0cToUbufF32,
        aclsan::InstructionId::FixL0cToUbufS32,     aclsan::InstructionId::CopyUbufToCbuf,
        aclsan::InstructionId::LoopSizeGmToCbuf,    aclsan::InstructionId::Loop1StrideGmToCbuf,
        aclsan::InstructionId::Loop2StrideGmToCbuf,
    };
    for (const auto id : newlyDefinedIds) {
        aclsan::ParsedTraceRecord record{};
        record.record.instrId = static_cast<uint32_t>(id);
        aclsan::DispatchTraceRecords({record}, decoder);
    }

    assert(g_decoderCallCount == newlyDefinedIds.size());
}

void TestNdDmaPadCountStatePreservesExactGmFootprint()
{
    ResetCapture();
    const aclsan::DeviceInstructionDecoder* decoder =
        aclsan::FindDeviceInstructionDecoder(aclsan::SocVersion::DAV_3510);
    assert(decoder != nullptr);

    aclsan::ParsedTraceRecord base{};
    base.blockType = ACLSAN_DEVICE_BLOCK_TYPE_AICORE_VECTOR;
    base.blockId = 4;
    base.phyCoreId = 6;
    base.launchId = 28;
    base.deviceId = 3;

    aclsan::ParsedTraceRecord padding = base;
    padding.record.instrId = 131;
    padding.record.category = aclsan::DeviceInstructionCategory::RegisterState;
    padding.record.args[0] = UINT64_C(0x0807060504030201);
    padding.instrExecId = 1;

    aclsan::ParsedTraceRecord loop0Stride = base;
    loop0Stride.record.instrId = 132;
    loop0Stride.record.category = aclsan::DeviceInstructionCategory::RegisterState;
    loop0Stride.record.args[0] = UINT64_C(1) << 20U;
    loop0Stride.instrExecId = 2;

    aclsan::ParsedTraceRecord loop1Stride = base;
    loop1Stride.record.instrId = 133;
    loop1Stride.record.category = aclsan::DeviceInstructionCategory::RegisterState;
    loop1Stride.record.args[0] = UINT64_C(8) << 20U;
    loop1Stride.instrExecId = 3;

    aclsan::ParsedTraceRecord loop2Stride = base;
    loop2Stride.record.instrId = 134;
    loop2Stride.record.category = aclsan::DeviceInstructionCategory::RegisterState;
    loop2Stride.instrExecId = 4;

    aclsan::ParsedTraceRecord loop3Stride = base;
    loop3Stride.record.instrId = 135;
    loop3Stride.record.category = aclsan::DeviceInstructionCategory::RegisterState;
    loop3Stride.instrExecId = 5;

    aclsan::ParsedTraceRecord loop4Stride = base;
    loop4Stride.record.instrId = 136;
    loop4Stride.record.category = aclsan::DeviceInstructionCategory::RegisterState;
    loop4Stride.instrExecId = 6;

    aclsan::ParsedTraceRecord memory = base;
    memory.record.instrId = 87;
    memory.record.pipeline = ACLSAN_DEVICE_PIPE_MTE2;
    memory.record.args[0] = 0x2000;
    memory.record.args[1] = 0x4000;
    memory.record.args[2] = (UINT64_C(3) << 4U) | (UINT64_C(2) << 24U) | (UINT64_C(1) << 44U);
    memory.record.args[3] =
        UINT64_C(1) | (UINT64_C(1) << 20U) | (UINT64_C(2) << 40U) | (UINT64_C(3) << 48U) | (UINT64_C(1) << 56U);
    memory.instrExecId = 7;

    const std::string logs = CaptureDebugLogs([&] {
        aclsan::DispatchTraceRecords(
            {padding, loop0Stride, loop1Stride, loop2Stride, loop3Stride, loop4Stride, memory}, *decoder);
    });

    assert(g_deviceMemoryCallbackCount == 1);
    const auto& access = g_deviceMemoryCallbacks[0];
    assert(access.address == 0x4000);
    assert(access.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_READ);
    assert(access.layoutKind == ACLSAN_MEM_LAYOUT_ND_AFFINE);
    assert(access.layout.ndAffine.elementBytes == 1);
    assert(access.layout.ndAffine.dims[0] == 3);
    assert(access.layout.ndAffine.dims[1] == 2);
    assert(access.layout.ndAffine.strides[0] == 1);
    assert(access.layout.ndAffine.strides[1] == 8);
    assert(logs.find("type=NdDmaPadCountParamField left=[1,3,5,7] right=[2,4,6,8]") != std::string::npos);
}

void TestDmaOuterLoopStateReachesMemoryCallback()
{
    ResetCapture();
    const aclsan::DeviceInstructionDecoder* decoder =
        aclsan::FindDeviceInstructionDecoder(aclsan::SocVersion::DAV_3510);
    assert(decoder != nullptr);

    aclsan::ParsedTraceRecord base{};
    base.blockType = ACLSAN_DEVICE_BLOCK_TYPE_AICORE_VECTOR;
    base.blockId = 5;
    base.phyCoreId = 7;
    base.launchId = 29;
    base.deviceId = 3;

    aclsan::ParsedTraceRecord loopSize = base;
    loopSize.record.instrId = 128;
    loopSize.record.category = aclsan::DeviceInstructionCategory::RegisterState;
    loopSize.record.args[0] = UINT64_C(2) | (UINT64_C(3) << 21U);
    loopSize.instrExecId = 1;

    aclsan::ParsedTraceRecord loop1Stride = base;
    loop1Stride.record.instrId = 129;
    loop1Stride.record.category = aclsan::DeviceInstructionCategory::RegisterState;
    loop1Stride.record.args[0] = UINT64_C(0x200) | (UINT64_C(0x20) << 40U);
    loop1Stride.instrExecId = 2;

    aclsan::ParsedTraceRecord loop2Stride = base;
    loop2Stride.record.instrId = 130;
    loop2Stride.record.category = aclsan::DeviceInstructionCategory::RegisterState;
    loop2Stride.record.args[0] = UINT64_C(0x1000) | (UINT64_C(0x40) << 40U);
    loop2Stride.instrExecId = 3;

    aclsan::ParsedTraceRecord memory = base;
    memory.record.instrId = 85;
    memory.record.pipeline = ACLSAN_DEVICE_PIPE_MTE2;
    memory.record.args[0] = 0x2000;
    memory.record.args[1] = 0x6000;
    memory.record.args[2] = (UINT64_C(2) << 4U) | (UINT64_C(32) << 25U);
    memory.record.args[3] = 64;
    memory.instrExecId = 4;

    aclsan::DispatchTraceRecords({loopSize, loop1Stride, loop2Stride, memory}, *decoder);

    assert(g_deviceMemoryCallbackCount == 1);
    const auto& access = g_deviceMemoryCallbacks[0];
    assert(access.address == 0x6000);
    assert(access.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_READ);
    assert(access.layoutKind == ACLSAN_MEM_LAYOUT_ND_AFFINE);
    assert(access.layout.ndAffine.rank == 3);
    assert(access.layout.ndAffine.elementBytes == 32);
    assert(access.layout.ndAffine.dims[0] == 2 && access.layout.ndAffine.strides[0] == 64);
    assert(access.layout.ndAffine.dims[1] == 2 && access.layout.ndAffine.strides[1] == 0x200);
    assert(access.layout.ndAffine.dims[2] == 3 && access.layout.ndAffine.strides[2] == 0x1000);
}

void TestUbufToGmOuterLoopStateReachesMemoryCallback()
{
    ResetCapture();
    const aclsan::DeviceInstructionDecoder* decoder =
        aclsan::FindDeviceInstructionDecoder(aclsan::SocVersion::DAV_3510);
    assert(decoder != nullptr);

    aclsan::ParsedTraceRecord base{};
    base.blockType = ACLSAN_DEVICE_BLOCK_TYPE_AICORE_VECTOR;
    base.blockId = 6;
    base.phyCoreId = 8;
    base.launchId = 30;
    base.deviceId = 3;

    aclsan::ParsedTraceRecord loopSize = base;
    loopSize.record.instrId = 125;
    loopSize.record.category = aclsan::DeviceInstructionCategory::RegisterState;
    loopSize.record.args[0] = UINT64_C(2) | (UINT64_C(3) << 21U);

    aclsan::ParsedTraceRecord loop1Stride = base;
    loop1Stride.record.instrId = 126;
    loop1Stride.record.category = aclsan::DeviceInstructionCategory::RegisterState;
    loop1Stride.record.args[0] = UINT64_C(0x400) | (UINT64_C(0x20) << 40U);

    aclsan::ParsedTraceRecord loop2Stride = base;
    loop2Stride.record.instrId = 127;
    loop2Stride.record.category = aclsan::DeviceInstructionCategory::RegisterState;
    loop2Stride.record.args[0] = UINT64_C(0x2000) | (UINT64_C(0x40) << 40U);

    aclsan::ParsedTraceRecord memory = base;
    memory.record.instrId = 83;
    memory.record.pipeline = ACLSAN_DEVICE_PIPE_MTE3;
    memory.record.args[0] = 0x7000;
    memory.record.args[1] = 0x2000;
    memory.record.args[2] = (UINT64_C(2) << 4U) | (UINT64_C(32) << 25U);
    memory.record.args[3] = 64;

    aclsan::DispatchTraceRecords({loopSize, loop1Stride, loop2Stride, memory}, *decoder);

    assert(g_deviceMemoryCallbackCount == 1);
    const auto& access = g_deviceMemoryCallbacks[0];
    assert(access.address == 0x7000);
    assert(access.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_WRITE);
    assert(access.layoutKind == ACLSAN_MEM_LAYOUT_ND_AFFINE);
    assert(access.layout.ndAffine.rank == 3);
    assert(access.layout.ndAffine.elementBytes == 32);
    assert(access.layout.ndAffine.dims[0] == 2 && access.layout.ndAffine.strides[0] == 64);
    assert(access.layout.ndAffine.dims[1] == 2 && access.layout.ndAffine.strides[1] == 0x400);
    assert(access.layout.ndAffine.dims[2] == 3 && access.layout.ndAffine.strides[2] == 0x2000);
}

void TestGmToL1OuterLoopStateReachesMemoryCallback()
{
    ResetCapture();
    const aclsan::DeviceInstructionDecoder* decoder =
        aclsan::FindDeviceInstructionDecoder(aclsan::SocVersion::DAV_3510);
    assert(decoder != nullptr);

    aclsan::ParsedTraceRecord base{};
    base.blockType = ACLSAN_DEVICE_BLOCK_TYPE_AICORE_CUBE;
    base.blockId = 7;
    base.phyCoreId = 9;
    base.launchId = 31;
    base.deviceId = 3;

    aclsan::ParsedTraceRecord loopSize = base;
    loopSize.record.instrId = 394;
    loopSize.record.category = aclsan::DeviceInstructionCategory::RegisterState;
    loopSize.record.args[0] = UINT64_C(2) | (UINT64_C(3) << 21U);

    aclsan::ParsedTraceRecord loop1Stride = base;
    loop1Stride.record.instrId = 395;
    loop1Stride.record.category = aclsan::DeviceInstructionCategory::RegisterState;
    loop1Stride.record.args[0] = UINT64_C(0x400) | (UINT64_C(0x20) << 40U);

    aclsan::ParsedTraceRecord loop2Stride = base;
    loop2Stride.record.instrId = 396;
    loop2Stride.record.category = aclsan::DeviceInstructionCategory::RegisterState;
    loop2Stride.record.args[0] = UINT64_C(0x2000) | (UINT64_C(0x40) << 40U);

    aclsan::ParsedTraceRecord memory = base;
    memory.record.instrId = 73;
    memory.record.pipeline = ACLSAN_DEVICE_PIPE_MTE2;
    memory.record.args[0] = 0x3000;
    memory.record.args[1] = 0x8000;
    memory.record.args[2] = (UINT64_C(2) << 4U) | (UINT64_C(2) << 25U);
    memory.record.args[3] = 3;

    aclsan::DispatchTraceRecords({loopSize, loop1Stride, loop2Stride, memory}, *decoder);

    assert(g_deviceMemoryCallbackCount == 1);
    const auto& access = g_deviceMemoryCallbacks[0];
    assert(access.address == 0x8000);
    assert(access.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_READ);
    assert(access.header.sourceKind == ACLSAN_DEVICE_SOURCE_MTE2);
    assert(access.layoutKind == ACLSAN_MEM_LAYOUT_ND_AFFINE);
    assert(access.layout.ndAffine.rank == 3);
    assert(access.layout.ndAffine.elementBytes == 64);
    assert(access.layout.ndAffine.dims[0] == 2 && access.layout.ndAffine.strides[0] == 96);
    assert(access.layout.ndAffine.dims[1] == 2 && access.layout.ndAffine.strides[1] == 0x400);
    assert(access.layout.ndAffine.dims[2] == 3 && access.layout.ndAffine.strides[2] == 0x2000);
}

void TestFixpipeLoop3StateReachesMemoryCallback()
{
    ResetCapture();
    const aclsan::DeviceInstructionDecoder* decoder =
        aclsan::FindDeviceInstructionDecoder(aclsan::SocVersion::DAV_3510);
    assert(decoder != nullptr);

    aclsan::ParsedTraceRecord base{};
    base.blockType = ACLSAN_DEVICE_BLOCK_TYPE_AICORE_CUBE;
    base.blockId = 8;
    base.phyCoreId = 10;
    base.launchId = 32;
    base.deviceId = 3;

    aclsan::ParsedTraceRecord loop3 = base;
    loop3.record.instrId = 90;
    loop3.record.category = aclsan::DeviceInstructionCategory::RegisterState;
    loop3.record.args[0] = UINT64_C(2) | (UINT64_C(7) << 16U) | (UINT64_C(100) << 32U);

    aclsan::ParsedTraceRecord memory = base;
    memory.record.instrId = 91;
    memory.record.pipeline = ACLSAN_DEVICE_PIPE_FIXPIPE;
    memory.record.args[0] = 0xa000;
    memory.record.args[1] = 0x4000;
    memory.record.args[2] = (UINT64_C(32) << 4U) | (UINT64_C(3) << 16U) | (UINT64_C(40) << 32U);
    memory.record.args[3] = UINT64_C(1) << 43U;

    aclsan::DispatchTraceRecords({loop3, memory}, *decoder);

    assert(g_deviceMemoryCallbackCount == 1);
    const auto& access = g_deviceMemoryCallbacks[0];
    assert(access.address == 0xa000);
    assert(access.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_WRITE);
    assert(access.header.sourceKind == ACLSAN_DEVICE_SOURCE_FIXPIPE);
    assert(access.layoutKind == ACLSAN_MEM_LAYOUT_ND_AFFINE);
    assert(access.layout.ndAffine.rank == 2);
    assert(access.layout.ndAffine.elementBytes == 128);
    assert(access.layout.ndAffine.dims[0] == 3 && access.layout.ndAffine.strides[0] == 160);
    assert(access.layout.ndAffine.dims[1] == 2 && access.layout.ndAffine.strides[1] == 400);
}

void TestDisabledCallbackIsNotInvoked()
{
    ResetCapture();
    g_callbackEnabled = false;
    const AclsanResourceData callbackData{};

    aclsan::AclsanCallbackDispatcher::DispatchResource(ACLSAN_CBID_RESOURCE_MEMORY_ALLOC, callbackData);
    assert(g_callbackCapture.calls == 0);
}

} // namespace

namespace aclsan {

bool InvokeCallback(AclsanCallbackDomain domain, AclsanCallbackId callbackId, const void* callbackData) noexcept
{
    if (!g_callbackEnabled) {
        return true;
    }
    assert(callbackData != nullptr);
    g_callbackCapture.domain = domain;
    g_callbackCapture.callbackId = callbackId;
    ++g_callbackCapture.calls;
    if (domain == ACLSAN_CB_DOMAIN_RESOURCE) {
        g_callbackCapture.resource = *static_cast<const AclsanResourceData*>(callbackData);
    } else if (domain == ACLSAN_CB_DOMAIN_SYNCHRONIZE) {
        g_callbackCapture.synchronize = *static_cast<const AclsanSynchronizeData*>(callbackData);
    } else if (domain == ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION && callbackId == ACLSAN_CBID_DEVICE_MEMORY_ACCESS) {
        assert(g_deviceMemoryCallbackCount < g_deviceMemoryCallbacks.size());
        g_deviceMemoryCallbacks[g_deviceMemoryCallbackCount] =
            *static_cast<const AclsanDeviceMemoryAccessData*>(callbackData);
        ++g_deviceMemoryCallbackCount;
    } else if (domain == ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION && callbackId == ACLSAN_CBID_DEVICE_SYNC) {
        assert(g_deviceSyncCallbackCount < g_deviceSyncCallbacks.size());
        g_deviceSyncCallbacks[g_deviceSyncCallbackCount] = *static_cast<const AclsanDeviceSyncData*>(callbackData);
        ++g_deviceSyncCallbackCount;
    }
    return true;
}

} // namespace aclsan

extern "C" void* acltoolGetOriginalRuntimeApi(aclrtApiId apiId)
{
    switch (apiId) {
        case ACL_RT_API_aclrtMalloc:
            if (!g_mallocOriginalAvailable) {
                return nullptr;
            }
            return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&FakeAclrtMalloc));
        case ACL_RT_API_aclrtFree:
            if (!g_freeOriginalAvailable) {
                return nullptr;
            }
            return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&FakeAclrtFree));
        case ACL_RT_API_aclrtSynchronizeStream:
            return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&FakeAclrtSynchronizeStream));
        case ACL_RT_API_aclrtSynchronizeStreamWithTimeout:
            return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&FakeAclrtSynchronizeStreamWithTimeout));
        case ACL_RT_API_aclrtGetDevice:
            return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&FakeAclrtGetDevice));
        case ACL_RT_API_aclrtBinaryGetGlobal:
            return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&FakeAclrtBinaryGetGlobal));
        case ACL_RT_API_aclrtGetFunctionAttribute:
            return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&FakeAclrtGetFunctionAttribute));
        case ACL_RT_API_aclrtGetSocName:
            return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&FakeAclrtGetSocName));
        case ACL_RT_API_aclrtGetDeviceInfo:
            return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&FakeAclrtGetDeviceInfo));
        default:
            return nullptr;
    }
}

extern "C" int32_t acltoolClearCallback(aclrtApiId) { return g_clearCallbackResult; }
extern "C" int32_t acltoolRegisterAclrtMallocCallbacks(aclrtMallocFunc) { return g_registerMallocResult; }
extern "C" int32_t acltoolRegisterAclrtFreeCallbacks(aclrtFreeFunc) { return 0; }
extern "C" int32_t acltoolRegisterAclrtSynchronizeStreamCallbacks(aclrtSynchronizeStreamFunc) { return 0; }
extern "C" int32_t acltoolRegisterAclrtSynchronizeStreamWithTimeoutCallbacks(aclrtSynchronizeStreamWithTimeoutFunc)
{
    return 0;
}
extern "C" int32_t acltoolRegisterAclrtGetFuncBySymbolCallbacks(aclrtGetFuncBySymbolFunc) { return 0; }
extern "C" int32_t acltoolRegisterAclrtBinaryUnLoadCallbacks(aclrtBinaryUnLoadFunc) { return 0; }
extern "C" int32_t acltoolRegisterAclrtResetDeviceCallbacks(aclrtResetDeviceFunc) { return 0; }
extern "C" int32_t acltoolRegisterAclrtBinaryLoadFromDataCallbacks(aclrtBinaryLoadFromDataFunc) { return 0; }
extern "C" int32_t acltoolRegisterAclrtBinaryGetFunctionCallbacks(aclrtBinaryGetFunctionFunc) { return 0; }
extern "C" int32_t acltoolRegisterAclrtBinaryGetFunctionByEntryCallbacks(aclrtBinaryGetFunctionByEntryFunc)
{
    return 0;
}
extern "C" int32_t acltoolRegisterAclrtLaunchKernelWithHostArgsCallbacks(aclrtLaunchKernelWithHostArgsFunc)
{
    return 0;
}

int main()
{
    TestMallocCallbackData();
    TestMallocPreservesOriginalRuntimeError();
    TestMallocSkipsCallbackWhenGetDeviceFails();
    TestMissingOriginalMallocAborts();
    TestFreeCallbackData();
    TestMissingOriginalFreeAborts();
    TestRuntimeHookRegistrationFailureAborts();
    TestRuntimeHookClearingFailureAborts();
    TestSynchronizeStreamCallbackData();
    TestSynchronizeStreamWithTimeoutCallbackData();
    TestSetPaddingRecordsUpdateLaunchStateWithoutCallback();
    TestUndefinedInstructionIdsSkipDecoder();
    TestDefinedInstructionIdUsesDecoder();
    TestNdDmaPadCountStatePreservesExactGmFootprint();
    TestDmaOuterLoopStateReachesMemoryCallback();
    TestUbufToGmOuterLoopStateReachesMemoryCallback();
    TestGmToL1OuterLoopStateReachesMemoryCallback();
    TestFixpipeLoop3StateReachesMemoryCallback();
    TestDisabledCallbackIsNotInvoked();
    return 0;
}
