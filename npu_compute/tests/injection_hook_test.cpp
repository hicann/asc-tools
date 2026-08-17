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

#include <cstdio>

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

int OriginalMalloc(void**, std::size_t, aclrtMemMallocPolicy)
{
    ++gOriginalMallocCalls;
    return 17;
}

int OriginalFree(void*) { return 0; }
int OriginalMemcpy(void*, std::size_t, const void*, std::size_t, aclrtMemcpyKind) { return 0; }
int OriginalMemset(void*, std::size_t, int, std::size_t) { return 0; }
int OriginalLaunch(void*, std::uint32_t, const void*, std::size_t, void*) { return 0; }
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

int ReplacementLaunch(void*, std::uint32_t, const void*, std::size_t, void*)
{
    ++gReplacementLaunchCalls;
    return 24;
}

} // namespace

int main()
{
    CHECK(RuntimeStubSetOriginFunction("aclrtMalloc", &OriginalMalloc) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtFree", &OriginalFree) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtMemcpy", &OriginalMemcpy) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtMemset", &OriginalMemset) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtLaunchKernel", &OriginalLaunch) == 0);

    CHECK(reinterpret_cast<aclrtMallocFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtMalloc)) == &OriginalMalloc);
    CHECK(acltoolClearCallback(ACL_RT_API_aclrtMalloc) == 0);

    CHECK(acltoolHookInit() == 0);
    CHECK(acltoolHookInit() == 0);

    void* pointer = nullptr;
    CHECK(aclrtMalloc(&pointer, 1, ACL_MEM_MALLOC_HUGE_FIRST) == 17);
    CHECK(gOriginalMallocCalls == 1);
    CHECK(gReplacementMallocCalls == 0);
    CHECK(aclrtSetDevice(0) == 0);
    CHECK(aclrtSynchronizeStream(nullptr) == 0);

    CHECK(acltoolRegisterAclrtMallocCallbacks(nullptr) == 0);
    CHECK(acltoolRegisterAclrtFreeCallbacks(nullptr) == 0);
    CHECK(acltoolRegisterAclrtMemcpyCallbacks(nullptr) == 0);
    CHECK(acltoolRegisterAclrtMemsetCallbacks(nullptr) == 0);
    CHECK(acltoolRegisterAclrtLaunchKernelCallbacks(nullptr) == 0);

    CHECK(acltoolRegisterAclrtMallocCallbacks(&ReplacementMalloc) == 0);
    CHECK(acltoolRegisterAclrtFreeCallbacks(&ReplacementFree) == 0);
    CHECK(acltoolRegisterAclrtMemcpyCallbacks(&ReplacementMemcpy) == 0);
    CHECK(acltoolRegisterAclrtMemsetCallbacks(&ReplacementMemset) == 0);
    CHECK(acltoolRegisterAclrtLaunchKernelCallbacks(&ReplacementLaunch) == 0);
    CHECK(aclrtMalloc(&pointer, 1, ACL_MEM_MALLOC_HUGE_FIRST) == 17);
    CHECK(aclrtFree(pointer) == 21);
    CHECK(aclrtMemcpy(nullptr, 0, nullptr, 0, ACL_MEMCPY_HOST_TO_HOST) == 22);
    CHECK(aclrtMemset(nullptr, 0, 0, 0) == 23);
    CHECK(aclrtLaunchKernel(nullptr, 0, nullptr, 0, nullptr) == 24);
    CHECK(gReplacementMallocCalls == 1);
    CHECK(gOriginalMallocCalls == 2);
    CHECK(gReplacementFreeCalls == 1);
    CHECK(gReplacementMemcpyCalls == 1);
    CHECK(gReplacementMemsetCalls == 1);
    CHECK(gReplacementLaunchCalls == 1);

    CHECK(acltoolClearCallback(ACL_RT_API_aclrtMalloc) == 0);
    CHECK(acltoolClearCallback(ACL_RT_API_aclrtMalloc) == 0);
    CHECK(aclrtMalloc(&pointer, 1, ACL_MEM_MALLOC_HUGE_FIRST) == 17);
    CHECK(gOriginalMallocCalls == 3);
    CHECK(gReplacementMallocCalls == 1);
    CHECK(acltoolClearCallback(static_cast<aclrtApiId>(ACL_RT_API_MAX)) == ACL_ERROR_INVALID_PARAM);

    CHECK(
        reinterpret_cast<aclrtLaunchKernelFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtLaunchKernel)) ==
        &OriginalLaunch);
    CHECK(
        reinterpret_cast<aclrtSynchronizeStreamFunc>(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtSynchronizeStream)) !=
        nullptr);
    CHECK(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtLaunchKernelWithHostArgs) != nullptr);
    CHECK(acltoolGetOriginalRuntimeApi(ACL_RT_API_aclrtBinaryLoadFromData) != nullptr);
    return 0;
}
