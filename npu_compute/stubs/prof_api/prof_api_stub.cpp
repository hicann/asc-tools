/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "npu_compute/prof_api_stub.h"

#include "common/debug_log.h"
#include "profiling/prof_api.h"

#include <dlfcn.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void* g_injectionHandle = nullptr;
bool g_injectionInitialized = false;
MsprofRawDataCallback g_rawDataCallback = nullptr;

using AcltoolInitializeFn = int (*)();

std::size_t CountConfiguredPmus(const MsprofConfig& config)
{
    if (config.configInfo.attrs == nullptr || config.configInfo.numAttrs != 1 ||
        config.configInfo.attrs[0].id != PROF_CONFIG_ATTR_AICORE_METRICS) {
        return 0;
    }

    std::size_t pmuCount = 0;
    for (std::uint32_t event : config.configInfo.attrs[0].value.aicoreMetrics) {
        if (event != MSPROF_INVALID_AICORE_METRIC) {
            ++pmuCount;
        }
    }
    return pmuCount;
}

} // namespace

extern "C" NPU_COMPUTE_EXPORT int ProfApiLoadApiInjectionFromEnv()
{
    const char* injectionPath = std::getenv("ACL_API_INJECTION");
    if (injectionPath == nullptr || injectionPath[0] == '\0') {
        npu_compute::detail::DebugLog("prof_api_stub", "ProfApiLoadApiInjectionFromEnv path is not set");
        return 0;
    }
    if (g_injectionInitialized) {
        npu_compute::detail::DebugLog("prof_api_stub", "ProfApiLoadApiInjectionFromEnv already initialized");
        return 0;
    }

    void* handle = g_injectionHandle;
    bool openedNow = false;
    if (handle == nullptr) {
        npu_compute::detail::DebugLog("prof_api_stub", "ProfApiLoadApiInjectionFromEnv loading path=%s", injectionPath);
        handle = dlopen(injectionPath, RTLD_NOW | RTLD_GLOBAL);
        if (handle == nullptr) {
            std::fprintf(stderr, "[prof_api] dlopen(%s) failed: %s\n", injectionPath, dlerror());
            return -1;
        }
        openedNow = true;
    } else {
        npu_compute::detail::DebugLog("prof_api_stub", "ProfApiLoadApiInjectionFromEnv reusing loaded handle");
    }

    dlerror();
    auto init = reinterpret_cast<AcltoolInitializeFn>(dlsym(handle, "acltoolInitialize"));
    const char* error = dlerror();
    if (error != nullptr || init == nullptr) {
        std::fprintf(
            stderr, "[prof_api] dlsym(acltoolInitialize) failed: %s\n", error == nullptr ? "symbol is null" : error);
        if (openedNow) {
            dlclose(handle);
        }
        return -1;
    }
    if (openedNow) {
        g_injectionHandle = handle;
    }

    const int result = init();
    npu_compute::detail::DebugLog("prof_api_stub", "ProfApiLoadApiInjectionFromEnv init result=%d", result);
    if (result == 0) {
        g_injectionInitialized = true;
    }
    return result;
}

extern "C" NPU_COMPUTE_EXPORT std::int32_t MsprofRegisterDataCallback(std::uint32_t type, void* callback)
{
    npu_compute::detail::DebugLog("prof_api_stub", "MsprofRegisterDataCallback type=%u callback=%p", type, callback);
    if (callback == nullptr) {
        return -1;
    }
    g_rawDataCallback = reinterpret_cast<MsprofRawDataCallback>(callback);
    return 0;
}

extern "C" NPU_COMPUTE_EXPORT std::int32_t MsprofStart(std::uint32_t dataType, const void* data, std::uint32_t length)
{
    if (data == nullptr || length != sizeof(MsprofConfig)) {
        npu_compute::detail::DebugLog(
            "prof_api_stub", "MsprofStart rejected type=%u data=%p length=%u", dataType, const_cast<void*>(data),
            length);
        return -1;
    }

    const auto* config = static_cast<const MsprofConfig*>(data);
    const std::size_t pmuCount = CountConfiguredPmus(*config);
    npu_compute::detail::DebugLog(
        "prof_api_stub", "MsprofStart type=%u profSwitch=0x%llx pmuCount=%zu", dataType,
        static_cast<unsigned long long>(config->profSwitch), pmuCount);
    return 0;
}

extern "C" NPU_COMPUTE_EXPORT std::int32_t MsprofStop(std::uint32_t dataType, const void*, std::uint32_t length)
{
    npu_compute::detail::DebugLog("prof_api_stub", "MsprofStop type=%u length=%u", dataType, length);

    MsprofRawData rawData{};
    rawData.isLastChunk = true;
    rawData.offset = 0;
    rawData.deviceId = 0;
    rawData.type = PMU_DATA_TYPE;
    // Emit one structurally valid PMU record so the integration stub exercises
    // the same decoder path as the production data module.
    std::array<std::uint32_t, 32> record{};
    record[0] = 0x6bd3002aU;
    record[1] = (1U << 16U) | 1U;
    record[2] = 100U;
    record[5] = 1U << 8U;
    record[6] = 1U << 16U;
    rawData.chunkSize = sizeof(record);
    std::memcpy(rawData.chunk, record.data(), sizeof(record));
    if (g_rawDataCallback != nullptr) {
        g_rawDataCallback(&rawData);
    }
    return 0;
}
