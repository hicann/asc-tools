/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_ACLPTI_PROFILING_REPLAY_MEMORY_H_
#define NPU_COMPUTE_ACLPTI_PROFILING_REPLAY_MEMORY_H_

#include "npu_compute/injection_hook.h"

#include <cstddef>
#include <cstdint>
#include <map>

namespace npu_compute::aclpti::profiling {

class ReplayMemory {
public:
    int MirrorMalloc(
        aclrtMallocFunc mallocFunction, aclrtFreeFunc freeFunction, void** devPtr, std::size_t size,
        aclrtMemMallocPolicy policy);
    int MirrorFree(aclrtFreeFunc freeFunction, void* devPtr);
    int MirrorMemcpy(
        aclrtMemcpyFunc memcpyFunction, void* destination, std::size_t destinationSize, const void* source,
        std::size_t count, aclrtMemcpyKind kind);
    int MirrorMemset(
        aclrtMemsetFunc memsetFunction, void* devPtr, std::size_t maxCount, std::int32_t value, std::size_t count);
    int Restore(aclrtMemcpyFunc memcpyFunction) const;

private:
    struct ShadowBuffer {
        void* shadow;
        std::size_t size;
    };

    bool FindShadowBuffer(const void* pointer, std::size_t count, ShadowBuffer* buffer, std::size_t* offset) const;

    std::map<uintptr_t, ShadowBuffer> shadowBuffers_;
};

} // namespace npu_compute::aclpti::profiling

#endif // NPU_COMPUTE_ACLPTI_PROFILING_REPLAY_MEMORY_H_
