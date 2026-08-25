/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "replay_memory.h"

#include "common/debug_log.h"

namespace npu_compute::aclpti::profiling {

int ReplayMemory::MirrorMalloc(
    aclrtMallocFunc mallocFunction, aclrtFreeFunc freeFunction, void** devPtr, std::size_t size,
    aclrtMemMallocPolicy policy)
{
    if (mallocFunction == nullptr || freeFunction == nullptr || devPtr == nullptr || *devPtr == nullptr || size == 0) {
        return -1;
    }

    void* shadow = nullptr;
    const int result = mallocFunction(&shadow, size, policy);
    if (result != 0 || shadow == nullptr) {
        return result == 0 ? -1 : result;
    }
    const uintptr_t origin = reinterpret_cast<uintptr_t>(*devPtr);
    const auto inserted = shadowBuffers_.emplace(origin, ShadowBuffer{shadow, size});
    if (!inserted.second) {
        freeFunction(shadow);
        return -1;
    }
    npu_compute::detail::DebugLog("aclpti", "shadow malloc origin=%p shadow=%p size=%zu", *devPtr, shadow, size);
    return 0;
}

int ReplayMemory::MirrorFree(aclrtFreeFunc freeFunction, void* devPtr)
{
    if (freeFunction == nullptr || devPtr == nullptr) {
        return -1;
    }

    const auto iterator = shadowBuffers_.find(reinterpret_cast<uintptr_t>(devPtr));
    if (iterator == shadowBuffers_.end()) {
        return 0;
    }
    npu_compute::detail::DebugLog("aclpti", "shadow free origin=%p shadow=%p", devPtr, iterator->second.shadow);
    const int result = freeFunction(iterator->second.shadow);
    shadowBuffers_.erase(iterator);
    return result;
}

int ReplayMemory::MirrorMemcpy(
    aclrtMemcpyFunc memcpyFunction, void* destination, std::size_t destinationSize, const void* source,
    std::size_t count, aclrtMemcpyKind kind)
{
    if (memcpyFunction == nullptr || count > destinationSize) {
        return -1;
    }

    ShadowBuffer destinationBuffer{};
    std::size_t destinationOffset = 0;
    if (!FindShadowBuffer(destination, count, &destinationBuffer, &destinationOffset)) {
        return 0;
    }

    if (kind != ACL_MEMCPY_DEVICE_TO_DEVICE && kind != ACL_MEMCPY_HOST_TO_DEVICE) {
        return 0;
    }

    void* shadowDestination = static_cast<uint8_t*>(destinationBuffer.shadow) + destinationOffset;
    const int result =
        memcpyFunction(shadowDestination, destinationBuffer.size - destinationOffset, source, count, kind);
    npu_compute::detail::DebugLog(
        "aclpti", "mirror memcpy kind=%d destination=%p shadow=%p count=%zu result=%d", kind, destination,
        shadowDestination, count, result);
    return result;
}

int ReplayMemory::MirrorMemset(
    aclrtMemsetFunc memsetFunction, void* devPtr, std::size_t maxCount, std::int32_t value, std::size_t count)
{
    if (memsetFunction == nullptr || count > maxCount) {
        return -1;
    }

    ShadowBuffer buffer{};
    std::size_t offset = 0;
    if (!FindShadowBuffer(devPtr, count, &buffer, &offset)) {
        return 0;
    }
    void* shadow = static_cast<uint8_t*>(buffer.shadow) + offset;
    const int result = memsetFunction(shadow, buffer.size - offset, value, count);
    npu_compute::detail::DebugLog(
        "aclpti", "mirror memset origin=%p shadow=%p value=%d count=%zu result=%d", devPtr, shadow, value, count,
        result);
    return result;
}

int ReplayMemory::Restore(aclrtMemcpyFunc memcpyFunction) const
{
    if (memcpyFunction == nullptr) {
        return -1;
    }
    for (const auto& entry : shadowBuffers_) {
        void* origin = reinterpret_cast<void*>(entry.first);
        const int result = memcpyFunction(
            origin, entry.second.size, entry.second.shadow, entry.second.size, ACL_MEMCPY_DEVICE_TO_DEVICE);
        if (result != 0) {
            return result;
        }
        npu_compute::detail::DebugLog(
            "aclpti", "restore origin=%p shadow=%p size=%zu", origin, entry.second.shadow, entry.second.size);
    }
    return 0;
}

bool ReplayMemory::FindShadowBuffer(
    const void* pointer, std::size_t count, ShadowBuffer* buffer, std::size_t* offset) const
{
    if (pointer == nullptr) {
        return false;
    }

    const uintptr_t address = reinterpret_cast<uintptr_t>(pointer);
    auto iterator = shadowBuffers_.upper_bound(address);
    if (iterator == shadowBuffers_.begin()) {
        return false;
    }
    --iterator;

    const std::size_t bufferOffset = static_cast<std::size_t>(address - iterator->first);
    if (bufferOffset > iterator->second.size || count > iterator->second.size - bufferOffset) {
        return false;
    }
    *buffer = iterator->second;
    *offset = bufferOffset;
    return true;
}

} // namespace npu_compute::aclpti::profiling
