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
#include <string>
#include <sys/wait.h>
#include <type_traits>
#include <unistd.h>
#include <utility>

#include "aclsan/aclsan_api.h"
#include "aclsan/aclsan_cbdata.h"
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

const char* FakeAclrtGetSocName() { return "Ascend950PR_9599"; }

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
    const aclsan::DeviceInstructionDecoder* decoder = aclsan::FindDeviceInstructionDecoder("Ascend950PR_9599");
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
        logs.find("[raw] type=AclsanRawTraceRecord blockId=3 blockType=2 phyCoreId=5 "
                  "pc=0x0 instrId=392 siteId=0 category=3 pipeline=0 "
                  "args=[0x1111222233334444,0x0,0x0,0x0,0x0] instrExecId=1") != std::string::npos);
    assert(
        logs.find("[raw] type=AclsanRawTraceRecord blockId=3 blockType=2 phyCoreId=5 "
                  "pc=0x0 instrId=392 siteId=0 category=3 pipeline=0 "
                  "args=[0xfedcba9876543210,0x0,0x0,0x0,0x0] instrExecId=2") != std::string::npos);
    assert(logs.find("[param] type=SetPaddingParamField value=0x1111222233334444") != std::string::npos);
    assert(logs.find("[param] type=SetPaddingParamField value=0xfedcba9876543210") != std::string::npos);
    assert(
        logs.find("[register] action=update register=set_padding launchId=27 blockType=2 blockId=3 "
                  "value=0x1111222233334444") != std::string::npos);
    assert(
        logs.find("[register] action=update register=set_padding launchId=27 blockType=2 blockId=3 "
                  "value=0xfedcba9876543210") != std::string::npos);
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
    TestDisabledCallbackIsNotInvoked();
    return 0;
}
