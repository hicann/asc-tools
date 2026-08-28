/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "aclpti/aclpti.h"
#include "npu_compute/runtime_stub_api.h"
#include "profiling/prof_api.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

#define CHECK(expression)                                                                 \
    do {                                                                                  \
        if (!(expression)) {                                                              \
            std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expression); \
            return 1;                                                                     \
        }                                                                                 \
    } while (false)

int DummyMalloc(void** pointer, std::size_t size, aclrtMemMallocPolicy)
{
    *pointer = std::malloc(size);
    return *pointer == nullptr ? -1 : 0;
}

int DummyFree(void* pointer)
{
    std::free(pointer);
    return 0;
}

int DummyMemcpy(void*, std::size_t, const void*, std::size_t, aclrtMemcpyKind) { return 0; }
int DummyMemset(void*, std::size_t, int, std::size_t) { return 0; }
int DummyLaunch(void*, uint32_t, const void*, std::size_t, void*) { return 0; }
int DummySynchronize(void*) { return 0; }

} // namespace

int main()
{
    CHECK(setenv("NPU_COMPUTE_TEST_PTI_INITIALIZE_FAILURE", "1", 1) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtMalloc", &DummyMalloc) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtFree", &DummyFree) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtMemcpy", &DummyMemcpy) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtMemset", &DummyMemset) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtLaunchKernel", &DummyLaunch) == 0);
    CHECK(RuntimeStubSetOriginFunction("aclrtSynchronizeStream", &DummySynchronize) == 0);

    aclptiSubscribeHandle subscriber = nullptr;
    CHECK(aclptiSubscribe(&subscriber, nullptr, nullptr, nullptr) == ACLPTI_ERROR_INTERNAL);
    CHECK(subscriber == nullptr);
    return 0;
}
