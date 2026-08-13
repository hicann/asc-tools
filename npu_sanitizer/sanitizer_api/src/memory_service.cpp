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

namespace aclsan {
namespace {

thread_local bool g_insideAclsanRuntime = false;

class RuntimeGuard {
public:
    RuntimeGuard() : old_(g_insideAclsanRuntime) { g_insideAclsanRuntime = true; }

    ~RuntimeGuard() { g_insideAclsanRuntime = old_; }

private:
    bool old_;
};

} // namespace

AclsanStatus ApiCore::MemoryAlloc(const AclsanMemoryAllocDesc* desc, void** ptr, AclsanMemoryHandle* memory)
{
    if (desc == nullptr || ptr == nullptr || desc->bytes == 0) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (desc->version != ACLSAN_API_VERSION || desc->size < sizeof(AclsanMemoryAllocDesc)) {
        return ACLSAN_STATUS_ERROR_VERSION_MISMATCH;
    }

    RuntimeGuard guard;
    void* allocated = std::malloc(static_cast<size_t>(desc->bytes));
    if (allocated == nullptr) {
        return ACLSAN_STATUS_ERROR_OUT_OF_MEMORY;
    }
    if ((desc->flags & ACLSAN_MEMORY_FLAG_ZERO_INIT) != 0) {
        std::memset(allocated, 0, static_cast<size_t>(desc->bytes));
    }

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    MemoryRecord record{};
    record.info.version = ACLSAN_API_VERSION;
    record.info.size = sizeof(record.info);
    record.info.ptr = allocated;
    record.info.bytes = desc->bytes;
    record.info.space = desc->space;
    record.info.deviceId = desc->deviceId;
    record.info.memoryId = nextMemory_++;
    record.info.flags = desc->flags | ACLSAN_MEMORY_FLAG_INTERNAL;
    memories_[allocated] = record;
    *ptr = allocated;
    if (memory != nullptr) {
        *memory = record.info.memoryId;
    }
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::MemoryFree(void* ptr)
{
    if (ptr == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    RuntimeGuard guard;
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = memories_.find(ptr);
    if (it == memories_.end()) {
        return ACLSAN_STATUS_ERROR_NOT_FOUND;
    }
    std::free(ptr);
    memories_.erase(it);
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::MemoryMemcpy(void* dst, uint64_t dstMax, const void* src, uint64_t bytes, AclsanMemcpyKind)
{
    if (dst == nullptr || src == nullptr || bytes > dstMax) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    RuntimeGuard guard;
    std::memcpy(dst, src, static_cast<size_t>(bytes));
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::MemoryMemset(void* dst, uint64_t dstMax, int32_t value, uint64_t bytes)
{
    if (dst == nullptr || bytes > dstMax) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    RuntimeGuard guard;
    std::memset(dst, value, static_cast<size_t>(bytes));
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::MemorySynchronizeStream(void*)
{
    RuntimeGuard guard;
    auto records = BuildSyntheticRecordsForSync();
    if (!records.empty()) {
        return IngestRawTraces(records.data(), records.size());
    }
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::MemoryGetInfo(const void* ptr, AclsanMemoryInfo* info) const
{
    if (ptr == nullptr || info == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = memories_.find(const_cast<void*>(ptr));
    if (it == memories_.end()) {
        return ACLSAN_STATUS_ERROR_NOT_FOUND;
    }
    *info = it->second.info;
    return ACLSAN_STATUS_SUCCESS;
}

} // namespace aclsan
