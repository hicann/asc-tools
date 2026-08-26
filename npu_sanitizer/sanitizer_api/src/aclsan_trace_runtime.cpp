/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "internal/aclsan_trace_runtime.h"

#include "internal/aclsan_dispatch_cb.h"
#include "internal/aclsan_internal.h"
#include "internal/aclsan_log.h"
#include "internal/aclsan_trace_buffer.h"
#include "npu_compute/injection_hook.h"
#include "device_runtime/device_binary_registry.h"
#include "trace_buffer_abi.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <utility>
#include <variant>

namespace aclsan {
namespace {

struct PendingTrace {
    uint64_t launchId = 0;
    aclrtFuncHandle function = nullptr;
    aclrtStream stream = nullptr;
    void* deviceBuffer = nullptr;
    uint32_t blockCount = 0;
    uint32_t recordsPerBlock = 0;
    std::vector<uint8_t> hostBuffer;
};

struct TraceRuntimeState {
    std::mutex mutex;
    uint64_t nextLaunchId = 1;
    std::vector<PendingTrace> pending;
};

TraceRuntimeState& State()
{
    static TraceRuntimeState state;
    return state;
}

device_runtime::DeviceBinaryRegistry& DeviceBinaries()
{
    static device_runtime::DeviceBinaryRegistry binaries;
    return binaries;
}

template <typename Function>
Function Original(aclrtApiId id)
{
    return reinterpret_cast<Function>(acltoolGetOriginalRuntimeApi(id));
}

bool ParsePositiveUint32(const char* name, uint32_t& value)
{
    const char* text = std::getenv(name);
    if (text == nullptr || text[0] == '\0') {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed == 0 || parsed > UINT32_MAX) {
        return false;
    }
    value = static_cast<uint32_t>(parsed);
    return true;
}

bool StrictModeEnabled()
{
    const char* strict = std::getenv("NPU_CHECK_DBI_STRICT");
    return strict != nullptr && std::strcmp(strict, "1") == 0;
}

bool IsInstrumented(aclrtFuncHandle function)
{
    return DeviceBinaries().IsFunctionInstrumented(reinterpret_cast<uintptr_t>(function));
}

uint64_t AllocateLaunchId()
{
    TraceRuntimeState& state = State();
    std::lock_guard<std::mutex> lock(state.mutex);
    uint64_t id = state.nextLaunchId++;
    if (id == 0) {
        id = state.nextLaunchId++;
    }
    return id;
}

aclError ExpandArguments(
    const void* hostArgs, size_t argsSize, const aclrtPlaceHolderInfo* placeholders, size_t placeholderCount,
    PreparedTraceLaunch& prepared, size_t& insertionOffset)
{
    if ((argsSize != 0 && hostArgs == nullptr) || (placeholderCount != 0 && placeholders == nullptr)) {
        return ACL_ERROR_INVALID_PARAM;
    }

    if (placeholderCount != 0) {
        const uint32_t lastAddressOffset = placeholders[placeholderCount - 1].addrOffset;
        if (lastAddressOffset > std::numeric_limits<uint32_t>::max() - sizeof(void*)) {
            return ACL_ERROR_INVALID_PARAM;
        }
        insertionOffset = static_cast<size_t>(lastAddressOffset) + sizeof(void*);
        if (insertionOffset > argsSize) {
            return ACL_ERROR_INVALID_PARAM;
        }
    } else {
        if (argsSize > std::numeric_limits<size_t>::max() - 7U) {
            return ACL_ERROR_INVALID_PARAM;
        }
        insertionOffset = (argsSize + 7U) & ~static_cast<size_t>(7U);
    }

    uint32_t configuredOffset = 0;
    if (!ParsePositiveUint32("NPU_CHECK_DBI_ARG_SIZE", configuredOffset) || configuredOffset != insertionOffset ||
        insertionOffset > std::numeric_limits<size_t>::max() - sizeof(void*)) {
        ASC_SAN_ERROR(
            "acl_san trace: HostArgs insertion offset %zu does not match NPU_CHECK_DBI_ARG_SIZE", insertionOffset);
        return ACL_ERROR_INVALID_PARAM;
    }

    const size_t expandedSize = std::max(argsSize, insertionOffset) + sizeof(void*);
    prepared.arguments.assign(expandedSize, 0);
    if (insertionOffset != 0) {
        std::memcpy(prepared.arguments.data(), hostArgs, std::min(argsSize, insertionOffset));
    }
    if (argsSize > insertionOffset) {
        std::memcpy(
            prepared.arguments.data() + insertionOffset + sizeof(void*),
            static_cast<const uint8_t*>(hostArgs) + insertionOffset, argsSize - insertionOffset);
    }

    if (placeholderCount != 0) {
        prepared.placeholders.assign(placeholders, placeholders + placeholderCount);
    }
    for (aclrtPlaceHolderInfo& placeholder : prepared.placeholders) {
        if (placeholder.dataOffset >= insertionOffset) {
            if (placeholder.dataOffset > std::numeric_limits<uint32_t>::max() - sizeof(void*)) {
                return ACL_ERROR_INVALID_PARAM;
            }
            placeholder.dataOffset += sizeof(void*);
        }
    }
    return ACL_SUCCESS;
}

void StoreHiddenPointer(PreparedTraceLaunch& prepared, size_t insertionOffset)
{
    std::memcpy(prepared.arguments.data() + insertionOffset, &prepared.deviceBuffer, sizeof(prepared.deviceBuffer));
}

void ReleaseDeviceBuffer(void* buffer) noexcept
{
    if (buffer == nullptr) {
        return;
    }
    const auto freeFunction = Original<aclrtFreeFunc>(ACL_RT_API_aclrtFree);
    if (freeFunction == nullptr || freeFunction(buffer) != ACL_SUCCESS) {
        ASC_SAN_ERROR("acl_san trace: failed to release launch-owned GM buffer %p", buffer);
    }
}

void DispatchTraceRecords(const std::vector<sanitizer::AscsanRawTraceRecord>& records) noexcept
{
    uint64_t serialNo = 0;
    for (const sanitizer::AscsanRawTraceRecord& record : records) {
        const TraceCallbackContext context{DecodeRawTraceTransferBytes(record), serialNo + 1, serialNo, record.blockId};
        const auto callbackData = TranslateRawTraceToCallbackData(record, context);
        if (!callbackData.has_value()) {
            ASC_SAN_ERROR(
                "acl_san trace: unsupported raw trace instrId=%llu pc=0x%llx block=%u",
                static_cast<unsigned long long>(record.instrId), static_cast<unsigned long long>(record.pc),
                record.blockId);
        } else if (const auto* memory = std::get_if<DeviceMemoryAccessDataArray>(&*callbackData)) {
            AclsanCallbackDispatcher::DispatchDeviceMemoryAccess(*memory);
        } else if (const auto* sync = std::get_if<AclsanDeviceSyncData>(&*callbackData)) {
            AclsanCallbackDispatcher::DispatchDeviceSync(*sync);
        }
        ++serialNo;
    }
}

} // namespace

void RecordTraceBinaryLoadFromData(
    aclrtBinHandle binary, bool instrumented, const void* image, size_t imageBytes) noexcept
{
    if (binary == nullptr) {
        return;
    }
    try {
        if (!DeviceBinaries().RecordBinaryLoadFromData(
                reinterpret_cast<uintptr_t>(binary), instrumented, image, imageBytes)) {
            ASC_SAN_ERROR("acl_san trace: failed to preserve device source for binary %p", binary);
        }
    } catch (...) {
        ASC_SAN_ERROR("acl_san trace: failed to record binary load for %p", binary);
    }
}

void RecordTraceBinaryUnload(aclrtBinHandle binary) noexcept
{
    DeviceBinaries().RecordBinaryUnload(reinterpret_cast<uintptr_t>(binary));
}

void RecordTraceBinaryFunctionLookup(aclrtBinHandle binary, aclrtFuncHandle function) noexcept
{
    if (binary == nullptr || function == nullptr) {
        return;
    }
    try {
        DeviceBinaries().RecordBinaryFunctionLookup(
            reinterpret_cast<uintptr_t>(binary), reinterpret_cast<uintptr_t>(function));
    } catch (...) {
        ASC_SAN_ERROR("acl_san trace: failed to record function %p for binary %p", function, binary);
    }
}

void RecordTraceFunctionLookup(aclrtFuncHandle function) noexcept
{
    if (function == nullptr) {
        return;
    }
    try {
        DeviceBinaries().RecordLatestBinaryFunctionLookup(reinterpret_cast<uintptr_t>(function));
    } catch (...) {
        ASC_SAN_ERROR("acl_san trace: failed to record function lookup for %p", function);
    }
}

void MarkTraceFunctionInstrumented(aclrtFuncHandle function) noexcept
{
    if (function == nullptr) {
        return;
    }
    try {
        DeviceBinaries().MarkFunctionInstrumented(reinterpret_cast<uintptr_t>(function));
    } catch (...) {
        ASC_SAN_ERROR("acl_san trace: failed to mark function %p as instrumented", function);
    }
}

aclError PrepareTraceLaunch(
    aclrtFuncHandle function, uint32_t blockCount, const void* hostArgs, size_t argsSize,
    const aclrtPlaceHolderInfo* placeholders, size_t placeholderCount, PreparedTraceLaunch& prepared) noexcept
{
    try {
        prepared = {};
        if (!IsInstrumented(function)) {
            return ACL_SUCCESS;
        }
        prepared.instrumented = true;
        prepared.launchId = AllocateLaunchId();
        prepared.blockCount = blockCount;

        size_t insertionOffset = 0;
        const aclError expandStatus =
            ExpandArguments(hostArgs, argsSize, placeholders, placeholderCount, prepared, insertionOffset);
        if (expandStatus != ACL_SUCCESS) {
            return expandStatus;
        }

        uint32_t capacity = aclsan::ASCSAN_TRACE_RECORDS_PER_BLOCK_DEFAULT;
        const char* capacityText = std::getenv("NPU_CHECK_TRACE_RECORDS_PER_BLOCK");
        const bool capacityValid = capacityText == nullptr || capacityText[0] == '\0' ||
                                   ParsePositiveUint32("NPU_CHECK_TRACE_RECORDS_PER_BLOCK", capacity);
        std::string error;
        if (!capacityValid ||
            !InitializeTraceBuffer(prepared.hostBuffer, blockCount, capacity, prepared.launchId, error)) {
            ASC_SAN_ERROR(
                "acl_san trace: cannot initialize launch buffer: %s",
                capacityValid ? error.c_str() : "invalid NPU_CHECK_TRACE_RECORDS_PER_BLOCK");
            StoreHiddenPointer(prepared, insertionOffset);
            return StrictModeEnabled() ? ACL_ERROR_FAILURE : ACL_SUCCESS;
        }
        prepared.recordsPerBlock = capacity;

        const auto mallocFunction = Original<aclrtMallocFunc>(ACL_RT_API_aclrtMalloc);
        const auto memcpyFunction = Original<aclrtMemcpyFunc>(ACL_RT_API_aclrtMemcpy);
        aclError status =
            mallocFunction == nullptr ?
                ACL_ERROR_UNINITIALIZE :
                mallocFunction(&prepared.deviceBuffer, prepared.hostBuffer.size(), ACL_MEM_MALLOC_HUGE_FIRST);
        if (status == ACL_SUCCESS && memcpyFunction != nullptr) {
            status = memcpyFunction(
                prepared.deviceBuffer, prepared.hostBuffer.size(), prepared.hostBuffer.data(),
                prepared.hostBuffer.size(), ACL_MEMCPY_HOST_TO_DEVICE);
        } else if (status == ACL_SUCCESS) {
            status = ACL_ERROR_UNINITIALIZE;
        }
        if (status != ACL_SUCCESS) {
            ASC_SAN_ERROR("acl_san trace: cannot allocate or initialize launch GM buffer, status=%d", status);
            ReleaseDeviceBuffer(prepared.deviceBuffer);
            prepared.deviceBuffer = nullptr;
            StoreHiddenPointer(prepared, insertionOffset);
            return StrictModeEnabled() ? status : ACL_SUCCESS;
        }

        StoreHiddenPointer(prepared, insertionOffset);
        return ACL_SUCCESS;
    } catch (const std::bad_alloc&) {
        ReleaseDeviceBuffer(prepared.deviceBuffer);
        prepared.deviceBuffer = nullptr;
        ASC_SAN_ERROR("acl_san trace: out of memory while preparing launch");
        return ACL_ERROR_BAD_ALLOC;
    } catch (...) {
        ReleaseDeviceBuffer(prepared.deviceBuffer);
        prepared.deviceBuffer = nullptr;
        ASC_SAN_ERROR("acl_san trace: unexpected failure while preparing launch");
        return ACL_ERROR_FAILURE;
    }
}

void CompleteTraceLaunch(
    PreparedTraceLaunch&& prepared, aclrtFuncHandle function, aclrtStream stream, aclError launchResult) noexcept
{
    if (!prepared.instrumented || prepared.deviceBuffer == nullptr) {
        return;
    }
    if (launchResult != ACL_SUCCESS) {
        ReleaseDeviceBuffer(prepared.deviceBuffer);
        return;
    }

    try {
        PendingTrace pending{
            prepared.launchId,
            function,
            stream,
            prepared.deviceBuffer,
            prepared.blockCount,
            prepared.recordsPerBlock,
            std::move(prepared.hostBuffer)};
        TraceRuntimeState& state = State();
        std::lock_guard<std::mutex> lock(state.mutex);
        state.pending.push_back(std::move(pending));
    } catch (...) {
        ASC_SAN_ERROR(
            "acl_san trace: failed to retain launch=%llu", static_cast<unsigned long long>(prepared.launchId));
        ReleaseDeviceBuffer(prepared.deviceBuffer);
    }
}

void CollectTraceStream(aclrtStream stream) noexcept
{
    std::vector<PendingTrace> completed;
    try {
        {
            TraceRuntimeState& state = State();
            std::lock_guard<std::mutex> lock(state.mutex);
            for (auto it = state.pending.begin(); it != state.pending.end();) {
                if (it->stream == stream) {
                    completed.push_back(std::move(*it));
                    it = state.pending.erase(it);
                } else {
                    ++it;
                }
            }
        }
    } catch (...) {
        ASC_SAN_ERROR("acl_san trace: failed to detach completed launches for stream=%p", stream);
        return;
    }

    const auto memcpyFunction = Original<aclrtMemcpyFunc>(ACL_RT_API_aclrtMemcpy);
    for (PendingTrace& pending : completed) {
        try {
            if (memcpyFunction == nullptr ||
                memcpyFunction(
                    pending.hostBuffer.data(), pending.hostBuffer.size(), pending.deviceBuffer,
                    pending.hostBuffer.size(), ACL_MEMCPY_DEVICE_TO_HOST) != ACL_SUCCESS) {
                ASC_SAN_ERROR(
                    "acl_san trace: D2H failed for launch=%llu", static_cast<unsigned long long>(pending.launchId));
                ReleaseDeviceBuffer(pending.deviceBuffer);
                continue;
            }

            TraceBufferParseResult parsed = ParseTraceBuffer(
                pending.hostBuffer.data(), pending.hostBuffer.size(), pending.blockCount, pending.recordsPerBlock,
                pending.launchId);
            if (!parsed.ok) {
                ASC_SAN_ERROR(
                    "acl_san trace: malformed buffer for launch=%llu: %s",
                    static_cast<unsigned long long>(pending.launchId), parsed.error.c_str());
            } else {
                if (!parsed.records.empty()) {
                    DispatchTraceRecords(parsed.records);
                }
                if (parsed.overflowCount != 0) {
                    ASC_SAN_ERROR(
                        "acl_san trace: launch=%llu dropped %llu records",
                        static_cast<unsigned long long>(pending.launchId),
                        static_cast<unsigned long long>(parsed.overflowCount));
                }
            }
        } catch (...) {
            ASC_SAN_ERROR(
                "acl_san trace: unexpected D2H processing failure for launch=%llu",
                static_cast<unsigned long long>(pending.launchId));
        }
        ReleaseDeviceBuffer(pending.deviceBuffer);
    }
}

void ResetTraceRuntimeState() noexcept
{
    {
        TraceRuntimeState& state = State();
        std::lock_guard<std::mutex> lock(state.mutex);
        state.pending.clear();
    }
    DeviceBinaries().Reset();
}

device_runtime::CallStackResult ResolveTraceDeviceCallStack(uint64_t pc) noexcept
{
    return DeviceBinaries().ResolveCallStack(pc);
}

} // namespace aclsan

#if defined(ACLSAN_ENABLE_TEST_API)
extern "C" ACLSAN_EXPORT void aclsanTestMarkInstrumentedFunction(aclrtFuncHandle function)
{
    aclsan::MarkTraceFunctionInstrumented(function);
}

extern "C" ACLSAN_EXPORT void aclsanTestRecordDeviceBinarySource(
    const void* binary, const void* image, size_t imageBytes)
{
    aclsan::RecordTraceBinaryLoadFromData(
        reinterpret_cast<aclrtBinHandle>(const_cast<void*>(binary)), true, image, imageBytes);
}

extern "C" ACLSAN_EXPORT void aclsanTestResetTraceRuntimeState() { aclsan::ResetTraceRuntimeState(); }
#endif
