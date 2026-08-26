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
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

#define CHECK(expression)                                                                 \
    do {                                                                                  \
        if (!(expression)) {                                                              \
            std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expression); \
            return 1;                                                                     \
        }                                                                                 \
    } while (false)

int g_originalDataCalls = 0;
int g_sentinelDataCalls = 0;
bool g_recurseDataLoad = false;
const void* g_forwardedData = nullptr;
size_t g_forwardedLength = 0;
const aclrtBinaryLoadOptions* g_forwardedDataOptions = nullptr;

class ScopedEnvironmentVariable {
public:
    explicit ScopedEnvironmentVariable(const char* name) : name_(name)
    {
        const char* value = std::getenv(name_);
        if (value != nullptr) {
            wasSet_ = true;
            value_ = value;
        }
    }

    ~ScopedEnvironmentVariable()
    {
        if (wasSet_) {
            setenv(name_, value_.c_str(), 1);
        } else {
            unsetenv(name_);
        }
    }

    ScopedEnvironmentVariable(const ScopedEnvironmentVariable&) = delete;
    ScopedEnvironmentVariable& operator=(const ScopedEnvironmentVariable&) = delete;

    int Set(const char* value) const { return setenv(name_, value, 1); }

    int Unset() const { return unsetenv(name_); }

private:
    const char* name_;
    bool wasSet_ = false;
    std::string value_;
};

aclError OriginalDataLoad(
    const void* data, size_t length, const aclrtBinaryLoadOptions* options, aclrtBinHandle* binHandle)
{
    ++g_originalDataCalls;
    g_forwardedData = data;
    g_forwardedLength = length;
    g_forwardedDataOptions = options;
    if (binHandle != nullptr) {
        *binHandle = reinterpret_cast<aclrtBinHandle>(0x51);
    }
    if (g_recurseDataLoad) {
        g_recurseDataLoad = false;
        return aclrtBinaryLoadFromData(data, length, options, binHandle);
    }
    return 81;
}

aclError SentinelDataLoad(const void*, size_t, const aclrtBinaryLoadOptions*, aclrtBinHandle*)
{
    ++g_sentinelDataCalls;
    return 91;
}

void Callback(void*, AclsanCallbackDomain, AclsanCallbackId, const void*) {}

} // namespace

int main()
{
    ScopedEnvironmentVariable dbiArch("NPU_CHECK_DBI_ARCH");
    ScopedEnvironmentVariable dbiArgSize("NPU_CHECK_DBI_ARG_SIZE");
    ScopedEnvironmentVariable dbiStrict("NPU_CHECK_DBI_STRICT");
    ScopedEnvironmentVariable probeObject("ACLSAN_PROBE_OBJECT");
    ScopedEnvironmentVariable probeCtrlBinary("ACLSAN_PROBE_CTRL_BINARY");
    ScopedEnvironmentVariable probeSymbolOrdering("ACLSAN_PROBE_SYMBOL_ORDERING");
    ScopedEnvironmentVariable probeWorkRoot("ACLSAN_PROBE_WORK_ROOT");
    ScopedEnvironmentVariable probeArgumentBytes("ACLSAN_PROBE_ARGUMENT_BYTES");

    CHECK(dbiArch.Unset() == 0);
    CHECK(dbiArgSize.Unset() == 0);
    CHECK(dbiStrict.Unset() == 0);
    CHECK(probeObject.Set("/missing/probe.o") == 0);
    CHECK(probeCtrlBinary.Set("/missing/ctrl.bin") == 0);
    CHECK(probeSymbolOrdering.Set("/missing/symbol_ordering.txt") == 0);
    CHECK(probeWorkRoot.Set("/missing/work") == 0);
    CHECK(probeArgumentBytes.Set("invalid") == 0);

    CHECK(RuntimeStubSetOriginFunction("aclrtBinaryLoadFromData", &OriginalDataLoad) == ACL_SUCCESS);

    AclsanSubscriberHandle subscriber = nullptr;
    CHECK(aclsanSubscribe(&subscriber, &Callback, nullptr) == ACLSAN_STATUS_SUCCESS);
    CHECK(acltoolRegisterAclrtBinaryLoadFromDataCallbacks(&SentinelDataLoad) == ACL_SUCCESS);
    CHECK(
        aclsanEnableCallback(1, subscriber, ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_MEMORY_ACCESS) ==
        ACLSAN_STATUS_SUCCESS);

    const unsigned char binary[] = {0x7f, 'E', 'L', 'F'};
    aclrtBinaryLoadOptions dataOptions{};
    aclrtBinHandle dataHandle = nullptr;
    CHECK(aclrtBinaryLoadFromData(binary, sizeof(binary), &dataOptions, &dataHandle) == 81);
    CHECK(g_sentinelDataCalls == 0);
    CHECK(g_originalDataCalls == 1);
    CHECK(g_forwardedData == binary);
    CHECK(g_forwardedLength == sizeof(binary));
    CHECK(g_forwardedDataOptions == &dataOptions);
    CHECK(dataHandle == reinterpret_cast<aclrtBinHandle>(0x51));

    CHECK(
        aclsanEnableCallback(0, subscriber, ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_MEMORY_ACCESS) ==
        ACLSAN_STATUS_SUCCESS);
    CHECK(acltoolRegisterAclrtBinaryLoadFromDataCallbacks(&SentinelDataLoad) == ACL_SUCCESS);
    CHECK(aclrtBinaryLoadFromData(binary, sizeof(binary), &dataOptions, &dataHandle) == 91);
    CHECK(g_sentinelDataCalls == 1);

    CHECK(
        aclsanEnableCallback(1, subscriber, ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_SYNC) ==
        ACLSAN_STATUS_SUCCESS);
    CHECK(aclrtBinaryLoadFromData(binary, sizeof(binary), &dataOptions, &dataHandle) == 81);

    CHECK(dbiArch.Set("dav-c310") == 0);
    CHECK(dbiArgSize.Set("24") == 0);
    const int dataCallsBeforeRecursion = g_originalDataCalls;
    g_recurseDataLoad = true;
    CHECK(aclrtBinaryLoadFromData(binary, sizeof(binary), &dataOptions, &dataHandle) == 81);
    CHECK(g_originalDataCalls == dataCallsBeforeRecursion + 2);

    CHECK(aclsanUnsubscribe(subscriber) == ACLSAN_STATUS_SUCCESS);
    CHECK(aclrtBinaryLoadFromData(binary, sizeof(binary), &dataOptions, &dataHandle) == 81);
    CHECK(g_sentinelDataCalls == 1);
    CHECK(g_originalDataCalls == 5);
    return 0;
}
