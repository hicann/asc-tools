/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "probe_runtime.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <sstream>
#include <unistd.h>
#include <vector>

namespace aclsan::probe {
namespace {

std::string SymbolizerPath()
{
    const char* configured = std::getenv("ACLSAN_SYMBOLIZER");
    if (configured != nullptr && configured[0] != '\0') {
        return configured;
    }

    const char* ascendHome = std::getenv("ASCEND_HOME_PATH");
    if (ascendHome != nullptr && ascendHome[0] != '\0') {
        std::string root = ascendHome;
        while (!root.empty() && root.back() == '/') {
            root.pop_back();
        }
        const std::vector<std::string> candidates{
            root + "/tools/mssanitizer/bin/llvm-symbolizer", root + "/tools/msopprof/bin/llvm-symbolizer"};
        for (const std::string& candidate : candidates) {
            if (access(candidate.c_str(), X_OK) == 0) {
                return candidate;
            }
        }
    }
    return "llvm-symbolizer";
}

struct ProbeLaunchLayout {
    uint32_t aicBlocks = 0;
    uint32_t aivBlocks = 0;
    uint32_t totalBlocks = 0;
    uint32_t aivOffset = 0;
};

bool MultiplyBlockCount(uint32_t blockCount, uint16_t ratio, uint32_t& result) noexcept
{
    if (ratio != 0 && blockCount > std::numeric_limits<uint32_t>::max() / ratio) {
        return false;
    }
    result = blockCount * ratio;
    return true;
}

aclError GetLaunchLayout(
    aclrtFuncHandle function, uint32_t blockCount, const ProbeRuntimeApi& api, ProbeLaunchLayout& layout) noexcept
{
    if (function == nullptr || blockCount == 0 || api.getFunctionAttribute == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }

    int64_t kernelTypeValue = 0;
    aclError result = api.getFunctionAttribute(function, ACL_FUNC_ATTR_KERNEL_TYPE, &kernelTypeValue);
    if (result != ACL_SUCCESS) {
        return result;
    }

    const auto kernelType = static_cast<aclrtKernelType>(kernelTypeValue);
    if (kernelType == ACL_KERNEL_TYPE_CUBE) {
        layout.aicBlocks = blockCount;
    } else if (kernelType == ACL_KERNEL_TYPE_VECTOR) {
        layout.aivBlocks = blockCount;
    } else if (kernelType == ACL_KERNEL_TYPE_AICORE || kernelType == ACL_KERNEL_TYPE_MIX) {
        int64_t kernelRatioValue = 0;
        result = api.getFunctionAttribute(function, ACL_FUNC_ATTR_KERNEL_RATIO, &kernelRatioValue);
        if (result != ACL_SUCCESS) {
            return result;
        }
        const uint64_t packedRatio = static_cast<uint64_t>(kernelRatioValue);
        const uint16_t aicRatio = static_cast<uint16_t>((packedRatio >> 16U) & 0xffffU);
        const uint16_t aivRatio = static_cast<uint16_t>(packedRatio & 0xffffU);
        if ((aicRatio == 0 && aivRatio == 0) || !MultiplyBlockCount(blockCount, aicRatio, layout.aicBlocks) ||
            !MultiplyBlockCount(blockCount, aivRatio, layout.aivBlocks)) {
            return ACL_ERROR_RT_INTERNAL_ERROR;
        }
    } else {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }

    if (layout.aicBlocks > std::numeric_limits<uint32_t>::max() - layout.aivBlocks) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    layout.totalBlocks = layout.aicBlocks + layout.aivBlocks;
    layout.aivOffset = layout.aicBlocks;
    if (layout.totalBlocks == 0 ||
        layout.totalBlocks > std::numeric_limits<size_t>::max() / sanitizer::kProbeBlockBytes) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    return ACL_SUCCESS;
}

} // namespace

aclError ProbeRuntime::LoadBinary(
    const void* data, size_t length, const aclrtBinaryLoadOptions* options, aclrtBinHandle* binHandle,
    const ImageTransformConfig& config, const ProbeRuntimeApi& api, ImageTransformFunction transform,
    std::string& error)
{
    if (api.binaryLoadFromData == nullptr || transform == nullptr || binHandle == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (binary_ != nullptr) {
            return api.binaryLoadFromData(data, length, options, binHandle);
        }
    }

    ImageTransformResult transformed;
    if (!transform(data, length, config, transformed, error)) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    std::unique_ptr<DeviceSymbolizer> symbolizer;
    try {
        if (!transformed.originalImage.empty() && !transformed.sessionDirectory.empty()) {
            symbolizer = std::make_unique<DeviceSymbolizer>(
                DeviceSymbolizerConfig{SymbolizerPath(), transformed.originalImage, transformed.sessionDirectory});
        }
    } catch (const std::bad_alloc&) {
        error = "cannot allocate device symbolizer";
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    const aclError result =
        api.binaryLoadFromData(transformed.image.data(), transformed.image.size(), options, binHandle);
    if (result != ACL_SUCCESS) {
        return result;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    binary_ = *binHandle;
    binaryId_ = nextBinaryId_++;
    if (nextBinaryId_ == 0) {
        nextBinaryId_ = 1;
    }
    transformed_ = std::move(transformed);
    symbolizer_ = std::move(symbolizer);
    return ACL_SUCCESS;
}

aclError ProbeRuntime::GetFunction(
    aclrtBinHandle binHandle, const char* kernelName, aclrtFuncHandle* funcHandle, const ProbeRuntimeApi& api)
{
    if (api.binaryGetFunction == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    const aclError result = api.binaryGetFunction(binHandle, kernelName, funcHandle);
    if (result == ACL_SUCCESS && funcHandle != nullptr && *funcHandle != nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (binary_ == binHandle) {
            functions_.insert(*funcHandle);
        }
    }
    return result;
}

aclError ProbeRuntime::GetFunctionByEntry(
    aclrtBinHandle binHandle, uint64_t functionEntry, aclrtFuncHandle* funcHandle, const ProbeRuntimeApi& api)
{
    if (api.binaryGetFunctionByEntry == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    const aclError result = api.binaryGetFunctionByEntry(binHandle, functionEntry, funcHandle);
    if (result == ACL_SUCCESS && funcHandle != nullptr && *funcHandle != nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (binary_ == binHandle) {
            functions_.insert(*funcHandle);
        }
    }
    return result;
}

void ProbeRuntime::RecordFunction(aclrtFuncHandle function)
{
    if (function == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (binary_ != nullptr) {
        functions_.insert(function);
    }
}

bool ProbeRuntime::IsTargetFunction(aclrtFuncHandle function) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return functions_.find(function) != functions_.end();
}

aclError ProbeRuntime::BindGlobal(const ProbeRuntimeApi& api, const char* name, const void* value, size_t valueBytes)
{
    if (api.binaryGetGlobal == nullptr || api.memcpy == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    void* slot = nullptr;
    size_t slotBytes = 0;
    aclError result = api.binaryGetGlobal(binary_, name, &slot, &slotBytes);
    if (result != ACL_SUCCESS) {
        return result;
    }
    if (slot == nullptr || slotBytes < valueBytes) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    return api.memcpy(slot, slotBytes, value, valueBytes, ACL_MEMCPY_HOST_TO_DEVICE);
}

aclError ProbeRuntime::PrepareLaunch(
    aclrtFuncHandle function, uint32_t blockCount, aclrtStream stream, const ProbeRuntimeApi& api)
{
    if (!IsTargetFunction(function)) {
        return ACL_SUCCESS;
    }
    if (api.mallocDevice == nullptr || api.freeDevice == nullptr || api.memcpy == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    ProbeLaunchLayout layout;
    const aclError layoutResult = GetLaunchLayout(function, blockCount, api, layout);
    if (layoutResult != ACL_SUCCESS) {
        return layoutResult;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (pending_) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    const size_t requiredBytes = static_cast<size_t>(layout.totalBlocks) * sanitizer::kProbeBlockBytes;
    if (probeOutput_ == nullptr || probeBytes_ != requiredBytes) {
        if (probeOutput_ != nullptr) {
            const aclError freeResult = api.freeDevice(probeOutput_);
            if (freeResult != ACL_SUCCESS) {
                return freeResult;
            }
            probeOutput_ = nullptr;
        }
        const aclError allocResult = api.mallocDevice(&probeOutput_, requiredBytes, ACL_MEM_MALLOC_HUGE_FIRST);
        if (allocResult != ACL_SUCCESS) {
            return allocResult;
        }
        probeBytes_ = requiredBytes;
    }

    std::vector<uint8_t> initial(requiredBytes, 0);
    for (uint32_t block = 0; block < layout.totalBlocks; ++block) {
        const uint32_t logicalBlock = block < layout.aivOffset ? block : block - layout.aivOffset;
        const sanitizer::ProbeBlockHeader header{0, sanitizer::kProbeRecordStartBytes, logicalBlock, 0};
        std::memcpy(initial.data() + static_cast<size_t>(block) * sanitizer::kProbeBlockBytes, &header, sizeof(header));
    }
    aclError result =
        api.memcpy(probeOutput_, requiredBytes, initial.data(), initial.size(), ACL_MEMCPY_HOST_TO_DEVICE);
    if (result != ACL_SUCCESS) {
        return result;
    }
    const uint64_t outputAddress = reinterpret_cast<uint64_t>(probeOutput_);
    result = BindGlobal(api, "g_sanitizerOutput", &outputAddress, sizeof(outputAddress));
    if (result != ACL_SUCCESS) {
        return result;
    }
    result = BindGlobal(api, "g_sanitizerAivOffset", &layout.aivOffset, sizeof(layout.aivOffset));
    if (result != ACL_SUCCESS) {
        return result;
    }
    blockCount_ = layout.totalBlocks;
    aivOffset_ = layout.aivOffset;
    pendingStream_ = stream;
    pending_ = true;
    return ACL_SUCCESS;
}

void ProbeRuntime::RecordLaunchResult(aclrtFuncHandle function, aclrtStream stream, aclError result)
{
    if (!IsTargetFunction(function)) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (pendingStream_ == stream && result != ACL_SUCCESS) {
        pending_ = false;
        pendingStream_ = nullptr;
    }
}

aclError ProbeRuntime::Collect(aclrtStream stream, const ProbeRuntimeApi& api, sanitizer::ProbeParseResult& result)
{
    std::lock_guard<std::mutex> lock(mutex_);
    result = {};
    if (!pending_ || pendingStream_ != stream || probeOutput_ == nullptr) {
        return ACL_SUCCESS;
    }
    if (api.memcpy == nullptr) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    std::vector<uint8_t> output(probeBytes_, 0);
    const aclError copyResult =
        api.memcpy(output.data(), output.size(), probeOutput_, probeBytes_, ACL_MEMCPY_DEVICE_TO_HOST);
    pending_ = false;
    pendingStream_ = nullptr;
    if (copyResult != ACL_SUCCESS) {
        return copyResult;
    }
    std::ostringstream diagnostics;
    if (!sanitizer::ParseProbeOutput(output.data(), output.size(), blockCount_, aivOffset_, diagnostics, result)) {
        return ACL_ERROR_RT_INTERNAL_ERROR;
    }
    return ACL_SUCCESS;
}

aclError ProbeRuntime::Clear(const ProbeRuntimeApi& api)
{
    std::lock_guard<std::mutex> lock(mutex_);
    aclError result = ACL_SUCCESS;
    if (probeOutput_ != nullptr) {
        if (api.freeDevice == nullptr) {
            result = ACL_ERROR_RT_INTERNAL_ERROR;
        } else {
            result = api.freeDevice(probeOutput_);
        }
    }
    binary_ = nullptr;
    binaryId_ = 0;
    functions_.clear();
    transformed_ = {};
    symbolizer_.reset();
    probeOutput_ = nullptr;
    probeBytes_ = 0;
    blockCount_ = 0;
    aivOffset_ = 0;
    pendingStream_ = nullptr;
    pending_ = false;
    return result;
}

CallStackResult ProbeRuntime::ResolveCallStack(uint64_t pc) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (symbolizer_ == nullptr) {
        return CallStackResult{pc, 0, false, "invalid_state", {}};
    }
    CallStackResult result = symbolizer_->ResolveCallStack(pc);
    result.binaryId = binaryId_;
    return result;
}

CallStackResult ProbeRuntime::ResolveCallStackWithRunner(uint64_t pc, const CommandRunner& runner) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (symbolizer_ == nullptr) {
        return CallStackResult{pc, 0, false, "invalid_state", {}};
    }
    CallStackResult result = symbolizer_->ResolveCallStackWithRunner(pc, runner);
    result.binaryId = binaryId_;
    return result;
}

bool ProbeRuntime::OwnsBinary(aclrtBinHandle binHandle) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return binary_ != nullptr && binary_ == binHandle;
}

bool ProbeRuntime::HasPending(aclrtStream stream) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_ && pendingStream_ == stream;
}

} // namespace aclsan::probe
