/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_SRC_NPU_COMPUTE_HARDWARE_INFO_TYPES_H_
#define NPU_COMPUTE_SRC_NPU_COMPUTE_HARDWARE_INFO_TYPES_H_

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace npu_compute {

struct HostInfo {
    std::uint32_t cpuPhysicalCount = 0;
    std::uint32_t cpuLogicalCount = 0;
    double memoryTotalSizeMb = 0;
    double diskTotalSizeGb = 0;
};

struct DeviceInfo {
    std::uint32_t npuCount = 0;
    std::string chipInfo;
    std::string archInfo;
};

struct CpuInfo {
    std::uint32_t controlCpuCount = 0;
    std::uint32_t aiCpuCount = 0;
    std::uint32_t aiCpuFrequencyMhz = 0;
};

struct AiCoreInfo {
    std::uint32_t aiCoreCount = 0;
    std::uint32_t aiCubeCount = 0;
    std::uint32_t aiVectorCount = 0;
    std::uint32_t aiCubeFrequencyMhz = 0;
    std::uint32_t aiVectorFrequencyMhz = 0;
};

struct MemoryInfo {
    double hbmTotalMb = 0;
    double hbmUsedMb = 0;
    std::uint32_t hbmFrequencyMhz = 0;
};

struct HardwareInfoSnapshot {
    HostInfo host;
    DeviceInfo device;
    CpuInfo cpu;
    AiCoreInfo aiCore;
    MemoryInfo memory;
};

using DiagnosticSink = std::function<void(std::string_view)>;

} // namespace npu_compute

#endif // NPU_COMPUTE_SRC_NPU_COMPUTE_HARDWARE_INFO_TYPES_H_
