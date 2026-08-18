/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_SRC_NPU_COMPUTE_HARDWARE_INFO_DEVICE_H_
#define NPU_COMPUTE_SRC_NPU_COMPUTE_HARDWARE_INFO_DEVICE_H_

#include "hardware_info_types.h"

#include <cstdint>
#include <string>

namespace npu_compute {

inline constexpr std::int32_t kDeviceAttributeAiCpuCoreCount = 1;
inline constexpr std::int32_t kDeviceAttributeAiCoreCount = 101;
inline constexpr std::int32_t kDeviceAttributeCubeCoreCount = 102;
inline constexpr std::int32_t kDeviceAttributeVectorCoreCount = 201;
inline constexpr std::int32_t kDeviceAttributeNpuArch = 601;

inline constexpr std::int32_t kPlatformMemorySize = 5;
inline constexpr std::int32_t kPlatformCubeFrequency = 6;
inline constexpr std::int32_t kPlatformVectorFrequency = 7;

class HardwareDeviceApi {
public:
    virtual ~HardwareDeviceApi() = default;

    virtual bool GetDeviceCount(std::int32_t* value) = 0;
    virtual bool GetSocName(std::string* value) = 0;
    virtual bool GetDeviceAttribute(std::int32_t deviceId, std::int32_t attribute, std::int64_t* value) = 0;
    virtual bool GetPlatformValue(std::int32_t type, std::string* value) = 0;
    virtual bool GetControlCpuCount(std::int32_t deviceId, std::uint32_t* value) = 0;
    virtual bool GetAiCpuFrequency(std::int32_t deviceId, std::uint32_t* value) = 0;
    virtual bool GetChipVersion(std::int32_t deviceId, std::string* value) = 0;
    virtual bool GetHbmUsage(std::int32_t deviceId, std::uint64_t* freeBytes, std::uint64_t* totalBytes) = 0;
    virtual bool GetHbmFrequency(std::int32_t deviceId, std::uint32_t* value) = 0;
};

bool CollectDevice0Info(
    HardwareDeviceApi& api, DeviceInfo* device, CpuInfo* cpu, AiCoreInfo* aiCore, MemoryInfo* memory,
    DiagnosticSink* diagnostics);

} // namespace npu_compute

#endif // NPU_COMPUTE_SRC_NPU_COMPUTE_HARDWARE_INFO_DEVICE_H_
