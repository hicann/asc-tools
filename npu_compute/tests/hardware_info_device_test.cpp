/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hardware_info_device.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <map>
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

class FakeHardwareDeviceApi final : public npu_compute::HardwareDeviceApi {
public:
    bool GetDeviceCount(std::int32_t* value) override
    {
        ++deviceCountCalls;
        if (failDeviceCount) {
            return false;
        }
        *value = deviceCount;
        return true;
    }

    bool GetSocName(std::string* value) override
    {
        ++socNameCalls;
        if (failSocName) {
            return false;
        }
        *value = socName;
        return true;
    }

    bool GetDeviceAttribute(std::int32_t deviceId, std::int32_t attribute, std::int64_t* value) override
    {
        deviceIds.push_back(deviceId);
        deviceAttributes.push_back(attribute);
        if (failedDeviceAttributes.count(attribute) != 0) {
            return false;
        }
        const auto iterator = deviceAttributeValues.find(attribute);
        if (iterator == deviceAttributeValues.end()) {
            return false;
        }
        *value = iterator->second;
        return true;
    }

    bool GetPlatformValue(std::int32_t type, std::string* value) override
    {
        platformTypes.push_back(type);
        if (failedPlatformTypes.count(type) != 0) {
            return false;
        }
        const auto iterator = platformValues.find(type);
        if (iterator == platformValues.end()) {
            return false;
        }
        *value = iterator->second;
        return true;
    }

    bool GetControlCpuCount(std::int32_t deviceId, uint32_t* value) override
    {
        deviceIds.push_back(deviceId);
        if (failControlCpuCount) {
            return false;
        }
        *value = controlCpuCount;
        return true;
    }

    bool GetAiCpuFrequency(std::int32_t deviceId, uint32_t* value) override
    {
        deviceIds.push_back(deviceId);
        if (failAiCpuFrequency) {
            return false;
        }
        *value = aiCpuFrequency;
        return true;
    }

    bool GetChipVersion(std::int32_t deviceId, std::string* value) override
    {
        deviceIds.push_back(deviceId);
        if (failChipVersion) {
            return false;
        }
        *value = chipVersion;
        return true;
    }

    bool GetHbmUsage(std::int32_t deviceId, uint64_t* freeBytes, uint64_t* totalBytes) override
    {
        deviceIds.push_back(deviceId);
        if (failHbmUsage) {
            return false;
        }
        *freeBytes = hbmFreeBytes;
        *totalBytes = hbmTotalBytes;
        return true;
    }

    bool GetHbmFrequency(std::int32_t deviceId, uint32_t* value) override
    {
        deviceIds.push_back(deviceId);
        if (failHbmFrequency) {
            return false;
        }
        *value = hbmFrequency;
        return true;
    }

    std::size_t SpecializedCallCount() const
    {
        return static_cast<std::size_t>(socNameCalls) + deviceIds.size() + deviceAttributes.size() +
               platformTypes.size();
    }

    std::int32_t deviceCount = 1;
    std::string socName = "Ascend950PR_9599";
    std::string chipVersion = "V100";
    uint32_t controlCpuCount = 1;
    uint32_t aiCpuFrequency = 1500;
    uint64_t hbmFreeBytes = 10ULL * 1024ULL * 1024ULL;
    uint64_t hbmTotalBytes = 16ULL * 1024ULL * 1024ULL;
    uint32_t hbmFrequency = 3200;
    std::map<std::int32_t, std::int64_t> deviceAttributeValues = {
        {npu_compute::kDeviceAttributeNpuArch, 3510},       {npu_compute::kDeviceAttributeAiCpuCoreCount, 6},
        {npu_compute::kDeviceAttributeAiCoreCount, 36},     {npu_compute::kDeviceAttributeCubeCoreCount, 36},
        {npu_compute::kDeviceAttributeVectorCoreCount, 72},
    };
    std::map<std::int32_t, std::string> platformValues = {
        {npu_compute::kPlatformMemorySize, "137438953472"},
        {npu_compute::kPlatformCubeFrequency, "1800"},
        {npu_compute::kPlatformVectorFrequency, "1700"},
    };

    bool failDeviceCount = false;
    bool failSocName = false;
    bool failControlCpuCount = false;
    bool failAiCpuFrequency = false;
    bool failChipVersion = false;
    bool failHbmUsage = false;
    bool failHbmFrequency = false;
    std::set<std::int32_t> failedDeviceAttributes;
    std::set<std::int32_t> failedPlatformTypes;

    int deviceCountCalls = 0;
    int socNameCalls = 0;
    std::vector<std::int32_t> deviceIds;
    std::vector<std::int32_t> deviceAttributes;
    std::vector<std::int32_t> platformTypes;
};

bool Contains(const std::vector<std::string>& diagnostics, std::string_view text)
{
    return std::any_of(diagnostics.begin(), diagnostics.end(), [text](const std::string& value) {
        return value.find(text) != std::string::npos;
    });
}

bool TestCompleteMapping()
{
    FakeHardwareDeviceApi api;
    npu_compute::DeviceInfo device;
    npu_compute::CpuInfo cpu;
    npu_compute::AiCoreInfo aiCore;
    npu_compute::MemoryInfo memory;
    std::vector<std::string> diagnostics;
    npu_compute::DiagnosticSink sink = [&diagnostics](std::string_view value) { diagnostics.emplace_back(value); };

    CHECK(npu_compute::CollectDevice0Info(api, &device, &cpu, &aiCore, &memory, &sink));
    CHECK(device.npuCount == 1);
    CHECK(device.chipInfo == "Ascend950PR_9599 V100");
    CHECK(device.archInfo == "3510");
    CHECK(cpu.controlCpuCount == 1);
    CHECK(cpu.aiCpuCount == 6);
    CHECK(cpu.aiCpuFrequencyMhz == 1500);
    CHECK(aiCore.aiCoreCount == 36);
    CHECK(aiCore.aiCubeCount == 36);
    CHECK(aiCore.aiVectorCount == 72);
    CHECK(aiCore.aiCubeFrequencyMhz == 1800);
    CHECK(aiCore.aiVectorFrequencyMhz == 1700);
    CHECK(memory.hbmTotalMb == 131072.0);
    CHECK(memory.hbmUsedMb == 6.0);
    CHECK(memory.hbmFrequencyMhz == 3200);
    CHECK(diagnostics.empty());

    const std::vector<std::int32_t> expectedAttributes = {
        npu_compute::kDeviceAttributeNpuArch,         npu_compute::kDeviceAttributeAiCpuCoreCount,
        npu_compute::kDeviceAttributeAiCoreCount,     npu_compute::kDeviceAttributeCubeCoreCount,
        npu_compute::kDeviceAttributeVectorCoreCount,
    };
    const std::vector<std::int32_t> expectedPlatformTypes = {
        npu_compute::kPlatformCubeFrequency,
        npu_compute::kPlatformVectorFrequency,
        npu_compute::kPlatformMemorySize,
    };
    CHECK(api.deviceAttributes == expectedAttributes);
    CHECK(api.platformTypes == expectedPlatformTypes);
    CHECK(!api.deviceIds.empty());
    CHECK(std::all_of(api.deviceIds.begin(), api.deviceIds.end(), [](std::int32_t value) { return value == 0; }));
    return true;
}

bool TestPartialFailuresAndInvalidValues()
{
    FakeHardwareDeviceApi api;
    api.failSocName = true;
    api.failedDeviceAttributes.insert(npu_compute::kDeviceAttributeAiCoreCount);
    api.deviceAttributeValues[npu_compute::kDeviceAttributeAiCpuCoreCount] = -1;
    api.platformValues[npu_compute::kPlatformCubeFrequency] = "invalid";
    api.platformValues[npu_compute::kPlatformVectorFrequency] = "4294967296";
    api.platformValues[npu_compute::kPlatformMemorySize] = "not-bytes";
    api.hbmFreeBytes = 20;
    api.hbmTotalBytes = 10;
    api.failHbmFrequency = true;

    npu_compute::DeviceInfo device;
    npu_compute::CpuInfo cpu;
    npu_compute::AiCoreInfo aiCore;
    npu_compute::MemoryInfo memory;
    std::vector<std::string> diagnostics;
    npu_compute::DiagnosticSink sink = [&diagnostics](std::string_view value) { diagnostics.emplace_back(value); };

    CHECK(npu_compute::CollectDevice0Info(api, &device, &cpu, &aiCore, &memory, &sink));
    CHECK(device.npuCount == 1);
    CHECK(device.chipInfo.empty());
    CHECK(device.archInfo == "3510");
    CHECK(cpu.controlCpuCount == 1);
    CHECK(cpu.aiCpuCount == 0);
    CHECK(cpu.aiCpuFrequencyMhz == 1500);
    CHECK(aiCore.aiCoreCount == 0);
    CHECK(aiCore.aiCubeCount == 36);
    CHECK(aiCore.aiVectorCount == 72);
    CHECK(aiCore.aiCubeFrequencyMhz == 0);
    CHECK(aiCore.aiVectorFrequencyMhz == 0);
    CHECK(memory.hbmTotalMb == 0);
    CHECK(memory.hbmUsedMb == 0);
    CHECK(memory.hbmFrequencyMhz == 0);
    CHECK(Contains(diagnostics, "GetSocName"));
    CHECK(Contains(diagnostics, "AI CPU core count"));
    CHECK(Contains(diagnostics, "AI Core count"));
    CHECK(Contains(diagnostics, "Cube frequency"));
    CHECK(Contains(diagnostics, "Vector frequency"));
    CHECK(Contains(diagnostics, "HBM total"));
    CHECK(Contains(diagnostics, "HBM usage"));
    CHECK(Contains(diagnostics, "GetHbmFrequency"));
    return true;
}

bool TestNoVisibleDeviceSkipsDeviceQueries()
{
    FakeHardwareDeviceApi api;
    api.deviceCount = 0;
    npu_compute::DeviceInfo device;
    npu_compute::CpuInfo cpu;
    npu_compute::AiCoreInfo aiCore;
    npu_compute::MemoryInfo memory;

    CHECK(npu_compute::CollectDevice0Info(api, &device, &cpu, &aiCore, &memory, nullptr));
    CHECK(api.deviceCountCalls == 1);
    CHECK(api.SpecializedCallCount() == 0);
    CHECK(device.npuCount == 0);
    return true;
}

bool TestInvalidDeviceCountAndOutputPointers()
{
    FakeHardwareDeviceApi api;
    api.deviceCount = -1;
    npu_compute::DeviceInfo device;
    npu_compute::CpuInfo cpu;
    npu_compute::AiCoreInfo aiCore;
    npu_compute::MemoryInfo memory;
    std::vector<std::string> diagnostics;
    npu_compute::DiagnosticSink sink = [&diagnostics](std::string_view value) { diagnostics.emplace_back(value); };

    CHECK(npu_compute::CollectDevice0Info(api, &device, &cpu, &aiCore, &memory, &sink));
    CHECK(device.npuCount == 0);
    CHECK(api.SpecializedCallCount() == 0);
    CHECK(Contains(diagnostics, "device count"));

    FakeHardwareDeviceApi nullApi;
    diagnostics.clear();
    CHECK(!npu_compute::CollectDevice0Info(nullApi, nullptr, &cpu, &aiCore, &memory, &sink));
    CHECK(nullApi.deviceCountCalls == 0);
    CHECK(Contains(diagnostics, "output is null"));
    return true;
}

} // namespace

int main()
{
    if (!TestCompleteMapping() || !TestPartialFailuresAndInvalidValues() || !TestNoVisibleDeviceSkipsDeviceQueries() ||
        !TestInvalidDeviceCountAndOutputPointers()) {
        return 1;
    }
    return 0;
}
