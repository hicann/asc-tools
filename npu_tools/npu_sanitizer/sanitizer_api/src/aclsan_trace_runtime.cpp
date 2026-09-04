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

#include "device_instr/decoder_registry.h"
#include "device_instr/arch/dav_3510/register_state_manager.h"
#include "device_instr/common/instruction_id.h"
#include "device_runtime/device_binary_registry.h"
#include "internal/aclsan_device_data.h"
#include "internal/aclsan_device_data_log.h"
#include "internal/aclsan_dispatch.h"
#include "internal/aclsan_log.h"
#include "internal/aclsan_runtime_hook.h"
#include "internal/aclsan_trace_buffer.h"
#include "injection/injection_hook.h"
#include "dbi/trace_buffer_abi.h"

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
    uint32_t recordsPerCore = 0;
    uint32_t physicalCoreCount = 0;
    uint32_t deviceId = 0;
    const aclsan::DeviceInstructionDecoder* decoder = nullptr;
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

constexpr bool StrictModeEnabled() { return true; }

bool QueryPhysicalCoreCount(uint32_t deviceId, uint32_t& physicalCoreCount)
{
    const auto getDeviceInfo =
        GetOriginalRuntimeFunction<aclrtGetDeviceInfoFunc>(ACL_RT_API_aclrtGetDeviceInfo, "aclrtGetDeviceInfo");
    int64_t cubeCoreCount = 0;
    int64_t vectorCoreCount = 0;
    if (getDeviceInfo(deviceId, ACL_DEV_ATTR_CUBE_CORE_NUM, &cubeCoreCount) != ACL_SUCCESS ||
        getDeviceInfo(deviceId, ACL_DEV_ATTR_VECTOR_CORE_NUM, &vectorCoreCount) != ACL_SUCCESS) {
        return false;
    }
    physicalCoreCount = static_cast<uint32_t>(cubeCoreCount + vectorCoreCount);
    return aclsan::IsTracePhysicalCoreTopologyValid(physicalCoreCount);
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

// 为已经DBI插桩的kernel扩展HostArgs，在原始参数中插入隐藏的device trace buffer指针
aclError ExpandArguments(
    const void* hostArgs, size_t argsSize, const aclrtPlaceHolderInfo* placeholders, size_t placeholderCount,
    uint32_t traceArgumentOffset, PreparedTraceLaunch& prepared)
{
    if ((argsSize != 0 && hostArgs == nullptr) || (placeholderCount != 0 && placeholders == nullptr)) {
        return ACL_ERROR_INVALID_PARAM;
    }

    size_t insertionOffset = 0;
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

    if (insertionOffset > traceArgumentOffset ||
        traceArgumentOffset > std::numeric_limits<size_t>::max() - sizeof(void*)) {
        return ACL_ERROR_INVALID_PARAM;
    }

    const size_t paddingBytes = traceArgumentOffset - insertionOffset;
    if (std::max(argsSize, insertionOffset) > std::numeric_limits<size_t>::max() - paddingBytes - sizeof(void*)) {
        return ACL_ERROR_INVALID_PARAM;
    }
    const size_t expandedSize = std::max(argsSize, insertionOffset) + paddingBytes + sizeof(void*);
    prepared.arguments.assign(expandedSize, 0);
    if (insertionOffset != 0) {
        std::memcpy(prepared.arguments.data(), hostArgs, std::min(argsSize, insertionOffset));
    }
    if (argsSize > insertionOffset) {
        std::memcpy(
            prepared.arguments.data() + traceArgumentOffset + sizeof(void*),
            static_cast<const uint8_t*>(hostArgs) + insertionOffset, argsSize - insertionOffset);
    }

    if (placeholderCount != 0) {
        prepared.placeholders.assign(placeholders, placeholders + placeholderCount);
    }
    for (aclrtPlaceHolderInfo& placeholder : prepared.placeholders) {
        if (placeholder.dataOffset >= insertionOffset) {
            const size_t shiftBytes = paddingBytes + sizeof(void*);
            if (placeholder.dataOffset > std::numeric_limits<uint32_t>::max() - shiftBytes) {
                return ACL_ERROR_INVALID_PARAM;
            }
            placeholder.dataOffset += static_cast<uint32_t>(shiftBytes);
        }
    }
    return ACL_SUCCESS;
}

void StoreHiddenPointer(PreparedTraceLaunch& prepared, size_t traceArgumentOffset)
{
    std::memcpy(prepared.arguments.data() + traceArgumentOffset, &prepared.deviceBuffer, sizeof(prepared.deviceBuffer));
}

void ReleaseDeviceBuffer(void* buffer) noexcept
{
    if (buffer == nullptr) {
        return;
    }
    const auto freeFunction = GetOriginalRuntimeFunction<aclrtFreeFunc>(ACL_RT_API_aclrtFree, "aclrtFree");
    if (freeFunction(buffer) != ACL_SUCCESS) {
        ASC_SAN_ERROR("acl_san trace: failed to release launch-owned GM buffer %p", buffer);
    }
}

aclError ResolveLaunchContext(PreparedTraceLaunch& prepared) noexcept
{
    const auto getSocName =
        GetOriginalRuntimeFunction<aclrtGetSocNameFunc>(ACL_RT_API_aclrtGetSocName, "aclrtGetSocName");

    const std::optional<aclsan::SocVersion> socVersion = aclsan::ResolveSocVersion(getSocName());
    if (!socVersion.has_value()) {
        ASC_SAN_ERROR("acl_san trace: get unsupported soc name %s.", getSocName());
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }

    prepared.decoder = aclsan::FindDeviceInstructionDecoder(*socVersion);
    if (prepared.decoder == nullptr) {
        ASC_SAN_ERROR("acl_san trace: cannot select Device instruction decoder for soc %s.", getSocName());
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }

    const auto getDevice = GetOriginalRuntimeFunction<aclrtGetDeviceFunc>(ACL_RT_API_aclrtGetDevice, "aclrtGetDevice");
    int32_t deviceId = -1;
    if (getDevice(&deviceId) != ACL_SUCCESS || deviceId < 0) {
        ASC_SAN_ERROR("acl_san trace: cannot query current Device ID");
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    prepared.deviceId = static_cast<uint32_t>(deviceId);
    if (!QueryPhysicalCoreCount(prepared.deviceId, prepared.physicalCoreCount)) {
        ASC_SAN_ERROR(
            "acl_san trace: cannot query a supported physical core topology for Device %u", prepared.deviceId);
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    return ACL_SUCCESS;
}

} // namespace

void DispatchTraceRecords(
    const std::vector<ParsedTraceRecord>& records, const aclsan::DeviceInstructionDecoder& decoder) noexcept
{
    if (records.empty()) {
        return;
    }

    dav3510::Dav3510RegisterStateManager registerState(records.front().launchId);
    for (const ParsedTraceRecord& parsed : records) {
        if (!IsDefinedInstructionId(parsed.record.instrId)) {
            continue;
        }
        std::optional<aclsan::DecodedInstruction> decoded = decoder.decode(parsed.record);
        if (!decoded.has_value()) {
            ASC_SAN_ERROR(
                "acl_san trace: unsupported raw trace instrId=%u pc=0x%llx block=%u", parsed.record.instrId,
                static_cast<unsigned long long>(parsed.record.pc), parsed.blockId);
            continue;
        }

        const dav3510::Dav3510CoreKey key{parsed.blockType, parsed.blockId};
        bool stateInstruction = true;
        if (const auto* value = std::get_if<Mte2SourceParamField>(&decoded->params)) {
            registerState.Update(key, *value);
        } else if (const auto* value = std::get_if<NdDmaPadCountParamField>(&decoded->params)) {
            registerState.Update(key, *value);
        } else if (const auto* value = std::get_if<NdDmaLoopStrideParamField>(&decoded->params)) {
            registerState.Update(key, *value);
        } else if (const auto* value = std::get_if<Mte2NzParamField>(&decoded->params)) {
            registerState.Update(key, *value);
        } else if (const auto* value = std::get_if<Loop3ParamField>(&decoded->params)) {
            registerState.Update(key, *value);
        } else if (const auto* value = std::get_if<DmaLoopSizeParamField>(&decoded->params)) {
            registerState.Update(key, *value);
        } else if (const auto* value = std::get_if<DmaLoopStrideParamField>(&decoded->params)) {
            registerState.Update(key, *value);
        } else if (const auto* value = std::get_if<SetPaddingParamField>(&decoded->params)) {
            registerState.Update(key, *value);
        } else if (std::holds_alternative<SetL12DParamField>(decoded->params)) {
            // SET_L1_2D is a local L1 write instruction, not persistent register state and not a GM access.
        } else {
            stateInstruction = false;
        }
        if (stateInstruction) {
            LogRawRecord(parsed);
            LogParamField(decoded->params);
            continue;
        }

        const std::optional<dav3510::Dav3510CoreRegisterState> state = registerState.Get(key);
        MemoryRegisterState memoryState{};
        if (state.has_value()) {
            memoryState.mte2Source = state->mte2Source;
            memoryState.ndDmaPadCount = state->ndDmaPadCount;
            memoryState.ndDmaLoopStrides = state->ndDmaLoopStrides;
            memoryState.mte2Nz = state->mte2Nz;
            memoryState.loop3 = state->loop3;
            memoryState.dmaLoopSizes = state->dmaLoopSizes;
            memoryState.dmaLoopStrides = state->dmaLoopStrides;
        }

        const auto callbackData = TranslateDecodedTraceToCallbackData(parsed, *decoded, memoryState);
        if (!callbackData.has_value()) {
            continue;
        }
        if (const auto* memory = std::get_if<DeviceMemoryAccessDataList>(&*callbackData)) {
            for (const AclsanDeviceMemoryAccessData& access : *memory) {
                AclsanCallbackDispatcher::DispatchDeviceMemoryAccess(access);
            }
        } else if (const auto* sync = std::get_if<AclsanDeviceSyncData>(&*callbackData)) {
            AclsanCallbackDispatcher::DispatchDeviceSync(*sync);
        }
    }
}

void RecordTraceBinaryLoadFromData(
    aclrtBinHandle binary, bool instrumented, uint32_t traceArgumentOffset, const void* image,
    size_t imageBytes) noexcept
{
    if (binary == nullptr) {
        return;
    }
    try {
        if (!DeviceBinaries().RecordBinaryLoadFromData(
                reinterpret_cast<uintptr_t>(binary), instrumented, traceArgumentOffset, image, imageBytes)) {
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

aclError PrepareTraceLaunch(
    aclrtFuncHandle function, uint32_t blockCount, const void* hostArgs, size_t argsSize,
    const aclrtPlaceHolderInfo* placeholders, size_t placeholderCount, PreparedTraceLaunch& prepared) noexcept
{
    try {
        prepared = {};
        uint32_t traceArgumentOffset = 0;
        if (!DeviceBinaries().GetFunctionTraceArgumentOffset(
                reinterpret_cast<uintptr_t>(function), traceArgumentOffset)) {
            return ACL_SUCCESS;
        }
        prepared.instrumented = true;
        prepared.launchId = AllocateLaunchId();
        prepared.blockCount = blockCount;
        const aclError contextStatus = ResolveLaunchContext(prepared);
        if (contextStatus != ACL_SUCCESS) {
            return contextStatus;
        }

        const aclError expandStatus =
            ExpandArguments(hostArgs, argsSize, placeholders, placeholderCount, traceArgumentOffset, prepared);
        if (expandStatus != ACL_SUCCESS) {
            return expandStatus;
        }

        uint32_t capacity = aclsan::ASCSAN_TRACE_RECORDS_PER_CORE_DEFAULT;
        const char* capacityText = std::getenv("NPU_CHECK_TRACE_RECORDS_PER_BLOCK");
        const bool capacityValid = capacityText == nullptr || capacityText[0] == '\0' ||
                                   ParsePositiveUint32("NPU_CHECK_TRACE_RECORDS_PER_BLOCK", capacity);
        std::string error;
        if (!capacityValid ||
            !InitializeTraceBuffer(
                prepared.hostBuffer, prepared.physicalCoreCount, blockCount, capacity, prepared.launchId, error)) {
            ASC_SAN_ERROR(
                "acl_san trace: cannot initialize launch buffer: %s",
                capacityValid ? error.c_str() : "invalid NPU_CHECK_TRACE_RECORDS_PER_BLOCK");
            StoreHiddenPointer(prepared, traceArgumentOffset);
            return StrictModeEnabled() ? ACL_ERROR_FAILURE : ACL_SUCCESS;
        }
        prepared.recordsPerCore = capacity;

        const auto mallocFunction = GetOriginalRuntimeFunction<aclrtMallocFunc>(ACL_RT_API_aclrtMalloc, "aclrtMalloc");
        const auto memcpyFunction = GetOriginalRuntimeFunction<aclrtMemcpyFunc>(ACL_RT_API_aclrtMemcpy, "aclrtMemcpy");
        aclError status = mallocFunction(&prepared.deviceBuffer, prepared.hostBuffer.size(), ACL_MEM_MALLOC_HUGE_FIRST);
        if (status == ACL_SUCCESS) {
            status = memcpyFunction(
                prepared.deviceBuffer, prepared.hostBuffer.size(), prepared.hostBuffer.data(),
                prepared.hostBuffer.size(), ACL_MEMCPY_HOST_TO_DEVICE);
        }
        if (status != ACL_SUCCESS) {
            ASC_SAN_ERROR("acl_san trace: cannot allocate or initialize launch GM buffer, status=%d", status);
            ReleaseDeviceBuffer(prepared.deviceBuffer);
            prepared.deviceBuffer = nullptr;
            StoreHiddenPointer(prepared, traceArgumentOffset);
            return StrictModeEnabled() ? status : ACL_SUCCESS;
        }

        StoreHiddenPointer(prepared, traceArgumentOffset);
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
            prepared.recordsPerCore,
            prepared.physicalCoreCount,
            prepared.deviceId,
            prepared.decoder,
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

    const auto memcpyFunction = GetOriginalRuntimeFunction<aclrtMemcpyFunc>(ACL_RT_API_aclrtMemcpy, "aclrtMemcpy");
    for (PendingTrace& pending : completed) {
        try {
            if (memcpyFunction(
                    pending.hostBuffer.data(), pending.hostBuffer.size(), pending.deviceBuffer,
                    pending.hostBuffer.size(), ACL_MEMCPY_DEVICE_TO_HOST) != ACL_SUCCESS) {
                ASC_SAN_ERROR(
                    "acl_san trace: D2H failed for launch=%llu", static_cast<unsigned long long>(pending.launchId));
                ReleaseDeviceBuffer(pending.deviceBuffer);
                continue;
            }

            TraceBufferParseResult parsed = ParseTraceBuffer(
                pending.hostBuffer.data(), pending.hostBuffer.size(), pending.physicalCoreCount, pending.blockCount,
                pending.recordsPerCore, pending.launchId, pending.deviceId);
            if (!parsed.ok) {
                ASC_SAN_ERROR(
                    "acl_san trace: malformed buffer for launch=%llu: %s",
                    static_cast<unsigned long long>(pending.launchId), parsed.error.c_str());
            } else {
                if (!parsed.records.empty() && pending.decoder != nullptr) {
                    DispatchTraceRecords(parsed.records, *pending.decoder);
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
