/**
* Copyright (c) 2026 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/

#ifndef KERNEL_SIMT_CPU_DEBUG
#define KERNEL_SIMT_CPU_DEBUG
#ifdef ASCENDC_CPU_DEBUG
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <chrono>

#include "stub_def.h"
#include "kernel_log.h"

namespace AscendC {
namespace Simt {
constexpr uint32_t THREAD_PER_WARP = 32;
// 2 piece interleave Shared memory in a warp to guarantee data exchange/modification without data race at warp level.
constexpr uint32_t MEMORY_PIECE = 2;
template <typename Func>
void FuncWrapper(Func func, uint32_t warpId, uint32_t threadIdx);

class Warp {
public:
    Warp() {}
    ~Warp();

    Warp &operator=(Warp &&other) = default;

    Warp(Warp &&) {}

    template <typename Func>
    void Schedule(Func func, uint32_t warpId, uint32_t idx)
    {
        threads[idx] = std::thread(FuncWrapper<decltype(func)>, func, warpId, idx);
    }

    void Done();

    template <typename T, typename Func>
    T WarpOp(T val, Func action)
    {
        // Shared memory in a warp to guarentee data exchange/modification without data race at warp level.
        std::unique_lock<std::mutex> lck(mtx_);
        auto currGeneration = syncGeneration;
        void* temp = reinterpret_cast<void *>(&data[currGeneration % MEMORY_PIECE]);
        T &dataToUpdate = *reinterpret_cast<T *>(temp);
        activeThreads--;
        if (activeThreads == 0) {
            syncGeneration++;
            activeThreads = THREAD_PER_WARP;
            dataToUpdate = action(val, dataToUpdate);
            isReset = false;
            cv_.notify_all();
        } else {
            if (!isReset) {
                dataToUpdate = val;
                isReset = true;
            } else {
                dataToUpdate = action(val, dataToUpdate);
            }
            bool notTimeout = cv_.wait_for(
                lck, std::chrono::seconds(5), [this, currGeneration] { return currGeneration != syncGeneration; });
            if (!notTimeout) {
                KERNEL_LOG(KERNEL_ERROR,
                    "Warp operation timeout, CPU Debug only supports all 32 threads must be involved in the same "
                    "warp operation. If it has already satisfied this condition, maybe deadlock occurred.");
            }
        }
        return dataToUpdate;
    }

    template <typename T>
    T WarpShuffleOp(T val, uint32_t laneToWrite, uint32_t laneToRead)
    {
        std::unique_lock<std::mutex> lck(mtx_);

        auto currGeneration = syncGeneration;
        void* temp = reinterpret_cast<void *>(&shuffleData[laneToWrite][currGeneration % MEMORY_PIECE]);
        T &dataToUpdate = *reinterpret_cast<T *>(temp);
        dataToUpdate = val;
        activeThreads--;
        if (activeThreads == 0) {
            syncGeneration++;
            activeThreads = THREAD_PER_WARP;
            cv_.notify_all();
        } else {
            bool notTimeout = cv_.wait_for(
                lck, std::chrono::seconds(5), [this, currGeneration] { return currGeneration != syncGeneration; });
            if (!notTimeout) {
                KERNEL_LOG(KERNEL_ERROR,
                    "Shuffle Warp operation timeout, CPU Debug only supports all 32 threads must be involved in the same "
                    "warp operation. If it has already satisfied this condition, maybe deadlock occurred.");
            }
        }

        void* temp2 = reinterpret_cast<void *>(&shuffleData[laneToRead][currGeneration % MEMORY_PIECE]);
        return *reinterpret_cast<T *>(temp2);
    }

private:
    uint32_t activeThreads{THREAD_PER_WARP};
    uint32_t syncGeneration{0};
    bool isReset{false};
    uint32_t shuffleData[THREAD_PER_WARP][MEMORY_PIECE];
    uint64_t data[MEMORY_PIECE]{0};

    std::mutex mtx_;
    std::condition_variable cv_;
    std::thread threads[THREAD_PER_WARP];
};

class ThreadBlock {
public:
    static ThreadBlock &GetBlockInstance();

    void Init(uint32_t num);

    template <typename Func>
    void Schedule(Func func, uint32_t idx)
    {
        ASCENDC_ASSERT((idx / THREAD_PER_WARP < warpNum_),
                       { KERNEL_LOG(KERNEL_ERROR, "thread idx %u exceeds warp count %u", idx, warpNum_); });
        warps_[idx / THREAD_PER_WARP].Schedule<Func>(func, idx / THREAD_PER_WARP, idx % THREAD_PER_WARP);
    }

    template <typename Func>
    void AtomicOp(Func action)
    {
        std::unique_lock<std::mutex> lck(mtx_);
        action();
    }

    void FinishJobs();

    void SyncAllThreads();

    void ThreadFinished();

public:
    ThreadBlock() : activeThreads(0), syncGeneration(0), threadThreshold(0), warpNum_(0) {}
    ~ThreadBlock()
    {
        FinishJobs();
    }

    std::vector<Warp> warps_;

    std::mutex mtx_;
    std::condition_variable cv_;
    uint32_t activeThreads{0};
    uint32_t syncGeneration{0};
    uint32_t threadThreshold{0};
    uint32_t warpNum_{0};
};

template <typename Func>
void FuncWrapper(Func func, uint32_t warpId, uint32_t threadIdx)
{
    uint32_t overallIdx = warpId * THREAD_PER_WARP + threadIdx;
    g_threadIdxX = overallIdx % g_threadDimX;
    g_threadIdxY = (overallIdx / g_threadDimX) % g_threadDimY;
    g_threadIdxZ = overallIdx / (g_threadDimY * g_threadDimX);
    func();
    ThreadBlock::GetBlockInstance().ThreadFinished();
}

uint32_t GetThreadIdx();

uint32_t GetLaneId();

uint32_t GetWarpId();

void Sync();
}  // namespace Simt
}  // namespace AscendC

#endif
#endif  // KERNEL_SIMT_CPU_DEBUG
