/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "acl/acl_rt.h"
#include "aclpti/aclpti.h"

#include "common/debug_log.h"
#include "npu_compute/prof_api_stub.h"
#include "npu_compute/runtime_stub_api.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <mutex>
#include <string_view>

namespace {

constexpr std::size_t kRuntimeApiCount = 22;

struct KernelArgs {
    uint8_t* value;
};

aclError RealAclrtLaunchKernelWithHostArgs(
    aclrtFuncHandle, uint32_t, aclrtStream, aclrtLaunchKernelCfg*, void*, std::size_t, aclrtPlaceHolderInfo*,
    std::size_t)
{
    return ACL_SUCCESS;
}

aclError RealAclrtMemcpy(void* dst, std::size_t destMax, const void* src, std::size_t count, aclrtMemcpyKind)
{
    if (dst == nullptr || src == nullptr || count > destMax) {
        return ACL_ERROR_INVALID_PARAM;
    }
    std::memmove(dst, src, count);
    return ACL_SUCCESS;
}

aclError RealAclrtBinaryLoadFromData(const void*, std::size_t, const aclrtBinaryLoadOptions*, aclrtBinHandle* binHandle)
{
    if (binHandle == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *binHandle = reinterpret_cast<aclrtBinHandle>(1);
    return ACL_SUCCESS;
}

aclError RealAclrtBinaryGetFunction(const aclrtBinHandle, const char*, aclrtFuncHandle* funcHandle)
{
    if (funcHandle == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *funcHandle = reinterpret_cast<aclrtFuncHandle>(1);
    return ACL_SUCCESS;
}

aclError RealAclrtMalloc(void** devPtr, std::size_t size, aclrtMemMallocPolicy)
{
    if (devPtr == nullptr || size == 0) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *devPtr = std::malloc(size);
    return *devPtr == nullptr ? ACL_ERROR_BAD_ALLOC : ACL_SUCCESS;
}

aclError RealAclrtMemset(void* devPtr, std::size_t maxCount, std::int32_t value, std::size_t count)
{
    if (devPtr == nullptr || count > maxCount) {
        return ACL_ERROR_INVALID_PARAM;
    }
    std::memset(devPtr, value, count);
    return ACL_SUCCESS;
}

aclError RealAclrtFree(void* devPtr)
{
    std::free(devPtr);
    return ACL_SUCCESS;
}

aclError RealAclrtCreateStream(aclrtStream* stream)
{
    if (stream == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *stream = reinterpret_cast<aclrtStream>(1);
    return ACL_SUCCESS;
}

aclError RealAclrtDestroyStream(aclrtStream) { return ACL_SUCCESS; }

aclError RealAclrtSetDevice(std::int32_t) { return ACL_SUCCESS; }

aclError RealAclrtGetDevice(std::int32_t* deviceId)
{
    if (deviceId == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *deviceId = 0;
    return ACL_SUCCESS;
}

aclError RealAclrtGetDeviceInfo(uint32_t, aclrtDevAttr attr, int64_t* value)
{
    if (value == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    if (attr == ACL_DEV_ATTR_CUBE_CORE_NUM) {
        *value = 36;
        return ACL_SUCCESS;
    }
    if (attr == ACL_DEV_ATTR_VECTOR_CORE_NUM) {
        *value = 72;
        return ACL_SUCCESS;
    }
    return ACL_ERROR_INVALID_PARAM;
}

aclError RealAclrtResetDevice(std::int32_t) { return ACL_SUCCESS; }

aclError RealAclrtSynchronizeStream(aclrtStream) { return ACL_SUCCESS; }

aclError RealAclrtSynchronizeStreamWithTimeout(aclrtStream, std::int32_t) { return ACL_SUCCESS; }

aclError RealAclrtBinaryGetFunctionByEntry(aclrtBinHandle, uint64_t, aclrtFuncHandle* funcHandle)
{
    if (funcHandle == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *funcHandle = reinterpret_cast<aclrtFuncHandle>(1);
    return ACL_SUCCESS;
}

aclError RealAclrtLaunchKernel(aclrtFuncHandle, uint32_t, const void* argsData, std::size_t argsSize, aclrtStream)
{
    if (argsData == nullptr || argsSize != sizeof(KernelArgs)) {
        return ACL_ERROR_INVALID_PARAM;
    }
    const auto* args = static_cast<const KernelArgs*>(argsData);
    if (args->value == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    ++*args->value;
    return ACL_SUCCESS;
}

aclError RealAclrtGetFuncBySymbol(const void* symbol, aclrtFuncHandle* funcHandle)
{
    if (symbol == nullptr || funcHandle == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *funcHandle = const_cast<void*>(symbol);
    return ACL_SUCCESS;
}

aclError RealAclrtBinaryUnLoad(aclrtBinHandle binHandle)
{
    return binHandle == nullptr ? ACL_ERROR_INVALID_PARAM : ACL_SUCCESS;
}

aclError RealAclrtBinaryGetGlobal(aclrtBinHandle, const char*, void** address, std::size_t* bytes)
{
    static std::uint64_t global = 0;
    if (address == nullptr || bytes == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *address = &global;
    *bytes = sizeof(global);
    return ACL_SUCCESS;
}

aclError RealAclrtGetFunctionAttribute(aclrtFuncHandle, aclrtFuncAttribute, std::int64_t* attrValue)
{
    if (attrValue == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    *attrValue = 0;
    return ACL_SUCCESS;
}

const char* RealAclrtGetSocName() { return "Ascend950PR_9599"; }

template <typename Function>
aclrtApiFunc ToGenericFunction(Function function)
{
    return reinterpret_cast<aclrtApiFunc>(function);
}

struct RuntimeEntry {
    const char* name;
    aclrtApiFunc origin;
    aclrtApiFunc current;
};

std::array<RuntimeEntry, kRuntimeApiCount> g_runtimeEntries = {{
    {"aclrtLaunchKernelWithHostArgs", ToGenericFunction(&RealAclrtLaunchKernelWithHostArgs),
     ToGenericFunction(&RealAclrtLaunchKernelWithHostArgs)},
    {"aclrtMemcpy", ToGenericFunction(&RealAclrtMemcpy), ToGenericFunction(&RealAclrtMemcpy)},
    {"aclrtBinaryLoadFromData", ToGenericFunction(&RealAclrtBinaryLoadFromData),
     ToGenericFunction(&RealAclrtBinaryLoadFromData)},
    {"aclrtBinaryGetFunction", ToGenericFunction(&RealAclrtBinaryGetFunction),
     ToGenericFunction(&RealAclrtBinaryGetFunction)},
    {"aclrtMalloc", ToGenericFunction(&RealAclrtMalloc), ToGenericFunction(&RealAclrtMalloc)},
    {"aclrtMemset", ToGenericFunction(&RealAclrtMemset), ToGenericFunction(&RealAclrtMemset)},
    {"aclrtFree", ToGenericFunction(&RealAclrtFree), ToGenericFunction(&RealAclrtFree)},
    {"aclrtCreateStream", ToGenericFunction(&RealAclrtCreateStream), ToGenericFunction(&RealAclrtCreateStream)},
    {"aclrtDestroyStream", ToGenericFunction(&RealAclrtDestroyStream), ToGenericFunction(&RealAclrtDestroyStream)},
    {"aclrtSetDevice", ToGenericFunction(&RealAclrtSetDevice), ToGenericFunction(&RealAclrtSetDevice)},
    {"aclrtResetDevice", ToGenericFunction(&RealAclrtResetDevice), ToGenericFunction(&RealAclrtResetDevice)},
    {"aclrtSynchronizeStream", ToGenericFunction(&RealAclrtSynchronizeStream),
     ToGenericFunction(&RealAclrtSynchronizeStream)},
    {"aclrtBinaryGetFunctionByEntry", ToGenericFunction(&RealAclrtBinaryGetFunctionByEntry),
     ToGenericFunction(&RealAclrtBinaryGetFunctionByEntry)},
    {"aclrtLaunchKernel", ToGenericFunction(&RealAclrtLaunchKernel), ToGenericFunction(&RealAclrtLaunchKernel)},
    {"aclrtGetFuncBySymbol", ToGenericFunction(&RealAclrtGetFuncBySymbol),
     ToGenericFunction(&RealAclrtGetFuncBySymbol)},
    {"aclrtBinaryUnLoad", ToGenericFunction(&RealAclrtBinaryUnLoad), ToGenericFunction(&RealAclrtBinaryUnLoad)},
    {"aclrtSynchronizeStreamWithTimeout", ToGenericFunction(&RealAclrtSynchronizeStreamWithTimeout),
     ToGenericFunction(&RealAclrtSynchronizeStreamWithTimeout)},
    {"aclrtGetDevice", ToGenericFunction(&RealAclrtGetDevice), ToGenericFunction(&RealAclrtGetDevice)},
    {"aclrtBinaryGetGlobal", ToGenericFunction(&RealAclrtBinaryGetGlobal),
     ToGenericFunction(&RealAclrtBinaryGetGlobal)},
    {"aclrtGetFunctionAttribute", ToGenericFunction(&RealAclrtGetFunctionAttribute),
     ToGenericFunction(&RealAclrtGetFunctionAttribute)},
    {"aclrtGetSocName", ToGenericFunction(&RealAclrtGetSocName), ToGenericFunction(&RealAclrtGetSocName)},
    {"aclrtGetDeviceInfo", ToGenericFunction(&RealAclrtGetDeviceInfo), ToGenericFunction(&RealAclrtGetDeviceInfo)},
}};

std::mutex g_runtimeMutex;

using EmitCallbackEvent = int (*)(uint32_t, uint32_t, std::int32_t);

void EmitTestCallbackEvent(aclptiCallbackId cbid, aclptiCallbackSite site, aclError retval)
{
    const char* enabled = std::getenv("NPU_COMPUTE_TEST_CALLBACK_EVENTS");
    if (enabled == nullptr || std::string_view(enabled) != "1") {
        return;
    }
    auto emit = reinterpret_cast<EmitCallbackEvent>(::dlsym(RTLD_DEFAULT, "AclPtiCallbackStubEmitRuntimeEvent"));
    if (emit != nullptr) {
        emit(cbid, static_cast<uint32_t>(site), static_cast<std::int32_t>(retval));
    }
}

RuntimeEntry* FindEntry(const char* name)
{
    if (name == nullptr) {
        return nullptr;
    }
    for (auto& entry : g_runtimeEntries) {
        if (std::string_view(entry.name) == name) {
            return &entry;
        }
    }
    return nullptr;
}

template <typename Function, typename... Args>
aclError CallCurrent(const char* name, Args... args)
{
    Function current = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_runtimeMutex);
        RuntimeEntry* entry = FindEntry(name);
        if (entry != nullptr) {
            current = reinterpret_cast<Function>(entry->current);
        }
    }
    return current == nullptr ? ACL_ERROR_UNINITIALIZE : current(args...);
}

} // namespace

extern "C" NPU_COMPUTE_EXPORT aclError RuntimeStubSetOrigin(const char* name, aclrtApiFunc function)
{
    if (function == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    std::lock_guard<std::mutex> lock(g_runtimeMutex);
    RuntimeEntry* entry = FindEntry(name);
    if (entry == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    entry->origin = function;
    entry->current = function;
    return ACL_SUCCESS;
}

extern "C" aclError aclrtApiInjectionGetFunc(const char* name, aclrtApiFunc* originFunc, aclrtApiFunc* currentFunc)
{
    if (originFunc == nullptr && currentFunc == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    std::lock_guard<std::mutex> lock(g_runtimeMutex);
    RuntimeEntry* entry = FindEntry(name);
    if (entry == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    if (originFunc != nullptr) {
        *originFunc = entry->origin;
    }
    if (currentFunc != nullptr) {
        *currentFunc = entry->current;
    }
    return ACL_SUCCESS;
}

extern "C" aclError aclrtApiInjectionSetFunc(const char* name, aclrtApiFunc func)
{
    if (func == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    std::lock_guard<std::mutex> lock(g_runtimeMutex);
    RuntimeEntry* entry = FindEntry(name);
    if (entry == nullptr) {
        return ACL_ERROR_INVALID_PARAM;
    }
    entry->current = func;
    return ACL_SUCCESS;
}

extern "C" int aclrtInit() { return ProfApiLoadApiInjectionFromEnv(); }

extern "C" aclError aclrtSetDevice(std::int32_t deviceId)
{
    return CallCurrent<aclError (*)(std::int32_t)>("aclrtSetDevice", deviceId);
}

extern "C" aclError aclrtGetDevice(std::int32_t* deviceId)
{
    return CallCurrent<aclError (*)(std::int32_t*)>("aclrtGetDevice", deviceId);
}

extern "C" aclError aclrtGetDeviceInfo(uint32_t deviceId, aclrtDevAttr attr, int64_t* value)
{
    return CallCurrent<aclError (*)(uint32_t, aclrtDevAttr, int64_t*)>("aclrtGetDeviceInfo", deviceId, attr, value);
}

extern "C" aclError aclrtResetDevice(std::int32_t deviceId)
{
    return CallCurrent<aclError (*)(std::int32_t)>("aclrtResetDevice", deviceId);
}

extern "C" aclError aclrtCreateStream(aclrtStream* stream)
{
    return CallCurrent<aclError (*)(aclrtStream*)>("aclrtCreateStream", stream);
}

extern "C" aclError aclrtDestroyStream(aclrtStream stream)
{
    return CallCurrent<aclError (*)(aclrtStream)>("aclrtDestroyStream", stream);
}

extern "C" aclError aclrtMalloc(void** devPtr, std::size_t size, aclrtMemMallocPolicy policy)
{
    EmitTestCallbackEvent(ACLPTI_RUNTIME_CBID_aclrtMalloc, ACLPTI_API_ENTER, ACL_SUCCESS);
    const aclError result =
        CallCurrent<aclError (*)(void**, std::size_t, aclrtMemMallocPolicy)>("aclrtMalloc", devPtr, size, policy);
    EmitTestCallbackEvent(ACLPTI_RUNTIME_CBID_aclrtMalloc, ACLPTI_API_EXIT, result);
    return result;
}

extern "C" aclError aclrtFree(void* devPtr) { return CallCurrent<aclError (*)(void*)>("aclrtFree", devPtr); }

extern "C" aclError aclrtMemcpy(
    void* dst, std::size_t destMax, const void* src, std::size_t count, aclrtMemcpyKind kind)
{
    return CallCurrent<aclError (*)(void*, std::size_t, const void*, std::size_t, aclrtMemcpyKind)>(
        "aclrtMemcpy", dst, destMax, src, count, kind);
}

extern "C" aclError aclrtMemset(void* devPtr, std::size_t maxCount, std::int32_t value, std::size_t count)
{
    return CallCurrent<aclError (*)(void*, std::size_t, std::int32_t, std::size_t)>(
        "aclrtMemset", devPtr, maxCount, value, count);
}

extern "C" aclError aclrtSynchronizeStream(aclrtStream stream)
{
    return CallCurrent<aclError (*)(aclrtStream)>("aclrtSynchronizeStream", stream);
}

extern "C" aclError aclrtSynchronizeStreamWithTimeout(aclrtStream stream, std::int32_t timeout)
{
    return CallCurrent<aclError (*)(aclrtStream, std::int32_t)>("aclrtSynchronizeStreamWithTimeout", stream, timeout);
}

extern "C" aclError aclrtBinaryLoadFromData(
    const void* data, std::size_t length, const aclrtBinaryLoadOptions* options, aclrtBinHandle* binHandle)
{
    return CallCurrent<aclError (*)(const void*, std::size_t, const aclrtBinaryLoadOptions*, aclrtBinHandle*)>(
        "aclrtBinaryLoadFromData", data, length, options, binHandle);
}

extern "C" aclError aclrtBinaryGetFunction(
    aclrtBinHandle binHandle, const char* kernelName, aclrtFuncHandle* funcHandle)
{
    return CallCurrent<aclError (*)(aclrtBinHandle, const char*, aclrtFuncHandle*)>(
        "aclrtBinaryGetFunction", binHandle, kernelName, funcHandle);
}

extern "C" aclError aclrtGetFuncBySymbol(const void* symbol, aclrtFuncHandle* funcHandle)
{
    return CallCurrent<aclError (*)(const void*, aclrtFuncHandle*)>("aclrtGetFuncBySymbol", symbol, funcHandle);
}

extern "C" aclError aclrtBinaryUnLoad(aclrtBinHandle binHandle)
{
    return CallCurrent<aclError (*)(aclrtBinHandle)>("aclrtBinaryUnLoad", binHandle);
}

extern "C" aclError aclrtBinaryGetGlobal(aclrtBinHandle binHandle, const char* name, void** address, std::size_t* bytes)
{
    return CallCurrent<aclError (*)(aclrtBinHandle, const char*, void**, std::size_t*)>(
        "aclrtBinaryGetGlobal", binHandle, name, address, bytes);
}

extern "C" aclError aclrtGetFunctionAttribute(
    aclrtFuncHandle funcHandle, aclrtFuncAttribute attrType, std::int64_t* attrValue)
{
    return CallCurrent<aclError (*)(aclrtFuncHandle, aclrtFuncAttribute, std::int64_t*)>(
        "aclrtGetFunctionAttribute", funcHandle, attrType, attrValue);
}

extern "C" const char* aclrtGetSocName()
{
    using Function = const char* (*)();
    Function current = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_runtimeMutex);
        RuntimeEntry* entry = FindEntry("aclrtGetSocName");
        if (entry != nullptr) {
            current = reinterpret_cast<Function>(entry->current);
        }
    }
    return current == nullptr ? nullptr : current();
}

extern "C" aclError aclrtLaunchKernelWithHostArgs(
    aclrtFuncHandle funcHandle, uint32_t numBlocks, aclrtStream stream, aclrtLaunchKernelCfg* cfg, void* hostArgs,
    std::size_t argsSize, aclrtPlaceHolderInfo* placeHolderArray, std::size_t placeHolderNum)
{
    return CallCurrent<aclError (*)(
        aclrtFuncHandle, uint32_t, aclrtStream, aclrtLaunchKernelCfg*, void*, std::size_t, aclrtPlaceHolderInfo*,
        std::size_t)>(
        "aclrtLaunchKernelWithHostArgs", funcHandle, numBlocks, stream, cfg, hostArgs, argsSize, placeHolderArray,
        placeHolderNum);
}

extern "C" aclError aclrtLaunchKernel(
    aclrtFuncHandle funcHandle, uint32_t numBlocks, const void* argsData, std::size_t argsSize, aclrtStream stream)
{
    EmitTestCallbackEvent(ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, ACLPTI_API_ENTER, ACL_SUCCESS);
    const aclError result = CallCurrent<aclError (*)(aclrtFuncHandle, uint32_t, const void*, std::size_t, aclrtStream)>(
        "aclrtLaunchKernel", funcHandle, numBlocks, argsData, argsSize, stream);
    EmitTestCallbackEvent(ACLPTI_RUNTIME_CBID_aclrtLaunchKernel, ACLPTI_API_EXIT, result);
    return result;
}
