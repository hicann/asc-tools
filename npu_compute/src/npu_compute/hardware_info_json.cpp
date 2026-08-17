/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "hardware_info_json.h"

#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>

namespace npu_compute {
namespace {

void SetError(const std::string& message, std::string* error)
{
    if (error != nullptr) {
        *error = message;
    }
}

bool ValidateCapacity(double value, std::string_view field, std::string* error)
{
    if (std::isfinite(value) && value >= 0) {
        return true;
    }
    SetError(std::string(field) + " must be a finite non-negative value", error);
    return false;
}

std::string FormatCapacity(double value)
{
    if (value == 0) {
        return "0";
    }

    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(2) << value;
    std::string result = stream.str();
    while (!result.empty() && result.back() == '0') {
        result.pop_back();
    }
    if (!result.empty() && result.back() == '.') {
        result.pop_back();
    }
    return result;
}

std::string EscapeJson(std::string_view value)
{
    static constexpr char kHex[] = "0123456789abcdef";
    std::string escaped;
    escaped.reserve(value.size());
    for (const unsigned char character : value) {
        switch (character) {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\b':
                escaped += "\\b";
                break;
            case '\f':
                escaped += "\\f";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                if (character < 0x20) {
                    escaped += "\\u00";
                    escaped.push_back(kHex[(character >> 4) & 0x0f]);
                    escaped.push_back(kHex[character & 0x0f]);
                } else {
                    escaped.push_back(static_cast<char>(character));
                }
                break;
        }
    }
    return escaped;
}

} // namespace

bool SerializeHardwareInfoJsonl(const HardwareInfoSnapshot& snapshot, std::string* jsonl, std::string* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (jsonl == nullptr) {
        SetError("HardwareInfo JSONL output is null", error);
        return false;
    }
    jsonl->clear();

    if (!ValidateCapacity(snapshot.host.memoryTotalSizeMb, "memory total size(MB)", error) ||
        !ValidateCapacity(snapshot.host.diskTotalSizeGb, "disk total size(GB)", error) ||
        !ValidateCapacity(snapshot.memory.hbmTotalMb, "hbm total(MB)", error) ||
        !ValidateCapacity(snapshot.memory.hbmUsedMb, "hbm used(MB)", error)) {
        return false;
    }

    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << "{\"category\":\"Host Info\",\"cpu physical count\":" << snapshot.host.cpuPhysicalCount
           << ",\"cpu logical count\":" << snapshot.host.cpuLogicalCount
           << ",\"memory total size(MB)\":" << FormatCapacity(snapshot.host.memoryTotalSizeMb)
           << ",\"disk total size(GB)\":" << FormatCapacity(snapshot.host.diskTotalSizeGb) << "}\n";
    output << "{\"category\":\"Device Info\",\"npu count\":" << snapshot.device.npuCount << ",\"chip info\":\""
           << EscapeJson(snapshot.device.chipInfo) << "\",\"arch info\":\"" << EscapeJson(snapshot.device.archInfo)
           << "\"}\n";
    output << "{\"category\":\"CPU Information\",\"control cpu count\":" << snapshot.cpu.controlCpuCount
           << ",\"ai cpu count\":" << snapshot.cpu.aiCpuCount
           << ",\"ai cpu frequency(MHZ)\":" << snapshot.cpu.aiCpuFrequencyMhz << "}\n";
    output << "{\"category\":\"AI Core Information\",\"ai core count\":" << snapshot.aiCore.aiCoreCount
           << ",\"ai cube count\":" << snapshot.aiCore.aiCubeCount
           << ",\"ai vector count\":" << snapshot.aiCore.aiVectorCount
           << ",\"ai cube frequency(MHZ)\":" << snapshot.aiCore.aiCubeFrequencyMhz
           << ",\"ai vector frequency(MHZ)\":" << snapshot.aiCore.aiVectorFrequencyMhz << "}\n";
    output << "{\"category\":\"Memory Information\",\"hbm total(MB)\":" << FormatCapacity(snapshot.memory.hbmTotalMb)
           << ",\"hbm used(MB)\":" << FormatCapacity(snapshot.memory.hbmUsedMb)
           << ",\"hbm frequency(MHZ)\":" << snapshot.memory.hbmFrequencyMhz << "}\n";

    *jsonl = output.str();
    return true;
}

} // namespace npu_compute
