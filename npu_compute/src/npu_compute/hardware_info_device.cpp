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

#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace npu_compute {
namespace {

constexpr std::int32_t kDeviceId = 0;
constexpr double kBytesPerMb = 1024.0 * 1024.0;

void Diagnose(DiagnosticSink* diagnostics, const std::string& message)
{
    if (diagnostics != nullptr && *diagnostics) {
        (*diagnostics)(message);
    }
}

std::string_view Trim(std::string_view value)
{
    constexpr std::string_view whitespace = " \t\r\n";
    const std::size_t first = value.find_first_not_of(whitespace);
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

template <typename Integer>
bool ParseUnsigned(std::string_view text, Integer* value)
{
    const std::string_view trimmed = Trim(text);
    if (trimmed.empty()) {
        return false;
    }
    Integer parsedValue = 0;
    const auto parsed = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), parsedValue);
    if (parsed.ec != std::errc{} || parsed.ptr != trimmed.data() + trimmed.size()) {
        return false;
    }
    *value = parsedValue;
    return true;
}

bool ReadCountAttribute(
    HardwareDeviceApi& api, std::int32_t attribute, std::string_view fieldName, uint32_t* result,
    DiagnosticSink* diagnostics)
{
    std::int64_t value = 0;
    if (!api.GetDeviceAttribute(kDeviceId, attribute, &value)) {
        Diagnose(diagnostics, "GetDeviceAttribute failed for Device 0: " + std::string(fieldName));
        return false;
    }
    if (value < 0 || static_cast<uint64_t>(value) > std::numeric_limits<uint32_t>::max()) {
        Diagnose(diagnostics, "invalid " + std::string(fieldName) + " returned for Device 0");
        return false;
    }
    *result = static_cast<uint32_t>(value);
    return true;
}

void CollectChipInfo(HardwareDeviceApi& api, DeviceInfo* device, DiagnosticSink* diagnostics)
{
    std::string socName;
    if (!api.GetSocName(&socName) || socName.empty()) {
        Diagnose(diagnostics, "GetSocName failed or returned an empty value");
    } else {
        device->chipInfo = socName;
    }

    std::string chipVersion;
    if (!api.GetChipVersion(kDeviceId, &chipVersion)) {
        Diagnose(diagnostics, "GetChipVersion failed for Device 0");
    } else if (!device->chipInfo.empty() && !chipVersion.empty()) {
        device->chipInfo += " " + chipVersion;
    }
}

void CollectArchitecture(HardwareDeviceApi& api, DeviceInfo* device, DiagnosticSink* diagnostics)
{
    std::int64_t value = 0;
    if (!api.GetDeviceAttribute(kDeviceId, kDeviceAttributeNpuArch, &value)) {
        Diagnose(diagnostics, "GetDeviceAttribute failed for Device 0: NPU architecture");
        return;
    }
    if (value < 0) {
        Diagnose(diagnostics, "invalid NPU architecture returned for Device 0");
        return;
    }
    device->archInfo = std::to_string(value);
}

void CollectCpuInfo(HardwareDeviceApi& api, CpuInfo* cpu, DiagnosticSink* diagnostics)
{
    if (!api.GetControlCpuCount(kDeviceId, &cpu->controlCpuCount)) {
        Diagnose(diagnostics, "GetControlCpuCount failed for Device 0");
    }
    ReadCountAttribute(api, kDeviceAttributeAiCpuCoreCount, "AI CPU core count", &cpu->aiCpuCount, diagnostics);
    if (!api.GetAiCpuFrequency(kDeviceId, &cpu->aiCpuFrequencyMhz)) {
        Diagnose(diagnostics, "GetAiCpuFrequency failed for Device 0");
    }
}

void ReadFrequency(
    HardwareDeviceApi& api, std::int32_t type, std::string_view fieldName, uint32_t* result,
    DiagnosticSink* diagnostics)
{
    std::string text;
    if (!api.GetPlatformValue(type, &text)) {
        Diagnose(diagnostics, "GetPlatformValue failed: " + std::string(fieldName));
        return;
    }
    if (!ParseUnsigned(text, result)) {
        Diagnose(diagnostics, "invalid " + std::string(fieldName));
    }
}

void CollectAiCoreInfo(HardwareDeviceApi& api, AiCoreInfo* aiCore, DiagnosticSink* diagnostics)
{
    ReadCountAttribute(api, kDeviceAttributeAiCoreCount, "AI Core count", &aiCore->aiCoreCount, diagnostics);
    ReadCountAttribute(api, kDeviceAttributeCubeCoreCount, "Cube Core count", &aiCore->aiCubeCount, diagnostics);
    ReadCountAttribute(api, kDeviceAttributeVectorCoreCount, "Vector Core count", &aiCore->aiVectorCount, diagnostics);
    ReadFrequency(api, kPlatformCubeFrequency, "Cube frequency", &aiCore->aiCubeFrequencyMhz, diagnostics);
    ReadFrequency(api, kPlatformVectorFrequency, "Vector frequency", &aiCore->aiVectorFrequencyMhz, diagnostics);
}

void CollectMemoryInfo(HardwareDeviceApi& api, MemoryInfo* memory, DiagnosticSink* diagnostics)
{
    std::string totalBytesText;
    uint64_t totalBytes = 0;
    if (!api.GetPlatformValue(kPlatformMemorySize, &totalBytesText)) {
        Diagnose(diagnostics, "GetPlatformValue failed: HBM total size");
    } else if (!ParseUnsigned(totalBytesText, &totalBytes)) {
        Diagnose(diagnostics, "invalid HBM total size");
    } else {
        memory->hbmTotalMb = static_cast<double>(totalBytes) / kBytesPerMb;
    }

    uint64_t freeBytes = 0;
    uint64_t allocatableBytes = 0;
    if (!api.GetHbmUsage(kDeviceId, &freeBytes, &allocatableBytes)) {
        Diagnose(diagnostics, "GetHbmUsage failed for Device 0");
    } else if (freeBytes > allocatableBytes) {
        Diagnose(diagnostics, "invalid HBM usage returned for Device 0");
    } else {
        memory->hbmUsedMb = static_cast<double>(allocatableBytes - freeBytes) / kBytesPerMb;
    }

    if (!api.GetHbmFrequency(kDeviceId, &memory->hbmFrequencyMhz)) {
        Diagnose(diagnostics, "GetHbmFrequency failed for Device 0");
    }
}

} // namespace

bool CollectDevice0Info(
    HardwareDeviceApi& api, DeviceInfo* device, CpuInfo* cpu, AiCoreInfo* aiCore, MemoryInfo* memory,
    DiagnosticSink* diagnostics)
{
    if (device == nullptr || cpu == nullptr || aiCore == nullptr || memory == nullptr) {
        Diagnose(diagnostics, "HardwareInfo output is null");
        return false;
    }

    *device = {};
    *cpu = {};
    *aiCore = {};
    *memory = {};

    std::int32_t deviceCount = 0;
    if (!api.GetDeviceCount(&deviceCount)) {
        Diagnose(diagnostics, "GetDeviceCount failed");
        return true;
    }
    if (deviceCount < 0) {
        Diagnose(diagnostics, "invalid device count");
        return true;
    }
    device->npuCount = static_cast<uint32_t>(deviceCount);
    if (deviceCount == 0) {
        return true;
    }

    CollectChipInfo(api, device, diagnostics);
    CollectArchitecture(api, device, diagnostics);
    CollectCpuInfo(api, cpu, diagnostics);
    CollectAiCoreInfo(api, aiCore, diagnostics);
    CollectMemoryInfo(api, memory, diagnostics);
    return true;
}

} // namespace npu_compute
