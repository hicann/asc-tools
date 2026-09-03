/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef ACLPTI_DATA_H_
#define ACLPTI_DATA_H_

#include "aclpti/aclpti_export.h"
#include "aclpti/aclpti_types.h"

#ifdef __cplusplus

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <vector>

enum aclptiCoreType : uint8_t {
    ACLPTI_CORE_TYPE_AIC = 0,
    ACLPTI_CORE_TYPE_AIV = 1,
};

struct aclptiPmuDataRow {
    struct SystemCounter {
        uint64_t taskStartSystemCounter;
        uint64_t taskEndSystemCounter;
    };

    struct CoreInfo {
        aclptiCoreType coreType;
        uint8_t coreId;
        uint64_t count;
    };

    struct CoreData {
        aclptiCoreType coreType;
        uint8_t coreId;
        uint64_t sampleCount = 0;
        double totalCycles = 0.0;
        bool overflow = false;
        std::map<uint32_t, double> values;
        std::map<uint32_t, uint64_t> valueCounts;
        std::vector<SystemCounter> systemCounters;
    };

    uint16_t blockId;
    uint16_t subBlockId;
    aclptiCoreType coreType;
    uint8_t coreId;
    std::vector<CoreInfo> coreInfos;
    double totalCycles = 0.0;
    bool overflow = false;
    std::map<uint32_t, double> values;
    std::vector<CoreData> coreData;
    std::vector<SystemCounter> systemCounters;
};

struct aclptiBlockKey {
    uint16_t blockId = 0;
    uint16_t subBlockId = 0;
    aclptiCoreType coreType = ACLPTI_CORE_TYPE_AIC;
    uint8_t coreId = 0;

    bool operator<(const aclptiBlockKey& other) const
    {
        if (blockId != other.blockId) {
            return blockId < other.blockId;
        }
        if (subBlockId != other.subBlockId) {
            return subBlockId < other.subBlockId;
        }
        if (coreType != other.coreType) {
            return coreType < other.coreType;
        }
        return coreId < other.coreId;
    }

    bool operator==(const aclptiBlockKey& other) const
    {
        return blockId == other.blockId && subBlockId == other.subBlockId && coreType == other.coreType &&
               coreId == other.coreId;
    }
};

struct aclptiTaskLogRow {
    uint8_t funcType;
    uint16_t taskId;
    uint16_t rtStreamId;
    uint64_t systemCounter;
    uint16_t blockId;
    uint16_t subBlockId;
    aclptiCoreType coreType;
    uint8_t coreTypeId;
};

struct aclptiRawDataChunk {
    uint64_t replayId;
    int32_t deviceId;
    int32_t chunkModule;
    size_t offset;
    bool isLastChunk;
    std::vector<uint8_t> bytes;
};

struct aclptiProfilingDataResult {
    aclptiResult status = ACLPTI_SUCCESS;
    std::map<uint16_t, std::vector<aclptiTaskLogRow>> taskLogs;
    std::map<aclptiBlockKey, std::vector<aclptiTaskLogRow>> blockLogs;
    std::map<aclptiBlockKey, aclptiPmuDataRow> pmuLogs;
    std::vector<aclptiRawDataChunk> pipelineData;
    std::vector<aclptiRawDataChunk> pcSamplingData;
    struct ErrorStats {
        uint64_t failedRecordCount = 0;
        std::map<uint64_t, uint64_t> failedRecordCountByReplay;
    } errorStats;
};

using aclptiProfilingDataCallback = std::function<aclptiResult(std::shared_ptr<const aclptiProfilingDataResult>)>;

ACLPTI_EXPORT aclptiResult aclptiRegisterProfilingDataCallback(aclptiProfilingDataCallback callback);

using aclptiDataModuleShutdownCallback = aclptiResult (*)(void* userData);

extern "C" ACLPTI_EXPORT aclptiResult
aclptiRegisterDataModuleShutdownCallback(aclptiDataModuleShutdownCallback callback, void* userData);

#endif // __cplusplus

#endif // ACLPTI_DATA_H_
