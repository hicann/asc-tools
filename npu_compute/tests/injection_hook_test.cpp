/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "npu_compute/injection_hook.h"
#include "npu_compute/runtime_stub_api.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

#define CHECK(expression)                                                                 \
    do {                                                                                  \
        if (!(expression)) {                                                              \
            std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expression); \
            return 1;                                                                     \
        }                                                                                 \
    } while (false)

int gOriginalMallocCalls = 0;
int gReplacementMallocCalls = 0;
int gReplacementFreeCalls = 0;
int gReplacementMemcpyCalls = 0;
int gReplacementMemsetCalls = 0;
int gReplacementLaunchCalls = 0;
int gReplacementGetFuncBySymbolCalls = 0;
int gReplacementBinaryUnLoadCalls = 0;
int gReplacementSynchronizeStreamWithTimeoutCalls = 0;
int gOriginalGetDeviceCalls = 0;
int gReplacementGetDeviceCalls = 0;
int gReplacementBinaryGetGlobalCalls = 0;
int gReplacementGetFunctionAttributeCalls = 0;
int gReplacementGetSocNameCalls = 0;
int gOriginalGetDeviceInfoCalls = 0;
int gReplacementGetDeviceInfoCalls = 0;
int32_t gLastSynchronizeStreamTimeout = 0;

int OriginalMalloc(void**, std::size_t, aclrtMemMallocPolicy)
{
    ++gOriginalMallocCalls;
    return 17;
}

int OriginalFree(void*) { return 0; }
int OriginalMemcpy(void*, std::size_t, const void*, std::size_t, aclrtMemcpyKind) { return 0; }
int OriginalMemset(void*, std::size_t, int, std::size_t) { return 0; }
int OriginalLaunch(void*, std::uint32_t, const void*, std::size_t, void*) { return 0; }
int OriginalGetDevice(int32_t* deviceId)
{
    ++gOriginalGetDeviceCalls;
    *deviceId = 4;
    return ACL_SUCCESS;
}

int OriginalBinaryGetGlobal(aclrtBinHandle, const char*, void** address, std::size_t* bytes)
{
    *address = reinterpret_cast<void*>(0x1234U);
    *bytes = 64;
    return ACL_SUCCESS;
}

int OriginalGetFunctionAttribute(aclrtFuncHandle, aclrtFuncAttribute, int64_t* value)
{
    *value = 32;
    return ACL_SUCCESS;
}

const char* OriginalGetSocName() { return "original-soc"; }

int OriginalGetDeviceInfo(uint32_t, aclrtDevAttr attr, int64_t* value)
{
    ++gOriginalGetDeviceInfoCalls;
    *value = attr == ACL_DEV_ATTR_CUBE_CORE_NUM ? 36 : 72;
    return ACL_SUCCESS;
}

int ReplacementMalloc(void** pointer, std::size_t size, aclrtMemMallocPolicy policy)
{
    ++gReplacementMallocCalls;
    const auto original = reinterpret_cast<aclrtMallocFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtMalloc));
    return original == nullptr ? -1 : original(pointer, size, policy);
}

int ReplacementFree(void*)
{
    ++gReplacementFreeCalls;
    return 21;
}

int ReplacementMemcpy(void*, std::size_t, const void*, std::size_t, aclrtMemcpyKind)
{
    ++gReplacementMemcpyCalls;
    return 22;
}

int ReplacementMemset(void*, std::size_t, int, std::size_t)
{
    ++gReplacementMemsetCalls;
    return 23;
}

int ReplacementLaunch(void*, uint32_t, const void*, std::size_t, void*)
{
    ++gReplacementLaunchCalls;
    return 24;
}

int ReplacementGetFuncBySymbol(const void* symbol, aclrtFuncHandle* funcHandle)
{
    ++gReplacementGetFuncBySymbolCalls;
    if (symbol == nullptr || funcHandle == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *funcHandle = const_cast<void*>(symbol);
    return 25;
}

int ReplacementBinaryUnLoad(aclrtBinHandle binHandle)
{
    ++gReplacementBinaryUnLoadCalls;
    return binHandle == nullptr ? ACL_ERROR_INVALID_PARAM : 26;
}

int ReplacementSynchronizeStreamWithTimeout(aclrtStream, int32_t timeout)
{
    ++gReplacementSynchronizeStreamWithTimeoutCalls;
    gLastSynchronizeStreamTimeout = timeout;
    return 27;
}

int ReplacementGetDevice(int32_t* deviceId)
{
    ++gReplacementGetDeviceCalls;
    *deviceId = 8;
    return ACL_SUCCESS;
}

int ReplacementBinaryGetGlobal(aclrtBinHandle, const char*, void** address, std::size_t* bytes)
{
    ++gReplacementBinaryGetGlobalCalls;
    *address = reinterpret_cast<void*>(0x5678U);
    *bytes = 128;
    return ACL_SUCCESS;
}

int ReplacementGetFunctionAttribute(aclrtFuncHandle, aclrtFuncAttribute, int64_t* value)
{
    ++gReplacementGetFunctionAttributeCalls;
    *value = 64;
    return ACL_SUCCESS;
}

const char* ReplacementGetSocName()
{
    ++gReplacementGetSocNameCalls;
    return "replacement-soc";
}

int ReplacementGetDeviceInfo(uint32_t, aclrtDevAttr attr, int64_t* value)
{
    ++gReplacementGetDeviceInfoCalls;
    *value = attr == ACL_DEV_ATTR_CUBE_CORE_NUM ? 18 : 36;
    return ACL_SUCCESS;
}

} // namespace

int main()
{
    static_assert(ACL_RT_API_aclrtGetFuncBySymbol == 14);
    static_assert(ACL_RT_API_aclrtBinaryUnLoad == 15);
    static_assert(ACL_RT_API_aclrtSynchronizeStreamWithTimeout == 16);
    static_assert(ACL_RT_API_aclrtGetDevice == 17);
    static_assert(ACL_RT_API_aclrtBinaryGetGlobal == 18);
    static_assert(ACL_RT_API_aclrtGetFunctionAttribute == 19);
    static_assert(ACL_RT_API_aclrtGetSocName == 20);
    static_assert(ACL_RT_API_aclrtGetDeviceInfo == 21);
    static_assert(ACL_RT_API_MAX == 22);

    CHECK(RuntimeStubSetOriginFunction("aclrtMalloc", &OriginalMalloc) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtFree", &OriginalFree) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtMemcpy", &OriginalMemcpy) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtMemset", &OriginalMemset) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtLaunchKernel", &OriginalLaunch) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtGetDevice", &OriginalGetDevice) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtBinaryGetGlobal", &OriginalBinaryGetGlobal) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtGetFunctionAttribute", &OriginalGetFunctionAttribute) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtGetSocName", &OriginalGetSocName) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtGetDeviceInfo", &OriginalGetDeviceInfo) == 0);

    CHECK(reinterpret_cast<aclrtMallocFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtMalloc)) == &OriginalMalloc);
    CHECK(
        reinterpret_cast<aclrtGetDeviceFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtGetDevice)) ==
        &OriginalGetDevice);
    CHECK(
        reinterpret_cast<aclrtBinaryGetGlobalFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtBinaryGetGlobal)) ==
        &OriginalBinaryGetGlobal);
    CHECK(
        reinterpret_cast<aclrtGetFunctionAttributeFunc>(
            acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtGetFunctionAttribute)) == &OriginalGetFunctionAttribute);
    CHECK(
        reinterpret_cast<aclrtGetSocNameFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtGetSocName)) ==
        &OriginalGetSocName);
    CHECK(
        reinterpret_cast<aclrtGetDeviceInfoFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtGetDeviceInfo)) ==
        &OriginalGetDeviceInfo);
    CHECK(acltoolClearCallback(ACL_RT_API_aclrtMalloc) == 0);

    CHECK(acltoolHookInit() == 0);
    CHECK(acltoolHookInit() == 0);

    void* pointer = nullptr;
    CHECK(aclrtMalloc(&pointer, 1, ACL_MEM_MALLOC_HUGE_FIRST) == 17);
    CHECK(gOriginalMallocCalls == 1);
    CHECK(gReplacementMallocCalls == 0);
    CHECK(aclrtSetDevice(0) == 0);
    CHECK(aclrtSynchronizeStream(nullptr) == 0);
    int32_t deviceId = -1;
    CHECK(aclrtGetDevice(&deviceId) == ACL_SUCCESS);
    CHECK(deviceId == 4);
    CHECK(gOriginalGetDeviceCalls == 1);

    CHECK(acltoolRegisterAclrtMallocCallbacks(nullptr) == 0);
    CHECK(acltoolRegisterAclrtFreeCallbacks(nullptr) == 0);
    CHECK(acltoolRegisterAclrtMemcpyCallbacks(nullptr) == 0);
    CHECK(acltoolRegisterAclrtMemsetCallbacks(nullptr) == 0);
    CHECK(acltoolRegisterAclrtLaunchKernelCallbacks(nullptr) == 0);
    CHECK(acltoolRegisterAclrtGetFuncBySymbolCallbacks(nullptr) == 0);
    CHECK(acltoolRegisterAclrtBinaryUnLoadCallbacks(nullptr) == 0);
    CHECK(acltoolRegisterAclrtSynchronizeStreamWithTimeoutCallbacks(nullptr) == 0);
    CHECK(acltoolRegisterAclrtGetDeviceCallbacks(nullptr) == 0);
    CHECK(acltoolRegisterAclrtBinaryGetGlobalCallbacks(nullptr) == 0);
    CHECK(acltoolRegisterAclrtGetFunctionAttributeCallbacks(nullptr) == 0);
    CHECK(acltoolRegisterAclrtGetSocNameCallbacks(nullptr) == 0);
    CHECK(acltoolRegisterAclrtGetDeviceInfoCallbacks(nullptr) == 0);

    CHECK(acltoolRegisterAclrtMallocCallbacks(&ReplacementMalloc) == 0);
    CHECK(acltoolRegisterAclrtFreeCallbacks(&ReplacementFree) == 0);
    CHECK(acltoolRegisterAclrtMemcpyCallbacks(&ReplacementMemcpy) == 0);
    CHECK(acltoolRegisterAclrtMemsetCallbacks(&ReplacementMemset) == 0);
    CHECK(acltoolRegisterAclrtLaunchKernelCallbacks(&ReplacementLaunch) == 0);
    CHECK(acltoolRegisterAclrtGetFuncBySymbolCallbacks(&ReplacementGetFuncBySymbol) == 0);
    CHECK(acltoolRegisterAclrtBinaryUnLoadCallbacks(&ReplacementBinaryUnLoad) == 0);
    CHECK(acltoolRegisterAclrtSynchronizeStreamWithTimeoutCallbacks(&ReplacementSynchronizeStreamWithTimeout) == 0);
    CHECK(acltoolRegisterAclrtGetDeviceCallbacks(&ReplacementGetDevice) == 0);
    CHECK(acltoolRegisterAclrtBinaryGetGlobalCallbacks(&ReplacementBinaryGetGlobal) == 0);
    CHECK(acltoolRegisterAclrtGetFunctionAttributeCallbacks(&ReplacementGetFunctionAttribute) == 0);
    CHECK(acltoolRegisterAclrtGetSocNameCallbacks(&ReplacementGetSocName) == 0);
    CHECK(acltoolRegisterAclrtGetDeviceInfoCallbacks(&ReplacementGetDeviceInfo) == 0);
    CHECK(aclrtMalloc(&pointer, 1, ACL_MEM_MALLOC_HUGE_FIRST) == 17);
    CHECK(aclrtFree(pointer) == 21);
    CHECK(aclrtMemcpy(nullptr, 0, nullptr, 0, ACL_MEMCPY_HOST_TO_HOST) == 22);
    CHECK(aclrtMemset(nullptr, 0, 0, 0) == 23);
    CHECK(aclrtLaunchKernel(nullptr, 0, nullptr, 0, nullptr) == 24);
    int symbol = 0;
    aclrtFuncHandle functionHandle = nullptr;
    CHECK(aclrtGetFuncBySymbol(&symbol, &functionHandle) == 25);
    CHECK(functionHandle == &symbol);
    CHECK(aclrtBinaryUnLoad(functionHandle) == 26);
    CHECK(aclrtSynchronizeStreamWithTimeout(nullptr, 1234) == 27);
    deviceId = -1;
    CHECK(aclrtGetDevice(&deviceId) == ACL_SUCCESS);
    CHECK(deviceId == 8);
    void* globalAddress = nullptr;
    std::size_t globalBytes = 0;
    CHECK(aclrtBinaryGetGlobal(nullptr, "global", &globalAddress, &globalBytes) == ACL_SUCCESS);
    CHECK(globalAddress == reinterpret_cast<void*>(0x5678U));
    CHECK(globalBytes == 128);
    int64_t attribute = 0;
    CHECK(aclrtGetFunctionAttribute(nullptr, ACL_FUNC_ATTR_KERNEL_TYPE, &attribute) == ACL_SUCCESS);
    CHECK(attribute == 64);
    CHECK(std::strcmp(aclrtGetSocName(), "replacement-soc") == 0);
    int64_t coreCount = 0;
    CHECK(aclrtGetDeviceInfo(0, ACL_DEV_ATTR_CUBE_CORE_NUM, &coreCount) == ACL_SUCCESS);
    CHECK(coreCount == 18);
    CHECK(gReplacementMallocCalls == 1);
    CHECK(gOriginalMallocCalls == 2);
    CHECK(gReplacementFreeCalls == 1);
    CHECK(gReplacementMemcpyCalls == 1);
    CHECK(gReplacementMemsetCalls == 1);
    CHECK(gReplacementLaunchCalls == 1);
    CHECK(gReplacementGetFuncBySymbolCalls == 1);
    CHECK(gReplacementBinaryUnLoadCalls == 1);
    CHECK(gReplacementSynchronizeStreamWithTimeoutCalls == 1);
    CHECK(gReplacementGetDeviceCalls == 1);
    CHECK(gReplacementBinaryGetGlobalCalls == 1);
    CHECK(gReplacementGetFunctionAttributeCalls == 1);
    CHECK(gReplacementGetSocNameCalls == 1);
    CHECK(gReplacementGetDeviceInfoCalls == 1);
    CHECK(gLastSynchronizeStreamTimeout == 1234);

    CHECK(acltoolClearCallback(ACL_RT_API_aclrtMalloc) == 0);
    CHECK(acltoolClearCallback(ACL_RT_API_aclrtMalloc) == 0);
    CHECK(aclrtMalloc(&pointer, 1, ACL_MEM_MALLOC_HUGE_FIRST) == 17);
    CHECK(gOriginalMallocCalls == 3);
    CHECK(gReplacementMallocCalls == 1);
    CHECK(acltoolClearCallback(ACL_RT_API_aclrtGetFuncBySymbol) == 0);
    CHECK(acltoolClearCallback(ACL_RT_API_aclrtBinaryUnLoad) == 0);
    CHECK(acltoolClearCallback(ACL_RT_API_aclrtSynchronizeStreamWithTimeout) == 0);
    CHECK(acltoolClearCallback(ACL_RT_API_aclrtGetDevice) == 0);
    CHECK(acltoolClearCallback(ACL_RT_API_aclrtBinaryGetGlobal) == 0);
    CHECK(acltoolClearCallback(ACL_RT_API_aclrtGetFunctionAttribute) == 0);
    CHECK(acltoolClearCallback(ACL_RT_API_aclrtGetSocName) == 0);
    CHECK(acltoolClearCallback(ACL_RT_API_aclrtGetDeviceInfo) == 0);
    deviceId = -1;
    CHECK(aclrtGetDevice(&deviceId) == ACL_SUCCESS);
    CHECK(deviceId == 4);
    CHECK(gOriginalGetDeviceCalls == 2);
    globalAddress = nullptr;
    globalBytes = 0;
    CHECK(aclrtBinaryGetGlobal(nullptr, "global", &globalAddress, &globalBytes) == ACL_SUCCESS);
    CHECK(globalAddress == reinterpret_cast<void*>(0x1234U));
    CHECK(globalBytes == 64);
    attribute = 0;
    CHECK(aclrtGetFunctionAttribute(nullptr, ACL_FUNC_ATTR_KERNEL_TYPE, &attribute) == ACL_SUCCESS);
    CHECK(attribute == 32);
    CHECK(std::strcmp(aclrtGetSocName(), "original-soc") == 0);
    coreCount = 0;
    CHECK(aclrtGetDeviceInfo(0, ACL_DEV_ATTR_CUBE_CORE_NUM, &coreCount) == ACL_SUCCESS);
    CHECK(coreCount == 36);
    CHECK(gOriginalGetDeviceInfoCalls == 1);
    CHECK(acltoolClearCallback(static_cast<aclrtApiId>(ACL_RT_API_MAX)) == ACL_ERROR_INVALID_PARAM);

    CHECK(
        reinterpret_cast<aclrtLaunchKernelFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtLaunchKernel)) ==
        &OriginalLaunch);
    CHECK(
        reinterpret_cast<aclrtSynchronizeStreamFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtSynchronizeStream)) !=
        nullptr);
    CHECK(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtLaunchKernelWithHostArgs) != nullptr);
    CHECK(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtBinaryLoadFromData) != nullptr);
    CHECK(
        reinterpret_cast<aclrtSynchronizeStreamWithTimeoutFunc>(
            acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtSynchronizeStreamWithTimeout)) != nullptr);
    return 0;
}
