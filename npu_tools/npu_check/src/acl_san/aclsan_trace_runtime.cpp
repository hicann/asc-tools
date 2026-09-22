/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "aclsan_trace_runtime.h"

#include "device_instr/decoder_registry.h"
#include "device_instr/arch/dav_3510/register_state_manager.h"
#include "device_instr/common/instruction_id.h"
#include "device_runtime/device_binary_registry.h"
#include "aclsan_device_data.h"
#include "aclsan_device_data_log.h"
#include "aclsan_dispatch.h"
#include "npu_tool_log.h"
#include "aclsan_runtime_hook.h"
#include "aclsan_trace_buffer.h"
#include "injection/injection_hook.h"
#include "dbi/trace_buffer_abi.h"

#include <algorithm>
#include <climits>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <optional>
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
    uint32_t physicalCoreCount = 0;
    uint32_t deviceId = 0;
    const aclsan::DeviceInstructionDecoder* decoder = nullptr;
    std::vector<uint8_t> hostBuffer;
    std::shared_ptr<void> binaryLease;
    std::vector<uint8_t> arguments;
    std::vector<aclrtPlaceHolderInfo> placeholders;
    std::vector<PreparedTraceLaunch::HostInput> hostInputs;
    std::vector<uint32_t> dispatchedRecordCounts;
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
    ASCTOOL_DEBUG(
        "[trace] Device %u core count: cube=%lld vector=%lld", deviceId, static_cast<long long>(cubeCoreCount),
        static_cast<long long>(vectorCoreCount));
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
        uint32_t lastAddressOffset = 0;
        for (size_t i = 0; i < placeholderCount; ++i) {
            lastAddressOffset = std::max(lastAddressOffset, placeholders[i].addrOffset);
        }
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

void StoreHiddenPointer(PreparedTraceLaunch& prepared, TraceArgumentMode argumentMode)
{
    if (argumentMode == TraceArgumentMode::HOST_ARGS) {
        std::memcpy(
            prepared.arguments.data() + prepared.traceArgumentOffset, &prepared.deviceBuffer,
            sizeof(prepared.deviceBuffer));
    }
}

void ReleaseDeviceBuffer(void* buffer) noexcept
{
    if (buffer == nullptr) {
        return;
    }
    const auto freeFunction = GetOriginalRuntimeFunction<aclrtFreeFunc>(ACL_RT_API_aclrtFree, "aclrtFree");
    if (freeFunction(buffer) != ACL_SUCCESS) {
        ASCTOOL_ERROR("acl_san trace: failed to release launch-owned GM buffer %p", buffer);
    }
}

void ReleaseHostInputs(std::vector<PreparedTraceLaunch::HostInput>& inputs) noexcept
{
    for (const auto& input : inputs) {
        ReleaseDeviceBuffer(input.address);
    }
    inputs.clear();
}

bool DispatchTraceSnapshot(PendingTrace& pending, bool executionComplete, TraceCollectionResult& collection)
{
    const auto memcpyFunction = GetOriginalRuntimeFunction<aclrtMemcpyFunc>(ACL_RT_API_aclrtMemcpy, "aclrtMemcpy");
    if (memcpyFunction(
            pending.hostBuffer.data(), pending.hostBuffer.size(), pending.deviceBuffer, pending.hostBuffer.size(),
            ACL_MEMCPY_DEVICE_TO_HOST) != ACL_SUCCESS) {
        ASCTOOL_ERROR("acl_san trace: D2H failed for launch=%llu", static_cast<unsigned long long>(pending.launchId));
        return false;
    }

    TraceBufferParseResult parsed = ParseTraceBuffer(
        pending.hostBuffer.data(), pending.hostBuffer.size(), pending.physicalCoreCount, pending.blockCount,
        pending.launchId, pending.deviceId);
    if (!parsed.ok) {
        ASCTOOL_ERROR(
            "acl_san trace: malformed buffer for launch=%llu: %s", static_cast<unsigned long long>(pending.launchId),
            parsed.error.c_str());
        return false;
    }

    collection.recordCount += parsed.records.size();
    collection.droppedRecordCount += parsed.overflowCount;
    if (executionComplete && !parsed.records.empty() && parsed.overflowCount == 0) {
        ++collection.completeLaunchCount;
    }

    if (pending.dispatchedRecordCounts.size() != pending.physicalCoreCount) {
        pending.dispatchedRecordCounts.assign(pending.physicalCoreCount, 0U);
    }
    std::vector<uint32_t> observedRecordCounts(pending.physicalCoreCount, 0U);
    std::vector<ParsedTraceRecord> newRecords;
    try {
        newRecords.reserve(parsed.records.size());
        for (const ParsedTraceRecord& record : parsed.records) {
            if (record.phyCoreId >= observedRecordCounts.size()) {
                ASCTOOL_ERROR(
                    "acl_san trace: launch=%llu produced out-of-range physical core ID %u",
                    static_cast<unsigned long long>(pending.launchId), record.phyCoreId);
                return false;
            }
            const uint32_t recordIndex = observedRecordCounts[record.phyCoreId]++;
            if (recordIndex >= pending.dispatchedRecordCounts[record.phyCoreId]) {
                newRecords.push_back(record);
            }
        }
    } catch (...) {
        ASCTOOL_ERROR(
            "acl_san trace: failed to stage records for launch=%llu",
            static_cast<unsigned long long>(pending.launchId));
        return false;
    }
    for (uint32_t phyCoreId = 0; phyCoreId < pending.physicalCoreCount; ++phyCoreId) {
        if (observedRecordCounts[phyCoreId] < pending.dispatchedRecordCounts[phyCoreId]) {
            ASCTOOL_ERROR(
                "acl_san trace: launch=%llu physical core %u record count regressed from %u to %u",
                static_cast<unsigned long long>(pending.launchId), phyCoreId, pending.dispatchedRecordCounts[phyCoreId],
                observedRecordCounts[phyCoreId]);
            return false;
        }
    }
    pending.dispatchedRecordCounts = std::move(observedRecordCounts);

    ASCTOOL_INFO(
        "acl_san trace: launch=%llu status=%s records=%zu newRecords=%zu dropped=%llu physicalCores=%u blocks=%u",
        static_cast<unsigned long long>(pending.launchId), executionComplete ? "complete" : "snapshot",
        parsed.records.size(), newRecords.size(), static_cast<unsigned long long>(parsed.overflowCount),
        pending.physicalCoreCount, pending.blockCount);
    if (!newRecords.empty() && pending.decoder != nullptr) {
        DispatchTraceRecords(newRecords, *pending.decoder, &pending.hostInputs);
    }
    if (parsed.overflowCount != 0) {
        ASCTOOL_ERROR(
            "acl_san trace: launch=%llu dropped %llu records", static_cast<unsigned long long>(pending.launchId),
            static_cast<unsigned long long>(parsed.overflowCount));
    }
    return true;
}

aclError ResolveLaunchContext(PreparedTraceLaunch& prepared) noexcept
{
    const auto getSocName =
        GetOriginalRuntimeFunction<aclrtGetSocNameFunc>(ACL_RT_API_aclrtGetSocName, "aclrtGetSocName");

    const char* socName = getSocName();
    const std::optional<aclsan::SocVersion> socVersion = aclsan::ResolveSocVersion(socName);
    if (!socVersion.has_value()) {
        ASCTOOL_ERROR("acl_san trace: get unsupported soc name %s.", socName != nullptr ? socName : "<null>");
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }

    prepared.decoder = aclsan::FindDeviceInstructionDecoder(*socVersion);
    if (prepared.decoder == nullptr) {
        ASCTOOL_ERROR("acl_san trace: cannot select Device instruction decoder for soc %s.", socName);
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }

    const auto getDevice = GetOriginalRuntimeFunction<aclrtGetDeviceFunc>(ACL_RT_API_aclrtGetDevice, "aclrtGetDevice");
    int32_t deviceId = -1;
    if (getDevice(&deviceId) != ACL_SUCCESS || deviceId < 0) {
        ASCTOOL_ERROR("acl_san trace: cannot query current Device ID");
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    prepared.deviceId = static_cast<uint32_t>(deviceId);
    if (!QueryPhysicalCoreCount(prepared.deviceId, prepared.physicalCoreCount)) {
        ASCTOOL_ERROR(
            "acl_san trace: cannot query a supported physical core topology for Device %u", prepared.deviceId);
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    return ACL_SUCCESS;
}

bool AddLayoutAxis(uint64_t count, int64_t stride, __int128& minimum, __int128& maximum) noexcept
{
    if (count == 0) {
        return false;
    }
    __int128 extent = 0;
    if (__builtin_mul_overflow(static_cast<__int128>(count - 1), static_cast<__int128>(stride), &extent)) {
        return false;
    }
    __int128* bound = extent < 0 ? &minimum : &maximum;
    return !__builtin_add_overflow(*bound, extent, bound);
}

bool AccessBounds(const AclsanDeviceMemoryAccessData& access, __int128& begin, __int128& end) noexcept
{
    __int128 minimum = 0;
    __int128 maximum = 0;
    uint64_t elementBytes = 0;
    switch (access.layoutKind) {
        case ACLSAN_MEM_LAYOUT_SCALAR:
            elementBytes = access.layout.scalar.bytes;
            break;
        case ACLSAN_MEM_LAYOUT_RANGE:
            elementBytes = access.layout.range.bytes;
            break;
        case ACLSAN_MEM_LAYOUT_BLOCK_REPEAT:
            elementBytes = access.layout.blockRepeat.blockSize;
            if (!AddLayoutAxis(
                    access.layout.blockRepeat.blockNum, access.layout.blockRepeat.blockStride, minimum, maximum) ||
                !AddLayoutAxis(
                    access.layout.blockRepeat.repeatTimes, access.layout.blockRepeat.repeatStride, minimum, maximum)) {
                return false;
            }
            break;
        case ACLSAN_MEM_LAYOUT_ND_AFFINE:
            elementBytes = access.layout.ndAffine.elementBytes;
            if (access.layout.ndAffine.rank == 0 || access.layout.ndAffine.rank > 5) {
                return false;
            }
            for (uint32_t axis = 0; axis < access.layout.ndAffine.rank; ++axis) {
                if (!AddLayoutAxis(
                        access.layout.ndAffine.dims[axis], access.layout.ndAffine.strides[axis], minimum, maximum)) {
                    return false;
                }
            }
            break;
        default:
            return false;
    }
    if (elementBytes == 0 || __builtin_add_overflow(static_cast<__int128>(access.address), minimum, &begin) ||
        __builtin_add_overflow(static_cast<__int128>(access.address), maximum, &end) ||
        __builtin_add_overflow(end, static_cast<__int128>(elementBytes), &end)) {
        return false;
    }
    const __int128 addressLimit = static_cast<__int128>(1) << 64U;
    return begin >= 0 && end >= begin && end <= addressLimit;
}

bool IsInternalHostInputAccess(
    const AclsanDeviceMemoryAccessData& access,
    const std::vector<PreparedTraceLaunch::HostInput>* internalInputs) noexcept
{
    if (internalInputs == nullptr || internalInputs->empty() || access.memorySpace != ACLSAN_DEVICE_MEMORY_SPACE_GM) {
        return false;
    }
    __int128 accessBegin = 0;
    __int128 accessEnd = 0;
    if (!AccessBounds(access, accessBegin, accessEnd)) {
        return false;
    }
    for (const auto& input : *internalInputs) {
        const __int128 inputBegin = reinterpret_cast<uintptr_t>(input.address);
        __int128 inputEnd = 0;
        if (input.address != nullptr && input.bytes != 0 &&
            !__builtin_add_overflow(inputBegin, static_cast<__int128>(input.bytes), &inputEnd) &&
            accessBegin >= inputBegin && accessEnd <= inputEnd) {
            return true;
        }
    }
    return false;
}

} // namespace

void DispatchTraceRecords(
    const std::vector<ParsedTraceRecord>& records, const aclsan::DeviceInstructionDecoder& decoder,
    const std::vector<PreparedTraceLaunch::HostInput>* internalInputs) noexcept
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
            ASCTOOL_ERROR(
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
                if (IsInternalHostInputAccess(access, internalInputs)) {
                    continue;
                }
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
            ASCTOOL_ERROR("acl_san trace: failed to preserve device source for binary %p", binary);
        }
    } catch (...) {
        ASCTOOL_ERROR("acl_san trace: failed to record binary load for %p", binary);
    }
}

void RecordTraceBinaryUnload(aclrtBinHandle binary) noexcept
{
    DeviceBinaries().RecordBinaryUnload(reinterpret_cast<uintptr_t>(binary));
}

void RecordTraceBinaryFunctionLookup(aclrtBinHandle binary, aclrtFuncHandle function, const char* functionName) noexcept
{
    if (binary == nullptr || function == nullptr) {
        return;
    }
    try {
        DeviceBinaries().RecordBinaryFunctionLookup(
            reinterpret_cast<uintptr_t>(binary), reinterpret_cast<uintptr_t>(function), functionName);
    } catch (...) {
        ASCTOOL_ERROR("acl_san trace: failed to record function %p for binary %p", function, binary);
    }
}

bool GetTraceFunctionName(aclrtFuncHandle function, std::string& functionName) noexcept
{
    return DeviceBinaries().GetFunctionName(reinterpret_cast<uintptr_t>(function), functionName);
}

void RecordTraceFunctionLookup(aclrtFuncHandle function) noexcept
{
    if (function == nullptr) {
        return;
    }
    try {
        DeviceBinaries().RecordLatestBinaryFunctionLookup(reinterpret_cast<uintptr_t>(function));
    } catch (...) {
        ASCTOOL_ERROR("acl_san trace: failed to record function lookup for %p", function);
    }
}

aclError PrepareTraceLaunch(
    aclrtFuncHandle function, uint32_t blockCount, const void* hostArgs, size_t argsSize,
    const aclrtPlaceHolderInfo* placeholders, size_t placeholderCount, TraceArgumentMode argumentMode,
    PreparedTraceLaunch& prepared) noexcept
{
    try {
        prepared = {};
        prepared.launchId = AllocateLaunchId();
        if (!DeviceBinaries().GetFunctionTraceArgumentOffset(
                reinterpret_cast<uintptr_t>(function), prepared.traceArgumentOffset)) {
            return ACL_SUCCESS;
        }
        prepared.instrumented = true;
        prepared.blockCount = blockCount;
        ACLSAN_RETURN_IF_ACL_ERROR(ResolveLaunchContext(prepared), "Failed to resolve trace launch context");

        if (argumentMode == TraceArgumentMode::HOST_ARGS) {
            ACLSAN_RETURN_IF_ACL_ERROR(
                ExpandArguments(
                    hostArgs, argsSize, placeholders, placeholderCount, prepared.traceArgumentOffset, prepared),
                "Failed to expand trace launch arguments");
        }

        std::string error;
        if (!InitializeTraceBuffer(
                prepared.hostBuffer, prepared.physicalCoreCount, blockCount, prepared.launchId, error)) {
            ASCTOOL_ERROR("acl_san trace: cannot initialize launch buffer: %s", error.c_str());
            StoreHiddenPointer(prepared, argumentMode);
            return StrictModeEnabled() ? ACL_ERROR_FAILURE : ACL_SUCCESS;
        }
        const auto mallocFunction = GetOriginalRuntimeFunction<aclrtMallocFunc>(ACL_RT_API_aclrtMalloc, "aclrtMalloc");
        const auto memcpyFunction = GetOriginalRuntimeFunction<aclrtMemcpyFunc>(ACL_RT_API_aclrtMemcpy, "aclrtMemcpy");
        aclError status = mallocFunction(&prepared.deviceBuffer, prepared.hostBuffer.size(), ACL_MEM_MALLOC_HUGE_FIRST);
        if (status == ACL_SUCCESS) {
            status = memcpyFunction(
                prepared.deviceBuffer, prepared.hostBuffer.size(), prepared.hostBuffer.data(),
                prepared.hostBuffer.size(), ACL_MEMCPY_HOST_TO_DEVICE);
        }
        if (status != ACL_SUCCESS) {
            ASCTOOL_ERROR("acl_san trace: cannot allocate or initialize launch GM buffer, status=%d", status);
            ReleaseDeviceBuffer(prepared.deviceBuffer);
            prepared.deviceBuffer = nullptr;
            StoreHiddenPointer(prepared, argumentMode);
            return StrictModeEnabled() ? status : ACL_SUCCESS;
        }

        StoreHiddenPointer(prepared, argumentMode);
        return ACL_SUCCESS;
    } catch (const std::bad_alloc&) {
        ReleaseDeviceBuffer(prepared.deviceBuffer);
        prepared.deviceBuffer = nullptr;
        ASCTOOL_ERROR("acl_san trace: out of memory while preparing launch");
        return ACL_ERROR_BAD_ALLOC;
    } catch (...) {
        ReleaseDeviceBuffer(prepared.deviceBuffer);
        prepared.deviceBuffer = nullptr;
        ASCTOOL_ERROR("acl_san trace: unexpected failure while preparing launch");
        return ACL_ERROR_FAILURE;
    }
}

aclError MaterializeTraceHostInputs(PreparedTraceLaunch& prepared) noexcept
{
    if (!prepared.instrumented || prepared.placeholders.empty()) {
        return ACL_SUCCESS;
    }
    std::vector<PreparedTraceLaunch::HostInput> inputs;
    const auto fail = [&](aclError status) {
        ReleaseHostInputs(inputs);
        return status;
    };
    try {
        const auto allocate = GetOriginalRuntimeFunction<aclrtMallocFunc>(ACL_RT_API_aclrtMalloc, "aclrtMalloc");
        const auto copy = GetOriginalRuntimeFunction<aclrtMemcpyFunc>(ACL_RT_API_aclrtMemcpy, "aclrtMemcpy");
        inputs.reserve(prepared.placeholders.size());
        for (size_t index = 0; index < prepared.placeholders.size(); ++index) {
            const auto& placeholder = prepared.placeholders[index];
            if (placeholder.addrOffset % sizeof(void*) != 0 || placeholder.addrOffset > prepared.traceArgumentOffset ||
                prepared.traceArgumentOffset - placeholder.addrOffset < sizeof(void*) ||
                placeholder.dataOffset < prepared.traceArgumentOffset + sizeof(void*) ||
                placeholder.dataOffset >= prepared.arguments.size()) {
                return fail(ACL_ERROR_INVALID_PARAM);
            }
            bool alias = false;
            for (size_t previous = 0; previous < index; ++previous) {
                const auto& earlier = prepared.placeholders[previous];
                if (earlier.addrOffset == placeholder.addrOffset) {
                    return fail(ACL_ERROR_INVALID_PARAM);
                }
                if (earlier.dataOffset == placeholder.dataOffset) {
                    std::memcpy(
                        prepared.arguments.data() + placeholder.addrOffset,
                        prepared.arguments.data() + earlier.addrOffset, sizeof(void*));
                    alias = true;
                }
            }
            if (alias) {
                continue;
            }
            size_t end = prepared.arguments.size();
            for (const auto& next : prepared.placeholders) {
                if (next.dataOffset > placeholder.dataOffset) {
                    end = std::min(end, static_cast<size_t>(next.dataOffset));
                }
            }
            const size_t bytes = end - placeholder.dataOffset;
            if (bytes > SIZE_MAX - 31U) {
                return fail(ACL_ERROR_INVALID_PARAM);
            }
            std::vector<uint8_t> payload((bytes + 31U) & ~size_t(31U), 0);
            ASCTOOL_DEBUG(
                "acl_san HostInput: launch=%llu index=%zu addrOffset=%u dataOffset=%u payloadBytes=%zu "
                "allocationBytes=%zu",
                static_cast<unsigned long long>(prepared.launchId), index, placeholder.addrOffset,
                placeholder.dataOffset, bytes, payload.size());
            std::memcpy(payload.data(), prepared.arguments.data() + placeholder.dataOffset, bytes);
            void* address = nullptr;
            aclError status = allocate(&address, payload.size(), ACL_MEM_MALLOC_HUGE_FIRST);
            if (status != ACL_SUCCESS) {
                return fail(status);
            }
            inputs.push_back({address, payload.size()});
            status = copy(address, payload.size(), payload.data(), payload.size(), ACL_MEMCPY_HOST_TO_DEVICE);
            if (status != ACL_SUCCESS) {
                return fail(status);
            }
            std::memcpy(prepared.arguments.data() + placeholder.addrOffset, &address, sizeof(address));
        }
        prepared.arguments.resize(prepared.traceArgumentOffset + sizeof(void*));
        prepared.placeholders.clear();
        prepared.hostInputs = std::move(inputs);
        return ACL_SUCCESS;
    } catch (const std::bad_alloc&) {
        return fail(ACL_ERROR_BAD_ALLOC);
    } catch (...) {
        return fail(ACL_ERROR_FAILURE);
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
        ReleaseHostInputs(prepared.hostInputs);
        return;
    }

    try {
        PendingTrace pending{
            prepared.launchId,
            function,
            stream,
            prepared.deviceBuffer,
            prepared.blockCount,
            prepared.physicalCoreCount,
            prepared.deviceId,
            prepared.decoder,
            std::move(prepared.hostBuffer),
            std::move(prepared.binaryLease),
            std::move(prepared.arguments),
            std::move(prepared.placeholders),
            std::move(prepared.hostInputs),
            std::vector<uint32_t>(prepared.physicalCoreCount, 0U)};
        TraceRuntimeState& state = State();
        std::lock_guard<std::mutex> lock(state.mutex);
        state.pending.push_back(std::move(pending));
    } catch (...) {
        ASCTOOL_ERROR(
            "acl_san trace: failed to retain launch=%llu", static_cast<unsigned long long>(prepared.launchId));
        AbortHookFailure("launch", "retain_trace", "cannot retain resources for an in-flight kernel");
    }
}

TraceCollectionResult CollectTraceStream(aclrtStream stream, bool executionComplete) noexcept
{
    TraceCollectionResult collection;
    std::vector<PendingTrace> completed;
    try {
        {
            TraceRuntimeState& state = State();
            std::lock_guard<std::mutex> lock(state.mutex);
            for (auto it = state.pending.begin(); it != state.pending.end();) {
                if (it->stream != stream) {
                    ++it;
                    continue;
                }
                // A timed-out deferred kernel may still own HostInput and variant resources. Read a trace snapshot so
                // diagnostics emitted before the blocking instruction are visible, but keep all launch resources
                // until Runtime confirms completion or resets the Device.
                if (!executionComplete && it->binaryLease) {
                    ++collection.launchCount;
                    (void)DispatchTraceSnapshot(*it, false, collection);
                    ++it;
                } else {
                    completed.push_back(std::move(*it));
                    it = state.pending.erase(it);
                }
            }
        }
    } catch (...) {
        ASCTOOL_ERROR("acl_san trace: failed to detach completed launches for stream=%p", stream);
        return collection;
    }

    collection.launchCount += completed.size();
    for (PendingTrace& pending : completed) {
        try {
            (void)DispatchTraceSnapshot(pending, true, collection);
        } catch (...) {
            ASCTOOL_ERROR(
                "acl_san trace: unexpected D2H processing failure for launch=%llu",
                static_cast<unsigned long long>(pending.launchId));
        }
        ReleaseDeviceBuffer(pending.deviceBuffer);
        ReleaseHostInputs(pending.hostInputs);
    }
    return collection;
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
