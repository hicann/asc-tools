/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_STUBS_PROF_API_INCLUDE_PROFILING_PROF_COMMON_H_
#define NPU_COMPUTE_STUBS_PROF_API_INCLUDE_PROFILING_PROF_COMMON_H_

#include <cstddef>
#include <cstdint>

#define MSPROF_MAX_DEV_NUM 64
#define MAX_DUMP_PATH_LEN 1024
#define MAX_SAMPLE_CONFIG_LEN 4096
#define RAW_DATA_MAXSIZE 256

#define PROF_AICORE_METRICS_MASK 0x00000004ULL
#define PROF_TASK_TIME_MASK 0x00000800ULL

inline constexpr std::size_t COMPUTE_AICORE_METRICS_NUM = 10;
inline constexpr std::uint32_t COMPUTE_INVALID_AICORE_METRIC_EVENT = 0xffffffffU;

enum MsprofConfigAttrId {
    MSPROF_AICOREMETRICS = 0,
    MSPROF_COMPUTE_INSTR_MODE = 1,
};

enum MsprofComputeInstrMode {
    COMPUTE_INSTR_MODE_BIU_PERF = 0x1,
    COMPUTE_INSTR_MODE_PC_SAMPLING = 0x2,
};

union MsprofConfigAttrValue {
    std::uint32_t aicoreMetrics[COMPUTE_AICORE_METRICS_NUM];
    std::uint32_t instrMode;
};

struct MsprofConfigAttr {
    MsprofConfigAttrId id;
    MsprofConfigAttrValue value;
};

struct MsprofConfigInfo {
    MsprofConfigAttr* attrs;
    std::size_t numAttrs;
};

struct MsprofConfig {
    std::uint64_t profSwitch;
    std::uint64_t profSwitchHi;
    std::uint32_t devNums;
    std::uint32_t devIdList[MSPROF_MAX_DEV_NUM + 1];
    std::uint32_t modelId;
    std::uint32_t type;
    std::uint32_t cacheFlag;
    std::uint32_t storageLimit;
    std::uint32_t metrics;
    std::uintptr_t fd;
    char dumpPath[MAX_DUMP_PATH_LEN];
    char sampleConfig[MAX_SAMPLE_CONFIG_LEN];
    MsprofConfigInfo configInfo;
};

enum RawDataType {
    API_DATA_TYPE = 0,
    EVENT_DATA_TYPE = 1,
    RUNTIMETRACK_DATA_TYPE = 3,
    PMU_DATA_TYPE = 4,
    DEFAULT_DATA_TYPE = 5,
    LOG_DATA_TYPE = 7,
    BIU_PERF_DATA_TYPE = 8,
    PC_SAMPLING_DATA_TYPE = 9
};

struct MsprofRawData {
    bool isLastChunk;
    std::size_t offset;
    std::int32_t chunkModule;
    std::int32_t deviceId;
    RawDataType type;
    std::size_t chunkSize;
    char chunk[RAW_DATA_MAXSIZE];
};

using MsprofRawDataCallback = std::int32_t (*)(MsprofRawData* rawData);

#endif // NPU_COMPUTE_STUBS_PROF_API_INCLUDE_PROFILING_PROF_COMMON_H_
