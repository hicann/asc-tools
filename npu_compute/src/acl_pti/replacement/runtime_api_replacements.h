/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_ACLPTI_REPLACEMENT_RUNTIME_API_REPLACEMENTS_H_
#define NPU_COMPUTE_ACLPTI_REPLACEMENT_RUNTIME_API_REPLACEMENTS_H_

#include "acl_pti/profiling/range_profiler.h"
#include "acl_pti/profiling/replay_memory.h"

#include "aclpti/aclpti_runtime_api.h"

#include <atomic>

namespace npu_compute::aclpti::callback {
class Dispatcher;
}

namespace npu_compute::aclpti::replacement {

class RuntimeApiReplacements {
public:
    static bool RegisterCallbacks(callback::Dispatcher& dispatcher);
    bool Initialize(profiling::ReplayMemory& replayMemory, profiling::RangeProfiler& rangeProfiler);

private:
    static RuntimeApiReplacements& Instance();
    bool RegisterReplacements();

    static aclError AclrtLaunchKernelWithHostArgsReplacement(
        aclrtFuncHandle funcHandle, uint32_t numBlocks, aclrtStream stream, aclrtLaunchKernelCfg* cfg, void* hostArgs,
        std::size_t argsSize, aclrtPlaceHolderInfo* placeHolderArray, std::size_t placeHolderNum);
    static aclError AclrtMemcpyReplacement(
        void* destination, std::size_t destinationSize, const void* source, std::size_t count, aclrtMemcpyKind kind);
    static aclError AclrtBinaryLoadFromDataReplacement(
        const void* data, std::size_t length, const aclrtBinaryLoadOptions* options, aclrtBinHandle* binHandle);
    static aclError AclrtBinaryGetFunctionReplacement(
        const aclrtBinHandle binHandle, const char* kernelName, aclrtFuncHandle* funcHandle);
    static aclError AclrtMallocReplacement(void** devPtr, std::size_t size, aclrtMemMallocPolicy policy);
    static aclError AclrtMemsetReplacement(void* devPtr, std::size_t maxCount, std::int32_t value, std::size_t count);
    static aclError AclrtFreeReplacement(void* devPtr);
    static aclError AclrtCreateStreamReplacement(aclrtStream* stream);
    static aclError AclrtDestroyStreamReplacement(aclrtStream stream);
    static aclError AclrtSetDeviceReplacement(std::int32_t deviceId);
    static aclError AclrtResetDeviceReplacement(std::int32_t deviceId);
    static aclError AclrtSynchronizeStreamReplacement(aclrtStream stream);
    static aclError AclrtBinaryGetFunctionByEntryReplacement(
        aclrtBinHandle binHandle, uint64_t funcEntry, aclrtFuncHandle* funcHandle);
    static aclError AclrtLaunchKernelReplacement(
        aclrtFuncHandle function, uint32_t blockCount, const void* argsData, std::size_t argsSize, aclrtStream stream);

    profiling::ReplayMemory* replayMemory_ = nullptr;
    profiling::RangeProfiler* rangeProfiler_ = nullptr;
    std::atomic<std::int32_t> currentDeviceId_{-1};
    bool initialized_ = false;
};

RuntimeApiReplacements& GetRuntimeApiReplacements();

} // namespace npu_compute::aclpti::replacement

#endif // NPU_COMPUTE_ACLPTI_REPLACEMENT_RUNTIME_API_REPLACEMENTS_H_
