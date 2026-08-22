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

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <vector>

enum aclptiCoreType : std::uint8_t {
    ACLPTI_CORE_TYPE_AIC = 0,
    ACLPTI_CORE_TYPE_AIV = 1,
};

struct aclptiPmuDataRow {
    struct SystemCounter {
        std::uint64_t taskStartSystemCounter;
        std::uint64_t taskEndSystemCounter;
    };

    struct CoreInfo {
        aclptiCoreType coreType;
        std::uint8_t coreId;
        std::uint64_t count;
    };

    struct CoreData {
        aclptiCoreType coreType;
        std::uint8_t coreId;
        std::uint64_t sampleCount = 0;
        double totalCycles = 0.0;
        bool overflow = false;
        std::map<std::uint32_t, double> values;
        std::map<std::uint32_t, std::uint64_t> valueCounts;
        std::vector<SystemCounter> systemCounters;
    };

    std::uint16_t blockId;
    std::uint16_t subBlockId;
    aclptiCoreType coreType;
    std::uint8_t coreId;
    std::vector<CoreInfo> coreInfos;
    double totalCycles = 0.0;
    bool overflow = false;
    std::map<std::uint32_t, double> values;
    std::vector<CoreData> coreData;
    std::vector<SystemCounter> systemCounters;
};

struct aclptiBlockKey {
    std::uint16_t blockId = 0;
    std::uint16_t subBlockId = 0;
    aclptiCoreType coreType = ACLPTI_CORE_TYPE_AIC;
    std::uint8_t coreId = 0;

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
    std::uint8_t funcType;
    std::uint16_t taskId;
    std::uint16_t rtStreamId;
    std::uint64_t systemCounter;
    std::uint16_t blockId;
    std::uint16_t subBlockId;
    aclptiCoreType coreType;
    std::uint8_t coreTypeId;
};

struct aclptiPmuDataResult {
    aclptiResult status = ACLPTI_SUCCESS;
    std::map<std::uint16_t, std::vector<aclptiTaskLogRow>> taskLogs;
    std::map<aclptiBlockKey, std::vector<aclptiTaskLogRow>> blockLogs;
    std::map<aclptiBlockKey, aclptiPmuDataRow> pmuLogs;
    struct ErrorStats {
        std::uint64_t failedRecordCount = 0;
        std::map<std::uint64_t, std::uint64_t> failedRecordCountByReplay;
    } errorStats;
};

using aclptiPmuDataCallback = std::function<aclptiResult(std::shared_ptr<const aclptiPmuDataResult>)>;

ACLPTI_EXPORT aclptiResult aclptiRegisterPmuDataCallback(aclptiPmuDataCallback callback);

using aclptiDataModuleShutdownCallback = aclptiResult (*)(void* userData);

extern "C" ACLPTI_EXPORT aclptiResult
aclptiRegisterDataModuleShutdownCallback(aclptiDataModuleShutdownCallback callback, void* userData);

#endif // __cplusplus

#endif // ACLPTI_DATA_H_
