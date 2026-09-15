/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "acl_pti/profiling/binary_instrumenter.h"
#include "acl_pti/profiling/binary_registry.h"
#include <cstdio>
#include <cstring>
#include <vector>

using namespace aclpti::profiling;
namespace {
int binaryTokens[32];
int functionTokens[32];
int loads = 0;
int failLoad = -1;
void* failLookup = nullptr;
void* failUnload = nullptr;
bool failTools = false;
bool failBinary = false;
bool failName = false;
std::vector<void*> unloaded;
const void* images[32]{};
std::size_t imageSizes[32]{};

bool ValidImage(aclrtBinHandle binary)
{
    const auto index = static_cast<int*>(binary) - binaryTokens;
    return imageSizes[index] >= 3 && std::memcmp(images[index], "ELF", 3) == 0;
}
#define CHECK(value)                                                 \
    do {                                                             \
        if (!(value)) {                                              \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
            return 1;                                                \
        }                                                            \
    } while (false)

aclError Load(const void* data, std::size_t size, const aclrtBinaryLoadOptions*, aclrtBinHandle* result)
{
    const int index = loads++;
    if (index == failLoad) {
        return ACL_ERROR_RT_FAILURE;
    }
    *result = &binaryTokens[index];
    images[index] = data;
    imageSizes[index] = size;
    return ACL_SUCCESS;
}
aclError Unload(aclrtBinHandle binary)
{
    if (!ValidImage(binary)) {
        return ACL_ERROR_INVALID_PARAM;
    }
    if (binary == failUnload) {
        return ACL_ERROR_RT_FAILURE;
    }
    unloaded.push_back(binary);
    return ACL_SUCCESS;
}
aclError Lookup(aclrtBinHandle binary, const char*, aclrtFuncHandle* result)
{
    if (!ValidImage(binary)) {
        return ACL_ERROR_INVALID_PARAM;
    }
    if (binary == failLookup) {
        return ACL_ERROR_RT_FAILURE;
    }
    *result = &functionTokens[static_cast<int*>(binary) - binaryTokens];
    return ACL_SUCCESS;
}
aclError Entry(aclrtBinHandle binary, std::uint64_t, aclrtFuncHandle* result)
{
    return Lookup(binary, "kernel", result);
}
} // namespace

extern "C" void* acltoolGetOriginalRuntimeApi(aclrtApiId id)
{
    switch (id) {
        case ACL_RT_API_aclrtBinaryLoadFromData:
            return reinterpret_cast<void*>(&Load);
        case ACL_RT_API_aclrtBinaryUnLoad:
            return reinterpret_cast<void*>(&Unload);
        case ACL_RT_API_aclrtBinaryGetFunction:
            return reinterpret_cast<void*>(&Lookup);
        case ACL_RT_API_aclrtBinaryGetFunctionByEntry:
            return reinterpret_cast<void*>(&Entry);
        default:
            return nullptr;
    }
}
extern "C" aclError aclrtFunctionGetBinary(aclrtFuncHandle function, aclrtBinHandle* binary)
{
    if (failBinary)
        return ACL_ERROR_RT_FAILURE;
    *binary = &binaryTokens[static_cast<int*>(function) - functionTokens];
    return ACL_SUCCESS;
}
extern "C" aclError aclrtGetFunctionName(aclrtFuncHandle, uint32_t, char* name)
{
    if (failName)
        return ACL_ERROR_RT_FAILURE;
    std::strcpy(name, "kernel");
    return ACL_SUCCESS;
}
namespace aclpti::profiling {
bool InstrumentKernelEnd(const void*, std::size_t, std::vector<char>& output)
{
    output = {'E', 'L', 'F'};
    return !failTools;
}
} // namespace aclpti::profiling

int main()
{
    BinaryRegistry registry;
    const char input[] = "ELF";
    aclrtBinHandle a = nullptr;
    aclrtFuncHandle f = nullptr;
    CHECK(Load(input, sizeof(input), nullptr, &a) == ACL_SUCCESS);
    failTools = true;
    CHECK(registry.RegisterBinary(input, sizeof(input), nullptr, a) == ACLPTI_ERROR_PROFILING_FAILED);
    CHECK(unloaded.empty()); // Profiling failure must not release the application's original binary.
    failTools = false;
    failLoad = loads;
    CHECK(registry.RegisterBinary(input, sizeof(input), nullptr, a) == ACLPTI_ERROR_PROFILING_FAILED);
    CHECK(unloaded.empty());
    failLoad = -1;
    const int companionIndex = loads;
    CHECK(registry.RegisterBinary(input, sizeof(input), nullptr, a) == ACLPTI_SUCCESS);
    CHECK(Lookup(a, "kernel", &f) == ACL_SUCCESS);
    failBinary = true;
    CHECK(registry.RegisterSymbolFunction(f) == ACLPTI_ERROR_PROFILING_FAILED);
    failBinary = false;
    failName = true;
    CHECK(registry.RegisterSymbolFunction(f) == ACLPTI_ERROR_PROFILING_FAILED);
    failName = false;
    CHECK(registry.RegisterSymbolFunction(f) == ACLPTI_SUCCESS);
    CHECK(registry.FindInstrumentedFunction(f) == &functionTokens[companionIndex]);

    aclrtBinHandle b = nullptr;
    aclrtFuncHandle g = nullptr;
    CHECK(Load(input, sizeof(input), nullptr, &b) == ACL_SUCCESS);
    CHECK(registry.RegisterBinary(input, sizeof(input), nullptr, b) == ACLPTI_SUCCESS);
    CHECK(Entry(b, 42, &g) == ACL_SUCCESS);
    CHECK(registry.RegisterSymbolFunction(g) == ACLPTI_SUCCESS);
    CHECK(registry.RegisterBinaryFunction(b, std::uint64_t(42), g) == ACLPTI_SUCCESS);
    CHECK(registry.FindInstrumentedFunction(f) != registry.FindInstrumentedFunction(g));

    failLookup = &binaryTokens[companionIndex];
    CHECK(registry.RegisterBinaryFunction(a, "kernel", f) == ACLPTI_ERROR_PROFILING_FAILED);
    CHECK(f != nullptr && registry.FindInstrumentedFunction(f) == nullptr);
    CHECK(registry.RegisterBinaryFunction(a, std::uint64_t(42), f) == ACLPTI_ERROR_PROFILING_FAILED);
    failLookup = nullptr;
    CHECK(registry.RegisterBinaryFunction(a, "kernel", f) == ACLPTI_SUCCESS);
    failUnload = &binaryTokens[companionIndex];
    {
        BinaryRegistry::UnloadContext context;
        CHECK(registry.PrepareBinaryUnload(a, context) == ACLPTI_ERROR_PROFILING_FAILED);
    }
    CHECK(registry.FindInstrumentedFunction(f) != nullptr);
    failUnload = nullptr;
    {
        BinaryRegistry::UnloadContext context;
        CHECK(registry.PrepareBinaryUnload(a, context) == ACLPTI_SUCCESS);
        CHECK(Unload(a) == ACL_SUCCESS);
        CHECK(registry.CompleteBinaryUnload(context) == ACLPTI_SUCCESS);
    }
    CHECK(registry.FindInstrumentedFunction(f) == nullptr);
    failUnload = b;
    {
        BinaryRegistry::UnloadContext context;
        CHECK(registry.PrepareBinaryUnload(b, context) == ACLPTI_SUCCESS);
        CHECK(Unload(b) == ACL_ERROR_RT_FAILURE);
        // Destruction releases the lock without removing the original record.
    }
    CHECK(registry.FindInstrumentedFunction(g) == nullptr);
    const auto count = unloaded.size();
    failUnload = nullptr;
    {
        BinaryRegistry::UnloadContext context;
        CHECK(registry.PrepareBinaryUnload(b, context) == ACLPTI_SUCCESS);
        CHECK(Unload(b) == ACL_SUCCESS);
        CHECK(registry.CompleteBinaryUnload(context) == ACLPTI_SUCCESS);
    }
    CHECK(unloaded.size() == count + 1);
}
