/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "aclpti/aclpti_data.h"
#include "pmu_data_consumer.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

#define CHECK(condition)                                                                         \
    do {                                                                                         \
        if (!(condition)) {                                                                      \
            std::fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            return 1;                                                                            \
        }                                                                                        \
    } while (false)

std::shared_ptr<const aclptiProfilingDataResult> Result(uint64_t replayId)
{
    auto result = std::make_shared<aclptiProfilingDataResult>();
    result->status = ACLPTI_SUCCESS;
    aclptiPmuDataRow row{};
    row.blockId = static_cast<uint16_t>(replayId);
    row.systemCounters.push_back({0x1122334455667788ULL, 0x8877665544332211ULL});
    result->pmuLogs.emplace(aclptiBlockKey{row.blockId, 0}, row);
    return result;
}

int main()
{
    std::mutex mutex;
    std::condition_variable ready;
    std::vector<uint64_t> processed;
    std::vector<uint64_t> startCounters;
    std::vector<uint64_t> endCounters;
    const std::thread::id submitThread = std::this_thread::get_id();
    std::atomic<bool> usedSubmitThread{false};

    auto consumer = npu_compute::PmuDataConsumer::Create([&](std::shared_ptr<const aclptiProfilingDataResult> result) {
        if (std::this_thread::get_id() == submitThread) {
            usedSubmitThread = true;
        }
        std::lock_guard<std::mutex> lock(mutex);
        const auto& row = result->pmuLogs.begin()->second;
        processed.push_back(row.blockId);
        startCounters.push_back(row.systemCounters.front().taskStartSystemCounter);
        endCounters.push_back(row.systemCounters.front().taskEndSystemCounter);
        ready.notify_all();
        return ACLPTI_SUCCESS;
    });
    CHECK(consumer != nullptr);
    CHECK(consumer->Start() == ACLPTI_SUCCESS);
    CHECK(consumer->Submit(Result(1)) == ACLPTI_SUCCESS);
    CHECK(consumer->Submit(Result(2)) == ACLPTI_SUCCESS);

    {
        std::unique_lock<std::mutex> lock(mutex);
        CHECK(ready.wait_for(lock, std::chrono::seconds(3), [&] { return processed.size() == 2; }));
    }
    CHECK(!usedSubmitThread.load());
    CHECK((processed == std::vector<uint64_t>{1, 2}));
    CHECK((startCounters == std::vector<uint64_t>{0x1122334455667788ULL, 0x1122334455667788ULL}));
    CHECK((endCounters == std::vector<uint64_t>{0x8877665544332211ULL, 0x8877665544332211ULL}));
    CHECK(consumer->ShutdownAndDrain() == ACLPTI_SUCCESS);
    CHECK(consumer->Submit(Result(3)) == ACLPTI_ERROR_INVALID_STATE);
    CHECK(consumer->ShutdownAndDrain() == ACLPTI_SUCCESS);
    return 0;
}
