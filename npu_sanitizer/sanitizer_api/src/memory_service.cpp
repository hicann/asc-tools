/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "api_core.h"

#include <cstdlib>
#include <cstring>

namespace ascsan {
namespace {

thread_local bool g_insideAscsanRuntime = false;

class RuntimeGuard {
public:
    RuntimeGuard() : old_(g_insideAscsanRuntime) { g_insideAscsanRuntime = true; }

    ~RuntimeGuard() { g_insideAscsanRuntime = old_; }

private:
    bool old_;
};

} // namespace

AscsanStatus ApiCore::MemoryAlloc(const AscsanMemoryAllocDesc* desc, void** ptr, AscsanMemoryHandle* memory)
{
    if (desc == nullptr || ptr == nullptr || desc->bytes == 0) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (desc->version != ASCSAN_API_VERSION || desc->size < sizeof(AscsanMemoryAllocDesc)) {
        return ASCSAN_STATUS_ERROR_VERSION_MISMATCH;
    }

    RuntimeGuard guard;
    void* allocated = std::malloc(static_cast<size_t>(desc->bytes));
    if (allocated == nullptr) {
        return ASCSAN_STATUS_ERROR_OUT_OF_MEMORY;
    }
    if ((desc->flags & ASCSAN_MEMORY_FLAG_ZERO_INIT) != 0) {
        std::memset(allocated, 0, static_cast<size_t>(desc->bytes));
    }

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    MemoryRecord record{};
    record.info.version = ASCSAN_API_VERSION;
    record.info.size = sizeof(record.info);
    record.info.ptr = allocated;
    record.info.bytes = desc->bytes;
    record.info.space = desc->space;
    record.info.deviceId = desc->deviceId;
    record.info.memoryId = nextMemory_++;
    record.info.flags = desc->flags | ASCSAN_MEMORY_FLAG_INTERNAL;
    memories_[allocated] = record;
    *ptr = allocated;
    if (memory != nullptr) {
        *memory = record.info.memoryId;
    }
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::MemoryFree(void* ptr)
{
    if (ptr == nullptr) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    RuntimeGuard guard;
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = memories_.find(ptr);
    if (it == memories_.end()) {
        return ASCSAN_STATUS_ERROR_NOT_FOUND;
    }
    std::free(ptr);
    memories_.erase(it);
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::MemoryMemcpy(void* dst, uint64_t dstMax, const void* src, uint64_t bytes, AscsanMemcpyKind)
{
    if (dst == nullptr || src == nullptr || bytes > dstMax) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    RuntimeGuard guard;
    std::memcpy(dst, src, static_cast<size_t>(bytes));
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::MemoryMemset(void* dst, uint64_t dstMax, int32_t value, uint64_t bytes)
{
    if (dst == nullptr || bytes > dstMax) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    RuntimeGuard guard;
    std::memset(dst, value, static_cast<size_t>(bytes));
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::MemorySynchronizeStream(void*)
{
    RuntimeGuard guard;
    auto records = BuildSyntheticRecordsForSync();
    if (!records.empty()) {
        return IngestRawTraces(records.data(), records.size());
    }
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::MemoryGetInfo(const void* ptr, AscsanMemoryInfo* info) const
{
    if (ptr == nullptr || info == nullptr) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = memories_.find(const_cast<void*>(ptr));
    if (it == memories_.end()) {
        return ASCSAN_STATUS_ERROR_NOT_FOUND;
    }
    *info = it->second.info;
    return ASCSAN_STATUS_SUCCESS;
}

} // namespace ascsan
