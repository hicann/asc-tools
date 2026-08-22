/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aclsan/aclsan_api.h"
#include "internal/aclsan_dispatch_cb.h"
#include "internal/aclsan_internal.h"
#include "internal/aclsan_log.h"
#include "npu_compute/injection_hook.h"
#include "probe/image_transformer.h"
#include "probe/probe_runtime.h"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <dlfcn.h>
#include <limits>
#include <set>
#include <string>
#include <variant>

namespace aclsan {
namespace {

bool IsHookRequired(const std::set<aclrtApiId>& requiredHooks, aclrtApiId apiId) noexcept
{
    return requiredHooks.find(apiId) != requiredHooks.end();
}

} // namespace

void DispatchProbeRecords(const sanitizer::ProbeParseResult& parseResult) noexcept
{
    ASC_SAN_DEBUG("[probe] readback records=%zu", parseResult.records.size());
    for (const sanitizer::ParsedProbeRecord& parsed : parseResult.records) {
        const TraceCallbackContext context{parsed.transferBytes, parsed.serialNo + 1, parsed.serialNo, parsed.coreId};
        const auto callbackData = TranslateRawTraceToCallbackData(parsed.record, context);
        if (!callbackData.has_value()) {
            ASC_SAN_ERROR(
                "[probe] unsupported raw trace: instrId=%llu pc=0x%llx block=%u",
                static_cast<unsigned long long>(parsed.record.instrId),
                static_cast<unsigned long long>(parsed.record.pc), parsed.record.blockId);
            continue;
        }
        if (const auto* memory = std::get_if<DeviceMemoryAccessDataArray>(&*callbackData)) {
            AclsanCallbackDispatcher::DispatchDeviceMemoryAccess(*memory);
        } else if (const auto* sync = std::get_if<AclsanDeviceSyncData>(&*callbackData)) {
            AclsanCallbackDispatcher::DispatchDeviceSync(*sync);
        }
    }
}

} // namespace aclsan

namespace {

constexpr char kProbeObjectEnv[] = "ACLSAN_PROBE_OBJECT";
constexpr char kProbeCtrlBinaryEnv[] = "ACLSAN_PROBE_CTRL_BINARY";
constexpr char kProbeSymbolOrderingEnv[] = "ACLSAN_PROBE_SYMBOL_ORDERING";
constexpr char kProbeWorkRootEnv[] = "ACLSAN_PROBE_WORK_ROOT";
constexpr char kProbeArgumentBytesEnv[] = "ACLSAN_PROBE_ARGUMENT_BYTES";

bool g_hookStateValid = true;

template <aclrtApiId ApiId>
struct RuntimeFunctionTraits;

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtLaunchKernelWithHostArgs> {
    using Type = aclrtLaunchKernelWithHostArgsFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtMemcpy> {
    using Type = aclrtMemcpyFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtBinaryLoadFromData> {
    using Type = aclrtBinaryLoadFromDataFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtBinaryGetFunction> {
    using Type = aclrtBinaryGetFunctionFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtMalloc> {
    using Type = aclrtMallocFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtFree> {
    using Type = aclrtFreeFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtResetDevice> {
    using Type = aclrtResetDeviceFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtSynchronizeStream> {
    using Type = aclrtSynchronizeStreamFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtSynchronizeStreamWithTimeout> {
    using Type = aclrtSynchronizeStreamWithTimeoutFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtBinaryGetFunctionByEntry> {
    using Type = aclrtBinaryGetFunctionByEntryFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtGetFuncBySymbol> {
    using Type = aclrtGetFuncBySymbolFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtBinaryUnLoad> {
    using Type = aclrtBinaryUnLoadFunc;
};

template <aclrtApiId ApiId>
typename RuntimeFunctionTraits<ApiId>::Type GetOriginalRuntimeFunction() noexcept
{
    using Function = typename RuntimeFunctionTraits<ApiId>::Type;
    const auto function = reinterpret_cast<Function>(acltoolGetOriginalRuntimeApi(ApiId));
    if (function == nullptr) {
        ASC_SAN_ERROR("acltoolGetOriginalRuntimeApi apiId=%u returns nullptr", static_cast<uint32_t>(ApiId));
    }
    return function;
}

aclsan::probe::AclrtBinaryGetGlobalFunc ResolveBinaryGetGlobal() noexcept
{
    static const auto function =
        reinterpret_cast<aclsan::probe::AclrtBinaryGetGlobalFunc>(dlsym(RTLD_DEFAULT, "aclrtBinaryGetGlobal"));
    return function;
}

aclsan::probe::AclrtGetFunctionAttributeFunc ResolveGetFunctionAttribute() noexcept
{
    static const auto function = reinterpret_cast<aclsan::probe::AclrtGetFunctionAttributeFunc>(
        dlsym(RTLD_DEFAULT, "aclrtGetFunctionAttribute"));
    return function;
}

aclsan::probe::ProbeRuntimeApi MakeProbeRuntimeApi() noexcept
{
    return {
        GetOriginalRuntimeFunction<ACL_RT_API_aclrtBinaryLoadFromData>(),
        GetOriginalRuntimeFunction<ACL_RT_API_aclrtBinaryGetFunction>(),
        GetOriginalRuntimeFunction<ACL_RT_API_aclrtBinaryGetFunctionByEntry>(),
        ResolveBinaryGetGlobal(),
        ResolveGetFunctionAttribute(),
        GetOriginalRuntimeFunction<ACL_RT_API_aclrtMalloc>(),
        GetOriginalRuntimeFunction<ACL_RT_API_aclrtFree>(),
        GetOriginalRuntimeFunction<ACL_RT_API_aclrtMemcpy>(),
    };
}

aclsan::probe::ProbeRuntime& Runtime() noexcept
{
    static aclsan::probe::ProbeRuntime runtime;
    return runtime;
}

const char* RequiredEnvironment(const char* name) noexcept
{
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        ASC_SAN_ERROR("[probe] missing environment variable: %s", name);
        return nullptr;
    }
    return value;
}

bool MakeTransformConfig(aclsan::probe::ImageTransformConfig& config) noexcept
{
    const char* probeObject = RequiredEnvironment(kProbeObjectEnv);
    const char* ctrlBinary = RequiredEnvironment(kProbeCtrlBinaryEnv);
    const char* symbolOrdering = RequiredEnvironment(kProbeSymbolOrderingEnv);
    const char* workRoot = RequiredEnvironment(kProbeWorkRootEnv);
    const char* argumentBytesText = RequiredEnvironment(kProbeArgumentBytesEnv);
    if (probeObject == nullptr || ctrlBinary == nullptr || symbolOrdering == nullptr || workRoot == nullptr ||
        argumentBytesText == nullptr) {
        return false;
    }

    errno = 0;
    char* end = nullptr;
    const unsigned long argumentBytes = std::strtoul(argumentBytesText, &end, 10);
    if (errno != 0 || end == argumentBytesText || *end != '\0' || argumentBytes == 0 ||
        argumentBytes > std::numeric_limits<uint32_t>::max()) {
        ASC_SAN_ERROR("[probe] invalid %s=%s", kProbeArgumentBytesEnv, argumentBytesText);
        return false;
    }
    config = {probeObject, ctrlBinary, symbolOrdering, workRoot, static_cast<uint32_t>(argumentBytes)};
    return true;
}

AclsanCallbackCommonData MakeCallbackCommonData(const char* apiName, int result, uint32_t size) noexcept
{
    return {ACLSAN_API_VERSION, size, apiName, result, 0, 0};
}

AclsanResourceData MakeDeviceResourceData(const char* apiName, int result, void* deviceAddress, uint64_t bytes) noexcept
{
    return {
        MakeCallbackCommonData(apiName, result, static_cast<uint32_t>(sizeof(AclsanResourceData))),
        deviceAddress,
        bytes,
        ACLSAN_MEMORY_SPACE_DEVICE,
        0,
        static_cast<uint64_t>(reinterpret_cast<uintptr_t>(deviceAddress))};
}

AclsanSynchronizeData MakeSynchronizeData(const char* apiName, aclrtStream stream, int result) noexcept
{
    return {MakeCallbackCommonData(apiName, result, static_cast<uint32_t>(sizeof(AclsanSynchronizeData))), stream};
}

void CollectProbeRecords(aclrtStream stream) noexcept
{
    if (!Runtime().HasPending(stream)) {
        return;
    }
    sanitizer::ProbeParseResult parseResult;
    const aclError collectResult = Runtime().Collect(stream, MakeProbeRuntimeApi(), parseResult);
    if (collectResult != ACL_SUCCESS) {
        ASC_SAN_ERROR("[probe] GM readback/parse failed: result=%d", collectResult);
    } else if (!parseResult.records.empty()) {
        aclsan::DispatchProbeRecords(parseResult);
        ASC_SAN_DEBUG("[probe] records=PASS count=%zu", parseResult.records.size());
    }
}

aclError aclrtMallocHook(void** deviceAddress, std::size_t size, aclrtMemMallocPolicy policy) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtMalloc>();
    if (original == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    const aclError result = original(deviceAddress, size, policy);
    if (result == ACL_SUCCESS && deviceAddress != nullptr) {
        const AclsanResourceData callbackData =
            MakeDeviceResourceData("aclrtMalloc", result, *deviceAddress, static_cast<uint64_t>(size));
        aclsan::AclsanCallbackDispatcher::DispatchResource(ACLSAN_CBID_RESOURCE_MEMORY_ALLOC, callbackData);
    }
    return result;
}

aclError aclrtFreeHook(void* deviceAddress) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtFree>();
    if (original == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    const aclError result = original(deviceAddress);
    if (result == ACL_SUCCESS && deviceAddress != nullptr) {
        const AclsanResourceData callbackData = MakeDeviceResourceData("aclrtFree", result, deviceAddress, 0);
        aclsan::AclsanCallbackDispatcher::DispatchResource(ACLSAN_CBID_RESOURCE_MEMORY_FREE, callbackData);
    }
    return result;
}

aclError aclrtBinaryLoadFromDataHook(
    const void* data, size_t length, const aclrtBinaryLoadOptions* options, aclrtBinHandle* binHandle) noexcept
{
    aclsan::probe::ImageTransformConfig config;
    if (!MakeTransformConfig(config)) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    std::string error;
    const aclError result = Runtime().LoadBinary(
        data, length, options, binHandle, config, MakeProbeRuntimeApi(), aclsan::probe::TransformDeviceImage, error);
    if (result != ACL_SUCCESS) {
        ASC_SAN_ERROR("[probe] binary transform/load failed: result=%d error=%s", result, error.c_str());
    } else {
        ASC_SAN_DEBUG("[probe] instrumented binary load=PASS bytes=%zu", length);
    }
    return result;
}

aclError aclrtBinaryGetFunctionHook(
    const aclrtBinHandle binHandle, const char* kernelName, aclrtFuncHandle* funcHandle) noexcept
{
    return Runtime().GetFunction(binHandle, kernelName, funcHandle, MakeProbeRuntimeApi());
}

aclError aclrtBinaryGetFunctionByEntryHook(
    aclrtBinHandle binHandle, uint64_t functionEntry, aclrtFuncHandle* funcHandle) noexcept
{
    return Runtime().GetFunctionByEntry(binHandle, functionEntry, funcHandle, MakeProbeRuntimeApi());
}

aclError aclrtGetFuncBySymbolHook(const void* symbol, aclrtFuncHandle* funcHandle) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtGetFuncBySymbol>();
    if (original == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    const aclError result = original(symbol, funcHandle);
    if (result == ACL_SUCCESS && funcHandle != nullptr) {
        Runtime().RecordFunction(*funcHandle);
    }
    return result;
}

aclError aclrtLaunchKernelWithHostArgsHook(
    aclrtFuncHandle funcHandle, uint32_t numBlocks, aclrtStream stream, aclrtLaunchKernelCfg* config, void* hostArgs,
    size_t argsSize, aclrtPlaceHolderInfo* placeHolderArray, size_t placeHolderNum) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtLaunchKernelWithHostArgs>();
    if (original == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    const bool target = Runtime().IsTargetFunction(funcHandle);
    if (target) {
        const aclError prepareResult = Runtime().PrepareLaunch(funcHandle, numBlocks, stream, MakeProbeRuntimeApi());
        if (prepareResult != ACL_SUCCESS) {
            ASC_SAN_ERROR("[probe] launch preparation failed: result=%d", prepareResult);
            return prepareResult;
        }
    }
    const aclError result =
        original(funcHandle, numBlocks, stream, config, hostArgs, argsSize, placeHolderArray, placeHolderNum);
    if (target) {
        Runtime().RecordLaunchResult(funcHandle, stream, result);
        ASC_SAN_DEBUG("[probe] origin launch result=%d blocks=%u argsSize=%zu", result, numBlocks, argsSize);
    }
    return result;
}

aclError aclrtSynchronizeStreamHook(aclrtStream stream) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtSynchronizeStream>();
    if (original == nullptr) {
        const AclsanSynchronizeData callbackData =
            MakeSynchronizeData("aclrtSynchronizeStream", stream, ACL_ERROR_RT_INTERNAL_ERROR);
        aclsan::AclsanCallbackDispatcher::DispatchSynchronizeEnd(callbackData);
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }

    const aclError result = original(stream);
    if (result == ACL_SUCCESS) {
        CollectProbeRecords(stream);
    }
    const AclsanSynchronizeData callbackData = MakeSynchronizeData("aclrtSynchronizeStream", stream, result);
    aclsan::AclsanCallbackDispatcher::DispatchSynchronizeEnd(callbackData);
    return result;
}

aclError aclrtSynchronizeStreamWithTimeoutHook(aclrtStream stream, int32_t timeout) noexcept
{
    printf("HERE first original aclrtSynchronizeStreamWithTimeoutHook ====================\n");
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtSynchronizeStreamWithTimeout>();
    printf("HERE SSSSSS original aclrtSynchronizeStreamWithTimeoutHook ====================\n");
    if (original == nullptr) {
        const AclsanSynchronizeData callbackData =
            MakeSynchronizeData("aclrtSynchronizeStreamWithTimeout", stream, ACL_ERROR_RT_INTERNAL_ERROR);
        aclsan::AclsanCallbackDispatcher::DispatchSynchronizeEnd(callbackData);
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }

    printf("HERE before original aclrtSynchronizeStreamWithTimeoutHook ====================\n");
    const aclError result = original(stream, timeout);
    printf("HERE after original aclrtSynchronizeStreamWithTimeoutHook ====================\n");
    if (result == ACL_SUCCESS) {
        CollectProbeRecords(stream);
    }
    const AclsanSynchronizeData callbackData = MakeSynchronizeData("aclrtSynchronizeStreamWithTimeout", stream, result);
    aclsan::AclsanCallbackDispatcher::DispatchSynchronizeEnd(callbackData);
    return result;
    // return 0;
}

aclError aclrtBinaryUnLoadHook(aclrtBinHandle binHandle) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtBinaryUnLoad>();
    if (original == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    if (!Runtime().OwnsBinary(binHandle)) {
        return original(binHandle);
    }
    const aclError clearResult = Runtime().Clear(MakeProbeRuntimeApi());
    const aclError result = original(binHandle);
    return result != ACL_SUCCESS ? result : clearResult;
}

aclError aclrtResetDeviceHook(int32_t deviceId) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtResetDevice>();
    if (original == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    const aclError clearResult = Runtime().Clear(MakeProbeRuntimeApi());
    const aclError result = original(deviceId);
    return result != ACL_SUCCESS ? result : clearResult;
}

using ConfigureHook = int32_t (*)(bool enable) noexcept;

struct RuntimeHookBinding {
    aclrtApiId apiId;
    ConfigureHook configure;
};

template <aclrtApiId ApiId, auto Register, auto Hook>
int32_t ConfigureRuntimeHook(bool enable) noexcept
{
    return enable ? Register(Hook) : acltoolClearCallback(ApiId);
}

template <aclrtApiId ApiId, auto Register, auto Hook>
constexpr RuntimeHookBinding MakeRuntimeHookBinding() noexcept
{
    return {ApiId, ConfigureRuntimeHook<ApiId, Register, Hook>};
}

const std::array<RuntimeHookBinding, 11> kRuntimeHookBindings = {{
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtLaunchKernelWithHostArgs, acltoolRegisterAclrtLaunchKernelWithHostArgsCallbacks,
        aclrtLaunchKernelWithHostArgsHook>(),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtBinaryLoadFromData, acltoolRegisterAclrtBinaryLoadFromDataCallbacks,
        aclrtBinaryLoadFromDataHook>(),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtBinaryGetFunction, acltoolRegisterAclrtBinaryGetFunctionCallbacks,
        aclrtBinaryGetFunctionHook>(),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtBinaryGetFunctionByEntry, acltoolRegisterAclrtBinaryGetFunctionByEntryCallbacks,
        aclrtBinaryGetFunctionByEntryHook>(),
    MakeRuntimeHookBinding<ACL_RT_API_aclrtMalloc, acltoolRegisterAclrtMallocCallbacks, aclrtMallocHook>(),
    MakeRuntimeHookBinding<ACL_RT_API_aclrtFree, acltoolRegisterAclrtFreeCallbacks, aclrtFreeHook>(),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtSynchronizeStream, acltoolRegisterAclrtSynchronizeStreamCallbacks,
        aclrtSynchronizeStreamHook>(),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtSynchronizeStreamWithTimeout, acltoolRegisterAclrtSynchronizeStreamWithTimeoutCallbacks,
        aclrtSynchronizeStreamWithTimeoutHook>(),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtGetFuncBySymbol, acltoolRegisterAclrtGetFuncBySymbolCallbacks, aclrtGetFuncBySymbolHook>(),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtBinaryUnLoad, acltoolRegisterAclrtBinaryUnLoadCallbacks, aclrtBinaryUnLoadHook>(),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtResetDevice, acltoolRegisterAclrtResetDeviceCallbacks, aclrtResetDeviceHook>(),
}};

bool ApplyRuntimeHookConfiguration(const std::set<aclrtApiId>& requiredHooks) noexcept
{
    bool success = true;
    for (const RuntimeHookBinding& binding : kRuntimeHookBindings) {
        const bool enable = aclsan::IsHookRequired(requiredHooks, binding.apiId);
        if (binding.configure(enable) != 0) {
            success = false;
        }
    }
    return success;
}

} // namespace

namespace aclsan {

AclsanStatus ResolveActiveDeviceCallStack(uint64_t pc, probe::CallStackResult* result) noexcept
{
    if (result == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_PARAMETER;
    }
    try {
        *result = Runtime().ResolveCallStack(pc);
        return ACLSAN_STATUS_SUCCESS;
    } catch (...) {
        return ACLSAN_STATUS_ERROR_INTERNAL;
    }
}

AclsanStatus ApplyRuntimeHooks(const std::set<aclrtApiId>& requiredHooks) noexcept
{
    if (!g_hookStateValid) {
        ASC_SAN_ERROR("acl_san: custom hook state is poisoned; restart the process before retrying");
        return ACLSAN_STATUS_ERROR_INJECTION_FAILED;
    }

    if (!ApplyRuntimeHookConfiguration(requiredHooks)) {
        if (!ApplyRuntimeHookConfiguration({})) {
            g_hookStateValid = false;
            ASC_SAN_ERROR("acl_san: custom configuration failed and clearing all custom slots failed");
        } else {
            ASC_SAN_ERROR("acl_san: custom configuration failed; all custom slots cleared");
        }
        return ACLSAN_STATUS_ERROR_INJECTION_FAILED;
    }
    return ACLSAN_STATUS_SUCCESS;
}

bool IsRuntimeHookStatePoisoned() noexcept { return !g_hookStateValid; }

} // namespace aclsan
