/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "profiling/prof_api.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

namespace {

int g_callbackCount = 0;
MsprofRawData g_rawData{};

int Check(bool condition, const char* expression, int line)
{
    if (condition) {
        return 0;
    }
    std::fprintf(stderr, "check failed at line %d: %s\n", line, expression);
    return 1;
}

#define CHECK(expression)                                      \
    do {                                                       \
        if (Check((expression), #expression, __LINE__) != 0) { \
            return 1;                                          \
        }                                                      \
    } while (false)

std::int32_t CaptureRawData(MsprofRawData* rawData)
{
    if (rawData == nullptr) {
        return -1;
    }
    ++g_callbackCount;
    g_rawData = *rawData;
    return 0;
}

} // namespace

int main()
{
    MsprofConfig defaultConfig{};
    CHECK(MsprofStart(8, &defaultConfig, sizeof(defaultConfig)) == 0);

    MsprofConfigAttr instrModeAttr{};
    instrModeAttr.id = PROF_CONFIG_ATTR_INSTR;
    MsprofConfig instrModeConfig{};
    instrModeConfig.configInfo.attrs = &instrModeAttr;
    instrModeConfig.configInfo.numAttrs = 1;
    CHECK(MsprofStart(8, &instrModeConfig, sizeof(instrModeConfig)) == 0);

    MsprofConfigAttr attr{};
    attr.id = PROF_CONFIG_ATTR_AICORE_METRICS;
    std::fill(std::begin(attr.value.aicoreMetrics), std::end(attr.value.aicoreMetrics), MSPROF_INVALID_AICORE_METRIC);
    attr.value.aicoreMetrics[0] = 0;

    MsprofConfig config{};
    config.profSwitch = PROF_AICORE_METRICS_MASK;
    config.configInfo.attrs = &attr;
    config.configInfo.numAttrs = 1;

    CHECK(MsprofRegisterDataCallback(8, reinterpret_cast<void*>(&CaptureRawData)) == 0);

    instrModeAttr.value.instrMode = PROF_COMPUTE_BIU_PERF;
    CHECK(MsprofStart(8, &instrModeConfig, sizeof(instrModeConfig)) == 0);
    CHECK(MsprofStop(8, &instrModeConfig, sizeof(instrModeConfig)) == 0);
    CHECK(g_callbackCount == 1);
    CHECK(g_rawData.type == BIU_PERF_DATA_TYPE);

    instrModeAttr.value.instrMode = PROF_COMPUTE_PC_SAMPLING;
    CHECK(MsprofStart(8, &instrModeConfig, sizeof(instrModeConfig)) == 0);
    CHECK(MsprofStop(8, &instrModeConfig, sizeof(instrModeConfig)) == 0);
    CHECK(g_callbackCount == 2);
    CHECK(g_rawData.type == PC_SAMPLING_DATA_TYPE);

    CHECK(MsprofStart(8, &config, sizeof(config)) == 0);
    CHECK(MsprofStop(8, &config, sizeof(config)) == 0);
    CHECK(g_callbackCount == 3);
    CHECK(g_rawData.isLastChunk);
    CHECK(g_rawData.type == PMU_DATA_TYPE);
    CHECK(g_rawData.deviceId == 0);
    CHECK(g_rawData.offset == 0);
    CHECK(g_rawData.chunkModule == 0);
    CHECK(g_rawData.chunkSize == 128);
    uint32_t magic = 0;
    std::memcpy(&magic, g_rawData.chunk, sizeof(magic));
    CHECK(magic == 0x6bd3002aU);
    return 0;
}
