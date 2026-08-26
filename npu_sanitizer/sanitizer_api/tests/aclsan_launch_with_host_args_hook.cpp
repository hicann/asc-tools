/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aclsan/aclsan_api.h"
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

struct LaunchArguments {
    aclrtFuncHandle function = nullptr;
    uint32_t blocks = 0;
    aclrtStream stream = nullptr;
    aclrtLaunchKernelCfg* config = nullptr;
    void* hostArgs = nullptr;
    size_t argsSize = 0;
    aclrtPlaceHolderInfo* placeholders = nullptr;
    size_t placeholderCount = 0;
};

LaunchArguments g_forwarded{};
int g_originalCalls = 0;
int g_sentinelCalls = 0;

aclError OriginalLaunch(
    aclrtFuncHandle function, uint32_t blocks, aclrtStream stream, aclrtLaunchKernelCfg* config, void* hostArgs,
    size_t argsSize, aclrtPlaceHolderInfo* placeholders, size_t placeholderCount)
{
    ++g_originalCalls;
    g_forwarded = {function, blocks, stream, config, hostArgs, argsSize, placeholders, placeholderCount};
    return 71;
}

aclError SentinelLaunch(
    aclrtFuncHandle, uint32_t, aclrtStream, aclrtLaunchKernelCfg*, void*, size_t, aclrtPlaceHolderInfo*, size_t)
{
    ++g_sentinelCalls;
    return 72;
}

void Callback(void*, AclsanCallbackDomain, AclsanCallbackId, const void*) {}

} // namespace

int main()
{
    CHECK(RuntimeStubSetOriginFunction("aclrtLaunchKernelWithHostArgs", &OriginalLaunch) == ACL_SUCCESS);

    AclsanSubscriberHandle subscriber = nullptr;
    CHECK(aclsanSubscribe(&subscriber, &Callback, nullptr) == ACLSAN_STATUS_SUCCESS);
    CHECK(acltoolRegisterAclrtLaunchKernelWithHostArgsCallbacks(&SentinelLaunch) == ACL_SUCCESS);
    CHECK(
        aclsanEnableCallback(1, subscriber, ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_MEMORY_ACCESS) ==
        ACLSAN_STATUS_SUCCESS);

    int functionStorage = 0;
    int streamStorage = 0;
    int configStorage = 0;
    int hostArgsStorage = 0;
    int placeholderStorage = 0;
    const auto function = reinterpret_cast<aclrtFuncHandle>(&functionStorage);
    const auto stream = reinterpret_cast<aclrtStream>(&streamStorage);
    auto* config = reinterpret_cast<aclrtLaunchKernelCfg*>(&configStorage);
    auto* placeholders = reinterpret_cast<aclrtPlaceHolderInfo*>(&placeholderStorage);

    CHECK(aclrtLaunchKernelWithHostArgs(function, 13, stream, config, &hostArgsStorage, 64, placeholders, 3) == 71);
    CHECK(g_sentinelCalls == 0);
    CHECK(g_originalCalls == 1);
    CHECK(g_forwarded.function == function);
    CHECK(g_forwarded.blocks == 13);
    CHECK(g_forwarded.stream == stream);
    CHECK(g_forwarded.config == config);
    CHECK(g_forwarded.hostArgs == &hostArgsStorage);
    CHECK(g_forwarded.argsSize == 64);
    CHECK(g_forwarded.placeholders == placeholders);
    CHECK(g_forwarded.placeholderCount == 3);

    CHECK(aclsanUnsubscribe(subscriber) == ACLSAN_STATUS_SUCCESS);
    CHECK(aclrtLaunchKernelWithHostArgs(function, 1, stream, config, &hostArgsStorage, 8, placeholders, 1) == 71);
    CHECK(g_originalCalls == 2);
    return 0;
}
