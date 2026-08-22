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
#include <cstdint>
#include <cstring>

#include "aclsan/aclsan_api.h"
#include "aclsan/aclsan_callback.h"
#include "../src/aclsan_dispatch_cb.cpp"
#include "../src/aclsan_hook_aclrt.cpp"

namespace {

struct CallbackCapture {
    AclsanCallbackDomain domain = ACLSAN_CB_DOMAIN_INVALID;
    AclsanCallbackId callbackId = ACLSAN_CBID_INVALID;
    AclsanResourceData resource{};
    AclsanSynchronizeData synchronize{};
    uint32_t calls = 0;
};

CallbackCapture g_callbackCapture{};
bool g_callbackEnabled = true;
uint32_t g_deviceMemoryCallbackCount = 0;
uint32_t g_deviceSyncCallbackCount = 0;
std::array<AclsanDeviceMemoryAccessData, 4> g_deviceMemoryCallbacks{};
std::array<AclsanDeviceSyncData, 2> g_deviceSyncCallbacks{};
void* g_lastFreedAddress = nullptr;
int32_t g_lastSynchronizeTimeout = 0;

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
}

void* Address(uintptr_t value) { return reinterpret_cast<void*>(value); }

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

void CheckCommonData(const AclsanCallbackCommonData& common, size_t expectedSize, const char* apiName)
{
    assert(common.version == ACLSAN_API_VERSION);
    assert(common.size == expectedSize);
    assert(std::strcmp(common.apiName, apiName) == 0);
    assert(common.result == ACL_SUCCESS);
    assert(common.correlationId == 0);
    assert(common.timestampNs == 0);
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
    assert(g_callbackCapture.resource.deviceId == 0);
    assert(g_callbackCapture.resource.resourceId == reinterpret_cast<uintptr_t>(deviceAddress));
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
    assert(g_callbackCapture.resource.deviceId == 0);
    assert(g_callbackCapture.resource.resourceId == reinterpret_cast<uintptr_t>(deviceAddress));
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

void TestDispatchesParsedProbeRecords()
{
    ResetCapture();
    sanitizer::ProbeParseResult result;
    sanitizer::ParsedProbeRecord copy{};
    copy.record.blockId = 1;
    copy.record.pc = 0x108;
    copy.record.instrId = 85;
    copy.record.args[0] = 0x200040;
    copy.record.args[1] = 0x100040;
    copy.record.args[2] = (1ULL << 4) | (128ULL << 25);
    copy.record.pipeline = ACLSAN_DEVICE_PIPE_MTE2;
    copy.transferBytes = 128;
    copy.serialNo = 2;
    copy.coreId = 1;
    result.records.push_back(copy);

    sanitizer::ParsedProbeRecord ubToGm{};
    ubToGm.record.blockId = 1;
    ubToGm.record.pc = 0x110;
    ubToGm.record.instrId = 83;
    ubToGm.record.args[0] = 0x300040;
    ubToGm.record.args[1] = 0x200040;
    ubToGm.record.args[2] = (1ULL << 4) | (128ULL << 25);
    ubToGm.record.pipeline = ACLSAN_DEVICE_PIPE_MTE3;
    ubToGm.transferBytes = 128;
    ubToGm.serialNo = 3;
    ubToGm.coreId = 1;
    result.records.push_back(ubToGm);

    sanitizer::ParsedProbeRecord flag{};
    flag.record.blockId = 0;
    flag.record.pc = 0x1000;
    flag.record.instrId = 440;
    flag.record.args[0] = 2;
    flag.record.args[1] = 3;
    flag.record.args[2] = 7;
    flag.record.pipeline = ACLSAN_DEVICE_PIPE_SCALAR;
    flag.serialNo = 4;
    result.records.push_back(flag);

    aclsan::DispatchProbeRecords(result);
    assert(g_deviceMemoryCallbackCount == 4);
    assert(g_deviceSyncCallbackCount == 1);
    assert(g_callbackCapture.calls == 5);

    const AclsanDeviceMemoryAccessData& source = g_deviceMemoryCallbacks[0];
    assert(source.address == 0x100040);
    assert(source.memorySpace == ACLSAN_DEVICE_MEMORY_SPACE_GM);
    assert(source.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_READ);
    assert(source.accessIndex == 0);
    assert(source.accessCount == 2);
    assert(source.header.siteId == 0);
    assert(source.header.instrExecId == 3);
    assert(source.header.serialNo == 2);
    assert(source.header.coreId == 1);
    assert(source.header.pipeline == ACLSAN_DEVICE_PIPE_MTE2);
    assert(source.layout.range.bytes == 128);

    const AclsanDeviceMemoryAccessData& destination = g_deviceMemoryCallbacks[1];
    assert(destination.address == 0x200040);
    assert(destination.memorySpace == ACLSAN_DEVICE_MEMORY_SPACE_UB);
    assert(destination.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_WRITE);
    assert(destination.accessIndex == 1);
    assert(destination.accessCount == 2);

    const AclsanDeviceMemoryAccessData& ubSource = g_deviceMemoryCallbacks[2];
    assert(ubSource.address == 0x200040);
    assert(ubSource.memorySpace == ACLSAN_DEVICE_MEMORY_SPACE_UB);
    assert(ubSource.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_READ);
    assert(ubSource.accessIndex == 0);
    assert(ubSource.header.pipeline == ACLSAN_DEVICE_PIPE_MTE3);

    const AclsanDeviceMemoryAccessData& gmDestination = g_deviceMemoryCallbacks[3];
    assert(gmDestination.address == 0x300040);
    assert(gmDestination.memorySpace == ACLSAN_DEVICE_MEMORY_SPACE_GM);
    assert(gmDestination.accessMode == ACLSAN_DEVICE_MEMORY_ACCESS_WRITE);
    assert(gmDestination.accessIndex == 1);

    assert(g_deviceSyncCallbacks[0].action == ACLSAN_DEVICE_SYNC_ACTION_SET);
    assert(g_deviceSyncCallbacks[0].instrExecId == 5);
    assert(g_deviceSyncCallbacks[0].srcPipe == 2);
    assert(g_deviceSyncCallbacks[0].dstPipe == 3);
    assert(g_deviceSyncCallbacks[0].objectId == 7);
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

bool IsCallbackEnabled(AclsanCallbackDomain, AclsanCallbackId) noexcept { return g_callbackEnabled; }

bool InvokeCallback(AclsanCallbackDomain domain, AclsanCallbackId callbackId, const void* callbackData) noexcept
{
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
            return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&FakeAclrtMalloc));
        case ACL_RT_API_aclrtFree:
            return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&FakeAclrtFree));
        case ACL_RT_API_aclrtSynchronizeStream:
            return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&FakeAclrtSynchronizeStream));
        case ACL_RT_API_aclrtSynchronizeStreamWithTimeout:
            return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(&FakeAclrtSynchronizeStreamWithTimeout));
        default:
            return nullptr;
    }
}

extern "C" int32_t acltoolClearCallback(aclrtApiId) { return 0; }
extern "C" int32_t acltoolRegisterAclrtMallocCallbacks(aclrtMallocFunc) { return 0; }
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
    TestFreeCallbackData();
    TestSynchronizeStreamCallbackData();
    TestSynchronizeStreamWithTimeoutCallbackData();
    TestDispatchesParsedProbeRecords();
    TestDisabledCallbackIsNotInvoked();
    return 0;
}
