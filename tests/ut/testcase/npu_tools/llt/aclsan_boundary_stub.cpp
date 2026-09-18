/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aclsan_boundary_stub.h"

#include "aclsan_device_call_stack.h"
#include "aclsan_dispatch.h"
#include "aclsan_runtime_hook.h"
#include "injection/injection_hook.h"

#include <mutex>

namespace aclsan_test {
namespace {

std::mutex g_boundaryMutex;
Boundary g_boundary;

Boundary TakeAndInstall(const Boundary& boundary)
{
    std::lock_guard<std::mutex> lock(g_boundaryMutex);
    Boundary previous = g_boundary;
    g_boundary = boundary;
    return previous;
}

const Boundary& Current() { return g_boundary; }

// 注入库注册面（源码编入本二进制）：默认直连 __real__ 真实实现，测试经
// registerRuntimeCallback / clearRuntimeCallback 整体接管（如 aclsan_hook_cbdata
// 用例的受控失败注入）。
#define ACLSAN_BOUNDARY_REGISTER_DEF(name, fnType, apiId)                     \
    extern "C" int32_t __real_##name(fnType);                                 \
    extern "C" int32_t __wrap_##name(fnType callback)                         \
    {                                                                         \
        if (const auto fn = aclsan_test::Current().registerRuntimeCallback) { \
            return fn(apiId, reinterpret_cast<void*>(callback));              \
        }                                                                     \
        return __real_##name(callback);                                       \
    }

} // namespace

BoundaryGuard::BoundaryGuard(const Boundary& boundary) : previous_(TakeAndInstall(boundary)) {}

BoundaryGuard::~BoundaryGuard() { TakeAndInstall(previous_); }

void ResetBoundary() { TakeAndInstall(Boundary{}); }

} // namespace aclsan_test

// ---------------------------------------------------------------------------
// 注入库边界（injection_hook.cpp / runtime_stub.cpp / prof_api_stub.cpp 源码编入
// 本二进制）：全部经 -Wl,--wrap 重定向到这里。
//   - acltoolHookInit：默认最小桩（返回 0，不装钩子），保持各用例环境隔离；
//     需要真实安装的用例（injection_hook_test）经 BoundaryGuard 接管为 __real__。
//   - acltoolGetOriginalRuntimeApi：默认转发真实实现（读 runtime 桩的 origin 表，
//     未设置时自然返回 nullptr，与既有语义一致）。
// ---------------------------------------------------------------------------
extern "C" int32_t __real_acltoolHookInit(void);

extern "C" int32_t __wrap_acltoolHookInit(void)
{
    const auto fn = aclsan_test::Current().hookInit;
    return fn != nullptr ? fn() : 0;
}

extern "C" void* __real_acltoolGetOriginalRuntimeApi(aclrtApiId);

extern "C" void* __wrap_acltoolGetOriginalRuntimeApi(aclrtApiId apiId)
{
    const auto fn = aclsan_test::Current().getOriginalRuntimeApi;
    return fn != nullptr ? fn(apiId) : __real_acltoolGetOriginalRuntimeApi(apiId);
}

// ---------------------------------------------------------------------------
// aclsan C API 与 aclsan::InvokeCallback：真实实现位于 aclsan_api.cpp，经
// -Wl,--wrap 重定向到这里；默认转发 __real__ 真实实现，测试可替换。
// ---------------------------------------------------------------------------
extern "C" {
AclsanStatus __real_aclsanSubscribe(AclsanSubscriberHandle*, AclsanCallbackFunc, void*);
AclsanStatus __real_aclsanUnsubscribe(AclsanSubscriberHandle);
AclsanStatus __real_aclsanEnableCallback(uint32_t, AclsanSubscriberHandle, AclsanCallbackDomain, AclsanCallbackId);
AclsanStatus __real_aclsanGetDeviceCallStack(uint64_t, AclsanDeviceCallStack*);

AclsanStatus __wrap_aclsanSubscribe(AclsanSubscriberHandle* handle, AclsanCallbackFunc callback, void* userdata)
{
    const auto fn = aclsan_test::Current().subscribe;
    return fn != nullptr ? fn(handle, callback, userdata) : __real_aclsanSubscribe(handle, callback, userdata);
}

AclsanStatus __wrap_aclsanUnsubscribe(AclsanSubscriberHandle handle)
{
    const auto fn = aclsan_test::Current().unsubscribe;
    return fn != nullptr ? fn(handle) : __real_aclsanUnsubscribe(handle);
}

AclsanStatus __wrap_aclsanEnableCallback(
    uint32_t enable, AclsanSubscriberHandle handle, AclsanCallbackDomain domain, AclsanCallbackId id)
{
    const auto fn = aclsan_test::Current().enableCallback;
    return fn != nullptr ? fn(enable, handle, domain, id) : __real_aclsanEnableCallback(enable, handle, domain, id);
}

AclsanStatus __wrap_aclsanGetDeviceCallStack(uint64_t pc, AclsanDeviceCallStack* result)
{
    const auto fn = aclsan_test::Current().getDeviceCallStack;
    return fn != nullptr ? fn(pc, result) : __real_aclsanGetDeviceCallStack(pc, result);
}

// aclsan::ApplyRuntimeHooks / ResolveActiveDeviceCallStack 的 mangled 名同样仅含
// [A-Za-z0-9_]，直接作为 extern "C" 符号名参与 --wrap，默认转发真实实现。
void __real__ZN6aclsan17ApplyRuntimeHooksERKSt3setI10aclrtApiIdSt4lessIS1_ESaIS1_EE(
    const std::set<aclrtApiId>&) noexcept;
AclsanStatus __real__ZN6aclsan28ResolveActiveDeviceCallStackEmPNS_14device_runtime15CallStackResultE(
    uint64_t, aclsan::device_runtime::CallStackResult*) noexcept;

void __wrap__ZN6aclsan17ApplyRuntimeHooksERKSt3setI10aclrtApiIdSt4lessIS1_ESaIS1_EE(
    const std::set<aclrtApiId>& requiredHooks) noexcept
{
    const auto fn = aclsan_test::Current().applyRuntimeHooks;
    if (fn != nullptr) {
        fn(requiredHooks);
        return;
    }
    __real__ZN6aclsan17ApplyRuntimeHooksERKSt3setI10aclrtApiIdSt4lessIS1_ESaIS1_EE(requiredHooks);
}

AclsanStatus __wrap__ZN6aclsan28ResolveActiveDeviceCallStackEmPNS_14device_runtime15CallStackResultE(
    uint64_t pc, aclsan::device_runtime::CallStackResult* result) noexcept
{
    const auto fn = aclsan_test::Current().resolveActiveDeviceCallStack;
    return fn != nullptr ?
               fn(pc, result) :
               __real__ZN6aclsan28ResolveActiveDeviceCallStackEmPNS_14device_runtime15CallStackResultE(pc, result);
}

// aclsan::InvokeCallback 的 mangled 名（_ZN6aclsan14InvokeCallbackE20AclsanCallbackDomainjPKv）
// 仅含 [A-Za-z0-9_]，可直接作为 extern "C" 符号名参与 --wrap。
bool __real__ZN6aclsan14InvokeCallbackE20AclsanCallbackDomainjPKv(AclsanCallbackDomain, AclsanCallbackId, const void*);

bool __wrap__ZN6aclsan14InvokeCallbackE20AclsanCallbackDomainjPKv(
    AclsanCallbackDomain domain, AclsanCallbackId id, const void* callbackData)
{
    const auto fn = aclsan_test::Current().invokeCallback;
    return fn != nullptr ? fn(domain, id, callbackData) :
                           __real__ZN6aclsan14InvokeCallbackE20AclsanCallbackDomainjPKv(domain, id, callbackData);
}
} // extern "C"

// ---------------------------------------------------------------------------
// 注入库注册面（原 aclsan_hook_cbdata_test 内联桩收敛至此，__real__ 为源码编入的
// injection_hook.cpp 真实实现）。
// ---------------------------------------------------------------------------
extern "C" int32_t __real_acltoolClearCallback(aclrtApiId);

extern "C" int32_t __wrap_acltoolClearCallback(aclrtApiId apiId)
{
    if (const auto fn = aclsan_test::Current().clearRuntimeCallback) {
        return fn(apiId);
    }
    return __real_acltoolClearCallback(apiId);
}

ACLSAN_BOUNDARY_REGISTER_DEF(acltoolRegisterAclrtMallocCallbacks, aclrtMallocFunc, ACL_RT_API_aclrtMalloc)
ACLSAN_BOUNDARY_REGISTER_DEF(acltoolRegisterAclrtFreeCallbacks, aclrtFreeFunc, ACL_RT_API_aclrtFree)
ACLSAN_BOUNDARY_REGISTER_DEF(
    acltoolRegisterAclrtSynchronizeStreamCallbacks, aclrtSynchronizeStreamFunc, ACL_RT_API_aclrtSynchronizeStream)
ACLSAN_BOUNDARY_REGISTER_DEF(
    acltoolRegisterAclrtSynchronizeStreamWithTimeoutCallbacks, aclrtSynchronizeStreamWithTimeoutFunc,
    ACL_RT_API_aclrtSynchronizeStreamWithTimeout)
ACLSAN_BOUNDARY_REGISTER_DEF(
    acltoolRegisterAclrtGetFuncBySymbolCallbacks, aclrtGetFuncBySymbolFunc, ACL_RT_API_aclrtGetFuncBySymbol)
ACLSAN_BOUNDARY_REGISTER_DEF(
    acltoolRegisterAclrtBinaryUnLoadCallbacks, aclrtBinaryUnLoadFunc, ACL_RT_API_aclrtBinaryUnLoad)
ACLSAN_BOUNDARY_REGISTER_DEF(
    acltoolRegisterAclrtResetDeviceCallbacks, aclrtResetDeviceFunc, ACL_RT_API_aclrtResetDevice)
ACLSAN_BOUNDARY_REGISTER_DEF(
    acltoolRegisterAclrtBinaryLoadFromDataCallbacks, aclrtBinaryLoadFromDataFunc, ACL_RT_API_aclrtBinaryLoadFromData)
ACLSAN_BOUNDARY_REGISTER_DEF(
    acltoolRegisterAclrtBinaryGetFunctionCallbacks, aclrtBinaryGetFunctionFunc, ACL_RT_API_aclrtBinaryGetFunction)
ACLSAN_BOUNDARY_REGISTER_DEF(
    acltoolRegisterAclrtBinaryGetFunctionByEntryCallbacks, aclrtBinaryGetFunctionByEntryFunc,
    ACL_RT_API_aclrtBinaryGetFunctionByEntry)
ACLSAN_BOUNDARY_REGISTER_DEF(
    acltoolRegisterAclrtLaunchKernelWithHostArgsCallbacks, aclrtLaunchKernelWithHostArgsFunc,
    ACL_RT_API_aclrtLaunchKernelWithHostArgs)
ACLSAN_BOUNDARY_REGISTER_DEF(
    acltoolRegisterAclrtLaunchKernelWithArgsArrayCallbacks, aclrtLaunchKernelWithArgsArrayFunc,
    ACL_RT_API_aclrtLaunchKernelWithArgsArray)
