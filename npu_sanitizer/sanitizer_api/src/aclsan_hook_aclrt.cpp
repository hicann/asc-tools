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
#include "internal/aclsan_internal.h"
#include "npu_compute/injection_hook.h"
#include "internal/aclsan_log.h"

#include <array>
#include <cstdint>
#include <set>

namespace aclsan {
namespace {

bool IsHookRequired(const std::set<aclrtApiId>& requiredHooks, aclrtApiId apiId) noexcept
{
    return requiredHooks.find(apiId) != requiredHooks.end();
}

} // namespace
} // namespace aclsan

namespace {

// 只有 Hook 配置失败且清空全部 custom Hook 也失败时，状态才会失效。
bool g_hookStateValid = true;

template <aclrtApiId ApiId>
struct RuntimeFunctionTraits;

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtMalloc> {
    using Type = aclrtMallocFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtFree> {
    using Type = aclrtFreeFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtSynchronizeStream> {
    using Type = aclrtSynchronizeStreamFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtGetFuncBySymbol> {
    using Type = aclrtGetFuncBySymbolFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtBinaryUnLoad> {
    using Type = aclrtBinaryUnLoadFunc;
};

template <>
struct RuntimeFunctionTraits<ACL_RT_API_aclrtResetDevice> {
    using Type = aclrtResetDeviceFunc;
};

template <aclrtApiId ApiId>
typename RuntimeFunctionTraits<ApiId>::Type GetOriginalRuntimeFunction() noexcept
{
    using Function = typename RuntimeFunctionTraits<ApiId>::Type;
    const auto funcPtr = reinterpret_cast<Function>(acltoolGetOriginalRuntimeApi(ApiId));
    if (funcPtr == nullptr) {
        ASC_SAN_ERROR("acltoolGetOriginalRuntimeApi apiId=%u returns nullptr", static_cast<uint32_t>(ApiId));
    }
    return funcPtr;
}

aclError aclrtMallocHook(void** deviceAddress, std::size_t size, aclrtMemMallocPolicy policy) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtMalloc>();
    if (original == nullptr) {
        return -1;
    }

    const int result = original(deviceAddress, size, policy);
    constexpr AclsanCallbackDomain domain = ACLSAN_CB_DOMAIN_RESOURCE;
    constexpr AclsanCallbackId callbackId = ACLSAN_CBID_RESOURCE_MEMORY_ALLOC;
    if (result == 0 && deviceAddress != nullptr && aclsan::IsCallbackEnabled(domain, callbackId)) {
        const AclsanResourceData callbackData{
            {ACLSAN_API_VERSION, static_cast<uint32_t>(sizeof(AclsanResourceData)), "aclrtMalloc", result, 0, 0},
            *deviceAddress,
            static_cast<uint64_t>(size),
            ACLSAN_DEVICE_MEMORY_SPACE_GM,
            0,
            static_cast<uint64_t>(reinterpret_cast<uintptr_t>(*deviceAddress))};
        if (!aclsan::InvokeCallback(domain, callbackId, &callbackData)) {
            ASC_SAN_ERROR("acl_san hook: aclrtMalloc resource callback failed");
        }
    }
    return result;
}

aclError aclrtFreeHook(void* deviceAddress) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtFree>();
    if (original == nullptr) {
        return -1;
    }

    const int result = original(deviceAddress);
    constexpr AclsanCallbackDomain domain = ACLSAN_CB_DOMAIN_RESOURCE;
    constexpr AclsanCallbackId callbackId = ACLSAN_CBID_RESOURCE_MEMORY_FREE;
    if (result == 0 && deviceAddress != nullptr && aclsan::IsCallbackEnabled(domain, callbackId)) {
        const AclsanResourceData callbackData{
            {ACLSAN_API_VERSION, static_cast<uint32_t>(sizeof(AclsanResourceData)), "aclrtFree", result, 0, 0},
            deviceAddress,
            0,
            ACLSAN_DEVICE_MEMORY_SPACE_GM,
            0,
            0};
        if (!aclsan::InvokeCallback(domain, callbackId, &callbackData)) {
            ASC_SAN_ERROR("acl_san hook: aclrtFree resource callback failed");
        }
    }
    return result;
}

// TODO: 待配合增加读取GM出来的probe result逻辑
aclError aclrtSynchronizeStreamHook(aclrtStream stream) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtSynchronizeStream>();
    if (original == nullptr) {
        aclsan::DispatchSynchronizeEnd(stream, -1);
        return -1;
    }

    const int result = original(stream);
    if (result == 0) {
        aclsan::DispatchMockDeviceRecords();
    }
    aclsan::DispatchSynchronizeEnd(stream, result);
    return result;
}

// TODO: 待配合probe增加逻辑
aclError aclrtGetFuncBySymbolHook(const void* symbol, aclrtFuncHandle* funcHandle) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtGetFuncBySymbol>();
    if (original == nullptr) {
        return -1;
    }

    ASC_SAN_DEBUG("[HOOK aclrtGetFuncBySymbol] symbol=%p", symbol);
    return original(symbol, funcHandle);
}

// TODO: 待配合probe增加逻辑
aclError aclrtBinaryUnLoadHook(aclrtBinHandle binHandle) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtBinaryUnLoad>();
    if (original == nullptr) {
        return -1;
    }

    ASC_SAN_DEBUG("[HOOK aclrtBinaryUnLoad] handle=%p", binHandle);
    return original(binHandle);
}

// TODO: 待配合probe增加逻辑
aclError aclrtResetDeviceHook(int32_t deviceId) noexcept
{
    const auto original = GetOriginalRuntimeFunction<ACL_RT_API_aclrtResetDevice>();
    if (original == nullptr) {
        return -1;
    }

    ASC_SAN_DEBUG("[HOOK aclrtResetDevice] device=%d", deviceId);
    return original(deviceId);
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

const std::array<RuntimeHookBinding, 6> kRuntimeHookBindings = {{
    MakeRuntimeHookBinding<ACL_RT_API_aclrtMalloc, acltoolRegisterAclrtMallocCallbacks, aclrtMallocHook>(),
    MakeRuntimeHookBinding<ACL_RT_API_aclrtFree, acltoolRegisterAclrtFreeCallbacks, aclrtFreeHook>(),
    MakeRuntimeHookBinding<
        ACL_RT_API_aclrtSynchronizeStream, acltoolRegisterAclrtSynchronizeStreamCallbacks,
        aclrtSynchronizeStreamHook>(),
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
