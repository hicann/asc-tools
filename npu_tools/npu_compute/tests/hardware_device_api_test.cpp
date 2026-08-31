/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hardware_device_api.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

#define CHECK(expression)                                                                 \
    do {                                                                                  \
        if (!(expression)) {                                                              \
            std::fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #expression); \
            return false;                                                                 \
        }                                                                                 \
    } while (false)

constexpr std::int32_t kAclSuccess = 0;
constexpr std::int32_t kAclHbmMem = 1;
constexpr std::int32_t kHalModuleTypeCcpu = 2;
constexpr std::int32_t kHalInfoTypeCoreNum = 3;
constexpr std::int32_t kDsmiDeviceTypeHbm = 2;
constexpr std::size_t kDsmiTextLength = 32;
constexpr std::size_t kDsmiAiCpuCount = 16;

struct FakeDsmiChipInfo {
    unsigned char chipType[kDsmiTextLength];
    unsigned char chipName[kDsmiTextLength];
    unsigned char chipVersion[kDsmiTextLength];
};

struct FakeDsmiAiCpuInfo {
    unsigned int maxFrequency;
    unsigned int currentFrequency;
    unsigned int aiCpuCount;
    unsigned int utilization[kDsmiAiCpuCount];
};

struct Calls {
    std::vector<uint32_t> deviceInfoDeviceIds;
    std::vector<std::int32_t> deviceAttributes;
    std::vector<std::int32_t> platformTypes;
    std::vector<std::int32_t> setDeviceIds;
    std::vector<std::int32_t> memoryAttributes;
    std::vector<uint32_t> halDeviceIds;
    std::vector<std::int32_t> halModuleTypes;
    std::vector<std::int32_t> halInfoTypes;
    std::vector<std::int32_t> dsmiAiCpuDeviceIds;
    std::vector<std::int32_t> dsmiChipDeviceIds;
    std::vector<std::int32_t> dsmiFrequencyDeviceIds;
    std::vector<std::int32_t> dsmiDeviceTypes;
};

Calls g_calls;

void ResetCalls() { g_calls = {}; }

extern "C" std::int32_t StubAclrtGetDeviceCount(uint32_t* value)
{
    *value = 2;
    return kAclSuccess;
}

extern "C" const char* StubAclrtGetSocName() { return "Ascend950PR_9599"; }

extern "C" std::int32_t StubAclrtGetDeviceInfo(uint32_t deviceId, std::int32_t attribute, std::int64_t* value)
{
    g_calls.deviceInfoDeviceIds.push_back(deviceId);
    g_calls.deviceAttributes.push_back(attribute);
    *value = 3510;
    return kAclSuccess;
}

extern "C" std::int32_t StubAclplatformGetDeviceInfo(std::int32_t type, char* value, uint32_t maxLength)
{
    g_calls.platformTypes.push_back(type);
    constexpr char kResult[] = "1800";
    if (maxLength < sizeof(kResult)) {
        return 1;
    }
    std::memcpy(value, kResult, sizeof(kResult));
    return kAclSuccess;
}

extern "C" std::int32_t StubAclrtSetDevice(std::int32_t deviceId)
{
    g_calls.setDeviceIds.push_back(deviceId);
    return kAclSuccess;
}

extern "C" std::int32_t StubAclrtGetMemInfo(std::int32_t attribute, std::size_t* freeBytes, std::size_t* totalBytes)
{
    g_calls.memoryAttributes.push_back(attribute);
    *freeBytes = 10U * 1024U * 1024U;
    *totalBytes = 16U * 1024U * 1024U;
    return kAclSuccess;
}

extern "C" std::int32_t StubHalGetDeviceInfo(
    uint32_t deviceId, std::int32_t moduleType, std::int32_t infoType, std::int64_t* value)
{
    g_calls.halDeviceIds.push_back(deviceId);
    g_calls.halModuleTypes.push_back(moduleType);
    g_calls.halInfoTypes.push_back(infoType);
    *value = 1;
    return 0;
}

extern "C" std::int32_t StubDsmiGetAiCpuInfo(std::int32_t deviceId, FakeDsmiAiCpuInfo* value)
{
    g_calls.dsmiAiCpuDeviceIds.push_back(deviceId);
    value->currentFrequency = 1500;
    return 0;
}

extern "C" std::int32_t StubDsmiGetChipInfo(std::int32_t deviceId, FakeDsmiChipInfo* value)
{
    g_calls.dsmiChipDeviceIds.push_back(deviceId);
    constexpr char kVersion[] = "V100";
    std::memcpy(value->chipVersion, kVersion, sizeof(kVersion));
    return 0;
}

extern "C" std::int32_t StubDsmiGetDeviceFrequency(std::int32_t deviceId, std::int32_t deviceType, unsigned int* value)
{
    g_calls.dsmiFrequencyDeviceIds.push_back(deviceId);
    g_calls.dsmiDeviceTypes.push_back(deviceType);
    *value = 3200;
    return 0;
}

template <typename Function>
void* Symbol(Function function)
{
    return reinterpret_cast<void*>(function);
}

class FakeDynamicSymbolResolver final : public npu_compute::DynamicSymbolResolver {
public:
    struct Library {
        std::string name;
    };

    void* FindLoadedSymbol(const char* name) override
    {
        requestedLoadedSymbols.emplace_back(name);
        const auto iterator = loadedSymbols.find(name);
        return iterator == loadedSymbols.end() ? nullptr : iterator->second;
    }

    void* OpenLibrary(const char* soname) override
    {
        openedLibraries.emplace_back(soname);
        if (availableLibraries.count(soname) == 0) {
            return nullptr;
        }
        auto library = std::make_unique<Library>();
        library->name = soname;
        void* handle = library.get();
        handles.emplace(handle, std::move(library));
        return handle;
    }

    void* FindLibrarySymbol(void* handle, const char* name) override
    {
        requestedLibrarySymbols.emplace_back(name);
        const auto handleIterator = handles.find(handle);
        if (handleIterator == handles.end()) {
            return nullptr;
        }
        const auto libraryIterator = librarySymbols.find(handleIterator->second->name);
        if (libraryIterator == librarySymbols.end()) {
            return nullptr;
        }
        const auto symbolIterator = libraryIterator->second.find(name);
        return symbolIterator == libraryIterator->second.end() ? nullptr : symbolIterator->second;
    }

    void CloseLibrary(void* handle) override
    {
        const auto iterator = handles.find(handle);
        if (iterator != handles.end()) {
            closedLibraries.push_back(iterator->second->name);
            handles.erase(iterator);
        }
    }

    void AddLoadedAclSymbols()
    {
        loadedSymbols = {
            {"aclrtGetDeviceCount", Symbol(&StubAclrtGetDeviceCount)},
            {"aclrtGetSocName", Symbol(&StubAclrtGetSocName)},
            {"aclrtGetDeviceInfo", Symbol(&StubAclrtGetDeviceInfo)},
            {"aclplatformGetDeviceInfo", Symbol(&StubAclplatformGetDeviceInfo)},
            {"aclrtSetDevice", Symbol(&StubAclrtSetDevice)},
            {"aclrtGetMemInfo", Symbol(&StubAclrtGetMemInfo)},
        };
    }

    void AddAclFallbackLibraries()
    {
        availableLibraries.insert("libascendcl.so");
        availableLibraries.insert("libplatform.so");
        librarySymbols["libascendcl.so"] = {
            {"aclrtGetDeviceCount", Symbol(&StubAclrtGetDeviceCount)},
            {"aclrtGetSocName", Symbol(&StubAclrtGetSocName)},
            {"aclrtGetDeviceInfo", Symbol(&StubAclrtGetDeviceInfo)},
            {"aclrtSetDevice", Symbol(&StubAclrtSetDevice)},
            {"aclrtGetMemInfo", Symbol(&StubAclrtGetMemInfo)},
        };
        librarySymbols["libplatform.so"] = {
            {"aclplatformGetDeviceInfo", Symbol(&StubAclplatformGetDeviceInfo)},
        };
    }

    void AddDriverLibraries()
    {
        availableLibraries.insert("libascend_hal.so");
        availableLibraries.insert("libdrvdsmi_host.so");
        librarySymbols["libascend_hal.so"] = {
            {"halGetDeviceInfo", Symbol(&StubHalGetDeviceInfo)},
        };
        librarySymbols["libdrvdsmi_host.so"] = {
            {"dsmi_get_aicpu_info", Symbol(&StubDsmiGetAiCpuInfo)},
            {"dsmi_get_chip_info", Symbol(&StubDsmiGetChipInfo)},
            {"dsmi_get_device_frequency", Symbol(&StubDsmiGetDeviceFrequency)},
        };
    }

    std::map<std::string, void*> loadedSymbols;
    std::set<std::string> availableLibraries;
    std::map<std::string, std::map<std::string, void*>> librarySymbols;
    std::map<void*, std::unique_ptr<Library>> handles;
    std::vector<std::string> requestedLoadedSymbols;
    std::vector<std::string> requestedLibrarySymbols;
    std::vector<std::string> openedLibraries;
    std::vector<std::string> closedLibraries;
};

bool Contains(const std::vector<std::string>& values, std::string_view expected)
{
    return std::find(values.begin(), values.end(), expected) != values.end();
}

bool TestLoadedSymbolsAndExactArguments()
{
    ResetCalls();
    auto resolver = std::make_shared<FakeDynamicSymbolResolver>();
    resolver->AddLoadedAclSymbols();
    resolver->AddDriverLibraries();
    {
        npu_compute::DynamicHardwareDeviceApi api(resolver);
        std::int32_t count = 0;
        std::string text;
        std::int64_t attributeValue = 0;
        uint32_t unsignedValue = 0;
        uint64_t freeBytes = 0;
        uint64_t totalBytes = 0;

        CHECK(api.GetDeviceCount(&count));
        CHECK(count == 2);
        CHECK(api.GetSocName(&text));
        CHECK(text == "Ascend950PR_9599");
        CHECK(api.GetDeviceAttribute(0, npu_compute::kDeviceAttributeNpuArch, &attributeValue));
        CHECK(attributeValue == 3510);
        CHECK(api.GetPlatformValue(npu_compute::kPlatformCubeFrequency, &text));
        CHECK(text == "1800");
        CHECK(api.GetControlCpuCount(0, &unsignedValue));
        CHECK(unsignedValue == 1);
        CHECK(api.GetAiCpuFrequency(0, &unsignedValue));
        CHECK(unsignedValue == 1500);
        CHECK(api.GetChipVersion(0, &text));
        CHECK(text == "V100");
        CHECK(api.GetHbmUsage(0, &freeBytes, &totalBytes));
        CHECK(freeBytes == 10U * 1024U * 1024U);
        CHECK(totalBytes == 16U * 1024U * 1024U);
        CHECK(api.GetHbmFrequency(0, &unsignedValue));
        CHECK(unsignedValue == 3200);

        CHECK(g_calls.deviceInfoDeviceIds == std::vector<uint32_t>{0});
        CHECK(g_calls.deviceAttributes == std::vector<std::int32_t>{npu_compute::kDeviceAttributeNpuArch});
        CHECK(g_calls.platformTypes == std::vector<std::int32_t>{npu_compute::kPlatformCubeFrequency});
        CHECK(g_calls.setDeviceIds == std::vector<std::int32_t>{0});
        CHECK(g_calls.memoryAttributes == std::vector<std::int32_t>{kAclHbmMem});
        CHECK(g_calls.halDeviceIds == std::vector<uint32_t>{0});
        CHECK(g_calls.halModuleTypes == std::vector<std::int32_t>{kHalModuleTypeCcpu});
        CHECK(g_calls.halInfoTypes == std::vector<std::int32_t>{kHalInfoTypeCoreNum});
        CHECK(g_calls.dsmiAiCpuDeviceIds == std::vector<std::int32_t>{0});
        CHECK(g_calls.dsmiChipDeviceIds == std::vector<std::int32_t>{0});
        CHECK(g_calls.dsmiFrequencyDeviceIds == std::vector<std::int32_t>{0});
        CHECK(g_calls.dsmiDeviceTypes == std::vector<std::int32_t>{kDsmiDeviceTypeHbm});
        CHECK(!Contains(resolver->openedLibraries, "libascendcl.so"));
        CHECK(!Contains(resolver->openedLibraries, "libplatform.so"));
        CHECK(!Contains(resolver->requestedLoadedSymbols, "aclrtResetDevice"));
        CHECK(!Contains(resolver->requestedLibrarySymbols, "aclrtResetDevice"));
    }
    CHECK(Contains(resolver->closedLibraries, "libascend_hal.so"));
    CHECK(Contains(resolver->closedLibraries, "libdrvdsmi_host.so"));
    return true;
}

bool TestSonameFallbackAndHandleLifetime()
{
    ResetCalls();
    auto resolver = std::make_shared<FakeDynamicSymbolResolver>();
    resolver->AddAclFallbackLibraries();
    {
        npu_compute::DynamicHardwareDeviceApi api(resolver);
        std::int32_t count = 0;
        std::string value;
        std::int64_t attributeValue = 0;
        uint64_t freeBytes = 0;
        uint64_t totalBytes = 0;
        CHECK(api.GetDeviceCount(&count));
        CHECK(api.GetSocName(&value));
        CHECK(api.GetDeviceAttribute(0, npu_compute::kDeviceAttributeNpuArch, &attributeValue));
        CHECK(api.GetPlatformValue(npu_compute::kPlatformMemorySize, &value));
        CHECK(api.GetHbmUsage(0, &freeBytes, &totalBytes));
        CHECK(std::count(resolver->openedLibraries.begin(), resolver->openedLibraries.end(), "libascendcl.so") == 1);
        CHECK(std::count(resolver->openedLibraries.begin(), resolver->openedLibraries.end(), "libplatform.so") == 1);
        CHECK(resolver->closedLibraries.empty());
    }
    CHECK(Contains(resolver->closedLibraries, "libascendcl.so"));
    CHECK(Contains(resolver->closedLibraries, "libplatform.so"));
    return true;
}

bool TestMissingDriverLibrariesAreIndependent()
{
    ResetCalls();
    auto resolver = std::make_shared<FakeDynamicSymbolResolver>();
    resolver->AddLoadedAclSymbols();
    npu_compute::DynamicHardwareDeviceApi api(resolver);
    uint32_t value = 0;
    std::string text;
    std::int32_t count = 0;

    CHECK(!api.GetControlCpuCount(0, &value));
    CHECK(!api.GetAiCpuFrequency(0, &value));
    CHECK(!api.GetChipVersion(0, &text));
    CHECK(!api.GetHbmFrequency(0, &value));
    CHECK(api.GetDeviceCount(&count));
    CHECK(count == 2);
    CHECK(Contains(resolver->openedLibraries, "libascend_hal.so"));
    CHECK(Contains(resolver->openedLibraries, "libdrvdsmi_host.so"));
    return true;
}

bool TestMissingSingleAndAllSymbols()
{
    ResetCalls();
    auto partialResolver = std::make_shared<FakeDynamicSymbolResolver>();
    partialResolver->availableLibraries.insert("libascendcl.so");
    partialResolver->librarySymbols["libascendcl.so"] = {
        {"aclrtGetDeviceCount", Symbol(&StubAclrtGetDeviceCount)},
    };
    npu_compute::DynamicHardwareDeviceApi partialApi(partialResolver);
    std::int32_t count = 0;
    std::int64_t attributeValue = 0;
    CHECK(partialApi.GetDeviceCount(&count));
    CHECK(!partialApi.GetDeviceAttribute(0, npu_compute::kDeviceAttributeNpuArch, &attributeValue));
    CHECK(partialApi.GetDeviceCount(&count));

    auto emptyResolver = std::make_shared<FakeDynamicSymbolResolver>();
    npu_compute::DynamicHardwareDeviceApi emptyApi(emptyResolver);
    std::string text;
    uint32_t value = 0;
    uint64_t freeBytes = 0;
    uint64_t totalBytes = 0;
    CHECK(!emptyApi.GetDeviceCount(&count));
    CHECK(!emptyApi.GetSocName(&text));
    CHECK(!emptyApi.GetDeviceAttribute(0, npu_compute::kDeviceAttributeNpuArch, &attributeValue));
    CHECK(!emptyApi.GetPlatformValue(npu_compute::kPlatformMemorySize, &text));
    CHECK(!emptyApi.GetControlCpuCount(0, &value));
    CHECK(!emptyApi.GetAiCpuFrequency(0, &value));
    CHECK(!emptyApi.GetChipVersion(0, &text));
    CHECK(!emptyApi.GetHbmUsage(0, &freeBytes, &totalBytes));
    CHECK(!emptyApi.GetHbmFrequency(0, &value));
    return true;
}

} // namespace

int main()
{
    if (!TestLoadedSymbolsAndExactArguments() || !TestSonameFallbackAndHandleLifetime() ||
        !TestMissingDriverLibrariesAreIndependent() || !TestMissingSingleAndAllSymbols()) {
        return 1;
    }
    return 0;
}
